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
