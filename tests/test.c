#include <libgupnp/gupnp-control-point.h>
#include <string.h>

static void
device_proxy_available_cb (GUPnPControlPoint *cp,
                           GUPnPDeviceProxy  *proxy)
{
        char *type;
        const char *location;
        GList *service_node;

        type = gupnp_device_info_get_device_type (GUPNP_DEVICE_INFO (proxy));
        location = gupnp_device_info_get_location (GUPNP_DEVICE_INFO (proxy));

        if (strcmp (type, GUPNP_DEVICE_TYPE_MEDIA_STREAMER_1) == 0) {
                g_print ("MediaStreamer device available at %s\n", location);
        } else if (strcmp (type, GUPNP_DEVICE_TYPE_INTERNET_GATEWAY_1) == 0) {
                g_print ("InternetGateway device available at %s\n", location);
        } else
                g_print ("Device of type %s available at %s\n", type, location);
        g_free (type);

        service_node = gupnp_device_proxy_list_services (proxy);
        for (; service_node; service_node = service_node->next) {
                gchar *service_type;

                service_type = gupnp_service_info_get_service_type (
                                GUPNP_SERVICE_INFO (service_node->data));
                g_print ("service of type %s available\n", service_type);
        }
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
        char *type;

        type = gupnp_service_info_get_service_type (GUPNP_SERVICE_INFO (proxy));
        if (strcmp (type, "urn:schemas-upnp-org:service:ContentDirectory:1") == 0) {
                char *result = NULL;;
                guint count, total;
                GError *error = NULL;
                gupnp_service_proxy_set_subscribed (proxy, TRUE);
                gupnp_service_proxy_send_action (proxy,
                                                 "Browse",
                                                 &error,
                                                 "ObjectID", G_TYPE_STRING, "0",
                                                 "BrowseFlag", G_TYPE_STRING, "BrowseDirectChildren",
                                                 "Filter", G_TYPE_STRING, "*",
                                                 "StartingIndex", G_TYPE_UINT, 0,
                                                 "RequestedCount", G_TYPE_UINT, 0,
                                                 "SortCriteria", G_TYPE_STRING, "",
                                                 NULL,
                                                 "Result", G_TYPE_STRING ,&result,
                                                 "NumberReturned", G_TYPE_UINT, &count,
                                                 "TotalMatches", G_TYPE_UINT, &total,
                                                 NULL);
                if (error) {
                        g_printerr ("Error: %s\n", error->message);
                        g_error_free (error);
                }

                g_print ("res: %s\n", result);
        }
        g_print ("Service available with type: %s\n", type);
        g_free (type);
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
        context = gupnp_context_new (NULL, NULL, 0, &error);
        if (error) {
                g_error (error->message);
                g_error_free (error);

                return 1;
        }

        cp = gupnp_control_point_new (context, "ssdp:all");

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

        gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

        main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_object_unref (cp);
        g_object_unref (context);

        return 0;
}
