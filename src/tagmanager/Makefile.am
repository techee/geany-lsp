AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir)/ctags/main \
	-I$(top_srcdir)/ctags/libreadtags \
	-I$(top_srcdir)/ctags/dsl \
	-DGEANY_PRIVATE \
	-DG_LOG_DOMAIN=\"Tagmanager\"

if USE_BUNDLED_REGEX
AM_CPPFLAGS += -I$(top_srcdir)/ctags/gnu_regex
endif

AM_CFLAGS = \
	$(GTK_CFLAGS) \
	@LIBGEANY_CFLAGS@

noinst_LTLIBRARIES = libtagmanager.la

tagmanager_includedir = $(includedir)/geany/tagmanager
tagmanager_include_HEADERS = \
	tm_source_file.h \
	tm_tag.h \
	tm_workspace.h \
	tm_parser.h


libtagmanager_la_SOURCES = \
	tm_ctags.h \
	tm_ctags.c \
	tm_parser.h \
	tm_parser.c \
	tm_parsers.h \
	tm_source_file.h \
	tm_source_file.c \
	tm_tag.h \
	tm_tag.c \
	tm_workspace.h \
	tm_workspace.c

libtagmanager_la_LIBADD = $(top_builddir)/ctags/libctags.la $(GTK_LIBS)
