AM_CFLAGS = $(LIBGUPNP_CFLAGS) -I$(top_srcdir)

noinst_PROGRAMS = test-client test-server test-introspection

test_client_SOURCES = test-client.c
test_client_LDADD   = $(top_builddir)/libgupnp/libgupnp-1.0.la

test_introspection_SOURCES = test-introspection.c
test_introspection_LDADD   = $(top_builddir)/libgupnp/libgupnp-1.0.la

test_server_SOURCES = test-server.c
test_server_LDADD   = $(top_builddir)/libgupnp/libgupnp-1.0.la

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
