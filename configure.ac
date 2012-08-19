AC_PREREQ([2.64])
AC_INIT([gupnp],
        [0.18.0],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=gupnp&component=gupnp],
        [gupnp],
        [http://www.gupnp.org/])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([libgupnp/gupnp.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([1.11 tar-ustar no-dist-gzip dist-xz])
AM_MAINTAINER_MODE([enable])

AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC
AC_FUNC_MMAP
AC_SYS_LARGEFILE

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT


PKG_CHECK_MODULES(GTest, glib-2.0 >= 2.16, [gtest=yes], [gtest=no])
AM_CONDITIONAL(HAVE_GTEST, [test x$gtest = xyes])

AC_MSG_CHECKING([operating system])
case "$target_os" in
     mingw*|cygwin*)
        AC_MSG_RESULT(Win32)
        UUID=uuid
        ;;
     darwin*)
        AC_MSG_RESULT(Darwin)
        UUID=
        ;;
     *)
        AC_MSG_RESULT($target_os)
        UUID=uuid
        ;;
esac

PKG_CHECK_MODULES(LIBGUPNP, glib-2.0 >= 2.24.0 \
                            gio-2.0 \
                            gmodule-2.0 \
                            gssdp-1.0 >= 0.11.2 \
                            libsoup-2.4 >= 2.28.2 \
                            libxml-2.0 \
                            $UUID)

PKG_CHECK_MODULES(GTHREAD, gthread-2.0)

AC_ARG_WITH([context_manager],
            AS_HELP_STRING(
                        [--with-context-manager=@<:@network-manager/connman/unix/linux@:>@],
                        [Context Manager backend to use]),,
            [with_context_manager="none"])

if test "x$with_context_manager" = "xnone"; then
        case "$target_os" in
        linux*)
                with_context_manager="linux"
                ;;
        *)
                with_context_manager="unix"
                ;;
        esac
fi

AC_CHECK_HEADERS([sys/socket.h linux/rtnetlink.h],
                [ HAVE_NETLINK=yes ],
                [ HAVE_NETLINK=no ],
                [ #ifdef HAVE_SYS_SOCKET_H
                  #include <sys/socket.h>
                  #endif
                ])
AM_CONDITIONAL([HAVE_NETLINK], [test "x$HAVE_NETLINK" = "xyes"])
AC_SUBST(HAVE_NETLINK)

AC_CHECK_HEADERS([linux/wireless.h], [], [],
                [ #ifdef HAVE_SYS_SOCKET_H
                  #include <sys/socket.h>
                  #endif
                ])

AC_MSG_CHECKING([Context Manager backend to use])
AC_MSG_RESULT([${with_context_manager}])

AM_CONDITIONAL([USE_NETWORK_MANAGER],
               [test "x$with_context_manager" = "xnetwork-manager"])
AS_IF([test "x$with_context_manager" = "xnetwork-manager"],
      [PKG_CHECK_MODULES(GIO, gio-2.0 >= 2.26)])

AM_CONDITIONAL([USE_CONNMAN],
               [test "x$with_context_manager" = "xconnman"])

if test "x$with_context_manager" = "xconnman"; then
        PKG_CHECK_MODULES(CONNMAN, glib-2.0 >= 2.28 connman >= 0.80)
fi

USE_NETLINK=no
AS_IF([test "x$with_context_manager" = "xlinux"],
      [AS_IF([test "x$HAVE_NETLINK" = "xno"],
             [AC_MSG_NOTICE([No rtnetlink found, falling back to static context manager])],
             [USE_NETLINK=yes])
      ],[])

AM_CONDITIONAL(USE_NETLINK, test "x$USE_NETLINK" = "xyes")
AC_SUBST(USE_NETLINK)

# glib-genmarshal
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

# Debugging
AC_ARG_ENABLE(debug,
        AS_HELP_STRING([--enable-debug], [enable debugging]),,
        enable_debug=no)
if test "x$enable_debug" = "xyes"; then
        CFLAGS="$CFLAGS -g -Wall -Werror"
fi

GOBJECT_INTROSPECTION_CHECK([0.6.4])

# Gtk-doc
GTK_DOC_CHECK([1.0])

AC_CONFIG_FILES([
Makefile
libgupnp/Makefile
examples/Makefile
tools/Makefile
tests/Makefile
tests/gtest/Makefile
doc/Makefile
doc/version.xml
gupnp-1.0.pc
gupnp-1.0-uninstalled.pc
])
AC_OUTPUT
