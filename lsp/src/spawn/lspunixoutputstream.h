/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2006-2007 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

// stolen from glib (so it can be used under windows too), removed pollable
// and descriptor based interface implementations

#ifndef __LSP_UNIX_OUTPUT_STREAM_H__
#define __LSP_UNIX_OUTPUT_STREAM_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define LSP_TYPE_UNIX_OUTPUT_STREAM         (lsp_unix_output_stream_get_type ())
#define LSP_UNIX_OUTPUT_STREAM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), LSP_TYPE_UNIX_OUTPUT_STREAM, LspUnixOutputStream))
#define LSP_UNIX_OUTPUT_STREAM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), LSP_TYPE_UNIX_OUTPUT_STREAM, LspUnixOutputStreamClass))
#define LSP_IS_UNIX_OUTPUT_STREAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), LSP_TYPE_UNIX_OUTPUT_STREAM))
#define LSP_IS_UNIX_OUTPUT_STREAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), LSP_TYPE_UNIX_OUTPUT_STREAM))
#define LSP_UNIX_OUTPUT_STREAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), LSP_TYPE_UNIX_OUTPUT_STREAM, LspUnixOutputStreamClass))

/**
 * GUnixOutputStream:
 *
 * Implements #GOutputStream for outputting to selectable unix file descriptors
 **/
typedef struct _LspUnixOutputStream         LspUnixOutputStream;
typedef struct _LspUnixOutputStreamClass    LspUnixOutputStreamClass;
typedef struct _LspUnixOutputStreamPrivate  LspUnixOutputStreamPrivate;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(LspUnixOutputStream, g_object_unref)

struct _LspUnixOutputStream
{
  GOutputStream parent_instance;

  /*< private >*/
  LspUnixOutputStreamPrivate *priv;
};

struct _LspUnixOutputStreamClass
{
  GOutputStreamClass parent_class;

  /*< private >*/
  /* Padding for future expansion */
  void (*_g_reserved1) (void);
  void (*_g_reserved2) (void);
  void (*_g_reserved3) (void);
  void (*_g_reserved4) (void);
  void (*_g_reserved5) (void);
};

GType           lsp_unix_output_stream_get_type     (void) G_GNUC_CONST;

GOutputStream * lsp_unix_output_stream_new          (gint     fd,
                                                   gboolean close_fd);
void            lsp_unix_output_stream_set_close_fd (LspUnixOutputStream *stream,
                                                   gboolean           close_fd);
gboolean        lsp_unix_output_stream_get_close_fd (LspUnixOutputStream *stream);
gint            lsp_unix_output_stream_get_fd       (LspUnixOutputStream *stream);
G_END_DECLS

#endif /* __G_UNIX_OUTPUT_STREAM_H__ */
