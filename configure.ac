AC_PREREQ(2.53)
AC_INIT(gupnp, 0.1, http://projects.o-hand.com/gupnp)
AM_INIT_AUTOMAKE()
AC_CONFIG_SRCDIR(libgupnp/gupnp-context.c)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AC_STDC_HEADERS
AC_PROG_LIBTOOL

PKG_CHECK_MODULES(LIBGUPNP, gobject-2.0 >= 2.9.1 gssdp-1.0 libsoup-2.2)

# glib-genmarshal
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

# Debugging
AC_ARG_ENABLE(debug,
	[  --enable-debug          enable debugging],,
        enable_debug=no)
if test "x$enable_debug" = "xyes"; then
        CFLAGS="$CFLAGS -g -Wall -Werror"
fi

# Gettext
GETTEXT_PACKAGE=libgupnp
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"], 
  [Define the gettext package to be used])

ALL_LINGUAS=""
AM_GLIB_GNU_GETTEXT
AM_GLIB_DEFINE_LOCALEDIR([LOCALEDIR])

GTK_DOC_CHECK([1.0])

AC_OUTPUT([
Makefile
libgupnp/Makefile
tests/Makefile
po/Makefile.in
po/Makefile
doc/Makefile
gupnp-1.0.pc
])
