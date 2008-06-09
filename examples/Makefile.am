AM_CFLAGS = $(LIBGUPNP_CFLAGS) -I$(top_srcdir)
LDADD = $(top_builddir)/libgupnp/libgupnp-1.0.la

noinst_PROGRAMS = light-server light-client

light_server_SOURCES = light-server.c
light_server_LDFLAGS = -export-dynamic

light_client_SOURCES = light-client.c

MAINTAINERCLEANFILES = Makefile.in
