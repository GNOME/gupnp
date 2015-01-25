AC_DEFUN([GUUL_CHECK_UUID],
[
    AS_IF([test "x$1" = "xinternal"],
          [AC_DEFINE([GUUL_INTERNAL],[1],[GUUL will be used internally])])

    GUUL_LIBS=
    GUUL_CFLAGS=
    AC_MSG_CHECKING([for uuid library])
    case "$host" in
        *-*-mingw*)
            uuid_found=windows
            AC_DEFINE([GUUL_PLATFORM_WINDOWS],[1],[Compiling for windows])
            GUUL_LIBS=-lrpcrt4
            ;;
        *darwin*)
            uuid_found=macosx
            AC_DEFINE([GUUL_PLATFORM_OSX],[1],[Compiling for OS X])
            ;;
        *bsd*)
            uuid_found=bsd
            ;;
        *)
            uuid_found=generic
            ;;
    esac

    dnl do fallback if we have a BSD that does not have the necessary functions
    AS_IF([test "x$uuid_found" = "xbsd"],
          [AC_SEARCH_LIBS([uuid_to_string], [c],
            [UUID_LIBS=
             AC_DEFINE([GUUL_PLATFORM_BSD],[1],[Compiling for BSD flavor])
            ],
            [uuid_found=generic])
          ])

    dnl for other platforms, use libuuid from the e2fs project
    AS_IF([test "x$uuid_found" = "xgeneric"],
          [PKG_CHECK_MODULES(UUID, [uuid],
            [
              AC_DEFINE([GUUL_PLATFORM_GENERIC],[1],[Using external library])
              GUUL_LIBS="$UUID_LIBS"
              GUUL_CFLAGS="$UUID_CFLAGS"
              GUUL_PKG=uuid
            ],
            [AC_MSG_ERROR([none])
          ])
    ])

    AC_SUBST(GUUL_LIBS)
    AC_SUBST(GUUL_CFLAGS)
    AC_SUBST(GUUL_PKG)
    GUUL_FLAVOR=$uuid_found
    AC_MSG_RESULT([using flavor $uuid_found])
])
