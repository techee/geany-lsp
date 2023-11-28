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

#include "lsp/lsp-command.h"
#include "lsp/lsp-utils.h"
#include "lsp/lsp-client.h"
#include "lsp/lsp-diagnostics.h"


static void command_cb(GObject *object, GAsyncResult *result, gpointer user_data)
{
	JsonrpcClient *self = (JsonrpcClient *)object;
	GVariant *return_value = NULL;
	GError *error = NULL;

	if (lsp_client_call_finish(self, result, &return_value, &error))
	{
		//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

		if (return_value)
			g_variant_unref(return_value);
	}
}


void lsp_command_send_request(LspServer *server, const gchar *cmd, GVariant *arguments)
{
	GVariant *node;

	if (arguments)
	{
		node = JSONRPC_MESSAGE_NEW (
			"command", JSONRPC_MESSAGE_PUT_STRING(cmd),
			"arguments", JSONRPC_MESSAGE_PUT_VARIANT(arguments)
		);
	}
	else
	{
		node = JSONRPC_MESSAGE_NEW (
			"command", JSONRPC_MESSAGE_PUT_STRING(cmd)
		);
	}

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	lsp_client_call_async(server->rpc_client, "workspace/executeCommand", node,
		command_cb, NULL);

	g_variant_unref(node);
}


static void code_action_cb(GObject *object, GAsyncResult *result, gpointer user_data)
{
	JsonrpcClient *self = (JsonrpcClient *)object;
	GVariant *return_value = NULL;
	GError *error = NULL;

	if (lsp_client_call_finish(self, result, &return_value, &error))
	{
		printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

		if (return_value)
			g_variant_unref(return_value);
	}
}


void lsp_command_send_code_action_request(LspServer *server, gint pos)
{
	GeanyDocument *doc = document_get_current();
	ScintillaObject *sci = doc->editor->sci;
	gint start_pos = pos;
	gint end_pos = pos;
	LspPosition lsp_start_pos = lsp_utils_scintilla_pos_to_lsp(sci, start_pos);
	LspPosition lsp_end_pos = lsp_utils_scintilla_pos_to_lsp(sci, end_pos);
	GVariant *diag_raw = lsp_diagnostics_get_diag_raw(pos);

	// TODO: works with current position only, support selection range in the future
	if (diag_raw)
	{
		GVariant *node;
		GVariantDict dict;
		GVariant *diagnostics, *diags_dict;
		gchar *doc_uri = lsp_utils_get_doc_uri(doc);
		GPtrArray *arr = g_ptr_array_new_full(1, (GDestroyNotify) g_variant_unref);

		g_ptr_array_add(arr, g_variant_ref(diag_raw));
		diagnostics = g_variant_new_array(G_VARIANT_TYPE_VARDICT,
			(GVariant **)(gpointer)arr->pdata, arr->len);

		g_variant_dict_init(&dict, NULL);
		g_variant_dict_insert_value(&dict, "diagnostics", diagnostics);
		diags_dict = g_variant_take_ref(g_variant_dict_end(&dict));

		node = JSONRPC_MESSAGE_NEW (
			"textDocument", "{",
				"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
			"}",
			"range", "{",
				"start", "{",
					"line", JSONRPC_MESSAGE_PUT_INT32(lsp_start_pos.line),
					"character", JSONRPC_MESSAGE_PUT_INT32(lsp_start_pos.character),
				"}",
				"end", "{",
					"line", JSONRPC_MESSAGE_PUT_INT32(lsp_end_pos.line),
					"character", JSONRPC_MESSAGE_PUT_INT32(lsp_end_pos.character),
				"}",
			"}",
			"context", "{",
				JSONRPC_MESSAGE_PUT_VARIANT((GVariant *)diags_dict),
			"}"
		);

		//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

		lsp_client_call_async(server->rpc_client, "textDocument/codeAction", node, code_action_cb, NULL);

		g_variant_unref(node);
		g_variant_unref(diags_dict);
	}

	g_free(doc_uri);
}
