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


#define ALL_AUTOCOMPLETION_KINDS \
	JSONRPC_MESSAGE_PUT_INT32(1),\
	JSONRPC_MESSAGE_PUT_INT32(2),\
	JSONRPC_MESSAGE_PUT_INT32(3),\
	JSONRPC_MESSAGE_PUT_INT32(4),\
	JSONRPC_MESSAGE_PUT_INT32(5),\
	JSONRPC_MESSAGE_PUT_INT32(6),\
	JSONRPC_MESSAGE_PUT_INT32(7),\
	JSONRPC_MESSAGE_PUT_INT32(8),\
	JSONRPC_MESSAGE_PUT_INT32(9),\
	JSONRPC_MESSAGE_PUT_INT32(10),\
	JSONRPC_MESSAGE_PUT_INT32(11),\
	JSONRPC_MESSAGE_PUT_INT32(12),\
	JSONRPC_MESSAGE_PUT_INT32(13),\
	JSONRPC_MESSAGE_PUT_INT32(14),\
	JSONRPC_MESSAGE_PUT_INT32(15),\
	JSONRPC_MESSAGE_PUT_INT32(16),\
	JSONRPC_MESSAGE_PUT_INT32(17),\
	JSONRPC_MESSAGE_PUT_INT32(18),\
	JSONRPC_MESSAGE_PUT_INT32(19),\
	JSONRPC_MESSAGE_PUT_INT32(20),\
	JSONRPC_MESSAGE_PUT_INT32(21),\
	JSONRPC_MESSAGE_PUT_INT32(22),\
	JSONRPC_MESSAGE_PUT_INT32(23),\
	JSONRPC_MESSAGE_PUT_INT32(24),\
	JSONRPC_MESSAGE_PUT_INT32(25),


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

	if (lsp_client_call_finish(self, result, &return_value))
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


static void stop_process(LspServer *s)
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

	msgwin_status_add("Sending shutdown request to LSP server %s", s->cmd);
	lsp_client_call_async(info->rpc_client, "shutdown", NULL, shutdown_cb, info);
}


static void stop_server(LspServer *s)
{
	LspServerConfig *cfg = &s->config;

	if (s->process)
	{
		s->startup_shutdown = TRUE;
		stop_process(s);
	}

	g_free(s->cmd);
	g_free(s->autocomplete_trigger_chars);
	g_free(s->signature_trigger_chars);
	g_free(s->initialize_response);

	g_strfreev(cfg->autocomplete_trigger_sequences);
	g_free(cfg->diagnostics_error_style);
	g_free(cfg->diagnostics_warning_style);
	g_free(cfg->diagnostics_info_style);
	g_free(cfg->diagnostics_hint_style);
}


static void free_server(gpointer data)
{
	LspServer *s = data;
	stop_server(s);
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
		;
	else
	{
		//printf("\n\nNOTIFICATION FROM SERVER: %s\n", method);
		//printf("params:\n%s\n\n\n", lsp_utils_json_pretty_print(params));
	}
}


static gboolean handle_call(JsonrpcClient* self, gchar* method, GVariant* id, GVariant* params,
	gpointer user_data)
{
	JsonNode *node = json_from_string("{}", NULL);
	GVariant *variant = json_gvariant_deserialize(node, NULL, NULL);

	lsp_log(lsp_server_get_log_info(self), LspLogServerMessageSent, method, params);

	//printf("\n\nREQUEST FROM SERVER: %s\n", method);
	//printf("params:\n%s\n\n\n", lsp_utils_json_pretty_print(params));

	lsp_log(lsp_server_get_log_info(self), LspLogServerMessageReceived, method, variant);
	g_variant_unref(variant);
	json_node_free(node);

	return FALSE;  //TODO: return TRUE for handled requests
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


static void initialize_cb(GObject *object, GAsyncResult *result, gpointer user_data)
{
	JsonrpcClient *self = (JsonrpcClient *)object;
	GVariant *return_value = NULL;
	gint filetype_id = GPOINTER_TO_INT(user_data);

	if (lsp_client_call_finish(self, result, &return_value))
	{
		if (lsp_servers && lsp_servers->len > filetype_id)
		{
			LspServer *s = lsp_servers->pdata[filetype_id];
			if (s->rpc_client == self)
			{
				GeanyDocument *current_doc = document_get_current();
				guint i;

				g_free(s->autocomplete_trigger_chars);
				s->autocomplete_trigger_chars = get_autocomplete_trigger_chars(return_value);

				g_free(s->signature_trigger_chars);
				s->signature_trigger_chars = get_signature_trigger_chars(return_value);

				s->use_incremental_sync = use_incremental_sync(return_value);

				s->initialize_response = lsp_utils_json_pretty_print(return_value);
				//printf("%s\n", lsp_utils_json_pretty_print(return_value));

				msgwin_status_add("LSP server %s initialized", s->cmd);

				lsp_client_notify(s->rpc_client, "initialized", NULL);
				s->startup_shutdown = FALSE;

				foreach_document(i)
				{
					GeanyDocument *doc = documents[i];

					// see on_document_activate() for detailed comment
					if (doc->file_type->id == filetype_id && (doc->changed || doc == current_doc))
					{
						lsp_sync_text_document_did_open(s, doc);
						if (doc == current_doc)
						{
							lsp_diagnostics_style_current_doc(s);
							lsp_diagnostics_redraw_current_doc(s);
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
		if (lsp_servers && lsp_servers->len > filetype_id)
		{
			LspServer *s = lsp_servers->pdata[filetype_id];
			if (s->rpc_client == self)
			{
				msgwin_status_add("LSP initialize request failed for LSP server %s", s->cmd);
				stop_process(s);
				start_lsp_server(s, filetype_id);
			}
		}
	}
}


static GVariant *get_init_options(LspServer *server)
{
	JsonNode *json_node = json_from_string("{}", NULL);
	GVariant *variant = json_gvariant_deserialize(json_node, NULL, NULL);
	gchar *file_contents;

	json_node_free(json_node);

	if (!server->config.initialization_options_file)
		return variant;

	if (!g_file_get_contents(server->config.initialization_options_file, &file_contents, NULL, NULL))
		return variant;

	json_node = json_from_string(file_contents, NULL);

	if (json_node)
	{
		g_variant_unref(variant);
		variant = json_gvariant_deserialize(json_node, NULL, NULL);
	}

	g_free(file_contents);

	return variant;
}


static void perform_initialize(LspServer *server, GeanyFiletypeID ft)
{
	GVariant *node;

	gchar *locale = lsp_utils_get_locale();
	gchar *project_base_uri = NULL;
	gchar *project_base = lsp_utils_get_project_base_path();

	if (project_base)
		project_base_uri = g_filename_to_uri(project_base, NULL, NULL);

	node = JSONRPC_MESSAGE_NEW (
		"processId", JSONRPC_MESSAGE_PUT_INT32(getpid()),
		"clientInfo", "{",
			"name", JSONRPC_MESSAGE_PUT_STRING("Geany"),
			"version", JSONRPC_MESSAGE_PUT_STRING("0.1"), //VERSION
		"}",
		"locale", JSONRPC_MESSAGE_PUT_STRING(locale),
		"rootPath", JSONRPC_MESSAGE_PUT_STRING(project_base),
		"workspaceFolders", "[",
			"{",
				"uri", JSONRPC_MESSAGE_PUT_STRING (project_base_uri),
				"name", JSONRPC_MESSAGE_PUT_STRING (project_base),
			"}",
		"]",
		//"rootUri", JSONRPC_MESSAGE_PUT_STRING(project_base_uri),
		"capabilities", "{",
			"textDocument", "{",
				"synchronization", "{",
					"willSave", JSONRPC_MESSAGE_PUT_BOOLEAN(FALSE),
					"willSaveWaitUntil", JSONRPC_MESSAGE_PUT_BOOLEAN(FALSE),
					"didSave", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
				"}",
				"completion", "{",
					"completionItemKind", "{",
						"valueSet", "[",
							ALL_AUTOCOMPLETION_KINDS
						"]",
					"}",
//					"contxtSupport", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
				"}",
				"hover", "{",
					"contentFormat", "[",
						JSONRPC_MESSAGE_PUT_STRING("plaintext"),
					"]",
				"}",
			"}",
		"}",
		"trace", JSONRPC_MESSAGE_PUT_STRING("off"),
		"workspaceFolders", JSONRPC_MESSAGE_PUT_STRING(NULL),
		"initializationOptions", "{",
			//TODO: JSONRPC_MESSAGE_PUT_VARIANT() has to be last, otherwise it strips the rest of the message
			//report to jsonrpc-glib
			JSONRPC_MESSAGE_PUT_VARIANT(get_init_options(server)),
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

	if (lsp_servers && lsp_servers->len > filetype_id)
	{
		LspServer *s = lsp_servers->pdata[filetype_id];
		if (s->process == process)
		{
			msgwin_status_add("LSP server %s crashed, restarting", s->cmd);
			stop_process(s);
			start_lsp_server(s, filetype_id);
		}
	}

}


static void start_lsp_server(LspServer *server, GeanyFiletypeID filetype_id)
{
	GInputStream *input_stream;
	GOutputStream *output_stream;
	GError *error = NULL;
	gchar ** argv;
	gint flags = G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE;

	if (server->restarts > 5)
		return;

	argv = g_strsplit_set(server->cmd, " ", -1);

	if (!server->config.show_server_stderr)
		flags |= G_SUBPROCESS_FLAGS_STDERR_SILENCE;
	server->process = g_subprocess_newv((const gchar * const *)argv, flags, &error);

	server->restarts++;
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

	get_bool(&s->config.autocomplete_use_label, kf, section, "autocomplete_use_label");
	get_bool(&s->config.autocomplete_apply_additional_edits, kf, section, "autocomplete_apply_additional_edits");
	get_bool(&s->config.diagnostics_enable, kf, section, "diagnostics_enable");

	get_str(&s->config.diagnostics_error_style, kf, section, "diagnostics_error_style");
	get_str(&s->config.diagnostics_warning_style, kf, section, "diagnostics_warning_style");
	get_str(&s->config.diagnostics_info_style, kf, section, "diagnostics_info_style");
	get_str(&s->config.diagnostics_hint_style, kf, section, "diagnostics_hint_style");

	get_bool(&s->config.hover_enable, kf, section, "hover_enable");
	get_bool(&s->config.signature_enable, kf, section, "signature_enable");
	get_bool(&s->config.goto_enable, kf, section, "goto_enable");
	get_bool(&s->config.document_symbols_enable, kf, section, "document_symbols_enable");
	get_bool(&s->config.show_server_stderr, kf, section, "show_server_stderr");
}


static void load_filetype_only_config(GKeyFile *kf, gchar *section, LspServer *s)
{
	get_str(&s->cmd, kf, section, "cmd");
	get_str(&s->config.rpc_log, kf, section, "rpc_log");
	get_str(&s->config.initialization_options_file, kf, section, "initialization_options_file");
}


LspLogInfo lsp_server_get_log_info(JsonrpcClient *client)
{
	LspLogInfo empty = {0, NULL};
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

	if (lsp_servers)
	{
		for (i = 0; i < lsp_servers->len; i++)
		{
			LspServer *s = lsp_servers->pdata[i];
			if (s->rpc_client == client)
				return s->log;
		}
	}

	return empty;
}


LspServer *lsp_server_get_if_running(GeanyDocument *doc)
{
	LspServer *s;

	if (!doc || !lsp_servers || lsp_utils_is_lsp_disabled_for_project())
		return NULL;

	s = lsp_servers->pdata[doc->file_type->id];
	if (s->startup_shutdown)
		return NULL;

	if (s->process)
		return s;

	if (s->referenced && s->referenced->process)
		return s->referenced;

	return NULL;
}


LspServer *lsp_server_get_for_ft(GeanyFiletype *ft)
{
	GKeyFile *kf, *kf_global;
	LspServer *s, *s2 = NULL;
	gchar *ref_lang = NULL;
	gchar *ft_name;

	if (!ft || !lsp_servers || lsp_utils_is_lsp_disabled_for_project())
		return NULL;

	s = lsp_servers->pdata[ft->id];
	if (s->startup_shutdown)
		return NULL;

	if (s->process)
		return s;

	if (s->referenced && s->referenced->process)
		return s->referenced;

	if (s->not_used)
		return NULL;

	if (s->restarts > 5)
	{
		msgwin_status_add("LSP server %s terminated more than 5 times, giving up", s->cmd);
		return NULL;
	}

	ft_name = ft->name;

	kf_global = read_keyfile(lsp_utils_get_global_config_filename());
	kf = read_keyfile(lsp_utils_get_config_filename());

	get_str(&ref_lang, kf_global, ft_name, "use");
	get_str(&ref_lang, kf, ft_name, "use");

	if (ref_lang)
	{
		GeanyFiletype *ft = filetypes_lookup_by_name(ref_lang);

		g_free(ref_lang);
		if (ft)
		{
			ft_name = ft->name;
			s2 = g_ptr_array_index(lsp_servers, ft->id);
			s->referenced = s2;
			if (s2->process)
			{
				g_key_file_free(kf);
				g_key_file_free(kf_global);
				return s2;
			}
		}
	}

	if (s2)
		s = s2;

	load_config(kf_global, "all", s);
	load_config(kf_global, ft_name, s);
	load_config(kf, "all", s);
	load_config(kf, ft_name, s);

	load_filetype_only_config(kf_global, ft_name, s);
	load_filetype_only_config(kf, ft_name, s);

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

	g_key_file_free(kf);
	g_key_file_free(kf_global);

	// the server isn't initialized when running for the first time because the async
	// handshake with the server hasn't completed yet
	return NULL;
}


LspServer *lsp_server_get(GeanyDocument *doc)
{
	if (!doc)
		return NULL;

	return lsp_server_get_for_ft(doc->file_type);
}


LspServerConfig *lsp_server_get_config(GeanyDocument *doc)
{
	LspServer *s = NULL;

	if (!doc || !lsp_servers || lsp_utils_is_lsp_disabled_for_project())
		return NULL;

	// make sure we attempted to start the server
	lsp_server_get(doc);

	s = lsp_servers->pdata[doc->file_type->id];

	if (s->process)
		;
	else if (s->referenced && s->referenced->process)
		s = s->referenced;

	if (s->process)
		return &s->config;

	return NULL;
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
	guint i;
	GeanyFiletype *filetype;

	if (lsp_servers)
		lsp_server_stop_all(FALSE);

	if (!servers_in_shutdown)
		servers_in_shutdown = g_ptr_array_new_full(0, (GDestroyNotify)free_shuthown_info);

	lsp_servers = g_ptr_array_new_full(0, free_server);

	for (i = 0; (filetype = filetypes_index(i)); i++)
	{
		LspServer *s = g_new0(LspServer, 1);
		g_ptr_array_add(lsp_servers, s);
	}
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
