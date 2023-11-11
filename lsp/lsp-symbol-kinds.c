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

#include "lsp/lsp-symbol-kinds.h"

#include <gtk/gtk.h>


static struct
{
	const gchar *icon_name;
	GdkPixbuf *pixbuf;
}
/* keep in sync with Geany */
geany_icons[TM_N_ICONS] = {
	[TM_ICON_CLASS]		= { "classviewer-class", NULL },
	[TM_ICON_MACRO]		= { "classviewer-macro", NULL },
	[TM_ICON_MEMBER]	= { "classviewer-member", NULL },
	[TM_ICON_METHOD]	= { "classviewer-method", NULL },
	[TM_ICON_NAMESPACE]	= { "classviewer-namespace", NULL },
	[TM_ICON_OTHER]		= { "classviewer-other", NULL },
	[TM_ICON_STRUCT]	= { "classviewer-struct", NULL },
	[TM_ICON_VAR]		= { "classviewer-var", NULL },
};


static LspGeanyIcon lsp_symbol_icons[LSP_SYMBOL_KIND_NUM] = {
	TM_ICON_NAMESPACE,  /* LspKindFile */
	TM_ICON_NAMESPACE,  /* LspKindModule */
	TM_ICON_NAMESPACE,  /* LspKindNamespace */
	TM_ICON_NAMESPACE,  /* LspKindPackage */
	TM_ICON_CLASS,      /* LspKindClass */
	TM_ICON_METHOD,     /* LspKindMethod */
	TM_ICON_MEMBER,     /* LspKindProperty */
	TM_ICON_MEMBER,     /* LspKindField */
	TM_ICON_METHOD,     /* LspKindConstructor */
	TM_ICON_STRUCT,     /* LspKindEnum */
	TM_ICON_CLASS,      /* LspKindInterface */
	TM_ICON_METHOD,     /* LspKindFunction */
	TM_ICON_VAR,        /* LspKindVariable */
	TM_ICON_MACRO,      /* LspKindConstant */
	TM_ICON_OTHER,      /* LspKindString */
	TM_ICON_OTHER,      /* LspKindNumber */
	TM_ICON_OTHER,      /* LspKindBoolean */
	TM_ICON_OTHER,      /* LspKindArray */
	TM_ICON_OTHER,      /* LspKindObject */
	TM_ICON_OTHER,      /* LspKindKey */
	TM_ICON_OTHER,      /* LspKindNull */
	TM_ICON_MEMBER,     /* LspKindEnumMember */
	TM_ICON_STRUCT,     /* LspKindStruct */
	TM_ICON_OTHER,      /* LspKindEvent */
	TM_ICON_METHOD,     /* LspKindOperator */
	TM_ICON_OTHER       /* LspKindTypeParameter */
};


static LspGeanyIcon lsp_completion_icons[LSP_COMPLETION_KIND_NUM] = {
	TM_ICON_MACRO,      // LspKindText - also used for macros by clangd
	TM_ICON_METHOD,     // LspKindMethod
	TM_ICON_METHOD,     // LspKindFunction
	TM_ICON_METHOD,     // LspKindConstructor
	TM_ICON_MEMBER,     // LspKindField
	TM_ICON_VAR,        // LspKindVariable
	TM_ICON_CLASS,      // LspKindClass
	TM_ICON_CLASS,      // LspKindInterface
	TM_ICON_NAMESPACE,  // LspKindModule
	TM_ICON_MEMBER,     // LspKindProperty
	TM_ICON_NAMESPACE,  // LspKindUnit
	TM_ICON_MACRO,      // LspKindValue
	TM_ICON_STRUCT,     // LspKindEnum
	TM_ICON_NONE,       // LspKindKeyword
	TM_ICON_NONE,       // LspKindSnippet
	TM_ICON_OTHER,      // LspKindColor
	TM_ICON_OTHER,      // LspKindFile
	TM_ICON_OTHER,      // LspKindReference
	TM_ICON_OTHER,      // LspKindFolder
	TM_ICON_MEMBER,     // LspKindEnumMember
	TM_ICON_MACRO,      // LspKindConstant
	TM_ICON_STRUCT,     // LspKindStruct
	TM_ICON_OTHER,      // LspKindEvent
	TM_ICON_OTHER,      // LspKindOperator
	TM_ICON_OTHER       // LspKindTypeParameter
};


LspGeanyIcon lsp_symbol_kinds_get_completion_icon(LspCompletionKind kind)
{
	if (kind < 1 || kind > LSP_COMPLETION_KIND_NUM)
		return TM_ICON_OTHER;

	return lsp_completion_icons[kind - 1];
}


LspGeanyIcon lsp_symbol_kinds_get_symbol_icon(LspSymbolKind kind)
{
	if (kind < 1 || kind > LSP_SYMBOL_KIND_NUM)
		return TM_ICON_OTHER;

	return lsp_symbol_icons[kind - 1];
}


static GdkPixbuf *get_tag_icon(const gchar *icon_name)
{
	static GtkIconTheme *icon_theme = NULL;
	static gint x = -1;

	if (G_UNLIKELY(x < 0))
	{
		gint dummy;
		icon_theme = gtk_icon_theme_get_default();
		gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &x, &dummy);
	}
	return gtk_icon_theme_load_icon(icon_theme, icon_name, x, 0, NULL);
}


GdkPixbuf *lsp_symbol_kinds_get_icon_pixbuf(LspGeanyIcon icon)
{
	if (!geany_icons[TM_ICON_CLASS].pixbuf)
	{
		guint i;
		for (i = 0; i < G_N_ELEMENTS(geany_icons); i++)
			geany_icons[i].pixbuf = get_tag_icon(geany_icons[i].icon_name);
	}

	if (icon < TM_N_ICONS)
		return geany_icons[icon].pixbuf;

	return NULL;
}


LspSymbolKind lsp_symbol_kinds_tm_to_lsp(TMTagType type)
{
	switch (type)
	{
		case tm_tag_undef_t:
			return LspSymbolKindVariable;
		case tm_tag_class_t:
			return LspSymbolKindClass;
		case tm_tag_enum_t:
			return LspSymbolKindEnum;
		case tm_tag_enumerator_t:
			return LspSymbolKindEnumMember;
		case tm_tag_field_t:
			return LspSymbolKindField;
		case tm_tag_function_t:
			return LspSymbolKindFunction;
		case tm_tag_interface_t:
			return LspSymbolKindInterface;
		case tm_tag_member_t:
			return LspSymbolKindProperty;
		case tm_tag_method_t:
			return LspSymbolKindMethod;
		case tm_tag_namespace_t:
			return LspSymbolKindNamespace;
		case tm_tag_package_t:
			return LspSymbolKindPackage;
		case tm_tag_prototype_t:
			return LspSymbolKindFunction;
		case tm_tag_struct_t:
			return LspSymbolKindStruct;
		case tm_tag_typedef_t:
			return LspSymbolKindStruct;
		case tm_tag_union_t:
			return LspSymbolKindStruct;
		case tm_tag_variable_t:
			return LspSymbolKindVariable;
		case tm_tag_externvar_t:
			return LspSymbolKindVariable;
		case tm_tag_macro_t:
			return LspSymbolKindConstant;
		case tm_tag_macro_with_arg_t:
			return LspSymbolKindFunction;
		case tm_tag_local_var_t:
			return LspSymbolKindVariable;
		case tm_tag_other_t:
			return LspSymbolKindVariable;
		case tm_tag_include_t:
			return LspSymbolKindPackage;
		default:
			break;
	}

	return LspSymbolKindVariable;
}
