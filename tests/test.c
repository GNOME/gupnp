#include <libgupnp/gupnp-control-point.h>

/**
 *  - Resurrect Control point
 *  - Control point asynchronously parses device descriptions (and caches them),
 *    and when read signals availability.
 *  - Different proxy constructors
 *  - On service-available:
 *        if uuid:device-UUID::urn:domain-name:service:serviceType:v,
 *        then do serviceProxy(xmlDoc, serviceType(urn..v))
 *        else do deviceProxy(xmlDoc, UDN(until ::))
 *  - Four signals: device-(un)available and service-(un)available
 *
 *
 *  After that:
 *  - GUPnPContext, which is a subclass of GSSDPClient, with port prop
 *    pass this to all constructors (control point being the only public one)
 *        SoupSession
 *        SoupServer
*/

static void
device_proxy_available_cb (GUPnPControlPoint *cp,
                           GUPnPDeviceProxy  *proxy)
{
        g_print ("Device available with type: %s\n",
                 gupnp_device_info_get_device_type (GUPNP_DEVICE_INFO (proxy)));
}

static void
device_proxy_unavailable_cb (GUPnPControlPoint *cp,
                             GUPnPDeviceProxy  *proxy)
{
}

static void
service_proxy_available_cb (GUPnPControlPoint *cp,
                            GUPnPServiceProxy *proxy)
{
        g_print ("Service available with type: %s\n",
                 gupnp_service_info_get_service_type
                                (GUPNP_SERVICE_INFO (proxy)));
}

static void
service_proxy_unavailable_cb (GUPnPControlPoint *cp,
                              GUPnPServiceProxy *proxy)
{
}

int
main (int argc, char **argv)
{
        GError *error;
        GUPnPContext *context;
        GUPnPControlPoint *cp;
        GMainLoop *main_loop;
        
        g_thread_init (NULL);
        g_type_init ();

        error = NULL;
        context = gupnp_context_new (NULL, &error);
        if (error) {
                g_error (error->message);
                g_error_free (error);

                return 1;
        }

        cp = gupnp_control_point_new (context, "upnp:rootdevice");

        g_signal_connect (cp,
                          "device-proxy-available",
                          G_CALLBACK (device_proxy_available_cb),
                          NULL);
        g_signal_connect (cp,
                          "device-proxy-unavailable",
                          G_CALLBACK (device_proxy_unavailable_cb),
                          NULL);
        g_signal_connect (cp,
                          "service-proxy-available",
                          G_CALLBACK (service_proxy_available_cb),
                          NULL);
        g_signal_connect (cp,
                          "service-proxy-unavailable",
                          G_CALLBACK (service_proxy_unavailable_cb),
                          NULL);

        gssdp_service_browser_set_active (GSSDP_SERVICE_BROWSER (cp), TRUE);

        main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_object_unref (cp);
        g_object_unref (context);

        return 0;
}
