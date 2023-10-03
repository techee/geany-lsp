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

#ifndef LSP_CLIENT_H
#define LSP_CLIENT_H 1

#include <jsonrpc-glib.h>

JsonrpcClient *lsp_client_new(GIOStream *io_stream);
void lsp_client_start_listening(JsonrpcClient *self);
void lsp_client_close(JsonrpcClient *self);

void lsp_client_call_async(JsonrpcClient *self, const gchar *method, GVariant *params,
	GAsyncReadyCallback callback, gpointer user_data);
gboolean lsp_client_call_finish(JsonrpcClient *self, GAsyncResult *result,
	GVariant **return_value);

void lsp_client_notify_async(JsonrpcClient *self, const gchar *method, GVariant *params,
	GAsyncReadyCallback callback, gpointer user_data);
gboolean lsp_client_notify_finish(JsonrpcClient *self, GAsyncResult *result);

gboolean lsp_client_notify(JsonrpcClient *self, const gchar *method, GVariant *params);

#endif  /* LSP_CLIENT_H */
