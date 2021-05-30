/*
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <libgupnp/gupnp-control-point.h>
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
subscription_lost_cb (G_GNUC_UNUSED GUPnPServiceProxy *proxy,
                      const GError                    *reason,
                      G_GNUC_UNUSED gpointer           user_data)
{
        g_print ("Lost subscription: %s\n", reason->message);
}

static void
notify_cb (G_GNUC_UNUSED GUPnPServiceProxy *proxy,
           const char                      *variable,
           GValue                          *value,
           gpointer                         user_data)
{
        g_print ("Received a notification for variable '%s':\n", variable);
        g_print ("\tvalue:     %d\n", g_value_get_uint (value));
        g_print ("\tuser_data: %s\n", (const char *) user_data);
}

static void
service_proxy_available_cb (G_GNUC_UNUSED GUPnPControlPoint *cp,
                            GUPnPServiceProxy               *proxy)
{
        const char *location;
        char *result = NULL;
        guint count, total;
        GError *error = NULL;
        GUPnPServiceProxyAction *action = NULL;

        location = gupnp_service_info_get_location (GUPNP_SERVICE_INFO (proxy));

        g_print ("ContentDirectory available:\n");
        g_print ("\tlocation: %s\n", location);

        /* We want to be notified whenever SystemUpdateID (of type uint)
         * changes */
        gupnp_service_proxy_add_notify (proxy,
                                        "SystemUpdateID",
                                        G_TYPE_UINT,
                                        notify_cb,
                                        (gpointer) "Test");

        /* Subscribe */
        g_signal_connect (proxy,
                          "subscription-lost",
                          G_CALLBACK (subscription_lost_cb),
                          NULL);

        gupnp_service_proxy_set_subscribed (proxy, TRUE);

        /* And test action IO */
        action = gupnp_service_proxy_action_new (
                                         "Browse",
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
                                         NULL);
        gupnp_service_proxy_call_action (proxy, action, NULL, &error);

        if (error) {
                g_printerr ("Error: %s\n", error->message);
                g_error_free (error);
                gupnp_service_proxy_action_unref (action);

                return;
        }

        gupnp_service_proxy_action_get_result (action,
                                               &error,
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
                gupnp_service_proxy_action_unref (action);

                return;
        }

        g_print ("Browse returned:\n");
        g_print ("\tResult:         %s\n", result);
        g_print ("\tNumberReturned: %u\n", count);
        g_print ("\tTotalMatches:   %u\n", total);

        gupnp_service_proxy_action_unref (action);
        g_free (result);
}

static void
service_proxy_unavailable_cb (G_GNUC_UNUSED GUPnPControlPoint *cp,
                              GUPnPServiceProxy               *proxy)
{
        const char *location;

        location = gupnp_service_info_get_location (GUPNP_SERVICE_INFO (proxy));

        g_print ("ContentDirectory unavailable:\n");
        g_print ("\tlocation: %s\n", location);
}

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
        GError *error;
        GUPnPContext *context;
        GUPnPControlPoint *cp;

        setlocale (LC_ALL, "");

        error = NULL;
        context = g_initable_new (GUPNP_TYPE_CONTEXT, NULL, &error, NULL);
        if (error) {
                g_printerr ("Error creating the GUPnP context: %s\n",
			    error->message);
                g_error_free (error);

                return EXIT_FAILURE;
        }

        /* We're interested in everything */
        cp = gupnp_control_point_new
                (context, "urn:schemas-upnp-org:service:ContentDirectory:1");

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
#ifndef G_OS_WIN32
        g_unix_signal_add (SIGINT, unix_signal_handler, NULL);
#else
        signal(SIGINT, interrupt_signal_handler);
#endif /* G_OS_WIN32 */

        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_object_unref (cp);
        g_object_unref (context);

        return EXIT_SUCCESS;
}
