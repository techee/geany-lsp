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

#include "lsp/lsp-hover.h"
#include "lsp/lsp-utils.h"
#include "lsp/lsp-client.h"

 
typedef struct {
	GeanyDocument *doc;
	gint pos;
} LspHoverData;


static ScintillaObject *calltip_sci;


static void hover_cb(GObject *object, GAsyncResult *result, gpointer user_data)
{
	JsonrpcClient *self = (JsonrpcClient *)object;
	GVariant *return_value = NULL;

	if (lsp_client_call_finish(self, result, &return_value))
	{
		GeanyDocument *doc = document_get_current();
		LspHoverData *data = user_data;

		if (doc == data->doc && gtk_widget_has_focus(GTK_WIDGET(doc->editor->sci)))
		{
			const gchar *str = NULL;

			JSONRPC_MESSAGE_PARSE(return_value, 
				"contents", "{",
					"value", JSONRPC_MESSAGE_GET_STRING(&str),
				"}");

			//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

			if (str && strlen(str) > 0)
			{
				calltip_sci = data->doc->editor->sci;
				SSM(calltip_sci, SCI_CALLTIPSHOW, data->pos, (sptr_t) str);
			}
		}

		if (return_value)
			g_variant_unref(return_value);
	}

	g_free(user_data);
}


void lsp_hover_send_request(LspServer *server, GeanyDocument *doc, gint pos)
{
	GVariant *node;
	ScintillaObject *sci = doc->editor->sci;
	LspPosition lsp_pos = lsp_utils_scintilla_pos_to_lsp(sci, pos);
	gchar *doc_uri = lsp_utils_get_doc_uri(doc);
	LspHoverData *data = g_new0(LspHoverData, 1);

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}",
		"position", "{",
			"line", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.line),
			"character", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.character),
		"}"
	);

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	data->doc = doc;
	data->pos = pos;

	lsp_client_call_async(server->rpc_client, "textDocument/hover", node,
		hover_cb, data);

	g_free(doc_uri);
	g_variant_unref(node);
}


void lsp_hover_hide_calltip(GeanyDocument *doc)
{
	if (doc->editor->sci == calltip_sci)
	{
		SSM(doc->editor->sci, SCI_CALLTIPCANCEL, 0, 0);
		calltip_sci = NULL;
	}
}
