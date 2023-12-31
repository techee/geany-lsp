# Adapted from Pidgin's plugins/Makefile.am, thanks

EXTRA_DIST = \
	demoproxytest.px

plugindir = $(libdir)/geany

plugins_includedir = $(includedir)/geany
plugins_include_HEADERS = \
	geanyfunctions.h \
	geanyplugin.h

demoplugin_la_LDFLAGS    = -module -avoid-version -no-undefined
demoproxy_la_LDFLAGS     = -module -avoid-version -no-undefined
classbuilder_la_LDFLAGS  = -module -avoid-version -no-undefined
htmlchars_la_LDFLAGS     = -module -avoid-version -no-undefined
export_la_LDFLAGS        = -module -avoid-version -no-undefined
saveactions_la_LDFLAGS   = -module -avoid-version -no-undefined
filebrowser_la_LDFLAGS   = -module -avoid-version -no-undefined
splitwindow_la_LDFLAGS   = -module -avoid-version -no-undefined
lsp_la_LDFLAGS           = -module -avoid-version -no-undefined

if PLUGINS
# Plugins to be installed
plugin_LTLIBRARIES = \
	classbuilder.la \
	htmlchars.la \
	export.la \
	saveactions.la \
	filebrowser.la \
	splitwindow.la \
	lsp.la

# Plugins not to be installed
noinst_LTLIBRARIES = \
	demoplugin.la  \
	demoproxy.la

demoplugin_la_SOURCES    = demoplugin.c
demoproxy_la_SOURCES     = demoproxy.c
classbuilder_la_SOURCES  = classbuilder.c
htmlchars_la_SOURCES     = htmlchars.c
export_la_SOURCES        = export.c
saveactions_la_SOURCES   = saveactions.c
filebrowser_la_SOURCES   = filebrowser.c
splitwindow_la_SOURCES   = splitwindow.c
lsp_la_SOURCES           = \
	lsp/json-glib/json-array.c \
	lsp/json-glib/json-builder.c \
	lsp/json-glib/json-debug.c \
	lsp/json-glib/json-gboxed.c \
	lsp/json-glib/json-generator.c \
	lsp/json-glib/json-gobject.c \
	lsp/json-glib/json-gvariant.c \
	lsp/json-glib/json-node.c \
	lsp/json-glib/json-object.c \
	lsp/json-glib/json-parser.c \
	lsp/json-glib/json-path.c \
	lsp/json-glib/json-reader.c \
	lsp/json-glib/json-scanner.c \
	lsp/json-glib/json-serializable.c \
	lsp/json-glib/json-utils.c \
	lsp/json-glib/json-value.c \
	lsp/jsonrpc-glib/jsonrpc-client.c \
	lsp/jsonrpc-glib/jsonrpc-input-stream.c \
	lsp/jsonrpc-glib/jsonrpc-message.c \
	lsp/jsonrpc-glib/jsonrpc-output-stream.c \
	lsp/jsonrpc-glib/jsonrpc-server.c \
	lsp/jsonrpc-glib/jsonrpc-marshalers.c \
	lsp/lsp-main.c \
	lsp/lsp-server.c \
	lsp/lsp-sync.c \
	lsp/lsp-utils.c \
	lsp/lsp-rpc.c \
	lsp/lsp-diagnostics.c \
	lsp/lsp-hover.c \
	lsp/lsp-signature.c \
	lsp/lsp-log.c \
	lsp/lsp-goto.c \
	lsp/lsp-symbols.c \
	lsp/lsp-symbol-kinds.c \
	lsp/lsp-semtokens.c \
	lsp/lsp-progress.c \
	lsp/lsp-goto-panel.c \
	lsp/lsp-goto-anywhere.c \
	lsp/lsp-tm-tag.c \
	lsp/lsp-format.c \
	lsp/lsp-highlight.c \
	lsp/lsp-rename.c \
	lsp/lsp-command.c \
	lsp/lsp-code-lens.c \
	lsp/lsp-autocomplete.c

lspconfdir = $(pkgdatadir)/lsp
dist_lspconf_DATA = lsp/data/lsp.conf

demoplugin_la_CFLAGS    = -DG_LOG_DOMAIN=\""Demoplugin"\"
demoproxy_la_CFLAGS     = -DG_LOG_DOMAIN=\""Demoproxy"\"
classbuilder_la_CFLAGS  = -DG_LOG_DOMAIN=\""Classbuilder"\"
htmlchars_la_CFLAGS     = -DG_LOG_DOMAIN=\""HTMLChars"\"
export_la_CFLAGS        = -DG_LOG_DOMAIN=\""Export"\"
saveactions_la_CFLAGS   = -DG_LOG_DOMAIN=\""SaveActions"\"
filebrowser_la_CFLAGS   = -DG_LOG_DOMAIN=\""FileBrowser"\"
splitwindow_la_CFLAGS   = -DG_LOG_DOMAIN=\""SplitWindow"\"
lsp_la_CFLAGS           = -DG_LOG_DOMAIN=\""LSP"\" -DJSON_COMPILATION -DJSONRPC_GLIB_COMPILATION

demoplugin_la_LIBADD    = $(top_builddir)/src/libgeany.la $(GTK_LIBS)
demoproxy_la_LIBADD     = $(top_builddir)/src/libgeany.la $(GTK_LIBS)
classbuilder_la_LIBADD  = $(top_builddir)/src/libgeany.la $(GTK_LIBS)
htmlchars_la_LIBADD     = $(top_builddir)/src/libgeany.la $(GTK_LIBS)
export_la_LIBADD        = $(top_builddir)/src/libgeany.la $(GTK_LIBS) -lm
saveactions_la_LIBADD   = $(top_builddir)/src/libgeany.la $(GTK_LIBS)
filebrowser_la_LIBADD   = $(top_builddir)/src/libgeany.la $(GTK_LIBS)
splitwindow_la_LIBADD   = $(top_builddir)/src/libgeany.la $(GTK_LIBS)
lsp_la_LIBADD           = $(top_builddir)/src/libgeany.la $(GTK_LIBS)

endif # PLUGINS

if MINGW
AM_CPPFLAGS = -DGEANY_DATADIR=\"data\" -DGEANY_LOCALEDIR=\"\"
else
AM_CPPFLAGS = -DGEANY_DATADIR=\"$(datadir)\" -DGEANY_LOCALEDIR=\""$(localedir)"\"
endif

AM_CPPFLAGS += \
	-DGTK \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/tagmanager \
	-I$(top_srcdir)/scintilla/lexilla/include \
	-I$(top_srcdir)/scintilla/include \
	-I$(top_srcdir)/plugins/lsp/json-glib \
	-I$(top_srcdir)/plugins/lsp/jsonrpc-glib \
	-I$(top_srcdir)/plugins/lsp \
	$(GTK_CFLAGS) \
	$(PLUGIN_CFLAGS)

#
# This part allows people to build their own plugins in here.
# Yes, it's a mess.
#
custom_plugins_cppflags = -DHAVE_CONFIG_H -I$(top_srcdir)
custom_plugins_ldflags = -module -avoid-version
if MINGW
SUFFIXES = .c .dll
.c.dll:
	$(CC) $(AM_CPPFLAGS) $(custom_plugins_cppflags) $(CPPFLAGS) $(CFLAGS) -o $@.o -c $<
	$(CC) -shared $@.o $(GTK_LIBS) $(custom_plugins_ldflags) $(LIBS) $(PLUGIN_LIBS) $(LDFLAGS) -o $@
else
SUFFIXES = .c .so
.c.so:
	$(LIBTOOL) --mode=compile $(CC) $(AM_CPPFLAGS) $(custom_plugins_cppflags) $(CPPFLAGS) $(CFLAGS) -c $< -o tmp$@.lo
	$(LIBTOOL) --mode=link    $(CC) $(CFLAGS) -o libtmp$@.la -rpath $(plugindir) tmp$@.lo $(LIBS) $(LDFLAGS) $(custom_plugins_ldflags) $(PLUGIN_LIBS)
	@rm -f tmp$@.lo tmp$@.o libtmp$@.la
	@cp .libs/libtmp$@.so* $@
	@rm -f .libs/libtmp$@.*
endif
