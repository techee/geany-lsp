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

#include "lsp/lsp-goto-anywhere.h"
#include "lsp/lsp-goto-panel.h"
#include "lsp/lsp-symbols.h"
#include "lsp/lsp-tm-tag.h"
#include "lsp/lsp-utils.h"

#include <gtk/gtk.h>
#include <geanyplugin.h>


typedef struct
{
	GeanyDocument *doc;
	gchar *query;
} DocQueryData;


extern GeanyData *geany_data;


static void workspace_symbol_cb(GPtrArray *symbols, gpointer user_data)
{
	lsp_goto_panel_fill(symbols);
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
	DocQueryData *data = user_data;
	GeanyDocument *doc = document_get_current();
	GPtrArray *symbols = lsp_symbols_doc_get_cached(doc);
	gchar *text = data->query;
	GPtrArray *filtered;
	TMTag *tag;
	guint i;

	if (doc != data->doc)
		return;

	filtered = filter_tag_list(symbols, text[0] ? text + 1 : text, tm_tag_max_t, FALSE, 0);
	foreach_ptr_array(tag, i, filtered)
	{
		// TODO: total hack, just storing path "somewhere"
		tag->inheritance = g_strdup(doc->real_path);
	}
	lsp_goto_panel_fill(filtered);

	g_ptr_array_free(filtered, TRUE);
	g_free(data->query);
	g_free(data);
}


static void goto_line(const gchar *line_str)
{
	GeanyDocument *doc = document_get_current();
	GPtrArray *empty_arr = g_ptr_array_new();
	gint lineno = atoi(line_str);

	lsp_goto_panel_fill(empty_arr);

	if (lineno > 0)
		navqueue_goto_line(doc, doc, lineno);

	g_ptr_array_free(empty_arr, TRUE);
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

	filtered = filter_tag_list(arr, file_str, tm_tag_max_t, FALSE, 0);
	lsp_goto_panel_fill(filtered);

	g_ptr_array_free(filtered, TRUE);
	g_ptr_array_free(arr, TRUE);
}


static void goto_tm_symbol(const gchar *query, GPtrArray *tags, TMParserType lang)
{
	GPtrArray *filtered = tags ? filter_tag_list(tags, query, tm_tag_max_t, TRUE, lang) : 
		g_ptr_array_new();
	lsp_goto_panel_fill(filtered);
	g_ptr_array_free(filtered, TRUE);
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
		{
			DocQueryData *data = g_new0(DocQueryData, 1);
			data->query = g_strdup(query_str);
			data->doc = doc;
			lsp_symbols_doc_request(doc, doc_symbol_cb, data);
		}
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

	if (prefill)
		query = get_current_iden(doc);
	else
		query = g_strdup("");
	SETPTR(query, g_strconcat(query_type, query, NULL));

	lsp_goto_panel_show(query, perform_lookup);

	g_free(query);
}


void lsp_goto_anywhere_for_workspace(void)
{
	goto_panel_query("#", TRUE);
}


void lsp_goto_anywhere_for_doc(void)
{
	goto_panel_query("@", TRUE);
}


void lsp_goto_anywhere_for_line(void)
{
	goto_panel_query(":", FALSE);
}


void lsp_goto_anywhere_for_file(void)
{
	goto_panel_query("", FALSE);
}
