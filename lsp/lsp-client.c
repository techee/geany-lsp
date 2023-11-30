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


typedef struct
{
	gchar *method_name;
	gpointer user_data;
	LspClientCallback callback;
} CallbackData;


static void call_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	JsonrpcClient *self = (JsonrpcClient *)source_object;
	CallbackData *data = user_data;
	GVariant *return_value = NULL;
	GError *error = NULL;

	//TODO: log errors
	if (jsonrpc_client_call_finish(self, res, &return_value, &error))
		lsp_log(lsp_server_get_log_info(self), LspLogClientMessageReceived, data->method_name, return_value);

	if (data->callback)
		data->callback(return_value, error, data->user_data);

	if (return_value)
		g_variant_unref(return_value);

	if (error)
		g_error_free(error);

	g_free(data->method_name);
	g_free(data);
}


void lsp_client_call_async(LspServer *srv, const gchar *method, GVariant *params,
	LspClientCallback callback, gpointer user_data)
{
	CallbackData *data = g_new0(CallbackData, 1);

	data->method_name = g_strdup(method);
	data->user_data = user_data;
	data->callback = callback;

	lsp_log(lsp_server_get_log_info(srv->rpc_client), LspLogClientMessageSent, method, params);

	jsonrpc_client_call_async(srv->rpc_client, method, params, NULL, call_cb, data);
}


static void notify_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	JsonrpcClient *self = (JsonrpcClient *)source_object;
	CallbackData *data = user_data;
	GVariant *return_value = NULL;
	GError *error = NULL;

	//TODO: log errors
	jsonrpc_client_send_notification_finish(self, res, &error);

	if (data->callback)
		data->callback(return_value, error, data->user_data);

	if (return_value)
		g_variant_unref(return_value);

	if (error)
		g_error_free(error);

	g_free(data);
}


void lsp_client_notify(LspServer *srv, const gchar *method, GVariant *params,
	LspClientCallback callback, gpointer user_data)
{
	gboolean params_added = FALSE;
	CallbackData *data = g_new0(CallbackData, 1);

	data->user_data = user_data;
	data->callback = callback;

	lsp_log(lsp_server_get_log_info(srv->rpc_client), LspLogClientNotificationSent, method, params);

	if (!params)
	{
		params = JSONRPC_MESSAGE_NEW("gopls_bug_workarond", JSONRPC_MESSAGE_PUT_STRING("https://github.com/golang/go/issues/57459"));
		params_added = TRUE;
	}

	jsonrpc_client_send_notification_async(srv->rpc_client, method, params, NULL, notify_cb, data);

	if (params_added)
		g_variant_unref(params);
}
