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

#include "lsp/lsp-goto.h"
#include "lsp/lsp-utils.h"
#include "lsp/lsp-client.h"


extern GeanyData *geany_data;


static void goto_location(GeanyDocument *old_doc, LspLocation *loc)
{
	gchar *fname = lsp_utils_get_real_path_from_uri(loc->uri);
	GeanyDocument *doc = document_open_file(fname, FALSE, NULL, NULL);

	if (doc)
		navqueue_goto_line(old_doc, doc, loc->range.start.line + 1);

	g_free(fname);
}


static void goto_cb(GObject *object, GAsyncResult *result, gpointer user_data)
{
	JsonrpcClient *self = (JsonrpcClient *)object;
	GVariant *return_value = NULL;

	if (lsp_client_call_finish(self, result, &return_value))
	{
		GeanyDocument *old_doc = user_data;
		gboolean doc_exists = FALSE;
		gint i;

		foreach_document(i)
		{
			if (old_doc == documents[i])
			{
				doc_exists = TRUE;
				break;
			}
		}

		if (doc_exists)
		{
			// array of locations
			if (g_variant_is_of_type(return_value, G_VARIANT_TYPE("av")))
			{
				GPtrArray *locations = NULL;
				GVariantIter iter;

				g_variant_iter_init(&iter, return_value);

				locations = lsp_utils_parse_locations(&iter);
				// possible TODO: show popup for more locations - but clangd at least
				// seems to always return the single "right" location based on the true
				// visibility of the symbol at the given place
				if (locations && locations->len > 0)
					goto_location(old_doc, locations->pdata[0]);

				g_ptr_array_free(locations, TRUE);
			}
			//single location
			else if (g_variant_is_of_type(return_value, G_VARIANT_TYPE("a{sv}")))
			{
				LspLocation *loc = lsp_utils_parse_location(return_value);

				if (loc)
					goto_location(old_doc, loc);

				lsp_utils_free_lsp_location(loc);
			}
		}

		//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

		if (return_value)
			g_variant_unref(return_value);
	}
}


void lsp_goto_send_request(LspServer *server, GeanyDocument *doc, gboolean definition)
{
	GVariant *node;
	ScintillaObject *sci = doc->editor->sci;
	gint pos = sci_get_current_position(sci);
	LspPosition lsp_pos = lsp_utils_scintilla_pos_to_lsp(sci, pos);
	gchar *doc_uri = lsp_utils_get_doc_uri(doc);
	const gchar *request = definition ? "textDocument/definition" : "textDocument/declaration";

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}",
		"position", "{",
			"line", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.line),
			"character", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.character),
		"}"
	);

	lsp_client_call_async(server->rpc_client, request, node, goto_cb, doc);

	g_free(doc_uri);
	g_variant_unref(node);
}
