AM_CFLAGS = $(LIBGUPNP_CFLAGS) -I$(top_srcdir)

libgupnpincdir = $(includedir)/gupnp-1.0/libgupnp

lib_LTLIBRARIES = libgupnp-1.0.la

libgupnpinc_HEADERS = 	gupnp-context.h		\
			gupnp-error.h		\
			gupnp.h

gupnp-marshal.c: gupnp-marshal.list
	$(GLIB_GENMARSHAL) --prefix=gupnp_marshal $(srcdir)/gupnp-marshal.list --header --body > gupnp-marshal.c

gupnp-marshal.h: gupnp-marshal.list
	$(GLIB_GENMARSHAL) --prefix=gupnp_marshal $(srcdir)/gupnp-marshal.list --header > gupnp-marshal.h

BUILT_SOURCES = gupnp-marshal.c gupnp-marshal.h

libgupnp_1_0_la_SOURCES = gupnp-context.c	\
			  gupnp-error.c		\
			  gupnp-socket-source.c	\
			  gupnp-socket-source.h \
			  $(BUILT_SOURCES)

libgupnp_1_0_la_LIBADD = $(LIBGUPNP_LIBS)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
