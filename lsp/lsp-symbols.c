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

#include "lsp/lsp-symbols.h"
#include "lsp/lsp-client.h"
#include "lsp/lsp-utils.h"

#if 0
/* keep in sync with icon names in symbols.c */
typedef enum
{
	TM_ICON_CLASS,
	TM_ICON_MACRO,
	TM_ICON_MEMBER,
	TM_ICON_METHOD,
	TM_ICON_NAMESPACE,
	TM_ICON_OTHER,
	TM_ICON_STRUCT,
	TM_ICON_VAR,
	TM_ICON_NONE,
	TM_N_ICONS = TM_ICON_NONE
} LspGeanyIcon;


typedef enum {
	LspKindFile = 1,
	LspKindModule,
	LspKindNamespace,
	LspKindPackage,
	LspKindClass,
	LspKindMethod,
	LspKindProperty,
	LspKindField,
	LspKindConstructor,
	LspKindEnum,
	LspKindInterface,
	LspKindFunction,
	LspKindVariable,
	LspKindConstant,
	LspKindString,
	LspKindNumber,
	LspKindBoolean,
	LspKindArray,
	LspKindObject,
	LspKindKey,
	LspKindNull,
	LspKindEnumMember,
	LspKindStruct,
	LspKindEvent,
	LspKindOperator,
	LspKindTypeParameter,
	LSP_KIND_NUM = LspKindTypeParameter
} LspSymbolKind;  /* enums different than in LspCompletionItemKind */


static LspGeanyIcon kind_icons[LSP_KIND_NUM] = {
	TM_ICON_NAMESPACE,  // LspKindFile
	TM_ICON_NAMESPACE,  // LspKindModule
	TM_ICON_NAMESPACE,  // LspKindNamespace
	TM_ICON_NAMESPACE,  // LspKindPackage
	TM_ICON_CLASS,      // LspKindClass
	TM_ICON_METHOD,     // LspKindMethod
	TM_ICON_MEMBER,     // LspKindProperty
	TM_ICON_MEMBER,     // LspKindField
	TM_ICON_METHOD,     // LspKindConstructor
	TM_ICON_STRUCT,     // LspKindEnum
	TM_ICON_CLASS,      // LspKindInterface
	TM_ICON_METHOD,     // LspKindFunction
	TM_ICON_VAR,        // LspKindVariable
	TM_ICON_MACRO,      // LspKindConstant
	TM_ICON_OTHER,      // LspKindString
	TM_ICON_OTHER,      // LspKindNumber
	TM_ICON_OTHER,      // LspKindBoolean
	TM_ICON_OTHER,      // LspKindArray
	TM_ICON_OTHER,      // LspKindObject
	TM_ICON_OTHER,      // LspKindKey
	TM_ICON_OTHER,      // LspKindNull
	TM_ICON_MEMBER,     // LspKindEnumMember
	TM_ICON_STRUCT,     // LspKindStruct
	TM_ICON_OTHER,      // LspKindEvent
	TM_ICON_OTHER,      // LspKindOperator
	TM_ICON_OTHER       // LspKindTypeParameter
};                         
#endif


static void symbols_cb(GObject *object, GAsyncResult *result, gpointer user_data)
{
	JsonrpcClient *self = (JsonrpcClient *)object;
	GVariant *return_value = NULL;

	if (lsp_client_call_finish(self, result, &return_value))
	{
		//printf("%s\n\n\n", lsp_utils_json_pretty_print(return_value));

		if (return_value)
			g_variant_unref(return_value);
	}

	g_free(user_data);
}


void lsp_symbols_send_request(LspServer *server, GeanyDocument *doc)
{
	GVariant *node;
	gchar *doc_uri = lsp_utils_get_doc_uri(doc);

	node = JSONRPC_MESSAGE_NEW (
		"textDocument", "{",
			"uri", JSONRPC_MESSAGE_PUT_STRING(doc_uri),
		"}"
	);

	//printf("%s\n\n\n", lsp_utils_json_pretty_print(node));

	lsp_client_call_async(server->rpc_client, "textDocument/documentSymbol", node,
		symbols_cb, NULL);

	g_free(doc_uri);
	g_variant_unref(node);
}
