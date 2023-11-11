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

#include "lsp/lsp-tm-tag.h"

#include <glib.h>

/* all here is copied from Geany - not part of its public API */

#define TAG_NEW(T)	((T) = g_slice_new0(TMTag))
#define TAG_FREE(T)	g_slice_free(TMTag, (T))


TMTag *lsp_tm_tag_new(void)
{
	TMTag *tag;

	TAG_NEW(tag);
	tag->refcount = 1;

	return tag;
}

/*
 Destroys a TMTag structure, i.e. frees all elements except the tag itself.
 @param tag The TMTag structure to destroy
 @see tm_tag_free()
*/
static void tm_tag_destroy(TMTag *tag)
{
	g_free(tag->name);
	g_free(tag->arglist);
	g_free(tag->scope);
	g_free(tag->inheritance);
	g_free(tag->var_type);
}


void lsp_tm_tag_unref(TMTag *tag)
{
	/* be NULL-proof because tm_tag_free() was NULL-proof and we indent to be a
	 * drop-in replacment of it */
	if (NULL != tag && g_atomic_int_dec_and_test(&tag->refcount))
	{
		tm_tag_destroy(tag);
		TAG_FREE(tag);
	}
}
