AM_CFLAGS = $(LIBGUPNP_CFLAGS) -I$(top_srcdir) $(GTHREAD_CFLAGS)
LDADD = $(top_builddir)/libgupnp/libgupnp-1.0.la

noinst_PROGRAMS = light-server light-client

light_server_SOURCES = light-server.c
light_server_LDFLAGS = -export-dynamic $(GTHREAD_LIBS)

light_client_SOURCES = light-client.c
light_client_LDFLAGS = $(GTHREAD_LIBS)

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = BinaryLight1.xml \
	     SwitchPower1.xml
