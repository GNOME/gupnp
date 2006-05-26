AM_CFLAGS = $(LIBGUPNP_CFLAGS) -I$(top_srcdir)

libgupnpincdir = $(includedir)/gupnp-1.0/libgupnp

lib_LTLIBRARIES = libgupnp-1.0.la

libgupnpinc_HEADERS = 	gupnp-context.h		 \
			gupnp.h

libgupnp_1_0_la_SOURCES = gupnp-context.c

libgupnp_1_0_la_LIBADD = $(LIBGUPNP_LIBS)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
