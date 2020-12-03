/*
 * Copyright (C) 2013 Intel Corporation.
 *
 * Author: Ludovic Ferrandis <ludovic.ferrandis@intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <libgupnp/gupnp-control-point.h>
#include <libgupnp/gupnp-context-manager.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <signal.h>
#include <glib.h>

GMainLoop *main_loop;

static void
interrupt_signal_handler (G_GNUC_UNUSED int signum)
{
        g_main_loop_quit (main_loop);
}

static void
device_proxy_available_cb (G_GNUC_UNUSED GUPnPControlPoint *cp,
                           GUPnPDeviceProxy                *proxy)
{
        const char *type, *location;

        type = gupnp_device_info_get_device_type (GUPNP_DEVICE_INFO (proxy));
        location = gupnp_device_info_get_location (GUPNP_DEVICE_INFO (proxy));

        g_print ("Device available:\n");
        g_print ("\ttype:     %s\n", type);
        g_print ("\tlocation: %s\n", location);
}

static void
device_proxy_unavailable_cb (G_GNUC_UNUSED GUPnPControlPoint *cp,
                             GUPnPDeviceProxy                *proxy)
{
        const char *type, *location;

        type = gupnp_device_info_get_device_type (GUPNP_DEVICE_INFO (proxy));
        location = gupnp_device_info_get_location (GUPNP_DEVICE_INFO (proxy));

        g_print ("Device unavailable:\n");
        g_print ("\ttype:     %s\n", type);
        g_print ("\tlocation: %s\n", location);
}

static void
service_proxy_available_cb (G_GNUC_UNUSED GUPnPControlPoint *cp,
                            GUPnPServiceProxy               *proxy)
{
        const char *type, *location;

        type = gupnp_service_info_get_service_type (GUPNP_SERVICE_INFO (proxy));
        location = gupnp_service_info_get_location (GUPNP_SERVICE_INFO (proxy));

        g_print ("Service available:\n");
        g_print ("\ttype:     %s\n", type);
        g_print ("\tlocation: %s\n", location);
}

static void
service_proxy_unavailable_cb (G_GNUC_UNUSED GUPnPControlPoint *cp,
                              GUPnPServiceProxy               *proxy)
{
        const char *type, *location;

        type = gupnp_service_info_get_service_type (GUPNP_SERVICE_INFO (proxy));
        location = gupnp_service_info_get_location (GUPNP_SERVICE_INFO (proxy));

        g_print ("Service unavailable:\n");
        g_print ("\ttype:     %s\n", type);
        g_print ("\tlocation: %s\n", location);
}

static void
context_available_cb(GUPnPContextManager *context_manager,
                     GUPnPContext *context,
                     gpointer user_data)
{
        GUPnPControlPoint *cp;
        GSSDPClient *client = GSSDP_CLIENT(context);

        g_print ("Context Available:\n");
        g_print ("\tServer ID:     %s\n", gssdp_client_get_server_id (client));
        g_print ("\tInterface:     %s\n", gssdp_client_get_interface (client));
        g_print ("\tHost IP  :     %s\n", gssdp_client_get_host_ip (client));
        g_print ("\tNetwork  :     %s\n", gssdp_client_get_network (client));
        g_print ("\tActive   :     %s\n", gssdp_client_get_active (client)? "TRUE" : "FALSE");


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
        gupnp_context_manager_manage_control_point(context_manager, cp);
        g_object_unref(cp);
}

static void
context_unavailable_cb(GUPnPContextManager *context_manager,
                       GUPnPContext *context,
                       gpointer user_data)
{
        GSSDPClient *client = GSSDP_CLIENT(context);

        g_print ("Context Unavailable:\n");
        g_print ("\tServer ID:     %s\n", gssdp_client_get_server_id (client));
        g_print ("\tInterface:     %s\n", gssdp_client_get_interface (client));
        g_print ("\tHost IP  :     %s\n", gssdp_client_get_host_ip (client));
        g_print ("\tNetwork  :     %s\n", gssdp_client_get_network (client));
        g_print ("\tActive   :     %s\n", gssdp_client_get_active (client)? "TRUE" : "FALSE");
}

static void
print_wl_entry(gpointer data, gpointer user_data)
{
        g_print ("\t\t\tEntry: %s\n", (char *)data);
}

static void
print_white_list_entries(GUPnPWhiteList *wl)
{
        GList *list;

        g_print ("\t\tWhite List Entries:\n");
        list = gupnp_white_list_get_entries(wl);
        g_list_foreach (list, print_wl_entry, NULL);
        g_print ("\n");
}

static gboolean
change_white_list(gpointer user_data)
{
        GUPnPContextManager *context_manager = user_data;
        GUPnPWhiteList *white_list;
        static int tomato = 0;

        g_print ("\nChange White List:\n");
        g_print ("\t Action number %d:\n", tomato);

        white_list = gupnp_context_manager_get_white_list(context_manager);

        switch (tomato) {
        case 0:
                g_print ("\t Add Entry eth0\n\n");
                gupnp_white_list_add_entry(white_list, "eth0");
                print_white_list_entries (white_list);
                break;
        case 1:
                g_print ("\t Enable WL\n\n");
                gupnp_white_list_set_enabled (white_list, TRUE);
                break;
        case 2:
                g_print ("\t Add Entry 127.0.0.1\n\n");
                gupnp_white_list_add_entry(white_list, "127.0.0.1");
                print_white_list_entries (white_list);
                break;
        case 3:
                g_print ("\t Add Entry eth5\n\n");
                gupnp_white_list_add_entry(white_list, "eth5");
                print_white_list_entries (white_list);
                break;
        case 4:
                g_print ("\t Remove Entry eth5\n\n");
                gupnp_white_list_remove_entry(white_list, "eth5");
                print_white_list_entries (white_list);
                break;
        case 5:
                g_print ("\t Clear all entries\n\n");
                gupnp_white_list_clear(white_list);
                print_white_list_entries (white_list);
                break;
        case 6:
                g_print ("\t Add Entry wlan2\n\n");
                gupnp_white_list_add_entry(white_list, "wlan2");
                print_white_list_entries(white_list);
                break;
        case 7:
                g_print ("\t Disable WL\n\n");
                gupnp_white_list_set_enabled (white_list, FALSE);
                break;
        case 8:
                g_print ("\t Enable WL\n\n");
                gupnp_white_list_set_enabled (white_list, TRUE);
                break;
        case 9:
                g_print ("\t Connect to wlan0\n\n");
                g_timeout_add_seconds (35, change_white_list, context_manager);
                break;
        case 10:
                g_print ("\t Add Entry wlan0\n\n");
                gupnp_white_list_add_entry(white_list, "wlan0");
                print_white_list_entries (white_list);
                break;
        //~ case 11:
                //~ g_print ("\t Enable WL\n");
                //~ gupnp_white_list_enable(white_list, FALSE);
                //~ break;
        default:
                break;
        }

        tomato++;

        return (tomato < 11) && (tomato != 10);
}

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
        GUPnPContextManager *cm;
        guint id;
#ifndef G_OS_WIN32
        struct sigaction sig_action;
#endif /* G_OS_WIN32 */

        setlocale (LC_ALL, "");

        cm = gupnp_context_manager_create(0);

        g_signal_connect(cm,
                         "context-available",
                         G_CALLBACK(context_available_cb),
                         NULL);

        g_signal_connect(cm,
                         "context-unavailable",
                         G_CALLBACK(context_unavailable_cb),
                         NULL);

        main_loop = g_main_loop_new (NULL, FALSE);

        id = g_timeout_add_seconds (5, change_white_list, cm);

#ifndef G_OS_WIN32
        /* Hook the handler for SIGTERM */
        memset (&sig_action, 0, sizeof (sig_action));
        sig_action.sa_handler = interrupt_signal_handler;
        sigaction (SIGINT, &sig_action, NULL);
#else
        signal(SIGINT, interrupt_signal_handler);
#endif /* G_OS_WIN32 */

        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_source_remove (id);

        g_object_unref (cm);

        return EXIT_SUCCESS;
}
