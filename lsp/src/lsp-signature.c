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

#include "lsp-signature.h"
#include "lsp-popup.h"
#include "lsp-utils.h"
#include "lsp-rpc.h"

#include <jsonrpc-glib.h>


typedef struct {
	GeanyDocument *doc;
	gint pos;
} LspSignatureData;


typedef struct {
	gchar *label;
	/* byte offsets of the active parameter within the label, -1 if unknown */
	gint bold_start;
	gint bold_end;
} LspSignatureInfo;


static GPtrArray *signatures = NULL;
static gint displayed_signature = 0;


static void signature_info_free(gpointer data)
{
	LspSignatureInfo *info = data;

	g_free(info->label);
	g_free(info);
}


static void show_signature(GeanyDocument *doc)
{
	LspSignatureInfo *info = signatures->pdata[displayed_signature];

	lsp_popup_show_signature(doc, sci_get_current_position(doc->editor->sci),
		info->label, info->bold_start, info->bold_end);
}


static gboolean is_identifier_char(gchar c)
{
	return g_ascii_isalnum(c) || c == '_';
}


/* find the parameter label substring within the signature label, preferring
 * occurrences not surrounded by identifier characters */
static void find_param_substring(const gchar *label, const gchar *param,
	gint *bold_start, gint *bold_end)
{
	gsize param_len = strlen(param);
	const gchar *fallback = NULL;
	const gchar *p = label;

	if (param_len == 0)
		return;

	while ((p = strstr(p, param)) != NULL)
	{
		if ((p == label || !is_identifier_char(p[-1])) && !is_identifier_char(p[param_len]))
		{
			*bold_start = p - label;
			*bold_end = *bold_start + param_len;
			return;
		}
		if (!fallback)
			fallback = p;
		p++;
	}

	if (fallback)
	{
		*bold_start = fallback - label;
		*bold_end = *bold_start + param_len;
	}
}


/* LSP parameter offsets are in UTF-16 code units within the label */
static gint utf16_offset_to_bytes(const gchar *label, gint64 utf16_offset)
{
	const gchar *p = label;
	gint64 units = 0;

	while (*p && units < utf16_offset)
	{
		units += g_utf8_get_char(p) >= 0x10000 ? 2 : 1;
		p = g_utf8_next_char(p);
	}

	return p - label;
}


static gint64 get_int64_from_variant(GVariant *variant, gint64 dflt)
{
	GVariant *v = variant;
	gint64 res = dflt;

	if (g_variant_is_of_type(v, G_VARIANT_TYPE_VARIANT))
		v = g_variant_get_variant(v);
	else
		g_variant_ref(v);

	if (g_variant_is_of_type(v, G_VARIANT_TYPE_INT64))
		res = g_variant_get_int64(v);
	else if (g_variant_is_of_type(v, G_VARIANT_TYPE_DOUBLE))
		res = (gint64)g_variant_get_double(v);

	g_variant_unref(v);
	return res;
}


/* computes the byte range of the active parameter within the signature label.
 * The parameter label is either a substring of the signature label, or a
 * [start, end) offset pair */
static void get_param_bold_range(GVariant *signature, const gchar *label,
	gint64 active_param, gint *bold_start, gint *bold_end)
{
	GVariantIter *iter = NULL;
	GVariant *param = NULL;
	gint64 index = 0;

	*bold_start = -1;
	*bold_end = -1;

	JSONRPC_MESSAGE_PARSE(signature, "parameters", JSONRPC_MESSAGE_GET_ITER(&iter));
	if (!iter)
		return;

	/* signature-specific value overriding the one from the SignatureHelp */
	g_variant_lookup(signature, "activeParameter", "x", &active_param);

	while (g_variant_iter_loop(iter, "v", &param))
	{
		GVariant *param_label;

		if (index++ != active_param)
			continue;

		param_label = g_variant_lookup_value(param, "label", NULL);
		if (param_label)
		{
			if (g_variant_is_of_type(param_label, G_VARIANT_TYPE_STRING))
			{
				find_param_substring(label,
					g_variant_get_string(param_label, NULL), bold_start, bold_end);
			}
			else if (g_variant_is_of_type(param_label, G_VARIANT_TYPE_ARRAY) &&
				g_variant_n_children(param_label) == 2)
			{
				GVariant *start_v = g_variant_get_child_value(param_label, 0);
				GVariant *end_v = g_variant_get_child_value(param_label, 1);
				gint64 start = get_int64_from_variant(start_v, -1);
				gint64 end = get_int64_from_variant(end_v, -1);

				if (start >= 0 && end > start)
				{
					*bold_start = utf16_offset_to_bytes(label, start);
					*bold_end = utf16_offset_to_bytes(label, end);
				}

				g_variant_unref(start_v);
				g_variant_unref(end_v);
			}

			g_variant_unref(param_label);
		}
	}

	g_variant_iter_free(iter);
}


static void signature_cb(GVariant *return_value, GError *error, gpointer user_data)
{
	if (!error)
	{
		GeanyDocument *current_doc = document_get_current();
		LspSignatureData *data = user_data;

		//printf("%s\n", lsp_utils_json_pretty_print(return_value));

		if (current_doc == data->doc)
		{
			if (!g_variant_is_of_type(return_value, G_VARIANT_TYPE_DICTIONARY) &&
				lsp_signature_showing_calltip(current_doc))
			{
				// null response
				lsp_signature_hide_calltip(current_doc);
			}
			else if (sci_get_current_position(current_doc->editor->sci) < data->pos + 10)
			{
				GVariantIter *iter = NULL;
				gint64 active = 0;
				gint64 active_param = 0;

				JSONRPC_MESSAGE_PARSE(return_value, "signatures", JSONRPC_MESSAGE_GET_ITER(&iter));
				JSONRPC_MESSAGE_PARSE(return_value, "activeSignature", JSONRPC_MESSAGE_GET_INT64(&active));
				JSONRPC_MESSAGE_PARSE(return_value, "activeParameter", JSONRPC_MESSAGE_GET_INT64(&active_param));

				if (signatures)
					g_ptr_array_free(signatures, TRUE);
				signatures = g_ptr_array_new_full(1, signature_info_free);

				if (iter)
				{
					GVariant *member = NULL;

					while (g_variant_iter_loop(iter, "v", &member))
					{
						const gchar *label = NULL;

						JSONRPC_MESSAGE_PARSE(member, "label", JSONRPC_MESSAGE_GET_STRING(&label));

						if (label)
						{
							LspSignatureInfo *info = g_new0(LspSignatureInfo, 1);

							info->label = g_strdup(label);
							get_param_bold_range(member, info->label, active_param,
								&info->bold_start, &info->bold_end);
							g_ptr_array_add(signatures, info);
						}
					}
				}

				displayed_signature = CLAMP(active, 0, (gint64)signatures->len - 1);

				if (signatures->len == 0)
					lsp_signature_hide_calltip(current_doc);
				else
					show_signature(current_doc);

				if (iter)
					g_variant_iter_free(iter);
			}
		}
	}

	g_free(user_data);
}


void lsp_signature_send_request(LspServer *server, GeanyDocument *doc, gboolean force)
{
	GVariant *node;
	gchar *doc_uri;
	LspSignatureData *data;
	ScintillaObject *sci = doc->editor->sci;
	gint pos = sci_get_current_position(sci);
	LspPosition lsp_pos = lsp_utils_scintilla_pos_to_lsp(sci, pos);
	gchar c = (pos > 0 && !force) ? sci_get_char_at(sci, SSM(sci, SCI_POSITIONBEFORE, pos, 0)) : '\0';
	const gchar *trigger_chars = server->signature_trigger_chars;

	if (!force && EMPTY(trigger_chars))
		return;

	if ((c == ')' && strchr(trigger_chars, '(') && !strchr(trigger_chars, ')')) ||
		(c == ']' && strchr(trigger_chars, '[') && !strchr(trigger_chars, ']')) ||
		(c == '>' && strchr(trigger_chars, '<') && !strchr(trigger_chars, '>')) ||
		(c == '}' && strchr(trigger_chars, '{') && !strchr(trigger_chars, '}')))
	{
		lsp_signature_hide_calltip(doc);
		return;
	}

	if (!force && !strchr(trigger_chars, c))
		return;

	doc_uri = lsp_utils_get_doc_uri(doc);

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}",
		"position", "{",
			"line", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.line),
			"character", JSONRPC_MESSAGE_PUT_INT32(lsp_pos.character),
		"}"
	);

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	data = g_new0(LspSignatureData, 1);
	data->doc = doc;
	data->pos = pos;

	lsp_rpc_call(server, "textDocument/signatureHelp", node,
		signature_cb, data);

	g_free(doc_uri);
	g_variant_unref(node);
}


gboolean lsp_signature_showing_calltip(GeanyDocument *doc)
{
	return lsp_popup_showing_signature(doc);
}


void lsp_signature_hide_calltip(GeanyDocument *doc)
{
	if (lsp_popup_showing_signature(doc))
		lsp_popup_hide(doc);

	if (signatures)
	{
		g_ptr_array_free(signatures, TRUE);
		signatures = NULL;
	}
}
