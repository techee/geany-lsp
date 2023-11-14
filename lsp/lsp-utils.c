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

#include "lsp/lsp-utils.h"

#include <geanyplugin.h>
#include <jsonrpc-glib.h>


//TODO: remove once part of geany-plugins
#define PLUGIN "lsp"


extern GeanyData *geany_data;

extern LspProjectConfigurationType project_configuration_type;
extern gchar *project_configuration_file;


LspPosition lsp_utils_scintilla_pos_to_lsp(ScintillaObject *sci, gint sci_pos)
{
	LspPosition lsp_pos;
	gint line_start_pos;

	lsp_pos.line = sci_get_line_from_position(sci, sci_pos);
	line_start_pos = sci_get_position_from_line(sci, lsp_pos.line);
	lsp_pos.character = SSM(sci, SCI_COUNTCODEUNITS, line_start_pos, sci_pos);
	return lsp_pos;
}


gint lsp_utils_lsp_pos_to_scintilla(ScintillaObject *sci, LspPosition lsp_pos)
{
	gint line_start_pos = sci_get_position_from_line(sci, lsp_pos.line);
	return SSM(sci, SCI_POSITIONRELATIVECODEUNITS, line_start_pos, lsp_pos.character);
}


gchar *lsp_utils_get_lsp_lang_name(GeanyDocument *doc)
{
	GString *s;
	const gchar *name;

	if (!doc || !doc->file_type)
		return NULL;

	name = doc->file_type->name;

	s = g_string_new(name);
	g_string_ascii_down(s);

	if (g_strcmp0(s->str, "none") == 0)
		name = "plaintext";
	else if (g_strcmp0(s->str, "batch") == 0)
		name = "bat";
	else if (g_strcmp0(s->str, "c++") == 0)
		name = "cpp";
	else if (g_strcmp0(s->str, "c#") == 0)
		name = "csharp";
	else if (g_strcmp0(s->str, "conf") == 0)
		name = "ini";
	else if (g_strcmp0(s->str, "cython") == 0)
		name = "python";
	else if (g_strcmp0(s->str, "f77") == 0)
		name = "fortran";
	else if (g_strcmp0(s->str, "freebasic") == 0)
		name = "vb";  // visual basic
	else if (g_strcmp0(s->str, "make") == 0)
		name = "makefile";
	else if (g_strcmp0(s->str, "matlab/octave") == 0)
		name = "matlab";
	else if (g_strcmp0(s->str, "sh") == 0)
		name = "shellscript";

	return g_string_free(s, FALSE);
}


gchar *lsp_utils_get_locale()
{
	gchar *locale = setlocale(LC_CTYPE, NULL);
	if (locale && strlen(locale) >= 2)
	{
		locale = g_strdup(locale);
		if (locale[2] == '_')
			locale[2] = '\0';
		else if (locale[3] == '_')
			locale[3] = '\0';
	}
	else {
		locale = g_strdup("en");
	}
	return locale;
}


gchar *lsp_utils_json_pretty_print(GVariant *variant)
{
	JsonNode *node;
	gchar *ret;

	if (!variant)
		return g_strdup("");

	node = json_gvariant_serialize(variant);
	if (node)
	{
		ret = json_to_string(node, TRUE);
		json_node_unref(node);
	}
	else
		ret = g_strdup("");
	return ret;
}


gchar *lsp_utils_get_doc_uri(GeanyDocument *doc)
{
	gchar *fname;

	g_return_val_if_fail(doc->real_path, NULL);

	fname = g_filename_to_uri(doc->real_path, NULL, NULL);

	g_return_val_if_fail(fname, NULL);

	return fname;
}


gchar *lsp_utils_get_real_path_from_uri_locale(const gchar *uri)
{
	gchar *fname;

	g_return_val_if_fail(uri, NULL);

	fname = g_filename_from_uri(uri, NULL, NULL);

	g_return_val_if_fail(fname, NULL);

	SETPTR(fname, utils_get_real_path(fname));

	g_return_val_if_fail(fname, NULL);

	return fname;
}


gchar *lsp_utils_get_real_path_from_uri_utf8(const gchar *uri)
{
	gchar *locale_fname = lsp_utils_get_real_path_from_uri_locale(uri);

	if (!locale_fname)
		return NULL;

	SETPTR(locale_fname, utils_get_utf8_from_locale(locale_fname));
	return locale_fname;
}


/* utf8 */
gchar *lsp_utils_get_project_base_path(void)
{
	GeanyProject *project = geany_data->app->project;
	if (project && !EMPTY(project->base_path))
	{
		if (g_path_is_absolute(project->base_path))
			return g_strdup(project->base_path);
		else
		{	/* build base_path out of project file name's dir and base_path */
			gchar *path;
			gchar *dir = g_path_get_dirname(project->file_name);

			if (utils_str_equal(project->base_path, "./"))
				return dir;

			path = g_build_filename(dir, project->base_path, NULL);
			g_free(dir);
			return path;
		}
	}
	return NULL;
}


/* locale */
const gchar *lsp_utils_get_global_config_filename(void)
{
	static gchar *filename = NULL;

	if (filename)
		return filename;

	return g_build_filename(geany_data->app->datadir, "lsp", "lsp.conf", NULL);
}


/* locale */
const gchar *lsp_utils_get_user_config_filename(void)
{
	static gchar *filename = NULL;
	gchar *dirname;

	if (filename)
		return filename;

	dirname = g_build_filename(geany_data->app->configdir, "plugins", PLUGIN, NULL);
	filename = g_build_filename(dirname, PLUGIN".conf", NULL);

	if (!g_file_test(filename, G_FILE_TEST_EXISTS))
	{
		const gchar *global_config = lsp_utils_get_global_config_filename();
		gchar *cont = NULL;

		utils_mkdir(dirname, TRUE);
		msgwin_status_add("User LSP config filename %s does not exist, creating", filename);
		if (!g_file_get_contents(global_config, &cont, NULL, NULL))
			msgwin_status_add("Cannot read global LSP config filename %s", global_config);
		if (!g_file_set_contents(filename, cont ? cont : "", -1, NULL))
			msgwin_status_add("Cannot write user LSP config filename %s", filename);
		g_free(cont);
	}

	g_free(dirname);
	return filename;
}


/* locale */
const gchar *lsp_utils_get_project_config_filename(void)
{
	if (project_configuration_type == ProjectConfigurationType && project_configuration_file)
	{
		if (g_file_test(project_configuration_file, G_FILE_TEST_EXISTS))
			return project_configuration_file;
	}

	return NULL;
}


/* locale */
const gchar *lsp_utils_get_config_filename(void)
{
	const gchar *project_fname = lsp_utils_get_project_config_filename();

	return project_fname ? project_fname : lsp_utils_get_user_config_filename();
}


gboolean lsp_utils_is_lsp_disabled_for_project(void)
{
	return geany->app->project && project_configuration_type == DisableConfigurationType;
}


LspPosition lsp_utils_parse_pos(GVariant *variant)
{
	LspPosition lsp_pos;

	JSONRPC_MESSAGE_PARSE(variant,
		"character", JSONRPC_MESSAGE_GET_INT64(&lsp_pos.character),
		"line", JSONRPC_MESSAGE_GET_INT64(&lsp_pos.line));

	return lsp_pos;
}


LspRange lsp_utils_parse_range(GVariant *variant)
{
	LspRange range;

	JSONRPC_MESSAGE_PARSE(variant,
		"start", "{",
			"character", JSONRPC_MESSAGE_GET_INT64(&range.start.character),
			"line", JSONRPC_MESSAGE_GET_INT64(&range.start.line),
		"}",
		"end", "{",
			"character", JSONRPC_MESSAGE_GET_INT64(&range.end.character),
			"line", JSONRPC_MESSAGE_GET_INT64(&range.end.line),
		"}");

	return range;
}


void lsp_utils_free_lsp_text_edit(LspTextEdit *e)
{
	if (!e)
		return;
	g_free(e->new_text);
	g_free(e);
}


LspTextEdit *lsp_utils_parse_text_edit(GVariant *variant)
{
	GVariant *range = NULL;
	const char *text = NULL;
	LspTextEdit *ret = NULL;

	gboolean success = JSONRPC_MESSAGE_PARSE(variant,
		"newText", JSONRPC_MESSAGE_GET_STRING(&text),
		"range", JSONRPC_MESSAGE_GET_VARIANT(&range));

	if (success)
	{
		ret = g_new0(LspTextEdit, 1);
		ret->new_text = g_strdup(text);
		ret->range = lsp_utils_parse_range(range);
	}

	return ret;
}


GPtrArray *lsp_utils_parse_text_edits(GVariantIter *iter)
{
	GPtrArray *ret = NULL;
	GVariant *val = NULL;

	if (!iter)
		return NULL;

	ret = g_ptr_array_new_full(1, (GDestroyNotify)lsp_utils_free_lsp_text_edit);
	while (g_variant_iter_loop(iter, "v", &val))
	{
		LspTextEdit *edit = lsp_utils_parse_text_edit(val);
		if (edit)
			g_ptr_array_add(ret, edit);
	}

	return ret;
}


void lsp_utils_apply_text_edit(ScintillaObject *sci, LspTextEdit *e, gboolean update_pos)
{
	gint start_pos;
	gint end_pos;

	if (!e)
		return;

	start_pos = lsp_utils_lsp_pos_to_scintilla(sci, e->range.start);
	end_pos = lsp_utils_lsp_pos_to_scintilla(sci, e->range.end);

	SSM(sci, SCI_DELETERANGE, start_pos, end_pos - start_pos);
	sci_insert_text(sci, start_pos, e->new_text);
	if (update_pos)
		sci_set_current_position(sci, start_pos + strlen(e->new_text), TRUE);
}


static gint sort_edits(gconstpointer a, gconstpointer b)
{
	LspTextEdit *e1 = *((LspTextEdit **)a);
	LspTextEdit *e2 = *((LspTextEdit **)b);
	gint line_delta = e2->range.start.line - e1->range.start.line;
	gint char_delta = e2->range.start.character - e1->range.start.character;

	if (line_delta != 0)
		return line_delta;

	return char_delta;
}


void lsp_utils_apply_text_edits(ScintillaObject *sci, LspTextEdit *edit, GPtrArray *edits)
{
	GPtrArray *arr;
	gint i;

	if (!edit && !edits)
		return;

	arr = g_ptr_array_new_full(edits ? edits->len : 1, NULL);
	if (edit)
		g_ptr_array_add(arr, edit);
	if (edits)
	{
		for (i = 0; i < edits->len; i++)
			g_ptr_array_add(arr, edits->pdata[i]);
	}

	// We get the text edit ranges against the original document. We need to sort
	// the edits by position in reverse order so we don't affect positions by applying
	// earlier edits (edits are guaranteed to be non-overlapping by the LSP specs)
	g_ptr_array_sort(arr, sort_edits);

	for (i = 0; i < arr->len; i++)
	{
		LspTextEdit *e = arr->pdata[i];
		lsp_utils_apply_text_edit(sci, e, e == edit);
	}

	g_ptr_array_free(arr, TRUE);
}


void lsp_utils_free_lsp_location(LspLocation *e)
{
	if (!e)
		return;
	g_free(e->uri);
	g_free(e);
}


LspLocation *lsp_utils_parse_location(GVariant *variant)
{
	LspLocation *ret = NULL;
	const gchar *uri;
	GVariant *range;

	gboolean success = JSONRPC_MESSAGE_PARSE(variant,
		"uri", JSONRPC_MESSAGE_GET_STRING(&uri),
		"range", JSONRPC_MESSAGE_GET_VARIANT(&range));

	if (success)
	{
		ret = g_new0(LspLocation, 1);
		ret->uri = g_strdup(uri);
		ret->range = lsp_utils_parse_range(range);

		g_variant_unref(range);
	}

	return ret;
}


GPtrArray *lsp_utils_parse_locations(GVariantIter *iter)
{
	GPtrArray *ret = NULL;
	GVariant *val = NULL;

	if (!iter)
		return NULL;

	ret = g_ptr_array_new_full(1, (GDestroyNotify)lsp_utils_free_lsp_location);
	while (g_variant_iter_loop(iter, "v", &val))
	{
		LspLocation *loc = lsp_utils_parse_location(val);
		if (loc)
			g_ptr_array_add(ret, loc);
	}

	return ret;
}


/* Stolen from Geany */
/* Wraps a string in place, replacing a space with a newline character.
 * wrapstart is the minimum position to start wrapping or -1 for default */
gboolean lsp_utils_wrap_string(gchar *string, gint wrapstart)
{
	gchar *pos, *linestart;
	gboolean ret = FALSE;

	if (wrapstart < 0)
		wrapstart = 80;

	for (pos = linestart = string; *pos != '\0'; pos++)
	{
		if (pos - linestart >= wrapstart && *pos == ' ')
		{
			*pos = '\n';
			linestart = pos;
			ret = TRUE;
		}
	}
	return ret;
}


GVariant *lsp_utils_parse_json_file(const gchar *fname)
{
	JsonNode *json_node = json_from_string("{}", NULL);
	GVariant *variant = json_gvariant_deserialize(json_node, NULL, NULL);
	gchar *file_contents;

	json_node_free(json_node);

	if (!fname)
		return variant;

	if (!g_file_get_contents(fname, &file_contents, NULL, NULL))
		return variant;

	json_node = json_from_string(file_contents, NULL);

	if (json_node)
	{
		g_variant_unref(variant);
		variant = json_gvariant_deserialize(json_node, NULL, NULL);
	}

	g_free(file_contents);

	return variant;
}
