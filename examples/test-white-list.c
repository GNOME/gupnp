/*
 * Copyright (C) 2013 Intel Corporation.
 *
 * Author: Ludovic Ferrandis <ludovic.ferrandis@intel.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <libgupnp/gupnp-control-point.h>
#include <libgupnp/gupnp-context-manager.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <signal.h>
#include <glib.h>

GMainLoop *main_loop;

#ifndef G_OS_WIN32
#include <glib-unix.h>
#endif

static gboolean
unix_signal_handler (gpointer user_data)
{
        g_main_loop_quit (main_loop);

        return G_SOURCE_REMOVE;
}


#ifdef G_OS_WIN32
/*
 *  Since in Windows this is basically called from
 *  the ConsoleCtrlHandler which does not share the restrictions of Unix signals
 */
static void
interrupt_signal_handler (G_GNUC_UNUSED int signum)
{
        unix_signal_handler (NULL);
}

#endif
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
print_context_filter_entries (GUPnPContextFilter *wl)
{
        GList *list;

        g_print ("\t\tContext filter Entries:\n");
        list = gupnp_context_filter_get_entries (wl);
        g_list_foreach (list, print_wl_entry, NULL);
        g_print ("\n");
}


static gboolean
change_context_filter (gpointer user_data)
{
        GUPnPContextManager *context_manager = user_data;
        GUPnPContextFilter *context_filter;
        static int tomato = 0;

        g_print ("\nChange Context filter:\n");
        g_print ("\t Action number %d:\n", tomato);

        context_filter =
                gupnp_context_manager_get_context_filter (context_manager);

        switch (tomato) {
        case 0:
                g_print ("\t Add Entry eth0\n\n");
                gupnp_context_filter_add_entry (context_filter, "eth0");
                print_context_filter_entries (context_filter);
                break;
        case 1:
                g_print ("\t Enable WL\n\n");
                gupnp_context_filter_set_enabled (context_filter, TRUE);
                break;
        case 2:
                g_print ("\t Add Entry 127.0.0.1\n\n");
                gupnp_context_filter_add_entry (context_filter, "127.0.0.1");
                print_context_filter_entries (context_filter);
                break;
        case 3:
                g_print ("\t Add Entry eth5\n\n");
                gupnp_context_filter_add_entry (context_filter, "eth5");
                print_context_filter_entries (context_filter);
                break;
        case 4:
                g_print ("\t Remove Entry eth5\n\n");
                gupnp_context_filter_remove_entry (context_filter, "eth5");
                print_context_filter_entries (context_filter);
                break;
        case 5:
                g_print ("\t Clear all entries\n\n");
                gupnp_context_filter_clear (context_filter);
                print_context_filter_entries (context_filter);
                break;
        case 6:
                g_print ("\t Add Entry wlan2\n\n");
                gupnp_context_filter_add_entry (context_filter, "wlan2");
                print_context_filter_entries (context_filter);
                break;
        case 7:
                g_print ("\t Disable WL\n\n");
                gupnp_context_filter_set_enabled (context_filter, FALSE);
                break;
        case 8:
                g_print ("\t Enable WL\n\n");
                gupnp_context_filter_set_enabled (context_filter, TRUE);
                break;
        case 9:
                g_print ("\t Connect to wlan0\n\n");
                g_timeout_add_seconds (35,
                                       change_context_filter,
                                       context_manager);
                break;
        case 10:
                g_print ("\t Add Entry wlan0\n\n");
                gupnp_context_filter_add_entry (context_filter, "wlan0");
                print_context_filter_entries (context_filter);
                break;
        //~ case 11:
        //~ g_print ("\t Enable WL\n");
        //~ gupnp_context_filter_enable(context_filter, FALSE);
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

        id = g_timeout_add_seconds (5, change_context_filter, cm);

#ifndef G_OS_WIN32
        g_unix_signal_add (SIGINT, unix_signal_handler, NULL);
#else
        signal(SIGINT, interrupt_signal_handler);
#endif /* G_OS_WIN32 */

        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_source_remove (id);

        g_object_unref (cm);

        return EXIT_SUCCESS;
}
