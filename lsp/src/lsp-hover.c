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

#include "lsp-hover.h"
#include "lsp-popup.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"

#include <jsonrpc-glib.h>


typedef struct {
	GeanyDocument *doc;
	gint pos;
	gboolean keyboard_invoked;
} LspHoverData;


static void hover_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	LspHoverData *data = user_data;

	/* no focus check for keyboard-invoked hover - when invoked from the
	 * menu, the editor may not have the focus back when the response
	 * arrives */
	if (doc == data->doc &&
		(data->keyboard_invoked || gtk_widget_has_focus(GTK_WIDGET(doc->editor->sci))))
	{
		const gchar *str = NULL;

		/* a failed request is treated like a response with nothing to
		 * display */
		if (!error)
		{
			JSONRPC_MESSAGE_PARSE(return_value,
				"contents", "{",
					"value", JSONRPC_MESSAGE_GET_STRING(&str),
				"}");

			//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));
		}

		/* don't let late responses of mouse-driven requests replace or
		 * hide keyboard-driven signature popups */
		if (!data->keyboard_invoked && lsp_popup_showing_signature(doc))
			;
		else if (str && strlen(str) > 0)
			lsp_popup_show(doc, data->pos, str);
		else if (!data->keyboard_invoked)
		{
			/* hide the popup when the mouse rests over a position for
			 * which the server has nothing to display or the request
			 * failed */
			lsp_popup_hide(doc);
		}
	}

	g_free(user_data);
}


void lsp_hover_send_request(LspServer *server, GeanyDocument *doc, gint pos,
	gboolean keyboard_invoked)
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
	data->keyboard_invoked = keyboard_invoked;

	lsp_rpc_call(server, "textDocument/hover", node,
		hover_cb, data);

	g_free(doc_uri);
	g_variant_unref(node);
}
