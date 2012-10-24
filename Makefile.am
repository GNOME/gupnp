ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = libgupnp tools examples tests doc vala

pkgconfig_DATA= gupnp-1.0.pc
pkgconfigdir = $(libdir)/pkgconfig

EXTRA_DIST = gupnp-1.0.pc.in gupnp-1.0-uninstalled.pc.in

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-introspection

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = aclocal.m4 compile config.guess config.sub configure depcomp install-sh ltmain.sh Makefile.in missing config.h.in
