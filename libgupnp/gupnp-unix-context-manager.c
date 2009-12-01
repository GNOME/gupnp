/*
 * Copyright (C) 2009 Nokia Corporation, all rights reserved.
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
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

/**
 * SECTION:gupnp-unix-context-manager
 * @short_description: Unix-specific implementation of #GUPnPContextManager.
 *
 */

#include <config.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <libsoup/soup-address.h>
#include <glib/gstdio.h>
#include <libgssdp/gssdp-error.h>

#include "gupnp-unix-context-manager.h"
#include "gupnp-context.h"

#ifdef USE_CONIC /* MAEMO */
#include <conic.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#endif /* MAEMO */

G_DEFINE_TYPE (GUPnPUnixContextManager,
               gupnp_unix_context_manager,
               GUPNP_TYPE_CONTEXT_MANAGER);

struct _GUPnPUnixContextManagerPrivate {
        GList *contexts; /* List of GUPnPContext instances */

        GSource *idle_context_creation_src;


#ifdef USE_CONIC /* MAEMO */
        ConIcConnection *conic;
        DBusConnection  *system_bus;
#endif /* MAEMO */
};

void
create_and_signal_context (GUPnPUnixContextManager *manager,
                           const char              *interface)
{
        GUPnPContext *context;
        GMainContext *main_context;
        guint port;

        GError *error;

        g_object_get (manager,
                      "main-context", &main_context,
                      "port", &port,
                      NULL);

        error = NULL;
        context = g_object_new (GUPNP_TYPE_CONTEXT,
                                "main-context", main_context,
                                "interface", interface,
                                "port", port,
                                "error", &error,
                                NULL);
        if (error != NULL) {
                if (!(error->domain == GSSDP_ERROR &&
                        error->code == GSSDP_ERROR_NO_IP_ADDRESS))
                        g_warning (
                            "Failed to create context for interface '%s': %s\n",
                            interface,
                            error->message);

                g_error_free (error);

                return;
        }

        g_signal_emit_by_name (manager,
                               "context-available",
                               context);

        manager->priv->contexts = g_list_append (manager->priv->contexts,
                                                 context);
}

/*
 * Create a context for all network interfaces that are up.
 */
static gboolean
create_contexts (gpointer data)
{
        GUPnPUnixContextManager *manager = (GUPnPUnixContextManager *) data;
        struct ifaddrs *ifa_list, *ifa;
        char *ret;
        GList *processed;

        manager->priv->idle_context_creation_src = NULL;

        if (manager->priv->contexts != NULL) {
               return FALSE;
        }

        ret = NULL;

        if (getifaddrs (&ifa_list) != 0) {
                g_warning ("Failed to retrieve list of network interfaces:%s\n",
                           strerror (errno));

                return FALSE;
        }

        processed = NULL;

        /* Create contexts for each up interface */
        for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next) {
                if (g_list_find_custom (processed,
                                        ifa->ifa_name,
                                        (GCompareFunc) strcmp) != NULL)
                        continue;

                if (ifa->ifa_flags & IFF_POINTOPOINT)
                        continue;

                if (ifa->ifa_flags & IFF_UP)
                        create_and_signal_context (manager, ifa->ifa_name);

                processed = g_list_append (processed, ifa->ifa_name);
        }

        g_list_free (processed);
        freeifaddrs (ifa_list);

        return FALSE;
}

static void
destroy_contexts (GUPnPUnixContextManager *manager)
{
        while (manager->priv->contexts) {
                GUPnPContext *context;

                context = GUPNP_CONTEXT (manager->priv->contexts->data);

                g_signal_emit_by_name (manager,
                                       "context-unavailable",
                                       context);
                g_object_unref (context);

                manager->priv->contexts = g_list_delete_link
                                        (manager->priv->contexts,
                                         manager->priv->contexts);
        }
}

#ifdef USE_CONIC /* MAEMO */
static void conic_event (ConIcConnection*      connection,
                         ConIcConnectionEvent* event,
                         gpointer              user_data)
{
        GUPnPUnixContextManager *manager;
        ConIcConnectionStatus status;
        const gchar* bearer;

        manager = GUPNP_UNIX_CONTEXT_MANAGER (user_data);
        status = con_ic_connection_event_get_status (event);

        bearer = con_ic_event_get_bearer_type (CON_IC_EVENT (event));
        if (bearer != NULL &&
            (strcmp (bearer, CON_IC_BEARER_WLAN_INFRA) == 0 ||
             strcmp (bearer, CON_IC_BEARER_WLAN_ADHOC) == 0)) {
                switch (status) {
                case CON_IC_STATUS_CONNECTED:
                        create_contexts (manager);
                        break;
                case CON_IC_STATUS_DISCONNECTED:
                        destroy_contexts (manager);
                        break;
                default:
                       /* Ignore other events */
                       break;
                }
        }
}
#else
static void
schedule_contexts_creation (GUPnPUnixContextManager *manager)
{
        GMainContext *main_context;

        manager->priv->idle_context_creation_src = NULL;

        g_object_get (manager,
                      "main-context", &main_context,
                      NULL);

        /* Create contexts in mainloop so that is happens after user has hooked
         * to the "context-available" signal.
         */
        manager->priv->idle_context_creation_src = g_idle_source_new ();
        g_source_attach (manager->priv->idle_context_creation_src,
            main_context);
        g_source_set_callback (manager->priv->idle_context_creation_src,
                               create_contexts,
                               manager,
                               NULL);
        g_source_unref (manager->priv->idle_context_creation_src);
}
#endif /* MAEMO */

static void
gupnp_unix_context_manager_init (GUPnPUnixContextManager *manager)
{
        manager->priv =
                G_TYPE_INSTANCE_GET_PRIVATE (manager,
                                             GUPNP_TYPE_UNIX_CONTEXT_MANAGER,
                                             GUPnPUnixContextManagerPrivate);
}

static void
gupnp_unix_context_manager_constructed (GObject *object)
{
        GObjectClass *parent_class;
        GUPnPUnixContextManager *manager;

        manager = GUPNP_UNIX_CONTEXT_MANAGER (object);

#ifdef USE_CONIC /* MAEMO */
        /* Initialize the system bus so libconic can receive ICD messages. */
        manager->priv->system_bus = dbus_bus_get (DBUS_BUS_SYSTEM, NULL);
        dbus_connection_setup_with_g_main (manager->priv->system_bus, NULL);

        manager->priv->conic = con_ic_connection_new ();
        g_assert (manager->priv->conic != NULL);

        g_signal_connect (manager->priv->conic,
                          "connection-event",
                          G_CALLBACK (conic_event),
                          manager);
        g_object_set (manager->priv->conic,
                      "automatic-connection-events",
                      TRUE,
                      NULL);
#else
        schedule_contexts_creation (manager);
#endif /* MAEMO */

        /* Chain-up */
        parent_class = G_OBJECT_CLASS (gupnp_unix_context_manager_parent_class);
        if (parent_class->constructed != NULL) {
                parent_class->constructed (object);
        }
}

static void
gupnp_unix_context_manager_dispose (GObject *object)
{
        GUPnPUnixContextManager *manager;
        GObjectClass *object_class;

        manager = GUPNP_UNIX_CONTEXT_MANAGER (object);

        destroy_contexts (manager);

#ifdef USE_CONIC /* MAEMO */
        if (manager->priv->conic != NULL) {
                g_object_unref (manager->priv->conic);
                manager->priv->conic = NULL;
        }

        if (manager->priv->system_bus != NULL) {
                dbus_connection_unref (manager->priv->system_bus);
                manager->priv->system_bus = NULL;
        }
#endif /* MAEMO */

        if (manager->priv->idle_context_creation_src) {
                g_source_destroy (manager->priv->idle_context_creation_src);
                manager->priv->idle_context_creation_src = NULL;
        }


        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_unix_context_manager_parent_class);
        object_class->dispose (object);
}

static void
gupnp_unix_context_manager_class_init (GUPnPUnixContextManagerClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->constructed  = gupnp_unix_context_manager_constructed;
        object_class->dispose      = gupnp_unix_context_manager_dispose;

        g_type_class_add_private (klass,
                                  sizeof (GUPnPUnixContextManagerPrivate));
}

