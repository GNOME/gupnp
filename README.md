# GUPnP

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

GUPnP is free software released under the GNU LGPL version 2.1 or later.

## Building

GUPnP uses the meson build system. To build GUPnP, the simplest variant is
```
$ meson setup build
$ meson compile -C build
```

There are several options to customize the build, please see (meson_options.txt) for
a brief overview.

## Developer documentation

The developer documentation is available at https://gnome.pages.gitlab.gnome.org/gupnp/docs/


## Running the tests

The tests usually work on localhost (127.0.0.1 and ::1).

On start-up, the ests will check for a network device called "gupnp0". It it exists, it will
additonally use that for running the tests.

To create such a device you can use NetworkManager, for example:
```
$ nmcli c a ifname gupnp0 type dummy ipv4.method link-local ipv6.method link-local
```
