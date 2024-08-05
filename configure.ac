AC_PREREQ([2.61])
dnl Don't forget when updating version number to do GP_CHECK_GEANY also
AC_INIT([geany-plugins], [2.1])
AM_INIT_AUTOMAKE([1.8 foreign dist-bzip2 tar-ustar subdir-objects])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_CONFIG_SRCDIR([lsp/Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4/cache])

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

dnl i18n
IT_PROG_INTLTOOL([0.35.0])
GP_I18N

dnl common checks
GP_CHECK_GEANY(2.0)
GP_CHECK_GTK_VERSION
GP_CHECK_CPPCHECK
GP_CHECK_CFLAGS
GP_CHECK_LDFLAGS
GP_CHECK_MINGW

dnl plugin checks
GP_CHECK_LSP

AC_CONFIG_FILES([
    Makefile
    po/Makefile.in
])
AC_OUTPUT
