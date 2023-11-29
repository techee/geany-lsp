/*
 * Copyright 2023 Jiri Techet <techet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "lsp/lsp-server.h"
#include "lsp/lsp-utils.h"
#include "lsp/lsp-client.h"
#include "lsp/lsp-sync.h"
#include "lsp/lsp-diagnostics.h"
#include "lsp/lsp-log.h"
#include "lsp/lsp-semtokens.h"
#include "lsp/lsp-progress.h"
#include "lsp/lsp-symbols.h"
#include "lsp/lsp-symbol-kinds.h"
#include "lsp/lsp-highlight.h"


typedef struct {
	GSubprocess *process;
	GIOStream *stream;
	JsonrpcClient *rpc_client;
	LspLogInfo log;
	gchar *cmd;  //just for logging
} ShutdownServerInfo;


static void start_lsp_server(LspServer *server, GeanyFiletypeID filetype_id);


extern GeanyData *geany_data;

extern LspProjectConfigurationType project_configuration_type;
static GPtrArray *lsp_servers = NULL;
static GPtrArray *servers_in_shutdown = NULL;


static void free_shuthown_info(ShutdownServerInfo *info)
{
	lsp_log_stop(info->log);
	g_object_unref(info->process);
	lsp_client_close(info->rpc_client);
	g_object_unref(info->rpc_client);
	//TODO: check if stream should be closed
	g_object_unref(info->stream);
	g_free(info->cmd);
	g_free(info);
}


static void force_terminate(ShutdownServerInfo *info)
{
	g_subprocess_send_signal(info->process, SIGTERM);
	//TODO: check if sleep can be added here and if g_subprocess_send_signal() is executed immediately
	g_subprocess_force_exit(info->process);
}


static void exit_cb(GObject *object, GAsyncResult *result, gpointer user_data)
{
	JsonrpcClient *self = (JsonrpcClient *)object;
	ShutdownServerInfo *info = user_data;

	if (!lsp_client_notify_finish(self, result))
		force_terminate(info);

	g_ptr_array_remove_fast(servers_in_shutdown, info);
}


static void shutdown_cb(GObject *object, GAsyncResult *result, gpointer user_data)
{
	JsonrpcClient *self = (JsonrpcClient *)object;
	GVariant *return_value = NULL;
	ShutdownServerInfo *info = user_data;

	if (lsp_client_call_finish(self, result, &return_value, NULL))
	{
		msgwin_status_add("Sending exit notification to LSP server %s", info->cmd);
		lsp_client_notify_async(info->rpc_client, "exit", NULL, exit_cb, info);

		if (return_value)
			g_variant_unref(return_value);
	}
	else
	{
		msgwin_status_add("Force terminating LSP server %s", info->cmd);
		force_terminate(info);
		g_ptr_array_remove_fast(servers_in_shutdown, info);
	}
}


static void stop_process(LspServer *s, gboolean terminate)
{
	ShutdownServerInfo *info = g_new0(ShutdownServerInfo, 1);

	info->process = s->process;
	s->process = NULL;
	info->rpc_client = s->rpc_client;
	s->rpc_client = NULL;
	info->stream = s->stream;
	s->stream = NULL;
	info->log = s->log;
	s->log.stream = NULL;
	info->cmd = g_strdup(s->cmd);

	g_ptr_array_add(servers_in_shutdown, info);

	if (terminate)
	{
		msgwin_status_add("Sending shutdown request to LSP server %s", s->cmd);
		lsp_client_call_async(info->rpc_client, "shutdown", NULL, shutdown_cb, info);
	}
	else
		g_ptr_array_remove_fast(servers_in_shutdown, info);

}


static void stop_server(LspServer *s)
{
	if (s->process)
	{
		s->startup_shutdown = TRUE;
		stop_process(s, TRUE);
	}
}


static void free_server(gpointer data)
{
	LspServer *s = data;
	LspServerConfig *cfg = &s->config;

	stop_server(s);

	g_free(s->cmd);
	g_free(s->ref_lang);
	g_free(s->autocomplete_trigger_chars);
	g_free(s->signature_trigger_chars);
	g_free(s->initialize_response);
	lsp_progress_free_all(s);

	g_strfreev(cfg->autocomplete_trigger_sequences);
	g_free(cfg->semantic_tokens_type_style);
	g_free(cfg->diagnostics_error_style);
	g_free(cfg->diagnostics_warning_style);
	g_free(cfg->diagnostics_info_style);
	g_free(cfg->diagnostics_hint_style);
	g_free(cfg->highlighting_style);
	g_free(cfg->formatting_options_file);

	g_free(s);
}


static void log_message(GVariant *params)
{
	gint64 type;
	const gchar *msg;
	gboolean success;

	success = JSONRPC_MESSAGE_PARSE(params,
		"type", JSONRPC_MESSAGE_GET_INT64(&type),
		"message", JSONRPC_MESSAGE_GET_STRING(&msg));

	if (success)
	{
		const gchar *type_str;
		gchar *stripped_msg = g_strdup(msg);

		switch (type)
		{
			case 1:
				type_str = "Error";
				break;
			case 2:
				type_str = "Warning";
				break;
			case 3:
				type_str = "Info";
				break;
			case 4:
				type_str = "Log";
				break;
			default:
				type_str = "Debug";
				break;
		}

		g_strstrip(stripped_msg);
		msgwin_status_add("%s: %s", type_str, stripped_msg);
		g_free(stripped_msg);
	}
}


static LspServer *srv_from_rpc_client(JsonrpcClient *client)
{
	if (lsp_servers)
	{
		gint i;
		for (i = 0; i < lsp_servers->len; i++)
		{
			LspServer *s = lsp_servers->pdata[i];
			if (s->rpc_client == client)
				return s;
		}
	}

	return NULL;
}


LspLogInfo lsp_server_get_log_info(JsonrpcClient *client)
{
	LspLogInfo empty = {0, TRUE, NULL};
	LspServer *s;
	gint i;

	if (servers_in_shutdown)
	{
		for (i = 0; i < servers_in_shutdown->len; i++)
		{
			ShutdownServerInfo *s = servers_in_shutdown->pdata[i];
			if (s->rpc_client == client)
				return s->log;
		}
	}

	s = srv_from_rpc_client(client);
	if (s)
		return s->log;

	return empty;
}


static void handle_notification(JsonrpcClient *self, gchar *method, GVariant *params,
	gpointer user_data)
{
	lsp_log(lsp_server_get_log_info(self), LspLogServerNotificationSent, method, params);

	if (g_strcmp0(method, "textDocument/publishDiagnostics") == 0)
		lsp_diagnostics_received(params);
	else if (g_strcmp0(method, "window/logMessage") == 0 ||
		g_strcmp0(method, "window/showMessage") == 0)
	{
		log_message(params);
	}
	else if (g_str_has_prefix(method, "$/"))
	{
		LspServer *srv = srv_from_rpc_client(self);
		lsp_progress_process_notification(srv, params);
	}
	else
	{
		//printf("\n\nNOTIFICATION FROM SERVER: %s\n", method);
		//printf("params:\n%s\n\n\n", lsp_utils_json_pretty_print(params));
	}
}


static gboolean handle_call(JsonrpcClient *self, gchar* method, GVariant *id, GVariant *params,
	gpointer user_data)
{
	JsonNode *node = json_from_string("{}", NULL);
	GVariant *variant = json_gvariant_deserialize(node, NULL, NULL);
	gboolean ret = FALSE;

	lsp_log(lsp_server_get_log_info(self), LspLogServerMessageSent, method, params);

	//printf("\n\nREQUEST FROM SERVER: %s\n", method);
	//printf("params:\n%s\n\n\n", lsp_utils_json_pretty_print(params));

	if (g_strcmp0(method, "window/workDoneProgress/create") == 0)
	{
		LspServer *srv = srv_from_rpc_client(self);
		gboolean have_token = FALSE;
		const gchar *token_str = NULL;
		gint64 token_int = 0;

		have_token = JSONRPC_MESSAGE_PARSE(params,
			"token", JSONRPC_MESSAGE_GET_STRING(&token_str)
		);
		if (!have_token)
		{
			have_token = JSONRPC_MESSAGE_PARSE(params,
				"token", JSONRPC_MESSAGE_GET_INT64(&token_int)
			);
		}

		if (srv && have_token)
		{
			LspProgressToken token = {token_int, (gchar *)token_str};
			lsp_progress_create(srv, token);
		}

		jsonrpc_client_reply_async(self, id, NULL, NULL, NULL, NULL);
		ret = TRUE;
	}
	else if (g_strcmp0(method, "workspace/applyEdit") == 0)
	{
		GVariant *edit, *node;
		gboolean success = FALSE;

		JSONRPC_MESSAGE_PARSE(params,
			"edit", JSONRPC_MESSAGE_GET_VARIANT(&edit)
		);

		success = lsp_utils_apply_workspace_edit(edit);

		node = JSONRPC_MESSAGE_NEW(
			"applied", JSONRPC_MESSAGE_PUT_BOOLEAN(success)
		);

		jsonrpc_client_reply_async(self, id, node, NULL, NULL, NULL);

		g_variant_unref(node);
		g_variant_unref(edit);
		ret = TRUE;
	}

	lsp_log(lsp_server_get_log_info(self), LspLogServerMessageReceived, method, variant);
	g_variant_unref(variant);
	json_node_free(node);

	return ret;
}


static gchar *get_autocomplete_trigger_chars(GVariant *node)
{
	GVariantIter *iter = NULL;
	GString *str = g_string_new("");

	JSONRPC_MESSAGE_PARSE(node,
		"capabilities", "{",
			"completionProvider", "{",
				"triggerCharacters", JSONRPC_MESSAGE_GET_ITER(&iter),
			"}",
		"}");

	if (iter)
	{
		GVariant *val = NULL;
		while (g_variant_iter_loop(iter, "v", &val))
			g_string_append(str, g_variant_get_string(val, NULL));
		g_variant_iter_free(iter);
	}

	return g_string_free(str, FALSE);
}


static gboolean supports_semantic_tokens(GVariant *node)
{
	gboolean val = FALSE;

	JSONRPC_MESSAGE_PARSE(node,
		"capabilities", "{",
			"semanticTokensProvider", "{",
				"full", "{",
					"delta", JSONRPC_MESSAGE_GET_BOOLEAN(&val),
				"}",
			"}",
		"}");

	return val;
}


static guint64 get_semantic_token_mask(GVariant *node)
{
	guint64 mask = 0;
	guint64 index = 1;
	GVariantIter *iter = NULL;

	JSONRPC_MESSAGE_PARSE(node,
		"capabilities", "{",
			"semanticTokensProvider", "{",
				"legend", "{",
					"tokenTypes", JSONRPC_MESSAGE_GET_ITER(&iter),
				"}",
			"}",
		"}");

	if (iter)
	{
		GVariant *val = NULL;
		while (g_variant_iter_loop(iter, "v", &val))
		{
			const gchar *str = g_variant_get_string(val, NULL);
			if (g_strcmp0(str, "namespace") == 0 ||
				g_strcmp0(str, "type") == 0 ||
				g_strcmp0(str, "class") == 0 ||
				g_strcmp0(str, "enum") == 0 ||
				g_strcmp0(str, "interface") == 0 ||
				g_strcmp0(str, "struct") == 0 ||
				g_strcmp0(str, "decorator") == 0)
			{
				mask |= index;
			}

			index <<= 1;
		}
		g_variant_iter_free(iter);
	}

	return mask;
}


static gchar *get_signature_trigger_chars(GVariant *node)
{
	GVariantIter *iter = NULL;
	GString *str = g_string_new("");

	JSONRPC_MESSAGE_PARSE(node,
		"capabilities", "{",
			"signatureHelpProvider", "{",
				"triggerCharacters", JSONRPC_MESSAGE_GET_ITER(&iter),
			"}",
		"}");

	if (iter)
	{
		GVariant *val = NULL;
		while (g_variant_iter_loop(iter, "v", &val))
			g_string_append(str, g_variant_get_string(val, NULL));
		g_variant_iter_free(iter);
	}

	return g_string_free(str, FALSE);
}


static gboolean use_incremental_sync(GVariant *node)
{
	gint64 val;

	gboolean success = JSONRPC_MESSAGE_PARSE(node,
		"capabilities", "{",
			"textDocumentSync", "{",
				"change", JSONRPC_MESSAGE_GET_INT64(&val),
			"}",
		"}");

	if (!success)
	{
		success = JSONRPC_MESSAGE_PARSE(node,
			"capabilities", "{",
				"textDocumentSync", JSONRPC_MESSAGE_GET_INT64(&val),
			"}");
	}

	// not supporting "0", i.e. no sync - not sure if any server uses it and how
	// Geany could work with it
	return success && val == 2;
}


static void update_config(GVariant *variant, gboolean *option, const gchar *key)
{
	gboolean val = FALSE;
	JSONRPC_MESSAGE_PARSE(variant,
		"capabilities", "{",
			key, JSONRPC_MESSAGE_GET_BOOLEAN(&val),
		"}");
	if (!val)
		*option = FALSE;
}


static LspServer *server_get_configured_for_ft(gint ft_id)
{
	LspServer *s;

	if (!lsp_servers || lsp_utils_is_lsp_disabled_for_project())
		return NULL;

	s = lsp_servers->pdata[ft_id];

	if (s->ref_lang)
	{
		GeanyFiletype *ft = filetypes_lookup_by_name(s->ref_lang);

		if (ft)
			s = lsp_servers->pdata[ft->id];
		else
			return NULL;
	}

	return s;
}


static void initialize_cb(GObject *object, GAsyncResult *result, gpointer user_data)
{
	JsonrpcClient *self = (JsonrpcClient *)object;
	GVariant *return_value = NULL;
	gint filetype_id = GPOINTER_TO_INT(user_data);

	if (lsp_client_call_finish(self, result, &return_value, NULL))
	{
		LspServer *s = server_get_configured_for_ft(filetype_id);

		if (s && s->rpc_client == self)
		{
			GeanyDocument *current_doc = document_get_current();
			guint i;

			g_free(s->autocomplete_trigger_chars);
			s->autocomplete_trigger_chars = get_autocomplete_trigger_chars(return_value);
			if (!*s->autocomplete_trigger_chars)
				s->config.autocomplete_enable = FALSE;

			g_free(s->signature_trigger_chars);
			s->signature_trigger_chars = get_signature_trigger_chars(return_value);
			if (!*s->signature_trigger_chars)
				s->config.signature_enable = FALSE;

			update_config(return_value, &s->config.hover_enable, "hoverProvider");
			update_config(return_value, &s->config.goto_enable, "definitionProvider");
			update_config(return_value, &s->config.document_symbols_enable, "documentSymbolProvider");
			update_config(return_value, &s->config.highlighting_enable, "documentHighlightProvider");

			s->supports_workspace_symbols = TRUE;
			update_config(return_value, &s->supports_workspace_symbols, "workspaceSymbolProvider");

			s->use_incremental_sync = use_incremental_sync(return_value);

			s->initialize_response = lsp_utils_json_pretty_print(return_value);
			//printf("%s\n", s->initialize_response);

			if (!supports_semantic_tokens(return_value))
				s->config.semantic_tokens_enable = FALSE;
			s->semantic_token_mask = get_semantic_token_mask(return_value);

			msgwin_status_add("LSP server %s initialized", s->cmd);

			lsp_client_notify(s->rpc_client, "initialized", NULL);
			s->startup_shutdown = FALSE;

			lsp_semtokens_init(filetype_id);

			foreach_document(i)
			{
				GeanyDocument *doc = documents[i];

				// see on_document_activate() for detailed comment
				if (doc->file_type->id == filetype_id && (doc->changed || doc == current_doc))
				{
					// returns NULL if e.g. configured not to use LSP outside project dir
					LspServer *s2 = lsp_server_get_if_running(doc);

					if (s2)
					{
						lsp_sync_text_document_did_open(s, doc);
						if (doc == current_doc)
						{
							lsp_diagnostics_style_current_doc(s);
							lsp_diagnostics_redraw_current_doc(s);
							lsp_highlight_style_current_doc(s);
							lsp_semtokens_style_current_doc(s);
						}
					}
				}
			}
		}

		if (return_value)
			g_variant_unref(return_value);
	}
	else
	{
		LspServer *s = server_get_configured_for_ft(filetype_id);

		if (s && s->rpc_client == self)
		{
			msgwin_status_add("LSP initialize request failed for LSP server %s", s->cmd);
			stop_process(s, TRUE);
			start_lsp_server(s, filetype_id);
		}
	}
}


static void perform_initialize(LspServer *server, GeanyFiletypeID ft)
{
	GVariant *node;

	gchar *locale = lsp_utils_get_locale();
	gchar *project_base_uri = NULL;
	gchar *project_base = lsp_utils_get_project_base_path();

	if (project_base)
		project_base_uri = g_filename_to_uri(project_base, NULL, NULL);

	node = JSONRPC_MESSAGE_NEW(
		"processId", JSONRPC_MESSAGE_PUT_INT64(getpid()),
		"clientInfo", "{",
			"name", JSONRPC_MESSAGE_PUT_STRING("Geany"),
			"version", JSONRPC_MESSAGE_PUT_STRING("0.1"),  //VERSION
		"}",
		"locale", JSONRPC_MESSAGE_PUT_STRING(locale),
		"rootPath", JSONRPC_MESSAGE_PUT_STRING(project_base),
		"workspaceFolders", "[", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING (project_base_uri),
			"name", JSONRPC_MESSAGE_PUT_STRING (project_base),
		"}", "]",
		//"rootUri", JSONRPC_MESSAGE_PUT_STRING(project_base_uri),
		"capabilities", "{",
			"window", "{",
				"workDoneProgress", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
			"}",
			"textDocument", "{",
				"synchronization", "{",
					"willSave", JSONRPC_MESSAGE_PUT_BOOLEAN(FALSE),
					"willSaveWaitUntil", JSONRPC_MESSAGE_PUT_BOOLEAN(FALSE),
					"didSave", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
				"}",
				"completion", "{",
					"completionItemKind", "{",
						"valueSet", "[",
							LSP_COMPLETION_KINDS,
						"]",
					"}",
	//				"contxtSupport", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
				"}",
				"hover", "{",
					"contentFormat", "[",
						"plaintext",
					"]",
				"}",
				"documentSymbol", "{",
					"symbolKind", "{",
						"valueSet", "[",
							LSP_SYMBOL_KINDS,
						"]",
					"}",
					"hierarchicalDocumentSymbolSupport", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
				"}",
				"semanticTokens", "{",
					"requests", "{",
						"range", JSONRPC_MESSAGE_PUT_BOOLEAN(FALSE),
						"full", "{",
							"delta", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
						"}",
					"}",
					"tokenTypes", "[",
						"namespace",
						"type",
						"class",
						"enum",
						"interface",
						"struct",
						"decorator",
					"]",
					"tokenModifiers", "[",
					"]",
					"formats", "[",
						"relative",
					"]",
					"overlappingTokenSupport", JSONRPC_MESSAGE_PUT_BOOLEAN(FALSE),
					"multilineTokenSupport", JSONRPC_MESSAGE_PUT_BOOLEAN(FALSE),
					"serverCancelSupport", JSONRPC_MESSAGE_PUT_BOOLEAN(FALSE),
					"augmentsSyntaxTokens", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
				"}",
			"}",
			"workspace", "{",
				"symbol", "{",
					"symbolKind", "{",
						"valueSet", "[",
							LSP_SYMBOL_KINDS,
						"]",
					"}",
				"}",
			"}",
		"}",
		"trace", JSONRPC_MESSAGE_PUT_STRING("off"),
		"initializationOptions", "{",
			JSONRPC_MESSAGE_PUT_VARIANT(
				lsp_utils_parse_json_file(server->config.initialization_options_file)),
		"}"
	);

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	msgwin_status_add("Sending initialize request to LSP server %s", server->cmd);

	server->startup_shutdown = TRUE;
	lsp_client_call_async(server->rpc_client, "initialize", node, initialize_cb, GINT_TO_POINTER(ft));

	g_free(locale);
	g_free(project_base);
	g_free(project_base_uri);
	g_variant_unref(node);
}


static GKeyFile *read_keyfile(const gchar *config_file)
{
	GError *error = NULL;
	GKeyFile *kf = g_key_file_new();

	if (!g_key_file_load_from_file(kf, config_file, G_KEY_FILE_NONE, &error))
	{
		msgwin_status_add("Failed to load LSP configuration file with message %s", error->message);
		g_error_free(error);
	}

	return kf;
}


static void process_stopped(GObject *source_object, GAsyncResult *res, gpointer data)
{
	GSubprocess *process = (GSubprocess *)source_object;
	GeanyFiletypeID filetype_id = GPOINTER_TO_INT(data);
	LspServer *s = server_get_configured_for_ft(filetype_id);

	if (s && s->process == process)
	{
		msgwin_status_add("LSP server %s stopped, restarting", s->cmd);
		stop_process(s, FALSE);
		start_lsp_server(s, filetype_id);
	}
}


static gboolean is_dead(LspServer *server)
{
	return server->restarts > 5;
}


static void start_lsp_server(LspServer *server, GeanyFiletypeID filetype_id)
{
	GInputStream *input_stream;
	GOutputStream *output_stream;
	GError *error = NULL;
	gchar ** argv;
	gint flags = G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE;

	server->restarts++;
	if (is_dead(server))
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, "LSP server %s terminated more than 5 times, giving up", server->cmd);
		return;
	}

	argv = g_strsplit_set(server->cmd, " ", -1);

	if (!server->config.show_server_stderr)
		flags |= G_SUBPROCESS_FLAGS_STDERR_SILENCE;
	server->process = g_subprocess_newv((const gchar * const *)argv, flags, &error);

	g_strfreev(argv);

	if (!server->process)
	{
		msgwin_status_add("LSP server process %s failed to start with error message: %s", server->cmd, error->message);
		g_error_free(error);
		return;
	}

	g_subprocess_wait_async(server->process, NULL, process_stopped, GINT_TO_POINTER(filetype_id));

	input_stream = g_subprocess_get_stdout_pipe(server->process);
	output_stream = g_subprocess_get_stdin_pipe(server->process);
	server->stream = g_simple_io_stream_new(input_stream, output_stream);

	server->rpc_client = lsp_client_new(server->stream);
	lsp_client_start_listening(server->rpc_client);
	server->log = lsp_log_start(&server->config);

	g_signal_connect(server->rpc_client, "handle-call", G_CALLBACK(handle_call), NULL);
	g_signal_connect(server->rpc_client, "notification", G_CALLBACK(handle_notification), NULL);

	perform_initialize(server, filetype_id);
}


static void get_bool(gboolean *dest, GKeyFile *kf, const gchar *section, const gchar *key)
{
	GError *error = NULL;
	gboolean bool_val = g_key_file_get_boolean(kf, section, key, &error);

	if (!error)
		*dest = bool_val;
	else
		g_error_free(error);
}


static void get_str(gchar **dest, GKeyFile *kf, const gchar *section, const gchar *key)
{
	gchar *str_val = g_key_file_get_string(kf, section, key, NULL);

	if (str_val)
	{
		g_free(*dest);
		*dest = str_val;
	}
}


static void get_int(gint *dest, GKeyFile *kf, const gchar *section, const gchar *key)
{
	GError *error = NULL;
	gint int_val = g_key_file_get_integer(kf, section, key, &error);

	if (!error)
		*dest = int_val;
	else
		g_error_free(error);
}


static void load_config(GKeyFile *kf, gchar *section, LspServer *s)
{
	gchar *str_val;

	get_bool(&s->config.use_outside_project_dir, kf, section, "lsp_use_outside_project_dir");
	get_bool(&s->config.use_without_project, kf, section, "lsp_use_without_project");
	get_bool(&s->config.rpc_log_full, kf, section, "rpc_log_full");

	get_bool(&s->config.autocomplete_enable, kf, section, "autocomplete_enable");

	str_val = g_key_file_get_string(kf, section, "autocomplete_trigger_sequences", NULL);
	if (str_val)
	{
		g_strfreev(s->config.autocomplete_trigger_sequences);
		s->config.autocomplete_trigger_sequences = g_strsplit(str_val, ";", -1);
		g_free(str_val);
	}

	get_int(&s->config.autocomplete_window_max_entries, kf, section, "autocomplete_window_max_entries");
	get_int(&s->config.autocomplete_window_max_displayed, kf, section, "autocomplete_window_max_displayed");
	get_int(&s->config.autocomplete_window_max_width, kf, section, "autocomplete_window_max_width");

	get_bool(&s->config.autocomplete_use_label, kf, section, "autocomplete_use_label");
	get_bool(&s->config.autocomplete_apply_additional_edits, kf, section, "autocomplete_apply_additional_edits");
	get_bool(&s->config.diagnostics_enable, kf, section, "diagnostics_enable");

	get_str(&s->config.diagnostics_error_style, kf, section, "diagnostics_error_style");
	get_str(&s->config.diagnostics_warning_style, kf, section, "diagnostics_warning_style");
	get_str(&s->config.diagnostics_info_style, kf, section, "diagnostics_info_style");
	get_str(&s->config.diagnostics_hint_style, kf, section, "diagnostics_hint_style");

	get_bool(&s->config.hover_enable, kf, section, "hover_enable");
	get_int(&s->config.hover_popup_max_lines, kf, section, "hover_popup_max_lines");
	get_int(&s->config.hover_popup_max_paragraphs, kf, section, "hover_popup_max_paragraphs");
	get_bool(&s->config.signature_enable, kf, section, "signature_enable");
	get_bool(&s->config.goto_enable, kf, section, "goto_enable");
	get_bool(&s->config.document_symbols_enable, kf, section, "document_symbols_enable");
	get_bool(&s->config.show_server_stderr, kf, section, "show_server_stderr");

	get_bool(&s->config.semantic_tokens_enable, kf, section, "semantic_tokens_enable");
	get_str(&s->config.semantic_tokens_type_style, kf, section, "semantic_tokens_type_style");

	get_str(&s->config.formatting_options_file, kf, section, "formatting_options_file");

	get_bool(&s->config.highlighting_enable, kf, section, "highlighting_enable");
	get_str(&s->config.highlighting_style, kf, section, "highlighting_style");
}


static void load_filetype_only_config(GKeyFile *kf, gchar *section, LspServer *s)
{
	get_str(&s->cmd, kf, section, "cmd");
	get_str(&s->ref_lang, kf, section, "use");
	get_str(&s->config.rpc_log, kf, section, "rpc_log");
	get_str(&s->config.initialization_options_file, kf, section, "initialization_options_file");
}


static LspServer *server_get_or_start_for_ft(GeanyFiletype *ft, gboolean launch_server)
{
	LspServer *s, *s2 = NULL;

	if (!ft || !lsp_servers || lsp_utils_is_lsp_disabled_for_project())
		return NULL;

	s = lsp_servers->pdata[ft->id];
	if (s->referenced)
		s = s->referenced;

	if (s->startup_shutdown)
		return NULL;

	if (s->process)
		return s;

	if (s->not_used)
		return NULL;

	if (is_dead(s))
		return NULL;

	if (!launch_server)
		return NULL;

	if (s->ref_lang)
	{
		GeanyFiletype *ft = filetypes_lookup_by_name(s->ref_lang);

		if (ft)
		{
			s2 = g_ptr_array_index(lsp_servers, ft->id);
			s->referenced = s2;
			if (s2->process)
				return s2;
		}
	}

	if (s2)
		s = s2;

	if (s->cmd)
		g_strstrip(s->cmd);
	if (EMPTY(s->cmd))
	{
		g_free(s->cmd);
		s->cmd = NULL;
		s->not_used = TRUE;
	}
	else
	{
		start_lsp_server(s, ft->id);
	}

	// the server isn't initialized when running for the first time because the async
	// handshake with the server hasn't completed yet
	return NULL;
}


LspServer *lsp_server_get_for_ft(GeanyFiletype *ft)
{
	return server_get_or_start_for_ft(ft, TRUE);
}


static gboolean is_lsp_valid_for_doc(LspServerConfig *cfg, GeanyDocument *doc)
{
	gchar *base_path, *real_path, *rel_path;
	gboolean inside_project;

	if (!cfg->use_without_project && !geany_data->app->project)
		return FALSE;

	if (!doc || !doc->real_path)
		return FALSE;

	if (cfg->use_outside_project_dir || !geany_data->app->project)
		return TRUE;

	base_path = lsp_utils_get_project_base_path();
	real_path = utils_get_utf8_from_locale(doc->real_path);
	rel_path = lsp_utils_get_relative_path(base_path, real_path);

	inside_project = rel_path && !g_str_has_prefix(rel_path, "..");

	g_free(rel_path);
	g_free(real_path);
	g_free(base_path);

	return inside_project;
}


static LspServer *server_get_for_doc(GeanyDocument *doc, gboolean launch_server)
{
	LspServer *srv;

	if (!doc)
		return NULL;

	srv = server_get_or_start_for_ft(doc->file_type, launch_server);

	if (!srv || !is_lsp_valid_for_doc(&srv->config, doc))
		return NULL;

	return srv;
}


LspServer *lsp_server_get(GeanyDocument *doc)
{
	return server_get_for_doc(doc, TRUE);
}


LspServer *lsp_server_get_if_running(GeanyDocument *doc)
{
	return server_get_for_doc(doc, FALSE);
}


static LspServer *server_get_configured_for_doc(GeanyDocument *doc)
{
	LspServer *s;

	if (!doc)
		return NULL;

	s = server_get_configured_for_ft(doc->file_type->id);
	if (!s)
		return NULL;

	if (!is_lsp_valid_for_doc(&s->config, doc))
		return NULL;

	return s;
}


LspServerConfig *lsp_server_get_config(GeanyDocument *doc)
{
	LspServer *s = server_get_configured_for_doc(doc);

	if (!s)
		return NULL;

	return &s->config;
}


gboolean lsp_server_is_usable(GeanyDocument *doc)
{
	LspServer *s = server_get_configured_for_doc(doc);

	if (!s)
		return FALSE;

	return !s->not_used && !is_dead(s);
}


void lsp_server_stop_all(gboolean wait)
{
	if (lsp_servers)
		g_ptr_array_free(lsp_servers, TRUE);
	lsp_servers = NULL;

	if (wait)
	{
		GMainContext *main_context = g_main_context_ref_thread_default();

		// this runs the main loop and blocks - otherwise gio won't return async results
		while (servers_in_shutdown->len > 0)
			g_main_context_iteration(main_context, TRUE);

		g_main_context_unref(main_context);
	}
}


void lsp_server_init_all(void)
{
	GKeyFile *kf_global = read_keyfile(lsp_utils_get_global_config_filename());
	GKeyFile *kf = read_keyfile(lsp_utils_get_config_filename());
	GeanyFiletype *ft;
	guint i;

	if (lsp_servers)
		lsp_server_stop_all(FALSE);

	if (!servers_in_shutdown)
		servers_in_shutdown = g_ptr_array_new_full(0, (GDestroyNotify)free_shuthown_info);

	lsp_servers = g_ptr_array_new_full(0, free_server);

	for (i = 0; (ft = filetypes_index(i)); i++)
	{
		LspServer *s = g_new0(LspServer, 1);
		g_ptr_array_add(lsp_servers, s);

		load_config(kf_global, "all", s);
		load_config(kf_global, ft->name, s);
		load_config(kf, "all", s);
		load_config(kf, ft->name, s);

		load_filetype_only_config(kf_global, ft->name, s);
		load_filetype_only_config(kf, ft->name, s);
	}

	g_key_file_free(kf);
	g_key_file_free(kf_global);
}


gboolean lsp_server_uses_init_file(gchar *path)
{
	guint i;

	if (!lsp_servers)
		return FALSE;

	for (i = 0; i < lsp_servers->len; i++)
	{
		LspServer *s = lsp_servers->pdata[i];

		if (s->config.initialization_options_file)
		{
			gboolean found = FALSE;
			gchar *p1 = utils_get_real_path(path);
			gchar *p2 = utils_get_real_path(s->config.initialization_options_file);

			found = g_strcmp0(p1, p2) == 0;

			g_free(p1);
			g_free(p2);

			if (found)
				return TRUE;
		}
	}

	return FALSE;
}


gchar *lsp_server_get_initialize_responses(void)
{
	GString *str = g_string_new("{");
	guint i;
	gboolean first = TRUE;

	if (!lsp_servers)
		return FALSE;

	for (i = 0; i < lsp_servers->len; i++)
	{
		LspServer *s = lsp_servers->pdata[i];

		if (s->cmd && s->initialize_response)
		{
			if (!first)
				g_string_append(str, "\n\n\"############################################################\": \"next server\",");
			first = FALSE;
			g_string_append(str, "\n\n\"");
			g_string_append(str, s->cmd);
			g_string_append(str, "\":\n");
			g_string_append(str, s->initialize_response);
			g_string_append_c(str, ',');
		}
	}
	if (g_str_has_suffix(str->str, ","))
		g_string_erase(str, str->len-1, 1);
	g_string_append(str, "\n}");

	return g_string_free(str, FALSE);
}
