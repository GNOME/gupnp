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

#include <libgupnp/gupnp-root-device.h>
#include <libgupnp/gupnp-service.h>
#include <stdio.h>
#include <locale.h>
#include <string.h>
#include <signal.h>

GMainLoop *main_loop;

static void
interrupt_signal_handler (int signum)
{
        g_main_loop_quit (main_loop);
}

static void
browse_cb (GUPnPService       *service,
           GUPnPServiceAction *action,
           gpointer            user_data)
{
        GList *locales;
        char *filter;

        g_print ("The \"Browse\" action was invoked.\n");

        g_print ("\tLocales: ");
        locales = gupnp_service_action_get_locales (action);
        while (locales) {
                g_print ("%s", (char *) locales->data);
                g_free (locales->data);
                locales = g_list_delete_link (locales, locales);
                if (locales)
                        g_print (", ");
        }
        g_print ("\n");

        gupnp_service_action_get (action,
                                  "Filter", G_TYPE_STRING, &filter,
                                  NULL);
        g_print ("\tFilter:  %s\n", filter);
        g_free (filter);

        gupnp_service_action_set (action,
                                  "Result", G_TYPE_STRING, "Hello world",
                                  "NumberReturned", G_TYPE_INT, 0,
                                  "TotalMatches", G_TYPE_INT, 0,
                                  NULL);

        gupnp_service_action_return (action);
}

static void
query_cb (GUPnPService *service,
          const char   *variable_name,
          GValue       *value,
          gpointer      user_data)
{
        g_value_init (value, G_TYPE_UINT);
        g_value_set_uint (value, 31415927);
}

static void
notify_failed_cb (GUPnPService *service,
                  const GList  *callback_urls,
                  const GError *reason,
                  gpointer      user_data)
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
        xmlDoc *doc;
        GUPnPRootDevice *dev;
        GUPnPServiceInfo *content_dir;
        struct sigaction sig_action;

        if (argc < 2) {
                fprintf (stderr, "Usage: %s DESCRIPTION_FILE\n", argv[0]);

                return 1;
        }
        
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

        g_print ("Running on port %d\n", gupnp_context_get_port (context));

        /* Host current directory */
        gupnp_context_host_path (context, ".", "");

        /* Parse device description file */
        doc = xmlParseFile (argv[1]);

        /* Create root device */
        dev = gupnp_root_device_new (context,
                                     doc,
                                     "/description.xml");

        /* Free doc when root device is destroyed */
        g_object_weak_ref (G_OBJECT (dev), (GWeakNotify) xmlFreeDoc, doc);

        /* Implement Browse action on ContentDirectory if available */
        content_dir = gupnp_device_info_get_service
                         (GUPNP_DEVICE_INFO (dev),
                          "urn:schemas-upnp-org:service:ContentDirectory:1");

        if (content_dir) {
                g_signal_connect (content_dir,
                                  "action-invoked::Browse",
                                  G_CALLBACK (browse_cb),
                                  NULL);

                g_signal_connect (content_dir,
                                  "query-variable::SystemUpdateID",
                                  G_CALLBACK (query_cb),
                                  NULL);

                g_signal_connect (content_dir,
                                  "notify-failed",
                                  G_CALLBACK (notify_failed_cb),
                                  NULL);

                g_timeout_add (5000, timeout, content_dir);
        }

        /* Run */
        gupnp_root_device_set_available (dev, TRUE);

        main_loop = g_main_loop_new (NULL, FALSE);
        
        /* Hook the handler for SIGTERM */
        memset (&sig_action, 0, sizeof (sig_action));
        sig_action.sa_handler = interrupt_signal_handler;
        sigaction (SIGINT, &sig_action, NULL);

        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        if (content_dir)
                g_object_unref (content_dir);

        g_object_unref (dev);
        g_object_unref (context);

        return 0;
}
