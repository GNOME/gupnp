if HAVE_INTROSPECTION
if HAVE_VAPIGEN
gupnp-1.0.vapi: gupnp-1.0.stamp
	@true

gupnp-1.0.stamp: $(top_builddir)/libgupnp/GUPnP-1.0.gir \
		$(srcdir)/GUPnP-1.0.metadata \
		$(srcdir)/gupnp-1.0-custom.vala
	$(AM_V_GEN)$(VAPIGEN) --metadatadir=$(srcdir) \
				  --pkg=libsoup-2.4 \
			      --library=gupnp-1.0 --pkg=gio-2.0 --pkg=libxml-2.0 \
			      $(top_builddir)/libgupnp/GUPnP-1.0.gir \
				  $(srcdir)/gupnp-1.0-custom.vala && \
	echo timestmap > $(@F)

vapidir = $(datadir)/vala/vapi
vapi_DATA = gupnp-1.0.vapi $(srcdir)/gupnp-1.0.deps
CLEANFILES = gupnp-1.0.vapi gupnp-1.0.stamp
EXTRA_DIST = gupnp-1.0.deps GUPnP-1.0.metadata gupnp-1.0-custom.vala
endif
endif
