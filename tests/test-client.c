/* 
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <libgupnp/gupnp-control-point.h>
#include <string.h>
#include <locale.h>
#include <signal.h>

GMainLoop *main_loop;

static void
interrupt_signal_handler (int signum)
{
        g_main_loop_quit (main_loop);
}

static void
device_proxy_available_cb (GUPnPControlPoint *cp,
                           GUPnPDeviceProxy  *proxy)
{
        char *type;
        const char *location;

        type = gupnp_device_info_get_device_type (GUPNP_DEVICE_INFO (proxy));
        location = gupnp_device_info_get_location (GUPNP_DEVICE_INFO (proxy));

        g_print ("Device available:\n");
        g_print ("\ttype:     %s\n", type);
        g_print ("\tlocation: %s\n", location);

        g_free (type);
}

static void
device_proxy_unavailable_cb (GUPnPControlPoint *cp,
                             GUPnPDeviceProxy  *proxy)
{
        char *type;
        const char *location;

        type = gupnp_device_info_get_device_type (GUPNP_DEVICE_INFO (proxy));
        location = gupnp_device_info_get_location (GUPNP_DEVICE_INFO (proxy));

        g_print ("Device unavailable:\n");
        g_print ("\ttype:     %s\n", type);
        g_print ("\tlocation: %s\n", location);

        g_free (type);
}

static void
notify_cb (GUPnPServiceProxy *proxy,
           const char        *variable,
           GValue            *value,
           gpointer           user_data)
{
        g_print ("Received a notification for variable '%s':\n", variable);
        g_print ("\tvalue:     %d\n", g_value_get_uint (value));
        g_print ("\tuser_data: %s\n", (const char *) user_data);
}

static void
service_proxy_available_cb (GUPnPControlPoint *cp,
                            GUPnPServiceProxy *proxy)
{
        char *type;
        const char *location;

        type = gupnp_service_info_get_service_type (GUPNP_SERVICE_INFO (proxy));
        location = gupnp_service_info_get_location (GUPNP_SERVICE_INFO (proxy));

        g_print ("Service available:\n");
        g_print ("\ttype:     %s\n", type);
        g_print ("\tlocation: %s\n", location);

        if (strcmp (type,
                    "urn:schemas-upnp-org:service:ContentDirectory:1") == 0) {
                /* We have a ContentDirectory - yay! Run some tests. */
                char *result = NULL;;
                guint count, total;
                GError *error = NULL;

                /* We want to be notified whenever SystemUpdateID (of type uint)
                 * changes */
                gupnp_service_proxy_add_notify (proxy,
                                                "SystemUpdateID",
                                                G_TYPE_UINT,
                                                notify_cb,
                                                "Test");

                /* Subscribe */
                gupnp_service_proxy_set_subscribed (proxy, TRUE);

                /* And test action IO */
                gupnp_service_proxy_send_action (proxy,
                                                 "Browse",
                                                 &error,
                                                 /* IN args */
                                                 "ObjectID",
                                                        G_TYPE_STRING,
                                                        "0",
                                                 "BrowseFlag",
                                                        G_TYPE_STRING,
                                                        "BrowseDirectChildren",
                                                 "Filter",
                                                        G_TYPE_STRING,
                                                        "*",
                                                 "StartingIndex",
                                                        G_TYPE_UINT,
                                                        0,
                                                 "RequestedCount",
                                                        G_TYPE_UINT,
                                                        0,
                                                 "SortCriteria",
                                                        G_TYPE_STRING,
                                                        "",
                                                 NULL,
                                                 /* OUT args */
                                                 "Result",
                                                        G_TYPE_STRING,
                                                        &result,
                                                 "NumberReturned",
                                                        G_TYPE_UINT,
                                                        &count,
                                                 "TotalMatches",
                                                        G_TYPE_UINT,
                                                        &total,
                                                 NULL);

                if (error) {
                        g_printerr ("Error: %s\n", error->message);
                        g_error_free (error);

                        g_free (type);

                        return;
                }

                g_print ("Browse returned:\n");
                g_print ("\tResult:         %s\n", result);
                g_print ("\tNumberReturned: %u\n", count);
                g_print ("\tTotalMatches:   %u\n", total);

                g_free (result);
        }

        g_free (type);
}

static void
service_proxy_unavailable_cb (GUPnPControlPoint *cp,
                              GUPnPServiceProxy *proxy)
{
        char *type;
        const char *location;

        type = gupnp_service_info_get_service_type (GUPNP_SERVICE_INFO (proxy));
        location = gupnp_service_info_get_location (GUPNP_SERVICE_INFO (proxy));

        g_print ("Service unavailable:\n");
        g_print ("\ttype:     %s\n", type);
        g_print ("\tlocation: %s\n", location);

        g_free (type);
}

int
main (int argc, char **argv)
{
        GError *error;
        GUPnPContext *context;
        GUPnPControlPoint *cp;
        struct sigaction sig_action;

        g_thread_init (NULL);
        g_type_init ();
        setlocale (LC_ALL, "");

        error = NULL;
        context = gupnp_context_new (NULL, NULL, 0, &error);
        if (error) {
                g_error (error->message);
                g_error_free (error);

                return 1;
        }

        /* We're interested in everything */
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
        
        /* Hook the handler for SIGTERM */
        memset (&sig_action, 0, sizeof (sig_action));
        sig_action.sa_handler = interrupt_signal_handler;
        sigaction (SIGINT, &sig_action, NULL);

        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_object_unref (cp);
        g_object_unref (context);

        return 0;
}
