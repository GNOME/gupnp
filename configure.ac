AC_PREREQ(2.53)
AC_INIT(gupnp, 0.13.2, http://www.gupnp.org/)
AM_INIT_AUTOMAKE()
AC_CONFIG_SRCDIR(libgupnp/gupnp.h)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_ISC_POSIX
AC_PROG_CC
AC_STDC_HEADERS
AC_PROG_LIBTOOL
AC_FUNC_MMAP
AC_SYS_LARGEFILE

PKG_CHECK_MODULES(LIBGUPNP, glib-2.0 >= 2.18 \
                            gobject-2.0 >= 2.18 \
                            gmodule-2.0 \
                            gssdp-1.0 >= 0.7 \
                            libsoup-2.4 >= 2.4.1 \
                            libxml-2.0 \
                            uuid)

PKG_CHECK_MODULES(GTHREAD, gthread-2.0)

AC_ARG_WITH([context_manager],
            AS_HELP_STRING(
                        [--with-context-manager=@<:@network-manager/unix@:>@],
                        [Context Manager backend to use]),,
            [with_context_manager="unix"])

AC_MSG_CHECKING([Context Manager backend to use])
AC_MSG_RESULT([${with_context_manager}])

AM_CONDITIONAL([USE_NETWORK_MANAGER],
               [test "x$with_context_manager" = "xnetwork-manager"])

USE_CONIC=no
if test "x$with_context_manager" = "xunix"; then
        dnl Use libconic if it's available
        PKG_CHECK_MODULES(LIBCONIC, conic >= 0.13,
                          [ USE_CONIC=yes
                            AC_SUBST(LIBCONIC_CFLAGS)
                            AC_SUBST(LIBCONIC_LIBS)
                          ],
                          [ USE_CONIC=no
                            AC_MSG_WARN([conic 0.13 or greater not found.])
                          ])
fi

AM_CONDITIONAL(USE_CONIC, test x$USE_CONIC = xyes)
AC_SUBST(USE_CONIC)

# We need dbus-glib if either NetworkManager or libconic is used
if test "x$with_context_manager" = "xnetwork-manager" -o \
        "x$with_context_manager" = "xunix" -a \
        "x$USE_CONIC" = "xyes"; then
        PKG_CHECK_MODULES(DBUS_GLIB, dbus-glib-1 >= 0.76)
fi

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

# Gtk-doc
GTK_DOC_CHECK([1.0])

AC_OUTPUT([
Makefile
libgupnp/Makefile
examples/Makefile
tools/Makefile
tests/Makefile
doc/Makefile
doc/version.xml
gupnp-1.0.pc
gupnp-1.0-uninstalled.pc
])
