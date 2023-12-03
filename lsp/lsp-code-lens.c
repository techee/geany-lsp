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

#include "lsp/lsp-code-lens.h"
#include "lsp/lsp-utils.h"
#include "lsp/lsp-client.h"
#include "lsp/lsp-sync.h"

#include <jsonrpc-glib.h>


extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;


static void code_lens_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	if (!error)
	{
		GeanyDocument *doc = user_data;
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
		}
	}
}


static gboolean retry_cb(gpointer user_data)
{
	GeanyDocument *doc = user_data;

	//printf("retrying code lens\n");
	if (doc == document_get_current())
	{
		LspServer *srv = lsp_server_get_if_running(doc);
		if (!lsp_server_is_usable(doc))
			;  // server died or misconfigured
		else if (!srv)
			return TRUE;  // retry
		else
		{
			// should be successful now
			lsp_code_lens_send_request(doc);
			return FALSE;
		}
	}

	// server shut down or document not current any more
	return FALSE;
}


void lsp_code_lens_send_request(GeanyDocument *doc)
{
	LspServer *server = lsp_server_get_if_running(doc);
	gchar *doc_uri;
	GVariant *node;

	if (!server)
	{
		// happens when Geany and LSP server started - we cannot send the request yet
		plugin_timeout_add(geany_plugin, 300, retry_cb, doc);
		return;
	}

	doc_uri = lsp_utils_get_doc_uri(doc);

	/* Geany requests symbols before firing "document-activate" signal so we may
	 * need to request document opening here */
	if (!lsp_sync_is_document_open(doc))
		lsp_sync_text_document_did_open(server, doc);

	node = JSONRPC_MESSAGE_NEW(
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}"
	);
	lsp_client_call(server, "textDocument/codeLens", node,
		code_lens_cb, doc);

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	g_free(doc_uri);
	g_variant_unref(node);
}
