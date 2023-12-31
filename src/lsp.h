/*
 *      lsp.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2023 The Geany contributors
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef GEANY_LSP_H
#define GEANY_LSP_H 1

#include "document.h"

G_BEGIN_DECLS

typedef void (*LspSymbolRequestCallback) (gpointer user_data);

typedef struct {
	gboolean (*autocomplete_available)(GeanyDocument *doc);
	void (*autocomplete_perform)(GeanyDocument *doc);

	gboolean (*calltips_available)(GeanyDocument *doc);
	void (*calltips_show)(GeanyDocument *doc);

	gboolean (*goto_available)(GeanyDocument *doc);
	void (*goto_perform)(GeanyDocument *doc, gint pos, gboolean definition);

	gboolean (*doc_symbols_available)(GeanyDocument *doc);
	void (*doc_symbols_request)(GeanyDocument *doc, LspSymbolRequestCallback callback, gpointer user_data);
	GPtrArray *(*doc_symbols_get_cached)(GeanyDocument *doc);

	gboolean (*symbol_highlight_available)(GeanyDocument *doc);
	void (*symbol_highlight_request)(GeanyDocument *doc, LspSymbolRequestCallback callback, gpointer user_data);
	const gchar *(*symbol_highlight_get_cached)(GeanyDocument *doc);

	gchar _dummy[1024];
} Lsp;


void lsp_register(Lsp *lsp);
void lsp_unregister(Lsp *lsp);


#ifdef GEANY_PRIVATE

guint lsp_get_symbols_icon_id(guint kind);

gboolean lsp_autocomplete_available(GeanyDocument *doc);
void lsp_autocomplete_perform(GeanyDocument *doc);

gboolean lsp_calltips_available(GeanyDocument *doc);
void lsp_calltips_show(GeanyDocument *doc);

gboolean lsp_goto_available(GeanyDocument *doc);
void lsp_goto_perform(GeanyDocument *doc, gint pos, gboolean definition);

gboolean lsp_doc_symbols_available(GeanyDocument *doc);
void lsp_doc_symbols_request(GeanyDocument *doc, LspSymbolRequestCallback callback, gpointer user_data);
GPtrArray *lsp_doc_symbols_get_cached(GeanyDocument *doc);

gboolean lsp_symbol_highlight_available(GeanyDocument *doc);
void lsp_symbol_highlight_request(GeanyDocument *doc, LspSymbolRequestCallback callback, gpointer user_data);
const gchar *lsp_symbol_highlight_get_cached(GeanyDocument *doc);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_LSP_H */
