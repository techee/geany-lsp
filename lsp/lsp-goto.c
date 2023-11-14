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
#include "lsp/lsp-goto-panel.h"


typedef struct {
	GeanyDocument *doc;
	gboolean show_in_msgwin;
} GotoData;


extern GeanyData *geany_data;


// TODO: free on plugin unload
GPtrArray *last_result;


static void goto_location(GeanyDocument *old_doc, LspLocation *loc)
{
	gchar *fname = lsp_utils_get_real_path_from_uri_locale(loc->uri);
	GeanyDocument *doc = document_open_file(fname, FALSE, NULL, NULL);

	if (doc)
		navqueue_goto_line(old_doc, doc, loc->range.start.line + 1);

	g_free(fname);
}


static void filter_symbols(const gchar *filter)
{
	GPtrArray *filtered;

	if (!last_result)
		return;

	filtered = lsp_goto_panel_filter(last_result, filter);
	lsp_goto_panel_fill(filtered);

	g_ptr_array_free(filtered, TRUE);
}


static void show_in_msgwin(LspLocation *loc)
{
	gchar *fname = lsp_utils_get_real_path_from_uri_utf8(loc->uri);
	msgwin_msg_add(COLOR_BLACK, -1, NULL, "%s:%ld:", fname, loc->range.start.line+1);
	g_free(fname);
}


static void goto_cb(GObject *object, GAsyncResult *result, gpointer user_data)
{
	JsonrpcClient *self = (JsonrpcClient *)object;
	GVariant *return_value = NULL;

	if (lsp_client_call_finish(self, result, &return_value))
	{
		GotoData *data = user_data;
		gboolean doc_exists = FALSE;
		gint i;

		foreach_document(i)
		{
			if (data->doc == documents[i])
			{
				doc_exists = TRUE;
				break;
			}
		}

		if (doc_exists)
		{
			if (data->show_in_msgwin)
			{
				msgwin_clear_tab(MSG_MESSAGE);
				msgwin_switch_tab(MSG_MESSAGE, TRUE);
			}

			// array of locations
			if (g_variant_is_of_type(return_value, G_VARIANT_TYPE("av")))
			{
				GPtrArray *locations = NULL;
				GVariantIter iter;

				g_variant_iter_init(&iter, return_value);

				locations = lsp_utils_parse_locations(&iter);

				if (locations && locations->len > 0)
				{
					if (data->show_in_msgwin)
					{
						LspLocation *loc;
						guint i;

						foreach_ptr_array(loc, i, locations)
						{
							show_in_msgwin(loc);
						}
					}
					else if (locations->len == 1)
						goto_location(data->doc, locations->pdata[0]);
					else
					{
						LspLocation *loc;
						guint i;

						if (last_result)
							g_ptr_array_free(last_result, TRUE);

						last_result = g_ptr_array_new_full(0, (GDestroyNotify)lsp_goto_panel_symbol_free);

						foreach_ptr_array(loc, i, locations)
						{
							LspGotoPanelSymbol *sym = g_new0(LspGotoPanelSymbol, 1);
							sym->file = lsp_utils_get_real_path_from_uri_utf8(loc->uri);
							sym->label = g_path_get_basename(sym->file);
							sym->line = loc->range.start.line+1;
							sym->icon = TM_ICON_OTHER;
							g_ptr_array_add(last_result, sym);
						}

						lsp_goto_panel_show("", filter_symbols);
					}
				}

				g_ptr_array_free(locations, TRUE);
			}
			//single location
			else if (g_variant_is_of_type(return_value, G_VARIANT_TYPE("a{sv}")))
			{
				LspLocation *loc = lsp_utils_parse_location(return_value);

				if (loc)
				{
					if (data->show_in_msgwin)
						show_in_msgwin(loc);
					else
						goto_location(data->doc, loc);
				}

				lsp_utils_free_lsp_location(loc);
			}
		}

		//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

		if (return_value)
			g_variant_unref(return_value);
	}

	g_free(user_data);
}


static void perform_goto(LspServer *server, GeanyDocument *doc, gint pos, const gchar *request,
	gboolean show_in_msgwin)
{
	GVariant *node;
	ScintillaObject *sci = doc->editor->sci;
	LspPosition lsp_pos = lsp_utils_scintilla_pos_to_lsp(sci, pos);
	gchar *doc_uri = lsp_utils_get_doc_uri(doc);
	GotoData *data = g_new0(GotoData, 1);

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}",
		"position", "{",
			"line", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.line),
			"character", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.character),
		"}"
	);

	data->doc = doc;
	data->show_in_msgwin = show_in_msgwin;
	lsp_client_call_async(server->rpc_client, request, node, goto_cb, data);

	g_free(doc_uri);
	g_variant_unref(node);
}


void lsp_goto_definition(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return;

	perform_goto(srv, doc, pos, "textDocument/definition", FALSE);
}


void lsp_goto_declaration(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return;

	perform_goto(srv, doc, pos, "textDocument/declaration", FALSE);
}


void lsp_goto_type_definition(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return;

	perform_goto(srv, doc, pos, "textDocument/typeDefinition", FALSE);
}


void lsp_goto_implementations(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return;

	perform_goto(srv, doc, pos, "textDocument/implementation", TRUE);
}


void lsp_goto_references(gint pos)
{
	GeanyDocument *doc = document_get_current();
	LspServer *srv = lsp_server_get(doc);

	if (!srv)
		return;

	perform_goto(srv, doc, pos, "textDocument/references", TRUE);
}
