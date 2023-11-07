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

#include "lsp/lsp-progress.h"


typedef struct
{
	LspProgressToken token;
	gchar *title;
} LspProgress;


static gint progress_num = 0;


static void progress_free(LspProgress *p)
{
	g_free(p->token.token_str);
	g_free(p->title);
	g_free(p);
}


void lsp_progress_create(LspServer *server, LspProgressToken token)
{
	LspProgress *p = g_new0(LspProgress, 1);

	p->token.token_str = g_strdup(token.token_str);
	p->token.token_int = token.token_int;

	server->progress_ops = g_slist_prepend(server->progress_ops, p);
}


static gboolean token_equal(LspProgressToken t1, LspProgressToken t2)
{
	if (t1.token_str != NULL || t2.token_str != NULL)
		return g_strcmp0(t1.token_str, t2.token_str) == 0;
	return t1.token_int == t2.token_int;
}


void lsp_progress_begin(LspServer *server, LspProgressToken token, const gchar *title, const gchar *message)
{
	GSList *node;

	foreach_slist(node, server->progress_ops)
	{
		LspProgress *p = node->data;
		if (token_equal(p->token, token))
		{
			p->title = g_strdup(title);
			ui_set_statusbar(FALSE, "%s: %s", p->title, message ? message : "");
			if (progress_num == 0)
				ui_progress_bar_start("");
			progress_num++;
			break;
		}
	}
}


void lsp_progress_report(LspServer *server, LspProgressToken token, const gchar *message)
{
	GSList *node;

	foreach_slist(node, server->progress_ops)
	{
		LspProgress *p = node->data;
		if (token_equal(p->token, token))
		{
			ui_set_statusbar(FALSE, "%s: %s", p->title, message ? message : "");
			break;
		}
	}
}


void lsp_progress_end(LspServer *server, LspProgressToken token, const gchar *message)
{
	GSList *node;

	foreach_slist(node, server->progress_ops)
	{
		LspProgress *p = node->data;
		if (token_equal(p->token, token))
		{
			if (progress_num > 0)
				progress_num--;
			if (progress_num == 0)
				ui_progress_bar_stop();

			if (message)
				ui_set_statusbar(FALSE, "%s: %s", p->title, message ? message : "");
			else
				ui_set_statusbar(FALSE, "");

			server->progress_ops = g_slist_remove_link(server->progress_ops, node);
			g_slist_free_full(node, (GDestroyNotify)progress_free);
			break;
		}
	}
}


void lsp_progress_free_all(LspServer *server)
{
	guint len = g_slist_length(server->progress_ops);

	g_slist_free_full(server->progress_ops, (GDestroyNotify)progress_free);
	server->progress_ops = 0;
	progress_num = MAX(0, progress_num - len);
	if (progress_num == 0)
		ui_progress_bar_stop();
}
