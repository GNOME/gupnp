/*
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <libgupnp/gupnp-root-device.h>
#include <libgupnp/gupnp-service.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <signal.h>


#ifndef G_OS_WIN32
#include <glib-unix.h>
#endif

GMainLoop *main_loop;

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
notify_failed_cb (G_GNUC_UNUSED GUPnPService *service,
                  G_GNUC_UNUSED const GList  *callback_urls,
                  const GError               *reason,
                  G_GNUC_UNUSED gpointer      user_data)
{
        g_print ("NOTIFY failed: %s\n", reason->message);
}

static gboolean
timeout (gpointer user_data)
{
        gupnp_service_notify (GUPNP_SERVICE (user_data),
                              "SystemUpdateID",
                              G_TYPE_UINT,
                              27182818,
                              NULL);

        return FALSE;
}

int
main (int argc, char **argv)
{
        GError *error;
        GUPnPContext *context;
        GUPnPRootDevice *dev;
        GUPnPServiceInfo *content_dir;

        if (argc < 2) {
                g_printerr ("Usage: %s DESCRIPTION_FILE\n", argv[0]);

                return EXIT_FAILURE;
        }

        setlocale (LC_ALL, "");

        error = NULL;
        context = g_initable_new (GUPNP_TYPE_CONTEXT, NULL, &error, NULL);
        if (error) {
                g_printerr ("Error creating the GUPnP context: %s\n",
			    error->message);
                g_error_free (error);

                return EXIT_FAILURE;
        }

        g_print ("Running on port %d\n", gupnp_context_get_port (context));

        /* Create root device */
        dev = gupnp_root_device_new (context, "description.xml", ".", &error);
        if (error != NULL) {
                g_printerr ("Error creating the GUPnP root device: %s\n",
                            error->message);
                g_error_free (error);

                return EXIT_FAILURE;
        }

        /* Implement Browse action on ContentDirectory if available */
        content_dir = gupnp_device_info_get_service
                         (GUPNP_DEVICE_INFO (dev),
                          "urn:schemas-upnp-org:service:ContentDirectory:1");

        if (content_dir) {
                gupnp_service_signals_autoconnect (GUPNP_SERVICE (content_dir),
                                                   NULL,
                                                   &error);
                if (error) {
                        g_warning ("Failed to autoconnect signals: %s",
                                   error->message);

                        g_error_free (error);
                        error = NULL;
                }

                g_signal_connect (content_dir,
                                  "notify-failed",
                                  G_CALLBACK (notify_failed_cb),
                                  NULL);

                g_timeout_add (5000, timeout, content_dir);
        }

        /* Run */
        gupnp_root_device_set_available (dev, TRUE);

        main_loop = g_main_loop_new (NULL, FALSE);

        /* Hook the handler for SIGINT */
#ifndef G_OS_WIN32
        g_unix_signal_add (SIGINT, unix_signal_handler, NULL);
#else
        signal(SIGINT, interrupt_signal_handler);
#endif /* G_OS_WIN32 */

        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        if (content_dir)
                g_object_unref (content_dir);

        g_object_unref (dev);
        g_object_unref (context);

        return EXIT_SUCCESS;
}
