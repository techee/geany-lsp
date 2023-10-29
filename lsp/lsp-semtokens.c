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

#include "lsp/lsp-semtokens.h"
#include "lsp/lsp-utils.h"
#include "lsp/lsp-client.h"
#include "lsp/lsp-sync.h"


typedef struct {
	GeanyDocument *doc;
	LspSymbolRequestCallback callback;
	gboolean delta;
	gpointer user_data;
} LspSemtokensUserData;


typedef struct {
	guint start;
	guint delete_count;
	GArray *data;
} SemanticTokensEdit;


extern GeanyPlugin *geany_plugin;

extern GeanyData *geany_data;

//TODO: possibly cache multiple files
static GArray *cached_tokens;
static gchar *cached_tokens_fname;
static gchar *cached_tokens_str;
static gchar *cached_result_id;


static SemanticTokensEdit *sem_tokens_edit_new(void)
{
	SemanticTokensEdit *edit = g_new0(SemanticTokensEdit, 1);
	edit->data = g_array_sized_new(FALSE, FALSE, sizeof(guint), 20);
	return edit;
}


static void sem_tokens_edit_free(SemanticTokensEdit *edit)
{
	g_array_free(edit->data, TRUE);
}


static void sem_tokens_edit_apply(SemanticTokensEdit *edit)
{
	g_return_if_fail(!cached_tokens || edit->start + edit->delete_count <= cached_tokens->len);

	g_array_remove_range(cached_tokens, edit->start, edit->delete_count);
	g_array_insert_vals(cached_tokens, edit->start, edit->data->data, edit->data->len);
}


const gchar *lsp_semtokens_get_cached(GeanyDocument *doc)
{
	if (cached_tokens_str && g_strcmp0(doc->real_path, cached_tokens_fname) == 0)
		return cached_tokens_str;

	g_free(cached_tokens_str);
	cached_tokens_str = g_strdup("");

	return cached_tokens_str;
}


static gchar *process_tokens(GArray *tokens, ScintillaObject *sci, guint64 token_mask)
{
	GHashTable *type_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	guint delta_line, delta_char, len, token_type;
	LspPosition last_pos = {0, 0};
	gboolean first = TRUE;
	GList *keys, *item;
	GString *type_str;
	gint i;

	for (i = 0; i < tokens->len; i++)
	{
		guint v = g_array_index(tokens, guint, i);

		switch (i % 5)
		{
			case 0:
				delta_line = v;
				break;
			case 1:
				delta_char = v;
				break;
			case 2:
				len = v;
				break;
			case 3:
				token_type = 1 << v;
				break;
		}

		if (i % 5 == 4)
		{
			last_pos.line += delta_line;
			if (delta_line == 0)
				last_pos.character += delta_char;
			else
				last_pos.character = delta_char;

			if (token_type & token_mask)
			{
				LspPosition end_pos = last_pos;
				gint sci_pos_start, sci_pos_end;
				gchar *str;

				end_pos.character += len;
				sci_pos_start = lsp_utils_lsp_pos_to_scintilla(sci, last_pos);
				sci_pos_end = lsp_utils_lsp_pos_to_scintilla(sci, end_pos);
				str = sci_get_contents_range(sci, sci_pos_start, sci_pos_end);
				if (str)
					g_hash_table_add(type_table, str);
			}
		}
	}

	keys = g_hash_table_get_keys(type_table);
	type_str = g_string_new("");

	foreach_list(item, keys)
	{
		if (!first)
			g_string_append_c(type_str, ' ');
		g_string_append(type_str, item->data);
		first = FALSE;
	}

	g_list_free(keys);
	g_hash_table_destroy(type_table);

	return g_string_free(type_str, FALSE);
}


static void process_full_result(GeanyDocument *doc, GVariant *result, guint64 token_mask)
{
	GVariantIter *iter = NULL;
	const gchar *result_id = NULL;

	JSONRPC_MESSAGE_PARSE(result,
		"resultId", JSONRPC_MESSAGE_GET_STRING(&result_id),
		"data", JSONRPC_MESSAGE_GET_ITER(&iter)
	);

	if (iter && result_id)
	{
		GVariant *val = NULL;

		g_free(cached_tokens_fname);
		cached_tokens_fname = g_strdup(doc->real_path);

		g_free(cached_result_id);
		cached_result_id = g_strdup(result_id);

		if (!cached_tokens)
			cached_tokens = g_array_sized_new(FALSE, FALSE, sizeof(guint), 1000);
		cached_tokens->len = 0;

		while (g_variant_iter_loop(iter, "v", &val))
		{
			guint v = g_variant_get_int64(val);
			g_array_append_val(cached_tokens, v);
		}

		g_free(cached_tokens_str);
		cached_tokens_str = process_tokens(cached_tokens, doc->editor->sci, token_mask);

		g_variant_iter_free(iter);
	}
}


static gint sort_edits(gconstpointer a, gconstpointer b)
{
	const SemanticTokensEdit *e1 = *((SemanticTokensEdit **) a);
	const SemanticTokensEdit *e2 = *((SemanticTokensEdit **) b);

	return e2->start - e1->start;
}


static void process_delta_result(GeanyDocument *doc, GVariant *result, guint64 token_mask)
{
	GVariantIter *iter = NULL;
	const gchar *result_id = NULL;

	JSONRPC_MESSAGE_PARSE(result,
		"resultId", JSONRPC_MESSAGE_GET_STRING(&result_id),
		"edits", JSONRPC_MESSAGE_GET_ITER(&iter)
	);

	if (iter && result_id && cached_tokens &&
		g_strcmp0(cached_tokens_fname, doc->real_path) == 0)
	{
		GPtrArray *edits = g_ptr_array_new_full(4, (GDestroyNotify)sem_tokens_edit_free);
		SemanticTokensEdit *edit;
		GVariant *val = NULL;
		guint i;
 
		g_free(cached_result_id);
		cached_result_id = g_strdup(result_id);

		while (g_variant_iter_loop(iter, "v", &val))
		{
			GVariantIter *iter2 = NULL;
			GVariant *val2 = NULL;
			gint64 delete_count = 0;
			gint64 start = 0;

			edit = sem_tokens_edit_new();

			JSONRPC_MESSAGE_PARSE(val,
				"start", JSONRPC_MESSAGE_GET_INT64(&start),
				"deleteCount", JSONRPC_MESSAGE_GET_INT64(&delete_count),
				"data", JSONRPC_MESSAGE_GET_ITER(&iter2)
			);

			edit->start = start;
			edit->delete_count = delete_count;

			while (g_variant_iter_loop(iter2, "v", &val2))
			{
				guint v = g_variant_get_int64(val2);
				g_array_append_val(edit->data, v);
			}

			g_ptr_array_add(edits, edit);

			g_variant_iter_free(iter2);
		}

		g_ptr_array_sort(edits, sort_edits);

		foreach_ptr_array(edit, i, edits)
			sem_tokens_edit_apply(edit);

		g_free(cached_tokens_str);
		cached_tokens_str = process_tokens(cached_tokens, doc->editor->sci, token_mask);

		g_ptr_array_free(edits, TRUE);
		g_variant_iter_free(iter);
	}
}


static void semtokens_cb(GObject *object, GAsyncResult *result, gpointer user_data)
{
	JsonrpcClient *self = (JsonrpcClient *)object;
	GVariant *return_value = NULL;
	LspSemtokensUserData *data = user_data;

	if (lsp_client_call_finish(self, result, &return_value))
	{
		GeanyDocument *doc = data->doc;
		gboolean doc_exists = FALSE;
		LspServer *srv;
		gint i;

		foreach_document(i)
		{
			if (doc == documents[i])
			{
				doc_exists = TRUE;
				break;
			}
		}

		srv = doc_exists ? lsp_server_get(doc) : NULL;

		if (doc_exists && srv)
		{
			//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

			if (data->delta)
				process_delta_result(doc, return_value, srv->semantic_token_mask);
			else
				process_full_result(doc, return_value, srv->semantic_token_mask);
		}

		if (return_value)
			g_variant_unref(return_value);
	}

	data->callback(data->user_data);

	g_free(user_data);
}


static gboolean retry_cb(gpointer user_data)
{
	LspSemtokensUserData *data = user_data;

	printf("retrying\n");  //TODO: remove
	if (data->doc == document_get_current())
	{
		LspServer *srv = lsp_server_get(data->doc);
		if (!srv)
			return TRUE;  // retry
		else
		{
			// should be successful now
			lsp_semtokens_send_request(data->doc, data->callback, data->user_data);
			g_free(data);
			return FALSE;
		}
	}

	// server shut down or document not current any more
	g_free(data);
	return FALSE;
}


void lsp_semtokens_send_request(GeanyDocument *doc, LspSymbolRequestCallback callback,
	gpointer user_data)
{
	LspSemtokensUserData *data = g_new0(LspSemtokensUserData, 1);
	LspServer *server = lsp_server_get(doc);
	gchar *doc_uri;
	GVariant *node;

	data->user_data = user_data;
	data->doc = doc;
	data->callback = callback;

	if (!server)
	{
		// happens when Geany and LSP server started - we cannot send the request yet
		plugin_timeout_add(geany_plugin, 300, retry_cb, data);
		return;
	}

	doc_uri = lsp_utils_get_doc_uri(doc);

	/* Geany requests symbols before firing "document-activate" signal so we may
	 * need to request document opening here */
	if (!lsp_sync_is_document_open(doc))
		lsp_sync_text_document_did_open(server, doc);

	data->delta = g_strcmp0(doc->real_path, cached_tokens_fname) == 0;

	if (data->delta)
	{
		node = JSONRPC_MESSAGE_NEW(
			"previousResultId", JSONRPC_MESSAGE_PUT_STRING(cached_result_id),
			"textDocument", "{",
				"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
			"}"
		);
		lsp_client_call_async(server->rpc_client, "textDocument/semanticTokens/full/delta", node,
			semtokens_cb, data);
	}
	else
	{
		node = JSONRPC_MESSAGE_NEW(
			"textDocument", "{",
				"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
			"}"
		);
		lsp_client_call_async(server->rpc_client, "textDocument/semanticTokens/full", node,
			semtokens_cb, data);
	}

	g_free(doc_uri);
	g_variant_unref(node);
}
