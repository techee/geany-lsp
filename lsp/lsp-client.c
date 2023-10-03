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

#include "lsp/lsp-client.h"
#include "lsp/lsp-server.h"
#include "lsp/lsp-log.h"

#include <stdio.h>


JsonrpcClient *lsp_client_new(GIOStream *io_stream)
{
	return jsonrpc_client_new(io_stream);;
}


void lsp_client_start_listening(JsonrpcClient *self)
{
	jsonrpc_client_start_listening(self);
}


void lsp_client_close(JsonrpcClient *self)
{
	jsonrpc_client_close(self, NULL, NULL);
}


typedef struct
{
	GAsyncReadyCallback callback;
	gchar *method_name;
	gpointer user_data;
} CallbackData;



void lsp_client_call_async(JsonrpcClient *self, const gchar *method, GVariant *params,
	GAsyncReadyCallback callback, gpointer user_data)
{
	lsp_log(lsp_server_get_log_info(self), LspLogClientMessageSent, method, params);

	jsonrpc_client_call_async(self, method, params, NULL, callback, user_data);
}


gboolean lsp_client_call_finish(JsonrpcClient *self, GAsyncResult *result,
	GVariant **return_value)
{
	gboolean ret = jsonrpc_client_call_finish(self, result, return_value, NULL);

	if (ret)
		lsp_log(lsp_server_get_log_info(self), LspLogClientMessageReceived, NULL, *return_value);

	return ret;
}


void lsp_client_notify_async(JsonrpcClient *self, const gchar *method, GVariant *params,
	GAsyncReadyCallback callback, gpointer user_data)
{
	gboolean params_added = FALSE;
	GCancellable *cancellable = NULL;

	lsp_log(lsp_server_get_log_info(self), LspLogClientNotificationSent, method, params);

	if (!params)
	{
		params = JSONRPC_MESSAGE_NEW("gopls_bug_workarond", JSONRPC_MESSAGE_PUT_STRING("https://github.com/golang/go/issues/57459"));
		params_added = TRUE;
	}

	jsonrpc_client_send_notification_async(self, method, params, cancellable, callback, user_data);

	if (params_added)
		g_variant_unref(params);
}


gboolean lsp_client_notify_finish(JsonrpcClient *self, GAsyncResult *result)
{
	return jsonrpc_client_send_notification_finish(self, result, NULL);
}


gboolean lsp_client_notify(JsonrpcClient *self, const gchar *method, GVariant *params)
{
	gboolean params_added = FALSE;

	lsp_log(lsp_server_get_log_info(self), LspLogClientNotificationSent, method, params);

	if (!params)
	{
		params = JSONRPC_MESSAGE_NEW("gopls_bug_workarond", JSONRPC_MESSAGE_PUT_STRING("https://github.com/golang/go/issues/57459"));
		params_added = TRUE;
	}

	jsonrpc_client_send_notification_async(self, method, params, NULL, NULL, NULL);

	if (params_added)
		g_variant_unref(params);

	return TRUE;
}
