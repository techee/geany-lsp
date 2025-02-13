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

#ifndef LSP_SYMBOLS_H
#define LSP_SYMBOLS_H 1

#include "lsp-server.h"

#include <glib.h>

void lsp_symbols_doc_request(GeanyDocument *doc, LspCallback callback,
	gpointer user_data);

GPtrArray *lsp_symbols_doc_get_cached(GeanyDocument *doc);


typedef void (*LspWorkspaceSymbolRequestCallback) (GPtrArray *arr, gpointer user_data);

void lsp_symbols_workspace_request(GeanyDocument *doc, const gchar *query, LspWorkspaceSymbolRequestCallback callback,
	gpointer user_data);

void lsp_symbols_destroy(GeanyDocument *doc);

#endif  /* LSP_SYMBOLS_H */
