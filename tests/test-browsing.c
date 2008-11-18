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
#include <stdlib.h>
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
        const char *type, *location;

        type = gupnp_device_info_get_device_type (GUPNP_DEVICE_INFO (proxy));
        location = gupnp_device_info_get_location (GUPNP_DEVICE_INFO (proxy));

        g_print ("Device available:\n");
        g_print ("\ttype:     %s\n", type);
        g_print ("\tlocation: %s\n", location);
}

static void
device_proxy_unavailable_cb (GUPnPControlPoint *cp,
                             GUPnPDeviceProxy  *proxy)
{
        const char *type, *location;

        type = gupnp_device_info_get_device_type (GUPNP_DEVICE_INFO (proxy));
        location = gupnp_device_info_get_location (GUPNP_DEVICE_INFO (proxy));

        g_print ("Device unavailable:\n");
        g_print ("\ttype:     %s\n", type);
        g_print ("\tlocation: %s\n", location);
}

static void
service_proxy_available_cb (GUPnPControlPoint *cp,
                            GUPnPServiceProxy *proxy)
{
        const char *type, *location;

        type = gupnp_service_info_get_service_type (GUPNP_SERVICE_INFO (proxy));
        location = gupnp_service_info_get_location (GUPNP_SERVICE_INFO (proxy));

        g_print ("Service available:\n");
        g_print ("\ttype:     %s\n", type);
        g_print ("\tlocation: %s\n", location);
}

static void
service_proxy_unavailable_cb (GUPnPControlPoint *cp,
                              GUPnPServiceProxy *proxy)
{
        const char *type, *location;

        type = gupnp_service_info_get_service_type (GUPNP_SERVICE_INFO (proxy));
        location = gupnp_service_info_get_location (GUPNP_SERVICE_INFO (proxy));

        g_print ("Service unavailable:\n");
        g_print ("\ttype:     %s\n", type);
        g_print ("\tlocation: %s\n", location);
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
                g_printerr ("Error creating the GUPnP context: %s\n",
			    error->message);
                g_error_free (error);

                return EXIT_FAILURE;
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

        return EXIT_SUCCESS;
}
