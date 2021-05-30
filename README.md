GUPnP
=====

GUPnP is an object-oriented open source framework for creating UPnP devices and
control points, written in C using GObject and libsoup. The GUPnP API is
intended to be easy to use, efficient and flexible.

The GUPnP framework consists of the following two libraries:

  * GSSDP implements resource discovery and announcement over SSDP.

  * GUPnP implements the UPnP specification: resource announcement and
    discovery, description, control, event notification, and presentation
    (GUPnP includes basic web server functionality through libsoup). GUPnP does
    not include helpers for construction or control of specific standardized
    resources (e.g. MediaServer); this is left for higher level libraries
    utilizing the GUPnP framework.

The GUPnP framework was born out of frustration with libupnp and its mess of
threads. GUPnP is entirely single-threaded (though asynchronous), integrates
with the GLib main loop, and provides the same set of features as libupnp while
hiding most of the UPnP internals through an elegant object-oriented design.

GUPnP is free software released under the GNU LGPL.
