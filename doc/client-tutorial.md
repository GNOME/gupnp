---
Title: Interacting with remote UPnP devices
---

# UPnP Client Tutorial

This chapter explains how to write an application which fetches the external IP address
from an UPnP-compliant modem. To do this, a Control Point is created, which searches for
services of the type `urn:schemas-upnp-org:service:WANIPConnection:1` which is part of
the Internet Gateway Devce specification.

As services are discovered, ServiceProxy objects are created by GUPnP to allow interaction
with the service, on which we can invoke the action `GetExternalIPAddress` to fetch the
external IP address.

## Finding Services

First, we initialize GUPnP and create a control point targeting the service type.
Then we connect a signal handler so that we are notified when services we are interested in
are found.


```c
#include <ibgupnp/gupnp-control-point.h>

static GMainLoop *main_loop;

static void
service_proxy_available_cb (GUPnPControlPoint *cp,
                            GUPnPServiceProxy *proxy,
                            gpointer           userdata)
{
  /* ... */
}

int
main (int argc, char **argv)
{
  GUPnPContext *context;
  GUPnPControlPoint *cp;

  /* Create a new GUPnP Context.  By here we are using the default GLib main
     context, and connecting to the current machine&apos;s default IP on an
     automatically generated port. */
  context = gupnp_context_new (NULL, 0, NULL);

  /* Create a Control Point targeting WAN IP Connection services */
  cp = gupnp_control_point_new
    (context, "urn:schemas-upnp-org:service:WANIPConnection:1");

  /* The service-proxy-available signal is emitted when any services which match
     our target are found, so connect to it */
  g_signal_connect (cp,
      "service-proxy-available2,
      G_CALLBACK (service_proxy_available_cb),
      NULL);

  /* Tell the Control Point to start searching */
  gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);
  
  /* Enter the main loop. This will start the search and result in callbacks to
     service_proxy_available_cb. */
  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);

  /* Clean up */
  g_main_loop_unref (main_loop);
  g_object_unref (cp);
  g_object_unref (context);
  
  return 0;
}
```

## Invoking Actions
Now we have an application which searches for the service we specified and
calls `service_proxy_available_cb` for each one it
found.  To get the external IP address we need to invoke the
`GetExternalIPAddress` action.  This action takes no in
arguments, and has a single out argument called "NewExternalIPAddress".
GUPnP has a set of methods to invoke actions where you pass a
`NULL`-terminated varargs list of (name, GType, value)
tuples for the in arguments, then a `NULL`-terminated
varargs list of (name, GType, return location) tuples for the out
arguments.

```c
static void
service_proxy_available_cb (GUPnPControlPoint *cp,
                            GUPnPServiceProxy *proxy,
                            gpointer           userdata)
{
  GError *error = NULL;
  char *ip = NULL;
  GUPnPServiceProxyAction *action = NULL;
  
  action = gupnp_service_proxy_action_new (
       /* Action name */
       "GetExternalIPAddress",
       /* IN args */
       NULL);
  gupnp_service_proxy_call_action (proxy,
                                   action,
                                   NULL,
                                   &error);
  if (error != NULL) {
    goto out;
  }

  gupnp_service_proxy_action_get_result (action,
       /* Error location */
       &error,
       /* OUT args */
       "NewExternalIPAddress",
       G_TYPE_STRING, &ip,
       NULL);
  
  if (error == NULL) {
    g_print ("External IP address is %s\n", ip);
    g_free (ip);
  }

out:
  if (error != NULL) {
    g_printerr ("Error: %s\n", error->message);
    g_error_free (error);
  }

  gupnp_service_proxy_action_unref (action);
  g_main_loop_quit (main_loop);
}
```

Note that `gupnp_service_proxy_call_action()` blocks until the service has
replied.  If you need to make non-blocking calls then use
`gupnp_service_proxy_call_action_async()`, which takes a callback that will be
called from the mainloop when the reply is received.


## Subscribing to state variable change notifications
It is possible to get change notifications for the service state variables 
that have attribute `sendEvents="yes"`. We'll demonstrate
this by modifying `service_proxy_available_cb` and using
`gupnp_service_proxy_add_notify()` to setup a notification callback:


```c
static void
external_ip_address_changed (GUPnPServiceProxy *proxy,
                             const char        *variable,
                             GValue            *value,
                             gpointer           userdata)
{
  g_print ("External IP address changed: %s\n", g_value_get_string (value));
}

static void
service_proxy_available_cb (GUPnPControlPoint *cp,
                            GUPnPServiceProxy *proxy,
                            gpointer           userdata)
{
  g_print ("Found a WAN IP Connection service\n");
  
  gupnp_service_proxy_set_subscribed (proxy, TRUE);
  if (!gupnp_service_proxy_add_notify (proxy,
                                       "ExternalIPAddress",
                                       G_TYPE_STRING,
                                       external_ip_address_changed,
                                       NULL)) {
    g_printerr ("Failed to add notify");
  }
}
```

## Generating wrappers

Using `gupnp_service_proxy_call_action()` and `gupnp_service_proxy_add_notify()`
can become tedious, because of the requirement to specify the types and deal
with GValues.  An
alternative is to use `gupnp-binding-tool`, which
generates wrappers that hide the boilerplate code from you.  Using a 
wrapper generated with prefix "ipconn" would replace
`gupnp_service_proxy_call_action()` with this code:

```c
ipconn_get_external_ip_address (proxy, &ip, &error);
```

State variable change notifications are friendlier with wrappers as well:

```c
static void
external_ip_address_changed (GUPnPServiceProxy *proxy,
                             const gchar       *external_ip_address,
                             gpointer           userdata)
{
  g_print ("External IP address changed: &apos;%s&apos;\n", external_ip_address);
}

static void
service_proxy_available_cb (GUPnPControlPoint *cp,
                            GUPnPServiceProxy *proxy
                            gpointer           userdata)
{
  g_print ("Found a WAN IP Connection service\n");
  
  gupnp_service_proxy_set_subscribed (proxy, TRUE);
  if (!ipconn_external_ip_address_add_notify (proxy,
                                              external_ip_address_changed,
                                              NULL)) {
    g_printerr ("Failed to add notify");
  }
}
```

