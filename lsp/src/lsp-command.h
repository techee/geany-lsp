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

#ifndef LSP_COMMAND_H
#define LSP_COMMAND_H 1

#include "lsp-server.h"

#include <glib.h>

typedef struct
{
	guint line;
	gchar *title;
	gchar *kind;
	gchar *command;
	GVariant *arguments;
	GVariant *edit;
	GVariant *data;
} LspCommand;

typedef gboolean (*CodeActionCallback) (GPtrArray *actions, gpointer user_data);

void lsp_command_free(LspCommand *cmd);

void lsp_command_perform(LspServer *server, LspCommand *cmd, LspCallback callback, gpointer user_data);

// Custom commands defined by the command_N configuration options. The index is
// 0-based and corresponds to the "Command N" keybinding N-1. The returned
// command is newly allocated and should be freed with lsp_command_free()
LspCommand *lsp_command_get_custom(LspServer *server, guint index);
gboolean lsp_command_has_custom(LspServer *server);

// Careful! Returning TRUE from actions_resolved_cb frees the actions array, FALSE passes the
// ownership to the caller
void lsp_command_send_code_action_request(GeanyDocument *doc, gint pos, CodeActionCallback actions_resolved_cb, gpointer user_data);

#endif  /* LSP_COMMAND_H */
