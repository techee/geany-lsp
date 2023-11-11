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

/* This file contains mostly stolen code from the Colomban Wendling's Commander
 * plugin. Thanks! */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "lsp/lsp-goto-panel.h"
#include "lsp/lsp-server.h"
#include "lsp/lsp-symbols.h"
#include "lsp/lsp-symbol-kinds.h"
#include "lsp/lsp-utils.h"
#include "lsp/lsp-tm-tag.h"

#include <gtk/gtk.h>
#include <geanyplugin.h>


enum {
	COL_ICON,
	COL_LABEL,
	COL_PATH,
	COL_LINENO,
	COL_COUNT
};


//TODO: free on plugin unload
struct {
	GtkWidget *panel;
	GtkWidget *entry;
	GtkWidget *tree_view;
	GtkListStore *store;
} panel_data = {
	NULL, NULL, NULL, NULL
};


extern GeanyData *geany_data;

static void create_panel(void);


static void tree_view_set_cursor_from_iter(GtkTreeView *view, GtkTreeIter *iter)
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path(gtk_tree_view_get_model(view), iter);
	gtk_tree_view_set_cursor(view, path, NULL, FALSE);
	gtk_tree_path_free(path);
}


static void tree_view_move_focus(GtkTreeView *view, GtkMovementStep step, gint amount)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeModel *model = gtk_tree_view_get_model(view);
	gboolean valid = FALSE;

	gtk_tree_view_get_cursor(view, &path, NULL);
	if (!path)
		valid = gtk_tree_model_get_iter_first(model, &iter);
	else
	{
		switch (step) {
			case GTK_MOVEMENT_BUFFER_ENDS:
				valid = gtk_tree_model_get_iter_first(model, &iter);
				if (valid && amount > 0)
				{
					GtkTreeIter prev;

					do {
						prev = iter;
					} while (gtk_tree_model_iter_next(model, &iter));
					iter = prev;
				}
				break;

			case GTK_MOVEMENT_PAGES:
				/* FIXME: move by page */
			case GTK_MOVEMENT_DISPLAY_LINES:
				gtk_tree_model_get_iter(model, &iter, path);
				if (amount > 0)
				{
					while ((valid = gtk_tree_model_iter_next(model, &iter)) && --amount > 0)
						;
				}
				else if (amount < 0)
				{
					while ((valid = gtk_tree_path_prev(path)) && --amount > 0)
						;

					if (valid)
						gtk_tree_model_get_iter(model, &iter, path);
				}
				break;

			default:
				g_assert_not_reached();
		}
		gtk_tree_path_free(path);
	}

	if (valid)
		tree_view_set_cursor_from_iter(view, &iter);
	else
		gtk_widget_error_bell(GTK_WIDGET(view));
}


static void tree_view_activate_focused_row(GtkTreeView *view)
{
	GtkTreePath *path;
	GtkTreeViewColumn *column;

	gtk_tree_view_get_cursor(view, &path, &column);
	if (path)
	{
		gtk_tree_view_row_activated(view, path, column);
		gtk_tree_path_free(path);
	}
}


static void fill_store(GtkListStore *store, GPtrArray *symbols)
{
	TMTag *tag;
	guint i;

	foreach_ptr_array(tag, i, symbols)
	{
		LspGeanyIcon icon;
		gchar *path;
		gchar *label;

		if (tag->file)
		{
			LspSymbolKind kind = lsp_symbol_kinds_tm_to_lsp(tag->type);
			icon = lsp_symbol_kinds_get_symbol_icon(kind);
			path = tag->file->file_name;
		}
		else
		{
			icon = lsp_symbol_kinds_get_symbol_icon((LspSymbolKind)tag->type);
			path = tag->inheritance;  // TODO: hack, we stored path to inheritance before
		}

		if (path && tag->line > 0)
			label = g_markup_printf_escaped("%s\n<small><i>%s:%lu</i></small>",
				tag->name, path, tag->line);
		else if (path)
			label = g_markup_printf_escaped("%s\n<small><i>%s</i></small>",
				tag->name, path);
		else
			label = g_markup_printf_escaped("%s", tag->name);

		gtk_list_store_insert_with_values(store, NULL, -1,
			COL_ICON, lsp_symbol_kinds_get_icon_pixbuf(icon),
			COL_LABEL, label,
			COL_PATH, path,
			COL_LINENO, tag->line,
			-1);

		g_free(label);
	}
}


static gboolean on_panel_key_press_event(GtkWidget *widget, GdkEventKey *event,
	gpointer dummy)
{
	switch (event->keyval) {
		case GDK_KEY_Escape:
			gtk_widget_hide(widget);
			return TRUE;

		case GDK_KEY_Tab:
			/* avoid leaving the entry */
			return TRUE;

		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_ISO_Enter:
			tree_view_activate_focused_row(GTK_TREE_VIEW(panel_data.tree_view));
			return TRUE;

		case GDK_KEY_Page_Up:
		case GDK_KEY_Page_Down:
			tree_view_move_focus(GTK_TREE_VIEW(panel_data.tree_view),
				GTK_MOVEMENT_PAGES, event->keyval == GDK_KEY_Page_Up ? -1 : 1);
		  return TRUE;

		case GDK_KEY_Up:
		case GDK_KEY_Down: {
			tree_view_move_focus(GTK_TREE_VIEW(panel_data.tree_view),
				GTK_MOVEMENT_DISPLAY_LINES, event->keyval == GDK_KEY_Up ? -1 : 1);
			return TRUE;
		}
	}

	return FALSE;
}


static void workspace_symbol_cb(GPtrArray *symbols, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeView *view = GTK_TREE_VIEW(panel_data.tree_view);

	gtk_list_store_clear(panel_data.store);

	fill_store(panel_data.store, symbols);

	if (gtk_tree_model_get_iter_first(gtk_tree_view_get_model(view), &iter))
		tree_view_set_cursor_from_iter(GTK_TREE_VIEW(panel_data.tree_view), &iter);
}


/* stolen from symbols.c and modified for our needs*/
static GPtrArray *filter_tag_list(GPtrArray *tags_array, const gchar *tag_filter, TMTagType tag_types,
	gboolean filter_lang, TMParserType lang)
{
	GPtrArray *tag_names = g_ptr_array_new();;
	guint i;
	guint j = 0;
	gchar **tf_strv;

	if (!tags_array)
		return tag_names;

	tf_strv = g_strsplit_set(tag_filter, " ", -1);

	for (i = 0; i < tags_array->len && j < 100; ++i)
	{
		TMTag *tag = TM_TAG(tags_array->pdata[i]);

		if (tag->type & tag_types && (!filter_lang || tag->lang == lang))
		{
			gboolean filtered = FALSE;
			gchar **val;
			gchar *full_tagname = tag->name;
			gchar *normalized_tagname = g_utf8_normalize(full_tagname, -1, G_NORMALIZE_ALL);

			foreach_strv(val, tf_strv)
			{
				gchar *normalized_val = g_utf8_normalize(*val, -1, G_NORMALIZE_ALL);

				if (normalized_tagname != NULL && normalized_val != NULL)
				{
					gchar *case_normalized_tagname = g_utf8_casefold(normalized_tagname, -1);
					gchar *case_normalized_val = g_utf8_casefold(normalized_val, -1);

					filtered = strstr(case_normalized_tagname, case_normalized_val) == NULL;
					g_free(case_normalized_tagname);
					g_free(case_normalized_val);
				}
				g_free(normalized_val);

				if (filtered)
					break;
			}
			if (!filtered)
			{
				g_ptr_array_add(tag_names, tag);
				j++;
			}

			g_free(normalized_tagname);
		}
	}

	g_strfreev(tf_strv);

	return tag_names;
}


static void doc_symbol_cb(gpointer user_data)
{
	GtkTreeView *view = GTK_TREE_VIEW(panel_data.tree_view);
	GeanyDocument *doc = document_get_current();
	GPtrArray *symbols = lsp_symbols_doc_get_cached(doc);
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(panel_data.entry));
	GtkTreeIter iter;
	GPtrArray *filtered;
	TMTag *tag;
	guint i;

	if (doc != user_data)
		return;

	gtk_list_store_clear(panel_data.store);

	filtered = filter_tag_list(symbols, text[0] ? text + 1 : text, tm_tag_max_t, FALSE, 0);
	foreach_ptr_array(tag, i, filtered)
	{
		// TODO: total hack, just storing path "somewhere"
		tag->inheritance = g_strdup(doc->real_path);
	}
	fill_store(panel_data.store, filtered);

	if (gtk_tree_model_get_iter_first(gtk_tree_view_get_model(view), &iter))
		tree_view_set_cursor_from_iter(GTK_TREE_VIEW(panel_data.tree_view), &iter);

	g_ptr_array_free(filtered, TRUE);
}


static void goto_line(const gchar *line_str)
{
	GeanyDocument *doc = document_get_current();
	gint lineno = atoi(line_str);

	gtk_list_store_clear(panel_data.store);

	if (lineno > 0)
		navqueue_goto_line(doc, doc, lineno);
}


static void goto_file(const gchar *file_str)
{
	GPtrArray *arr = g_ptr_array_new_full(0, (GDestroyNotify)lsp_tm_tag_unref);
	GPtrArray *filtered;
	guint i;

	foreach_document(i)
	{
		GeanyDocument *doc = documents[i];
		TMTag *tag;

		if (!doc->file_name)
			continue;

		tag = lsp_tm_tag_new();
		tag->name = g_path_get_basename(doc->file_name);
		// TODO: total hack, just storing path "somewhere"
		tag->inheritance = g_strdup(doc->real_path);
		tag->type = tm_tag_other_t;
		g_ptr_array_add(arr, tag);
	}

	gtk_list_store_clear(panel_data.store);
	filtered = filter_tag_list(arr, file_str, tm_tag_max_t, FALSE, 0);
	fill_store(panel_data.store, filtered);

	g_ptr_array_free(filtered, TRUE);
	g_ptr_array_free(arr, TRUE);
}


static void goto_tm_symbol(const gchar *query, GPtrArray *tags, TMParserType lang)
{
	gtk_list_store_clear(panel_data.store);

	if (tags)
	{
		GPtrArray *filtered = filter_tag_list(tags, query, tm_tag_max_t, TRUE, lang);
		fill_store(panel_data.store, filtered);
		g_ptr_array_free(filtered, TRUE);
	}

}


static void perform_lookup(const gchar *query)
{
	GeanyDocument *doc = document_get_current();
	const gchar *query_str = query ? query : "";

	if (g_str_has_prefix(query_str, "#"))
	{
		if (lsp_server_get(doc))
			lsp_symbols_workspace_request(doc->file_type, query_str+1, workspace_symbol_cb, NULL);
		else
			// TODO: possibly improve performance by binary searching the start and the end point
			goto_tm_symbol(query_str+1, geany_data->app->tm_workspace->tags_array, doc->file_type->lang);
	}
	else if (g_str_has_prefix(query_str, "@"))
	{
		if (lsp_server_get(doc))
			lsp_symbols_doc_request(doc, doc_symbol_cb, doc);
		else
		{
			GPtrArray *tags = doc->tm_file ? doc->tm_file->tags_array : g_ptr_array_new();
			goto_tm_symbol(query_str+1, tags, doc->file_type->lang);
			if (!doc->tm_file)
				g_ptr_array_free(tags, TRUE);
		}
	}
	else if (g_str_has_prefix(query_str, ":"))
		goto_line(query_str+1);
	else
		goto_file(query_str);
}


static void on_entry_text_notify(GObject *object, GParamSpec *pspec, gpointer dummy)
{
	GtkTreeIter iter;
	GtkTreeView *view  = GTK_TREE_VIEW(panel_data.tree_view);
	GtkTreeModel *model = gtk_tree_view_get_model(view);
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(panel_data.entry));

	perform_lookup(text);

	if (gtk_tree_model_get_iter_first(model, &iter))
		tree_view_set_cursor_from_iter(view, &iter);
}


static void on_entry_activate(GtkEntry *entry, gpointer dummy)
{
	tree_view_activate_focused_row(GTK_TREE_VIEW(panel_data.tree_view));
}


static void on_panel_hide(GtkWidget *widget, gpointer dummy)
{
	gtk_list_store_clear(panel_data.store);
}


static void on_panel_show(GtkWidget *widget, gpointer dummy)
{
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(panel_data.entry));
	gboolean select_first = TRUE;

	if (text && (text[0] == ':' || text[0] == '#' || text[0] == '@'))
		select_first = FALSE;

	gtk_widget_grab_focus(panel_data.entry);
	gtk_editable_select_region(GTK_EDITABLE(panel_data.entry), select_first ? 0 : 1, -1);
}


static void on_view_row_activated(GtkTreeView *view, GtkTreePath *path,
	GtkTreeViewColumn *column, gpointer dummy)
{
	GtkTreeModel *model = gtk_tree_view_get_model(view);
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		GeanyDocument *doc;
		gchar *file_path;
		gint line;

		gtk_tree_model_get(model, &iter,
			COL_PATH, &file_path,
			COL_LINENO, &line,
			-1);

		doc = document_open_file(file_path, FALSE, NULL, NULL);

		if (doc && line > 0)
			navqueue_goto_line(document_get_current(), doc, line);

		g_free(file_path);
	}

	gtk_widget_hide(panel_data.panel);
}


static void create_panel(void)
{
	GtkWidget *frame, *box, *scroll;
	GtkTreeViewColumn *col;
	GtkCellRenderer *renderer;

	panel_data.panel = g_object_new(GTK_TYPE_WINDOW,
		"decorated", FALSE,
		"default-width", 500,
		"default-height", 350,
		"transient-for", geany_data->main_widgets->window,
		"window-position", GTK_WIN_POS_CENTER_ON_PARENT,
		"type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
		"skip-taskbar-hint", TRUE,
		"skip-pager-hint", TRUE,
		NULL);
	g_signal_connect(panel_data.panel, "focus-out-event",
		G_CALLBACK(gtk_widget_hide), NULL);
	g_signal_connect(panel_data.panel, "show",
		G_CALLBACK(on_panel_show), NULL);
	g_signal_connect(panel_data.panel, "hide",
		G_CALLBACK(on_panel_hide), NULL);
	g_signal_connect(panel_data.panel, "key-press-event",
		G_CALLBACK(on_panel_key_press_event), NULL);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(panel_data.panel), frame);

	box = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), box);

	panel_data.entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(box), panel_data.entry, FALSE, TRUE, 0);

	scroll = g_object_new(GTK_TYPE_SCROLLED_WINDOW,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);
	gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);

	panel_data.tree_view = gtk_tree_view_new();
	gtk_widget_set_can_focus(panel_data.tree_view, FALSE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(panel_data.tree_view), FALSE);

	panel_data.store = gtk_list_store_new(COL_COUNT,
		GDK_TYPE_PIXBUF,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(panel_data.tree_view), GTK_TREE_MODEL(panel_data.store));
	g_object_unref(panel_data.store);

	renderer = gtk_cell_renderer_pixbuf_new();
	col = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(col, renderer, FALSE);
	gtk_tree_view_column_set_attributes(col, renderer, "pixbuf", COL_ICON, NULL);
	g_object_set(renderer, "xalign", 0.0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(panel_data.tree_view), col);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	col = gtk_tree_view_column_new_with_attributes(NULL, renderer,
		"markup", COL_LABEL,
		NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(panel_data.tree_view), col);

	g_signal_connect(panel_data.tree_view, "row-activated",
		G_CALLBACK(on_view_row_activated), NULL);
	gtk_container_add(GTK_CONTAINER(scroll), panel_data.tree_view);

	/* connect entry signals after the view is created as they use it */
	g_signal_connect(panel_data.entry, "notify::text",
		G_CALLBACK(on_entry_text_notify), NULL);
	g_signal_connect(panel_data.entry, "activate",
		G_CALLBACK(on_entry_activate), NULL);

	gtk_widget_show_all(frame);
}


static gchar *get_current_iden(GeanyDocument *doc)
{
	//TODO: use configured wordchars (also change in Geany)
	const gchar *wordchars = GEANY_WORDCHARS;
	GeanyFiletypeID ft = doc->file_type->id;
	ScintillaObject *sci = doc->editor->sci;
	gint current_pos = sci_get_current_position(sci);
	gint start_pos, end_pos, pos;

	if (ft == GEANY_FILETYPES_LATEX)
		wordchars = GEANY_WORDCHARS"\\"; /* add \ to word chars if we are in a LaTeX file */
	else if (ft == GEANY_FILETYPES_CSS)
		wordchars = GEANY_WORDCHARS"-"; /* add - because they are part of property names */

	pos = current_pos;
	while (TRUE)
	{
		gint new_pos = SSM(sci, SCI_POSITIONBEFORE, pos, 0);
		if (new_pos == pos)
			break;
		if (pos - new_pos == 1)
		{
			gchar c = sci_get_char_at(sci, new_pos);
			if (!strchr(wordchars, c))
				break;
		}
		pos = new_pos;
	}
	start_pos = pos;

	pos = current_pos;
	while (TRUE)
	{
		gint new_pos = SSM(sci, SCI_POSITIONAFTER, pos, 0);
		if (new_pos == pos)
			break;
		if (new_pos - pos == 1)
		{
			gchar c = sci_get_char_at(sci, pos);
			if (!strchr(wordchars, c))
				break;
		}
		pos = new_pos;
	}
	end_pos = pos;

	if (start_pos == end_pos)
		return g_strdup("");

	return sci_get_contents_range(sci, start_pos, end_pos);
}


static void goto_panel_query(const gchar *query_type, gboolean prefill)
{
	GeanyDocument *doc = document_get_current();
	gchar *query;

	if (!doc)
		return;

	if (!panel_data.panel)
		create_panel();

	if (prefill)
		query = get_current_iden(doc);
	else
		query = g_strdup("");
	SETPTR(query, g_strconcat(query_type, query, NULL));

	gtk_entry_set_text(GTK_ENTRY(panel_data.entry), query);
	gtk_list_store_clear(panel_data.store);
	gtk_widget_show(panel_data.panel);

	perform_lookup(query);

	g_free(query);
}


void lsp_goto_panel_for_workspace(void)
{
	goto_panel_query("#", TRUE);
}


void lsp_goto_panel_for_doc(void)
{
	goto_panel_query("@", TRUE);
}


void lsp_goto_panel_for_line(void)
{
	goto_panel_query(":", FALSE);
}


void lsp_goto_panel_for_file(void)
{
	goto_panel_query("", FALSE);
}
