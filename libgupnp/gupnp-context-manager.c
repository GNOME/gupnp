/*
 * Copyright (C) 2009 Nokia Corporation.
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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
#include <stdlib.h>
#include <stdio.h>
#include <libsoup/soup-address.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "gupnp.h"
#include "gupnp-marshal.h"

#include "gupnp-unix-context-manager.h"

G_DEFINE_ABSTRACT_TYPE (GUPnPContextManager,
                        gupnp_context_manager,
                        G_TYPE_OBJECT);

struct _GUPnPContextManagerPrivate {
        guint              port;

        GUPnPContextManager *impl;

        GList *objects; /* control points and root devices */
};

enum {
        PROP_0,
        PROP_MAIN_CONTEXT,
        PROP_PORT,
};

enum {
        CONTEXT_AVAILABLE,
        CONTEXT_UNAVAILABLE,
        SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

static void
on_context_unavailable (GUPnPContextManager    *manager,
                        GUPnPContext           *context,
                        G_GNUC_UNUSED gpointer *user_data)
{
        GList *l;

        /* Make sure we don't send anything on now unavailable network */
        g_object_set (context, "active", FALSE, NULL);

        /* Unref all associated objects */
        l = manager->priv->objects;

        while (l) {
                GUPnPContext *obj_context = NULL;

                if (GUPNP_IS_CONTROL_POINT (l->data)) {
                        GUPnPControlPoint *cp;

                        cp = GUPNP_CONTROL_POINT (l->data);
                        obj_context = gupnp_control_point_get_context (cp);
                } else if (GUPNP_IS_ROOT_DEVICE (l->data)) {
                        GUPnPDeviceInfo *info;

                        info = GUPNP_DEVICE_INFO (l->data);
                        obj_context = gupnp_device_info_get_context (info);
                } else {
                        g_assert_not_reached ();
                }

                if (context == obj_context) {
                        GList *next = l->next;

                        g_object_unref (l->data);

                        manager->priv->objects =
                                g_list_delete_link (manager->priv->objects, l);
                        l = next;
                } else {
                        l = l->next;
                }
        }
}

static void
gupnp_context_manager_init (GUPnPContextManager *manager)
{
        manager->priv =
                G_TYPE_INSTANCE_GET_PRIVATE (manager,
                                             GUPNP_TYPE_CONTEXT_MANAGER,
                                             GUPnPContextManagerPrivate);
}

static void
gupnp_context_manager_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
        GUPnPContextManager *manager;
        GUPnPContextManagerPrivate *priv;

        manager = GUPNP_CONTEXT_MANAGER (object);
        priv = manager->priv;

        switch (property_id) {
        case PROP_PORT:
                priv->port = g_value_get_uint (value);
                break;
        case PROP_MAIN_CONTEXT:
                if (g_value_get_pointer (value) != NULL)
                        g_warning ("GUPnPContextManager:main-context is "
                                   "deprecated. Use "
                                   "g_main_context_push_thread_default()"
                                   "instead.");
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
                g_warning ("GUPnPContextManager:main-context is deprecated. "
                           "Use g_main_context_push_thread_default()"
                           "instead.");
                g_value_set_pointer (value,
                                     g_main_context_get_thread_default ());
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

        g_list_foreach (manager->priv->objects, (GFunc) g_object_unref, NULL);
        g_list_free (manager->priv->objects);
        manager->priv->objects = NULL;

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_context_manager_parent_class);
        object_class->dispose (object);
}

static void
gupnp_context_manager_class_init (GUPnPContextManagerClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_context_manager_set_property;
        object_class->get_property = gupnp_context_manager_get_property;
        object_class->dispose      = gupnp_context_manager_dispose;

        g_type_class_add_private (klass, sizeof (GUPnPContextManagerPrivate));

        /**
         * GSSDPClient:main-context:
         *
         * The #GMainContext to pass to created #GUPnPContext objects. Set to
         * NULL to use the default.
         *
         * Deprecated: 0.17.2: Use g_main_context_push_thread_default()
         *             instead.
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
         * GUPnPContextManager:port:
         *
         * Port the contexts listen on, or 0 if you don't care what
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
         * GUPnPContextManager::context-available:
         * @context_manager: The #GUPnPContextManager that received the signal
         * @context: The now available #GUPnPContext
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
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE,
                              1,
                              GUPNP_TYPE_CONTEXT);

        /**
         * GUPnPContextManager::context-unavailable:
         * @context_manager: The #GUPnPContextManager that received the signal
         * @context: The now unavailable #GUPnPContext
         *
         * Signals the unavailability of a #GUPnPContext.
         *
         **/
        signals[CONTEXT_UNAVAILABLE] =
                g_signal_new_class_handler
                                        ("context-unavailable",
                                         GUPNP_TYPE_CONTEXT_MANAGER,
                                         G_SIGNAL_RUN_FIRST,
                                         G_CALLBACK (on_context_unavailable),
                                         NULL, NULL,
                                         g_cclosure_marshal_VOID__OBJECT,
                                         G_TYPE_NONE,
                                         1,
                                         GUPNP_TYPE_CONTEXT);
}

/**
 * gupnp_context_manager_new:
 * @main_context: (allow-none): Deprecated: 0.17.2: %NULL. If you want to use
 *                a different main context use
 *                g_main_context_push_thread_default() instead.
 * @port: Port to create contexts for, or 0 if you don't care what port is used.
 *
 * Same as gupnp_context_manager_create().
 *
 * Returns: (transfer full): A new #GUPnPContextManager object.
 * Deprecated: 0.17.2: Use gupnp_context_manager_create().
 **/
GUPnPContextManager *
gupnp_context_manager_new (GMainContext *main_context,
                           guint         port)
{
    if (main_context)
            g_warning ("gupnp_context_manager_new::main_context is"
                       " deprecated. Use "
                       " g_main_context_push_thread_default() instead");

    return gupnp_context_manager_create (port);
}

#ifdef HAVE_LINUX_RTNETLINK_H
#include "gupnp-linux-context-manager.h"
#endif

/**
 * gupnp_context_manager_create:
 * @port: Port to create contexts for, or 0 if you don't care what port is used.
 *
 * Factory-method to create a new #GUPnPContextManager. The final type of the
 * #GUPnPContextManager depends on the compile-time selection or - in case of
 * NetworkManager - on its availability during runtime. If it is not available,
 * the implementation falls back to the basic Unix context manager instead.
 *
 * Returns: (transfer full): A new #GUPnPContextManager object.
 **/
GUPnPContextManager *
gupnp_context_manager_create (guint port)
{
#if defined(USE_NETWORK_MANAGER) || defined (USE_CONNMAN)
        GDBusConnection *system_bus;
#endif
        GUPnPContextManager *impl;
        GType impl_type = G_TYPE_INVALID;
#ifdef G_OS_WIN32
#include "gupnp-windows-context-manager.h"

        impl_type = GUPNP_TYPE_WINDOWS_CONTEXT_MANAGER;
#else
#ifdef USE_NETWORK_MANAGER
#include "gupnp-network-manager.h"
        system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

        if (gupnp_network_manager_is_available ())
                impl_type = GUPNP_TYPE_NETWORK_MANAGER;
#elif USE_CONNMAN
#include "gupnp-connman-manager.h"
        system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

       if (gupnp_connman_manager_is_available ())
                impl_type = GUPNP_TYPE_CONNMAN_MANAGER;
#endif

        if (impl_type == G_TYPE_INVALID) {
            /* Either user requested us to use the Linux CM explicitly or we
             * are using one of the DBus managers but it's not available, so we
             * fall-back to it. */
#if defined (USE_NETLINK) || defined (HAVE_LINUX_RTNETLINK_H)
                if (gupnp_linux_context_manager_is_available ())
                        impl_type = GUPNP_TYPE_LINUX_CONTEXT_MANAGER;
                else
                    impl_type = GUPNP_TYPE_UNIX_CONTEXT_MANAGER;
#else
                impl_type = GUPNP_TYPE_UNIX_CONTEXT_MANAGER;
#endif
        }
#endif /* G_OS_WIN32 */
        impl = g_object_new (impl_type,
                             "port", port,
                             NULL);

#if defined(USE_NETWORK_MANAGER) || defined(USE_CONNMAN)
        g_object_unref (system_bus);
#endif
        return impl;
}

/**
 * gupnp_context_manager_rescan_control_points:
 * @manager: A #GUPnPContextManager
 *
 * This function starts a rescan on every control point managed by @manager.
 * Only the active control points send discovery messages.
 * This function should be called when servers are suspected to have
 * disappeared.
 **/
void
gupnp_context_manager_rescan_control_points (GUPnPContextManager *manager)
{
        GList *l;

        g_return_if_fail (GUPNP_IS_CONTEXT_MANAGER (manager));

        l = manager->priv->objects;

        while (l) {
                if (GUPNP_IS_CONTROL_POINT (l->data)) {
                        GSSDPResourceBrowser *browser =
                                GSSDP_RESOURCE_BROWSER (l->data);
                        gssdp_resource_browser_rescan (browser);
                }

                l = l->next;
        }
}

/**
 * gupnp_context_manager_manage_control_point:
 * @manager: A #GUPnPContextManager
 * @control_point: The #GUPnPControlPoint to be taken care of
 *
 * By calling this function, you are asking @manager to keep a reference to
 * @control_point until it's associated #GUPnPContext is no longer available.
 * You usually want to call this function from
 * #GUPnPContextManager::context-available handler after you create a
 * #GUPnPControlPoint object for the newly available context.
 **/
void
gupnp_context_manager_manage_control_point (GUPnPContextManager *manager,
                                            GUPnPControlPoint   *control_point)
{
        g_return_if_fail (GUPNP_IS_CONTEXT_MANAGER (manager));
        g_return_if_fail (GUPNP_IS_CONTROL_POINT (control_point));

        manager->priv->objects = g_list_append (manager->priv->objects,
                                                g_object_ref (control_point));
}

/**
 * gupnp_context_manager_manage_root_device:
 * @manager: A #GUPnPContextManager
 * @root_device: The #GUPnPRootDevice to be taken care of
 *
 * By calling this function, you are asking @manager to keep a reference to
 * @root_device when it's associated #GUPnPContext is no longer available. You
 * usually want to call this function from
 * #GUPnPContextManager::context-available handler after you create a
 * #GUPnPRootDevice object for the newly available context.
 **/
void
gupnp_context_manager_manage_root_device (GUPnPContextManager *manager,
                                          GUPnPRootDevice     *root_device)
{
        g_return_if_fail (GUPNP_IS_CONTEXT_MANAGER (manager));
        g_return_if_fail (GUPNP_IS_ROOT_DEVICE (root_device));

        manager->priv->objects = g_list_append (manager->priv->objects,
                                                g_object_ref (root_device));
}

/**
 * gupnp_context_manager_get_port:
 * @manager: A #GUPnPContextManager
 *
 * Get the network port associated with this context manager.
 * Returns: The network port asssociated with this context manager.
 */
guint
gupnp_context_manager_get_port (GUPnPContextManager *manager)
{
        g_return_val_if_fail (GUPNP_IS_CONTEXT_MANAGER (manager), 0);

        return manager->priv->port;
}
