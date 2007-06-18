AM_CFLAGS = $(LIBGUPNP_CFLAGS) -I$(top_srcdir)

libgupnpincdir = $(includedir)/gupnp-1.0/libgupnp

lib_LTLIBRARIES = libgupnp-1.0.la

libgupnpinc_HEADERS = gupnp-context.h		\
		      gupnp-control-point.h	\
		      gupnp-device.h		\
		      gupnp-device-info.h	\
	              gupnp-device-proxy.h	\
		      gupnp-error.h		\
		      gupnp-root-device.h	\
		      gupnp-service.h		\
		      gupnp-service-info.h	\
		      gupnp-service-proxy.h	\
		      gupnp.h

gupnp-marshal.c: gupnp-marshal.list
	$(GLIB_GENMARSHAL) --prefix=gupnp_marshal $(srcdir)/gupnp-marshal.list --header --body > gupnp-marshal.c

gupnp-marshal.h: gupnp-marshal.list
	$(GLIB_GENMARSHAL) --prefix=gupnp_marshal $(srcdir)/gupnp-marshal.list --header > gupnp-marshal.h

BUILT_SOURCES = gupnp-marshal.c gupnp-marshal.h

libgupnp_1_0_la_SOURCES = gupnp-context.c		\
			  gupnp-context-private.h	\
			  gupnp-control-point.c		\
			  gupnp-device.c		\
			  gupnp-device-info.c		\
			  gupnp-device-proxy.c		\
			  gupnp-device-proxy-private.h	\
			  gupnp-error.c			\
			  gupnp-root-device.c		\
			  gupnp-service-info.c		\
			  gupnp-service-proxy.c		\
			  gupnp-service-proxy-private.h \
			  xml-util.c			\
			  xml-util.h			\
			  gena-protocol.h		\
			  $(BUILT_SOURCES)

libgupnp_1_0_la_LIBADD = $(LIBGUPNP_LIBS)

EXTRA_DIST = gupnp-marshal.list

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
