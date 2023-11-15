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

#include "lsp/lsp-highlight.h"
#include "lsp/lsp-utils.h"
#include "lsp/lsp-client.h"


typedef struct {
	GeanyDocument *doc;
	gint pos;
	gchar *identifier;
} LspHighlightData;


static gint indicator;


static void clear_indicators(ScintillaObject *sci)
{
	sci_indicator_set(sci, indicator);
	sci_indicator_clear(sci, 0, sci_get_length(sci));
}


void lsp_highlight_style_current_doc(LspServer *server)
{
	GeanyDocument *doc = document_get_current();
	ScintillaObject *sci = doc->editor->sci;

	if (indicator > 0)
		clear_indicators(sci);
	indicator = lsp_utils_set_indicator_style(sci, server->config.highlighting_style);
}


static void highlight_range(GeanyDocument *doc, LspRange range)
{
	ScintillaObject *sci = doc->editor->sci;
	gint start_pos = lsp_utils_lsp_pos_to_scintilla(sci, range.start);
	gint end_pos = lsp_utils_lsp_pos_to_scintilla(sci, range.end);

//	printf("%d   %d\n", start_pos, end_pos);
	editor_indicator_set_on_range(doc->editor, indicator, start_pos, end_pos);
}


static void highlight_cb(GObject *object, GAsyncResult *result, gpointer user_data)
{
	JsonrpcClient *self = (JsonrpcClient *)object;
	GVariant *return_value = NULL;
	LspHighlightData *data = user_data;

	if (lsp_client_call_finish(self, result, &return_value))
	{
		GeanyDocument *doc = document_get_current();

		if (doc == data->doc)
		{
			//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

			clear_indicators(doc->editor->sci);

			if (g_variant_is_of_type(return_value, G_VARIANT_TYPE("av")))
			{
				GVariant *member = NULL;
				GVariantIter iter;

				g_variant_iter_init(&iter, return_value);

				while (g_variant_iter_loop(&iter, "v", &member))
				{
					GVariant *range = NULL;

					JSONRPC_MESSAGE_PARSE(member,
						"range", JSONRPC_MESSAGE_GET_VARIANT(&range)
						);

					if (range)
					{
						LspRange r = lsp_utils_parse_range(range);
						gint start_pos = lsp_utils_lsp_pos_to_scintilla(doc->editor->sci, r.start);
						gint end_pos = lsp_utils_lsp_pos_to_scintilla(doc->editor->sci, r.end);
						gchar *ident = sci_get_contents_range(doc->editor->sci, start_pos, end_pos);

						//clangd returns highlight for 'editor' in 'doc-|>editor' where
						//'|' is the caret position and also other cases, which is strange
						//restrict to identifiers only
						if (g_strcmp0(ident, data->identifier) == 0)
							highlight_range(doc, r);
						g_free(ident);
						g_variant_unref(range);
					}
				}
			}
		}

		if (return_value)
			g_variant_unref(return_value);
	}

	g_free(data->identifier);
	g_free(user_data);
}


void lsp_highlight_send_request(LspServer *server, GeanyDocument *doc)
{
	GVariant *node;
	ScintillaObject *sci = doc->editor->sci;
	gint pos = sci_get_current_position(sci);
	LspPosition lsp_pos = lsp_utils_scintilla_pos_to_lsp(sci, pos);
	gchar *doc_uri = lsp_utils_get_doc_uri(doc);
	gchar *iden = lsp_utils_get_current_iden(doc);

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

	if (iden)
	{
		LspHighlightData *data = g_new0(LspHighlightData, 1);

		data->doc = doc;
		data->pos = pos;
		data->identifier = iden;
		lsp_client_call_async(server->rpc_client, "textDocument/documentHighlight", node,
			highlight_cb, data);
	}
	else
		clear_indicators(doc->editor->sci);

	g_free(doc_uri);
	g_variant_unref(node);
}
