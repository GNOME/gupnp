AC_PREREQ([2.64])
AC_INIT([gupnp],
        [1.0.1],
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

AX_REQUIRE_DEFINED([GTK_DOC_CHECK])
AX_REQUIRE_DEFINED([GOBJECT_INTROSPECTION_CHECK])
AX_CHECK_ENABLE_DEBUG([yes])

# Check for programs
AC_PROG_CC
AC_FUNC_MMAP
AC_SYS_LARGEFILE

AX_IS_RELEASE([git-directory])
AX_COMPILER_FLAGS([WARN_CFLAGS])

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT

GUUL_CHECK_UUID([internal])
UUID_LIBS=$GUUL_PKG
AC_SUBST([UUID_LIBS])

AC_MSG_CHECKING([operating system])
os_win32=no
case "$target_os" in
     mingw*|cygwin*)
        os_win32=yes
        AC_MSG_RESULT(Win32)
        ;;
     darwin*)
        AC_MSG_RESULT(Darwin)
        ;;
     *)
        AC_MSG_RESULT($target_os)
        ;;
esac

PKG_CHECK_MODULES(LIBGUPNP, glib-2.0 >= 2.40.0 \
                            gio-2.0 \
                            gmodule-2.0 \
                            gssdp-1.0 >= 0.14.15 \
                            libsoup-2.4 >= 2.48.0 \
                            libxml-2.0)
PKG_CHECK_MODULES(GTHREAD, gthread-2.0)

AM_CONDITIONAL(OS_WIN32, test "x$os_win32" = "xyes")
if test "x$os_win32" = "xno"; then
    AC_ARG_WITH([context_manager],
            AS_HELP_STRING(
                        [--with-context-manager=@<:@network-manager/connman/unix/linux@:>@],
                        [Context Manager backend to use]),,
            [with_context_manager="none"])
else
    with_context_manager="windows"
fi

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

AC_CHECK_HEADERS([ifaddrs.h],
                 [ HAVE_IFADDRS_H=yes ],
                 [ HAVE_IFADDRS_H=no ])
AM_CONDITIONAL([HAVE_IFADDRS_H], [test "x$HAVE_IFADDRS_H" = "xyes"])
AC_SUBST(HAVE_IFADDRS_H)

AC_CHECK_HEADERS([linux/wireless.h], [], [],
                [ #ifdef HAVE_SYS_SOCKET_H
                  #include <sys/socket.h>
                  #endif
                ])

AC_MSG_CHECKING([Context Manager backend to use])
AC_MSG_RESULT([${with_context_manager}])

AM_CONDITIONAL([USE_NETWORK_MANAGER],
               [test "x$with_context_manager" = "xnetwork-manager"])

AM_CONDITIONAL([USE_CONNMAN],
               [test "x$with_context_manager" = "xconnman"])

USE_NETLINK=no
AS_IF([test "x$with_context_manager" = "xlinux"],
      [AS_IF([test "x$HAVE_NETLINK" = "xno"],
             [AS_IF([test "x$HAVE_IFADDRS" = "xyes"],
	     AC_MSG_NOTICE([No rtnetlink found, falling back to static context manager]),
	     AC_MSG_ERROR([No rtnetlink nor ifaddrs.h found]))],
             [USE_NETLINK=yes])
      ],[])

AM_CONDITIONAL(USE_NETLINK, test "x$USE_NETLINK" = "xyes")
AC_SUBST(USE_NETLINK)

# glib-genmarshal
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

GOBJECT_INTROSPECTION_CHECK([0.6.4])

# vapigen
have_vapigen=no
AS_IF([test "x$found_introspection" = "xyes"],
      [
       dnl output of g-i >= 1.36.0 needs vapigen >= 0.20 to process
       AC_MSG_CHECKING([whether G-I is 1.36 or newer])
       VAPIGEN_MIN_VERSION=0.14
       AS_IF([pkg-config --atleast-version=1.36.0 gobject-introspection-1.0],
             [
              AC_MSG_RESULT([yes])
              VAPIGEN_MIN_VERSION=0.20
             ],[AC_MSG_RESULT([no])]
       )
       GUPNP_PROG_VAPIGEN([$VAPIGEN_MIN_VERSION])
      ]
)
AS_IF([test "x$VAPIGEN" != "x"], [have_vapigen=yes])
AM_CONDITIONAL([HAVE_VAPIGEN], [test "x$VAPIGEN" != "x"])

# Gtk-doc
GTK_DOC_CHECK([1.14],[--flavour no-tmpl])

AC_CONFIG_FILES([
Makefile
libgupnp/Makefile
examples/Makefile
tools/Makefile
tests/Makefile
tests/gtest/Makefile
doc/Makefile
doc/version.xml
vala/Makefile
gupnp-1.0.pc
gupnp-1.0-uninstalled.pc
])
AC_OUTPUT

echo "
    ${PACKAGE} ${VERSION}
    ---------------------

    Prefix:                ${prefix}
    GObject-Introspection: ${found_introspection}
    VALA bindings:         ${have_vapigen}
    Context manager:       ${with_context_manager}
    UUID Library:          ${GUUL_FLAVOR}
"
