AM_CFLAGS = $(LIBGUPNP_CFLAGS) -I$(top_srcdir)

noinst_PROGRAMS = test

test_SOURCES = test.c
test_LDADD   = $(top_builddir)/libgupnp/libgupnp-1.0.la

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
