if HAVE_GTEST
TESTS_ENVIRONMENT = G_SLICE=debug-blocks \
		    LD_LIBRARY_PATH=$(top_builddir)/libgupnp/.libs:$(LD_LIBRARY_PATH)

TESTS=$(check_PROGRAMS)

check_PROGRAMS = test-context

test_context_SOURCES = test-context.c
endif

LDADD = \
	$(top_builddir)/libgupnp/libgupnp-1.0.la \
	$(LIBGUPNP_LIBS)
AM_CFLAGS = \
	    $(LIBGUPNP_CFLAGS) \
	    -I $(top_srcdir) \
	    -DDATA_PATH="\"$(srcdir)/data\""

EXTRA_DIST=data/random4k.bin
