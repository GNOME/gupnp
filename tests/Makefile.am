SUBDIRS = gtest

AM_CFLAGS = $(LIBGUPNP_CFLAGS) $(GTHREAD_CFLAGS) -I$(top_srcdir)

noinst_PROGRAMS = test-browsing      \
		  test-proxy         \
		  test-server        \
		  test-introspection

test_browsing_SOURCES = test-browsing.c
test_browsing_LDADD   = $(top_builddir)/libgupnp/libgupnp-1.0.la \
			$(GTHREAD_LIBS) \
			$(LIBGUPNP_LIBS)

test_proxy_SOURCES = test-proxy.c
test_proxy_LDADD   = $(top_builddir)/libgupnp/libgupnp-1.0.la \
		     $(GTHREAD_LIBS) \
		     $(LIBGUPNP_LIBS)

test_introspection_SOURCES = test-introspection.c
test_introspection_LDADD   = $(top_builddir)/libgupnp/libgupnp-1.0.la \
			     $(GTHREAD_LIBS) \
			     $(LIBGUPNP_LIBS)

test_server_SOURCES = test-server.c
test_server_LDADD   = $(top_builddir)/libgupnp/libgupnp-1.0.la \
		      $(GTHREAD_LIBS) \
		      $(LIBGUPNP_LIBS)
test_server_LDFLAGS = -export-dynamic

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
