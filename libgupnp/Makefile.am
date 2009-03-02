LTVERSION = 2:0:0

AM_CFLAGS = $(LIBGUPNP_CFLAGS) $(DBUS_GLIB_CFLAGS) -I$(top_srcdir)

libgupnpincdir = $(includedir)/gupnp-1.0/libgupnp

lib_LTLIBRARIES = libgupnp-1.0.la

libgupnpinc_HEADERS = gupnp-context.h			\
		      gupnp-context-manager.h		\
		      gupnp-control-point.h		\
		      gupnp-device.h			\
		      gupnp-device-info.h		\
	              gupnp-device-proxy.h		\
		      gupnp-error.h			\
		      gupnp-resource-factory.h		\
		      gupnp-root-device.h		\
		      gupnp-service.h			\
		      gupnp-service-info.h		\
		      gupnp-service-proxy.h		\
	              gupnp-service-introspection.h	\
	              gupnp-types.h			\
		      gupnp.h

gupnp-marshal.c: gupnp-marshal.list
	$(GLIB_GENMARSHAL) --prefix=gupnp_marshal $(srcdir)/gupnp-marshal.list --header --body > gupnp-marshal.c

gupnp-marshal.h: gupnp-marshal.list
	$(GLIB_GENMARSHAL) --prefix=gupnp_marshal $(srcdir)/gupnp-marshal.list --header > gupnp-marshal.h

BUILT_SOURCES = gupnp-marshal.c gupnp-marshal.h

libgupnp_1_0_la_LDFLAGS = -version-info $(LTVERSION)

libgupnp_1_0_la_SOURCES = http-headers.c			\
			  http-headers.h			\
			  gupnp-context.c			\
			  gupnp-context-private.h		\
		     	  gupnp-context-manager.c		\
		     	  gupnp-context-manager-private.h	\
		     	  gupnp-unix-context-manager.c		\
		     	  gupnp-unix-context-manager.h		\
			  gupnp-control-point.c			\
			  gupnp-device.c			\
			  gupnp-device-info.c			\
			  gupnp-device-info-private.h		\
			  gupnp-device-proxy.c			\
			  gupnp-error.c				\
			  gupnp-error-private.h			\
			  gupnp-resource-factory.c		\
		      	  gupnp-resource-factory-private.h	\
			  gupnp-root-device.c			\
		          gupnp-service.c			\
			  gupnp-service-info.c			\
			  gupnp-service-proxy.c			\
	                  gupnp-service-introspection.c		\
	                  gupnp-service-introspection-private.h	\
	                  gupnp-types.c				\
	                  gupnp-types-private.h			\
			  gvalue-util.c				\
			  gvalue-util.h				\
			  xml-util.c				\
			  xml-util.h				\
			  gena-protocol.h			\
			  $(BUILT_SOURCES)

libgupnp_1_0_la_LIBADD = $(LIBGUPNP_LIBS) $(DBUS_GLIB_LIBS)

EXTRA_DIST = gupnp-marshal.list

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
