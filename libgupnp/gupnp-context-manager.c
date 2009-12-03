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
#include <stdlib.h>
#include <stdio.h>
#include <libsoup/soup-address.h>
#include <glib/gstdio.h>

#include "gupnp.h"
#include "gupnp-marshal.h"

#include "gupnp-unix-context-manager.h"

G_DEFINE_TYPE (GUPnPContextManager,
               gupnp_context_manager,
               G_TYPE_OBJECT);

struct _GUPnPContextManagerPrivate {
        GMainContext      *main_context;

        guint              port;

        GUPnPContextManager *impl;

        GList *objects; /* control points and root devices */
};

enum {
        PROP_0,
        PROP_MAIN_CONTEXT,
        PROP_PORT,
        PROP_CONTEXT_MANAGER
};

enum {
        CONTEXT_AVAILABLE,
        CONTEXT_UNAVAILABLE,
        SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

static void
on_context_available (GUPnPContextManager *impl,
                      GUPnPContext        *context,
                      gpointer            *user_data)
{
        GUPnPContextManager *manager = GUPNP_CONTEXT_MANAGER (user_data);

        /* Just proxy the signal */
        g_signal_emit (manager,
                       signals[CONTEXT_AVAILABLE],
                       0,
                       context);
}

static void
on_context_unavailable (GUPnPContextManager *impl,
                        GUPnPContext        *context,
                        gpointer            *user_data)
{
        GUPnPContextManager *manager;
        GList *l;

        manager = GUPNP_CONTEXT_MANAGER (user_data);

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

        /* Just proxy the signal */
        g_signal_emit (manager,
                       signals[CONTEXT_UNAVAILABLE],
                       0,
                       context);
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
                priv->main_context = g_value_get_pointer (value);
                break;
        case PROP_CONTEXT_MANAGER:
                priv->impl = g_value_get_object (value);
                if (priv->impl != NULL) {
                        priv->impl = g_object_ref (priv->impl);

                        g_signal_connect (priv->impl,
                                          "context-available",
                                          G_CALLBACK (on_context_available),
                                          manager);
                        g_signal_connect (priv->impl,
                                          "context-unavailable",
                                          G_CALLBACK (on_context_unavailable),
                                          manager);
                }
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
        case PROP_CONTEXT_MANAGER:
                g_value_set_object (value, manager->priv->impl);
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

        if (manager->priv->impl != NULL) {
                g_object_unref (manager->priv->impl);
                manager->priv->impl = NULL;
        }

        g_list_foreach (manager->priv->objects, (GFunc) g_object_unref, NULL);
        g_list_free (manager->priv->objects);
        manager->priv->objects = NULL;

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
         * GUPnPContextManager:context-manager
         *
         * The actual GUPnPContextManager implementation used. This is an
         * internal property and therefore Application developer should just
         * ignore it.
         *
         **/
        g_object_class_install_property
                (object_class,
                 PROP_CONTEXT_MANAGER,
                 g_param_spec_object ("context-manager",
                                      "ContextManager",
                                      "ContextManager implemention",
                                      GUPNP_TYPE_CONTEXT_MANAGER,
                                      G_PARAM_WRITABLE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_PRIVATE |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPContextManager::context-available
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
         * GUPnPContextManager::context-unavailable
         * @context_manager: The #GUPnPContextManager that received the signal
         * @context: The now unavailable #GUPnPContext
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
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE,
                              1,
                              GUPNP_TYPE_CONTEXT);
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
        GUPnPContextManager *manager;
        GUPnPContextManager *impl;
        GType impl_type = G_TYPE_INVALID;

#ifdef USE_NETWORK_MANAGER
#include "gupnp-network-manager.h"

        if (gupnp_network_manager_is_available (main_context))
                impl_type = GUPNP_TYPE_NETWORK_MANAGER;
#endif

        if (impl_type == G_TYPE_INVALID)
                impl_type = GUPNP_TYPE_UNIX_CONTEXT_MANAGER;

        impl = g_object_new (impl_type,
                             "main-context", main_context,
                             "port", port,
                             NULL);

        manager = g_object_new (GUPNP_TYPE_CONTEXT_MANAGER,
                                "main-context", main_context,
                                "port", port,
                                "context-manager", impl,
                                NULL);
        g_object_unref (impl);

        return manager;
}

/**
 * gupnp_context_manager_manage_control_point
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
 * gupnp_context_manager_manage_root_device
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

