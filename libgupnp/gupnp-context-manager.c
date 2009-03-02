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
 * SECTION:gupnp-context-manager
 * @short_description: Manages #GUPnPContext objects.
 *
 * A Utility class that takes care of creation and destruction of
 * #GUPnPContext objects for all available network interfaces as they go up
 * (connect) and down (disconnect), respectively.
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

#include "gupnp-context-manager.h"
#include "gupnp-context.h"
#include "gupnp-marshal.h"

G_DEFINE_TYPE (GUPnPContextManager,
               gupnp_context_manager,
               G_TYPE_OBJECT);

struct _GUPnPContextManagerPrivate {
        GMainContext      *main_context;

        guint              port;
};

enum {
        PROP_0,
        PROP_MAIN_CONTEXT,
        PROP_PORT
};

enum {
        CONTEXT_AVAILABLE,
        CONTEXT_UNAVAILABLE,
        SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

static void
create_and_signal_context (GUPnPContextManager *manager,
                           const char          *host_ip)
{
        GUPnPContext *context;
        GError *error;

        if (host_ip == NULL)
                return;

        error = NULL;
        context = gupnp_context_new (manager->priv->main_context,
                                     host_ip,
                                     manager->priv->port,
                                     &error);
        if (error != NULL) {
                g_warning ("Failed to create context for host/IP '%s': %s\n",
                           host_ip,
                           error->message);

                g_error_free (error);

                return;
        }

        g_signal_emit (manager,
                       signals[CONTEXT_AVAILABLE],
                       0,
                       context);

        g_object_unref (context);
}

/* FIXME: We currently don't deal with IPv6 because libsoup doesn't seem to
 * like it and we get the screen full of warnings and errors. Please fix that
 * and uncomment the IPv6 handling here.
 */
static char *
get_host_ip_from_iface (struct ifaddrs *ifa)
{
        char ip[INET6_ADDRSTRLEN];
        const char *host_ip = NULL;
        struct sockaddr_in *s4;
        /*struct sockaddr_in6 *s6;*/

        if (ifa->ifa_addr == NULL)
                return NULL;

        if (!(ifa->ifa_flags & IFF_UP))
                return NULL;

        switch (ifa->ifa_addr->sa_family) {
                case AF_INET:
                        s4 = (struct sockaddr_in *) ifa->ifa_addr;
                        host_ip = inet_ntop (AF_INET,
                                        &s4->sin_addr, ip, sizeof (ip));
                        break;
                /*case AF_INET6:
                        s6 = (struct sockaddr_in6 *) ifa->ifa_addr;
                        host_ip = inet_ntop (AF_INET6,
                                        &s6->sin6_addr, ip, sizeof (ip));
                        break;*/
                default:
                        break; /* Unknown: ignore */
        }

        return g_strdup (host_ip);
}

/*
 * Create a context for all network interfaces that are up.
 */
static gboolean
create_contexts (gpointer data)
{
        GUPnPContextManager *manager = (GUPnPContextManager *) data;
        struct ifaddrs *ifa_list, *ifa;
        char *ret;

        ret = NULL;

        if (getifaddrs (&ifa_list) != 0) {
                g_warning ("Failed to retrieve list of network interfaces:%s\n",
                           strerror (errno));

                return FALSE;
        }

        for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next) {
                char *host_ip;

                host_ip = get_host_ip_from_iface (ifa);

                if (host_ip != NULL) {
                        create_and_signal_context (manager, host_ip);

                        g_free (host_ip);
                }
        }

        freeifaddrs (ifa_list);

        return FALSE;
}

static void
gupnp_context_manager_init (GUPnPContextManager *manager)
{
        manager->priv =
                G_TYPE_INSTANCE_GET_PRIVATE (manager,
                                             GUPNP_TYPE_CONTEXT_MANAGER,
                                             GUPnPContextManagerPrivate);
}

static GObject *
gupnp_context_manager_constructor (GType                  type,
                                   guint                  n_props,
                                   GObjectConstructParam *props)
{
	GObject *object;
	GObjectClass *parent_class;
	GUPnPContextManager *manager;

	parent_class = G_OBJECT_CLASS (gupnp_context_manager_parent_class);
	object = parent_class->constructor (type, n_props, props);

        manager = GUPNP_CONTEXT_MANAGER (object);

        /* Create contexts in mainloop so that is happens after user has hooked
         * to the "context-available" signal.
         */
        g_idle_add (create_contexts, manager);

	return object;
}

static void
gupnp_context_manager_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
        GUPnPContextManager *manager;

        manager = GUPNP_CONTEXT_MANAGER (object);

        switch (property_id) {
        case PROP_PORT:
                manager->priv->port = g_value_get_uint (value);
                break;
        case PROP_MAIN_CONTEXT:
                manager->priv->main_context = g_value_get_pointer (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_context_manager_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
        GUPnPContextManager *manager;

        manager = GUPNP_CONTEXT_MANAGER (object);

        switch (property_id) {
        case PROP_PORT:
                g_value_set_uint (value, manager->priv->port);
                break;
        case PROP_MAIN_CONTEXT:
                g_value_set_pointer (value, manager->priv->main_context);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_context_manager_dispose (GObject *object)
{
        GUPnPContextManager *manager;
        GObjectClass *object_class;

        manager = GUPNP_CONTEXT_MANAGER (object);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_context_manager_parent_class);
        object_class->dispose (object);
}

static void
gupnp_context_manager_finalize (GObject *object)
{
        GUPnPContextManager *manager;
        GObjectClass *object_class;

        manager = GUPNP_CONTEXT_MANAGER (object);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_context_manager_parent_class);
        object_class->finalize (object);
}

static void
gupnp_context_manager_class_init (GUPnPContextManagerClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->constructor  = gupnp_context_manager_constructor;
        object_class->set_property = gupnp_context_manager_set_property;
        object_class->get_property = gupnp_context_manager_get_property;
        object_class->dispose      = gupnp_context_manager_dispose;
        object_class->finalize     = gupnp_context_manager_finalize;

        g_type_class_add_private (klass, sizeof (GUPnPContextManagerPrivate));

        /**
         * GSSDPClient:main-context
         *
         * The #GMainContext to pass to created #GUPnPContext objects. Set to
         * NULL to use the default.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_MAIN_CONTEXT,
                 g_param_spec_pointer
                         ("main-context",
                          "Main context",
                          "GMainContext to pass to created GUPnPContext"
                          " objects",
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                          G_PARAM_STATIC_BLURB));

        /**
         * GUPnPContextManager:port
         *
         * @port: Port to create contexts for, or 0 if you don't care what
         * port is used by #GUPnPContext objects created by this object.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_PORT,
                 g_param_spec_uint ("port",
                                    "Port",
                                    "Port to create contexts for",
                                    0, G_MAXUINT, SOUP_ADDRESS_ANY_PORT,
                                    G_PARAM_READWRITE |
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_STATIC_NAME |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB));

        /**
         * GUPnPContextManager::context-available
         *
         * Signals the availability of new #GUPnPContext.
         *
         **/
        signals[CONTEXT_AVAILABLE] =
                g_signal_new ("context-available",
                              GUPNP_TYPE_CONTEXT_MANAGER,
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              gupnp_marshal_VOID__OBJECT,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_OBJECT);

        /**
         * GUPnPContextManager::context-unavailable
         *
         * Signals the unavailability of a #GUPnPContext.
         *
         **/
        signals[CONTEXT_UNAVAILABLE] =
                g_signal_new ("context-unavailable",
                              GUPNP_TYPE_CONTEXT_MANAGER,
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              gupnp_marshal_VOID__OBJECT,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_OBJECT);
}

/**
 * gupnp_context_manager_new
 * @port: Port to create contexts for, or 0 if you don't care what port is used.
 * @main_context: GMainContext to pass to created GUPnPContext objects.
 *
 * Create a new #GUPnPContextManager.
 *
 * Return value: A new #GUPnPContextManager object.
 **/
GUPnPContextManager *
gupnp_context_manager_new (GMainContext *main_context,
                           guint         port)
{
        return g_object_new (GUPNP_TYPE_CONTEXT_MANAGER,
                             "main-context", main_context,
                             "port", port,
                             NULL);
}

