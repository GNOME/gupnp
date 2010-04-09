LTVERSION = 3:0:0

if USE_NETWORK_MANAGER
CONTEXT_MANAGER_IMPL = gupnp-network-manager.c  \
		       gupnp-network-manager.h
CONTEXT_MANAGER_CFLAGS = -DUSE_NETWORK_MANAGER
else
CONTEXT_MANAGER_IMPL =
CONTEXT_MANAGER_CFLAGS =
endif

if USE_CONIC
CONIC_CFLAGS = $(LIBCONIC_CFLAGS) -DUSE_CONIC
else
CONIC_CFLAGS =
endif

AM_CFLAGS = $(LIBGUPNP_CFLAGS) \
	    $(DBUS_GLIB_CFLAGS) \
	    $(CONIC_CFLAGS) \
	    -I$(top_srcdir) \
	    $(CONTEXT_MANAGER_CFLAGS)

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
		      gupnp-xml-doc.h	                \
	              gupnp-types.h			\
		      gupnp.h

gupnp-marshal.c: gupnp-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=gupnp_marshal $(srcdir)/gupnp-marshal.list --header --body > gupnp-marshal.c

gupnp-marshal.h: gupnp-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --prefix=gupnp_marshal $(srcdir)/gupnp-marshal.list --header > gupnp-marshal.h

BUILT_SOURCES = gupnp-marshal.c gupnp-marshal.h

libgupnp_1_0_la_LDFLAGS = -version-info $(LTVERSION)

libgupnp_1_0_la_SOURCES = http-headers.c			\
			  http-headers.h			\
			  gupnp-context.c			\
			  gupnp-context-private.h		\
		     	  gupnp-context-manager.c		\
		     	  $(CONTEXT_MANAGER_IMPL) 		\
			  gupnp-unix-context-manager.c  	\
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
		      	  gupnp-xml-doc.c	                \
	                  gupnp-types.c				\
	                  gupnp-types-private.h			\
			  gvalue-util.c				\
			  gvalue-util.h				\
			  xml-util.c				\
			  xml-util.h				\
			  gena-protocol.h			\
			  $(BUILT_SOURCES)

libgupnp_1_0_la_LIBADD = $(LIBGUPNP_LIBS) $(DBUS_GLIB_LIBS) $(LIBCONIC_LIBS)

EXTRA_DIST = gupnp-marshal.list 	   \
	     gupnp-network-manager.c   	   \
	     gupnp-network-manager.h       \
	     gupnp-unix-context-manager.c  \
	     gupnp-unix-context-manager.h


if HAVE_INTROSPECTION

GUPnP-1.0.gir: libgupnp-1.0.la $(INTROSPECTION_SCANNER)
	$(AM_V_GEN) \
	$(INTROSPECTION_SCANNER) -v \
		--namespace=GUPnP --nsversion=1.0 \
		--include=libxml2-2.0 \
		--include=GObject-2.0 \
		--include=Soup-2.4 \
		--include=GSSDP-1.0 \
		--library=$< \
		--libtool="$(top_builddir)/libtool" \
		--pkg libxml-2.0 \
		--pkg gobject-2.0 \
		--pkg gssdp-1.0 \
		--pkg libsoup-2.4 \
		--output $@ \
		-I$(top_srcdir) \
		$(addprefix $(top_srcdir)/libgupnp/, $(libgupnpinc_HEADERS)) \
		$(top_srcdir)/libgupnp/gupnp-context.c \
		$(top_srcdir)/libgupnp/gupnp-context-manager.c	\
		$(top_srcdir)/libgupnp/gupnp-control-point.c \
		$(top_srcdir)/libgupnp/gupnp-device.c \
		$(top_srcdir)/libgupnp/gupnp-device-info.c \
		$(top_srcdir)/libgupnp/gupnp-device-proxy.c \
		$(top_srcdir)/libgupnp/gupnp-error.c \
		$(top_srcdir)/libgupnp/gupnp-resource-factory.c \
		$(top_srcdir)/libgupnp/gupnp-root-device.c \
		$(top_srcdir)/libgupnp/gupnp-service.c	\
		$(top_srcdir)/libgupnp/gupnp-service-info.c \
		$(top_srcdir)/libgupnp/gupnp-service-proxy.c \
		$(top_srcdir)/libgupnp/gupnp-service-introspection.c \
		$(top_srcdir)/libgupnp/gupnp-xml-doc.c	\
		$(top_srcdir)/libgupnp/gupnp-types.c

girdir = $(datadir)/gir-1.0
gir_DATA = GUPnP-1.0.gir

typelibdir = $(libdir)/girepository-1.0/
typelib_DATA = $(gir_DATA:.gir=.typelib)

%.typelib: %.gir $(INTROSPECTION_COMPILER)
	$(AM_V_GEN) \
	$(INTROSPECTION_COMPILER) \
		--includedir=$(srcdir) \
		--includedir=. \
		$(INTROSPECTION_COMPILER_OPTS) $< -o $(@F)

endif

CLEANFILES = $(BUILT_SOURCES) $(gir_DATA) $(typelib_DATA)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
