/*
 * Copyright (C) 2009 Nokia Corporation.
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 * Copyright (C) 2013 Intel Corporation.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
 *         Ludovic Ferrandis <ludovic.ferrandis@intel.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#define G_LOG_DOMAIN "gupnp-context-manager"

#include <config.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <libgssdp/gssdp-enums.h>

#include "gupnp.h"

#ifdef HAVE_IFADDRS_H
#include "gupnp-unix-context-manager.h"
#endif

#ifdef G_OS_WIN32
#include "gupnp-windows-context-manager.h"
#elif defined(USE_NETWORK_MANAGER)
#include "gupnp-network-manager.h"
#elif defined(USE_CONNMAN)
#include "gupnp-connman-manager.h"
#elif defined(USE_NETLINK)
#include "gupnp-linux-context-manager.h"
#endif

struct _GUPnPContextManagerPrivate {
        guint              port;
        GSocketFamily      family;
        GSSDPUDAVersion    uda_version;
        gint32             boot_id;

        GUPnPContextManager *impl;

        GPtrArray *control_points;
        GPtrArray *root_devices;

        GList *filtered; /* Filtered contexts */

        // map of context -> managed objects, doubles also as a set of seen contexts
        GHashTable *contexts;

        GUPnPContextFilter *context_filter;
        gboolean syntesized_internal;
};
typedef struct _GUPnPContextManagerPrivate GUPnPContextManagerPrivate;

/**
 * GUPnPContextManager:
 *
 * A manager for [class@GUPnP.Context] instances.
 *
 * This utility class that takes care of dynamic creation and destruction of
 * #GUPnPContext objects for all available network interfaces as they go up
 * (connect) and down (disconnect), respectively.
 *
 * The final implementation depends either on the underlying operating system
 * or can configured during compile time.
 *
 * It also provides a simple filtering facility if required. See [method@GUPnP.ContextManager.get_context_filter] and
 * [class@GUPnP.ContextFilter] for details.
 *
 * Since: 0.14.0
 */
G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GUPnPContextManager,
                                     gupnp_context_manager,
                                     G_TYPE_OBJECT)

enum
{
        PROP_0,
        PROP_PORT,
        PROP_SOCKET_FAMILY,
        PROP_UDA_VERSION,
        PROP_CONTEXT_FILTER
};

enum {
        CONTEXT_AVAILABLE,
        CONTEXT_UNAVAILABLE,
        SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

static void
handle_update (GUPnPRootDevice *root_device, gpointer user_data)
{
        gint32 *output = user_data;
        gint32 boot_id;
        GSSDPResourceGroup *group = NULL;
        GSSDPClient *client = NULL;

        group = gupnp_root_device_get_ssdp_resource_group (root_device);
        client = gssdp_resource_group_get_client (group);
        g_object_get (G_OBJECT (client), "boot-id", &boot_id, NULL);
        gssdp_resource_group_update (group, ++boot_id);

        *output = boot_id;
}

static gboolean
context_filtered (GUPnPContextFilter *filter, GUPnPContext *context)
{
        return !gupnp_context_filter_is_empty (filter) &&
               gupnp_context_filter_get_enabled (filter) &&
               !gupnp_context_filter_check_context (filter, context);
}

static GPtrArray *
ensure_context (GHashTable *contexts, GUPnPContext *context)
{
        GPtrArray *objects = g_hash_table_lookup (contexts, context);
        if (objects == NULL) {
                objects = g_ptr_array_new_with_free_func (g_object_unref);
                g_hash_table_insert (contexts, g_object_ref (context), objects);
        }

        return objects;
}

static void
do_boot_id_update_for_root_devices (GUPnPContextManager *manager)
{
        GUPnPContextManagerPrivate *priv;
        priv = gupnp_context_manager_get_instance_private (manager);

        // Nothing to do for UDA 1.0. It does not have the boot-id
        // concept
        if (priv->uda_version == GSSDP_UDA_VERSION_1_0) {
                return;
        }

        gint32 boot_id = -1;
        g_ptr_array_foreach (priv->root_devices,
                             (GFunc) handle_update,
                             &boot_id);
        if (boot_id > 1) {
                priv->boot_id = boot_id;
        }
}

static void
on_context_available (GUPnPContextManager    *manager,
                      GUPnPContext           *context,
                      G_GNUC_UNUSED gpointer *user_data)
{
        GUPnPContextManagerPrivate *priv;
        priv = gupnp_context_manager_get_instance_private (manager);

        if (priv->syntesized_internal)
                return;

        ensure_context (priv->contexts, context);

        if (context_filtered (priv->context_filter, context)) {
                /* If the context doesn't match, block the notification
                 * and disable the context */
                g_signal_stop_emission_by_name (manager, "context-available");

                /* Make sure we don't send anything on now blocked network */
                g_object_set (context, "active", FALSE, NULL);

                /* Save it in case we need to re-enable it */
                priv->filtered =
                        g_list_prepend (priv->filtered, g_object_ref (context));

                return;
        }

        do_boot_id_update_for_root_devices (manager);

        /* The new client gets the current boot-id */
        gssdp_client_set_boot_id (GSSDP_CLIENT (context), priv->boot_id);
}

static void
on_context_unavailable (GUPnPContextManager    *manager,
                        GUPnPContext           *context,
                        G_GNUC_UNUSED gpointer *user_data)
{
        GUPnPContextManagerPrivate *priv;
        priv = gupnp_context_manager_get_instance_private (manager);

        if (priv->syntesized_internal)
                return;

        /* Make sure we don't send anything on now unavailable network */
        g_object_set (context, "active", FALSE, NULL);

        GList *ctx = g_list_find (priv->filtered, context);
        if (ctx != NULL) {
                g_signal_stop_emission_by_name (manager, "context-unavailable");

                priv->filtered = g_list_remove_link (priv->filtered, ctx);
                g_object_unref (ctx->data);
                g_list_free (ctx);
        }

        g_hash_table_remove (priv->contexts, context);

        // The context was not announced, we can just silenty leave after removing
        // it from the list of all contexts.
        if (ctx != NULL)
                return;

        // Handle the boot-id changes because of changed contexts
        do_boot_id_update_for_root_devices (manager);
}

static void
on_context_filter_change_cb (GUPnPContextFilter *context_filter,
                             GParamSpec *pspec,
                             gpointer user_data)
{
        GUPnPContextManager *manager = GUPNP_CONTEXT_MANAGER (user_data);
        GUPnPContextManagerPrivate *priv;
        gboolean enabled;

        priv = gupnp_context_manager_get_instance_private (manager);
        enabled = gupnp_context_filter_get_enabled (context_filter);

        if (!enabled) {
                // Don't care. Nothing to do
                return;
        }

        GHashTableIter iter;
        g_hash_table_iter_init (&iter, priv->contexts);
        GUPnPContext *key;
        while (g_hash_table_iter_next (&iter, (gpointer *) &key, NULL)) {
                GList *filtered = g_list_find (priv->filtered, key);

                if (context_filtered (context_filter, key)) {
                        // This context was already filtered. Nothing to
                        // do
                        if (filtered != NULL) {
                                continue;
                        }

                        // This context is now filtered
                        priv->filtered = g_list_prepend (priv->filtered, key);

                        // Drop all references to the objects we manage
                        g_hash_table_iter_replace (
                                &iter,
                                g_ptr_array_new_with_free_func (
                                        g_object_unref));

                        // Synthesize unavailable signal
                        priv->syntesized_internal = TRUE;
                        g_object_set (G_OBJECT (key), "active", FALSE, NULL);
                        g_signal_emit (manager,
                                       signals[CONTEXT_UNAVAILABLE],
                                       0,
                                       key);
                        priv->syntesized_internal = FALSE;

                } else {
                        // The context is not filtered
                        if (filtered == NULL) {
                                // The context wasn't filtered before
                                // -> nothing to do
                                continue;
                        }

                        // Drop the reference from the filter list
                        priv->filtered =
                                g_list_delete_link (priv->filtered, filtered);

                        g_object_set (G_OBJECT (key), "active", TRUE, NULL);

                        priv->syntesized_internal = TRUE;
                        g_signal_emit (manager,
                                       signals[CONTEXT_AVAILABLE],
                                       0,
                                       key);
                        priv->syntesized_internal = FALSE;
                }
        }
}

static void
on_context_filter_enabled_cb (GUPnPContextFilter *context_filter,
                              GParamSpec *pspec,
                              gpointer user_data)
{
        GUPnPContextManager *manager = GUPNP_CONTEXT_MANAGER (user_data);
        GUPnPContextManagerPrivate *priv;

        gboolean enabled;
        gboolean is_empty;

        enabled = gupnp_context_filter_get_enabled (context_filter);
        is_empty = gupnp_context_filter_is_empty (context_filter);
        priv = gupnp_context_manager_get_instance_private (manager);

        // we have switched from enabled to disabled. Flush the filtered
        // context queue
        if (!enabled) {
                while (priv->filtered != NULL) {
                        // This is ok since the filter is disabled. The
                        // callback will not modify the list as well

                        // Do not block our handler, that is what we want here
                        g_object_set (G_OBJECT (priv->filtered->data),
                                      "active",
                                      TRUE,
                                      NULL);

                        g_signal_emit (manager,
                                       signals[CONTEXT_AVAILABLE],
                                       0,
                                       priv->filtered->data);

                        priv->filtered = g_list_delete_link (priv->filtered,
                                                             priv->filtered);
                }

                return;
        }

        // We have switched from disabled to enabled, but the filter is empty.
        // Nothing to do.
        if (enabled && is_empty) {
                return;
        }

        GHashTableIter iter;
        g_hash_table_iter_init (&iter, priv->contexts);
        GUPnPContext *key;
        while (g_hash_table_iter_next (&iter, (gpointer *) &key, NULL)) {
                if (context_filtered (context_filter, key)) {
                        // This context is now filtered
                        priv->filtered = g_list_prepend (priv->filtered, key);

                        // Drop all references to the objects we manage
                        g_hash_table_iter_replace (
                                &iter,
                                g_ptr_array_new_with_free_func (
                                        g_object_unref));

                        // Synthesize unavailable signal
                        priv->syntesized_internal = TRUE;
                        g_object_set (G_OBJECT (key), "active", FALSE, NULL);
                        g_signal_emit (manager,
                                       signals[CONTEXT_UNAVAILABLE],
                                       0,
                                       key);
                        priv->syntesized_internal = FALSE;
                }
        }
}

static void
gupnp_context_manager_init (GUPnPContextManager *manager)
{
        GUPnPContextManagerPrivate *priv;

        priv = gupnp_context_manager_get_instance_private (manager);

        priv->context_filter = g_object_new (GUPNP_TYPE_CONTEXT_FILTER, NULL);
        priv->contexts =
                g_hash_table_new_full (g_direct_hash,
                                       g_direct_equal,
                                       g_object_unref,
                                       (GDestroyNotify) g_ptr_array_unref);
        priv->control_points = g_ptr_array_new ();
        priv->root_devices = g_ptr_array_new ();

        g_signal_connect_after (priv->context_filter,
                                "notify::entries",
                                G_CALLBACK (on_context_filter_change_cb),
                                manager);

        g_signal_connect_after (priv->context_filter,
                                "notify::enabled",
                                G_CALLBACK (on_context_filter_enabled_cb),
                                manager);
        priv->boot_id = time(NULL);
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
        priv = gupnp_context_manager_get_instance_private (manager);

        switch (property_id) {
        case PROP_PORT:
                priv->port = g_value_get_uint (value);
                break;
        case PROP_SOCKET_FAMILY:
                priv->family = g_value_get_enum (value);
                break;
        case PROP_UDA_VERSION:
                priv->uda_version = g_value_get_enum (value);
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
        GUPnPContextManagerPrivate *priv;

        manager = GUPNP_CONTEXT_MANAGER (object);
        priv = gupnp_context_manager_get_instance_private (manager);

        switch (property_id) {
        case PROP_PORT:
                g_value_set_uint (value, priv->port);
                break;
        case PROP_SOCKET_FAMILY:
                g_value_set_enum (value, priv->family);
                break;
        case PROP_UDA_VERSION:
                g_value_set_enum (value, priv->uda_version);
                break;
        case PROP_CONTEXT_FILTER:
                g_value_set_object (value, priv->context_filter);
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
        GUPnPContextFilter *filter;
        GObjectClass *object_class;
        GUPnPContextManagerPrivate *priv;

        manager = GUPNP_CONTEXT_MANAGER (object);
        priv = gupnp_context_manager_get_instance_private (manager);

        filter = priv->context_filter;

        g_signal_handlers_disconnect_by_func (filter,
                                              on_context_filter_enabled_cb,
                                              manager);

        g_signal_handlers_disconnect_by_func (filter,
                                              on_context_filter_change_cb,
                                              NULL);

        g_hash_table_destroy (priv->contexts);

        g_ptr_array_free (priv->control_points, TRUE);
        g_ptr_array_free (priv->root_devices, TRUE);

        g_list_free_full (priv->filtered, g_object_unref);
        priv->filtered = NULL;

        g_clear_object (&filter);

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

        /**
         * GUPnPContextManager:port:(attributes org.gtk.Property.get=gupnp_context_manager_get_port)
         *
         * Port the contexts listen on, or 0 if you don't care what
         * port is used by #GUPnPContext objects created by this object.
         **/
        g_object_class_install_property (
                object_class,
                PROP_PORT,
                g_param_spec_uint ("port",
                                   "Port",
                                   "Port to create contexts for",
                                   0,
                                   G_MAXUINT,
                                   0,
                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                           G_PARAM_STATIC_NAME |
                                           G_PARAM_STATIC_NICK |
                                           G_PARAM_STATIC_BLURB));
        /**
         * GUPnPContextManager:family:(attributes org.gtk.Property.get=gupnp_context_manager_get_socket_family)
         *
         * The socket family to create contexts for. Use %G_SOCKET_FAMILY_INVALID
         * for any or %G_SOCKET_FAMILY_IPV4 for IPv4 contexts or
         * %G_SOCKET_FAMILY_IPV6 for IPv6 contexts
         *
         * Since: 1.2.0
         **/
        g_object_class_install_property
                (object_class,
                 PROP_SOCKET_FAMILY,
                 g_param_spec_enum ("family",
                                    "Address family",
                                    "Address family to create contexts for",
                                    G_TYPE_SOCKET_FAMILY,
                                    G_SOCKET_FAMILY_INVALID,
                                    G_PARAM_READWRITE |
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_STATIC_STRINGS));

        /**
         * GUPnPContextManager:uda-version:(attributes org.gtk.Property.get=gupnp_context_manager_get_uda_version)
         *
         * The UDA version the contexts will support. Use %GSSDP_UDA_VERSION_UNSPECIFIED
         * for using the default UDA version.
         *
         * Since: 1.2.0
         **/
        g_object_class_install_property
                (object_class,
                 PROP_UDA_VERSION,
                 g_param_spec_enum ("uda-version",
                                    "UDA version",
                                    "UDA version the created contexts will implement",
                                    GSSDP_TYPE_UDA_VERSION,
                                    GSSDP_UDA_VERSION_UNSPECIFIED,
                                    G_PARAM_READWRITE |
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_STATIC_STRINGS));

        /**
         * GUPnPContextManager:context-filter:(attributes org.gtk.Property.get=gupnp_context_manager_get_context_filter)
         *
         * The context filter to use.
         **/
        g_object_class_install_property (
                object_class,
                PROP_CONTEXT_FILTER,
                g_param_spec_object ("context-filter",
                                     "Context Filter",
                                     "The Context Filter to use",
                                     GUPNP_TYPE_CONTEXT_FILTER,
                                     G_PARAM_READABLE | G_PARAM_STATIC_NAME |
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
                g_signal_new_class_handler ("context-available",
                                            GUPNP_TYPE_CONTEXT_MANAGER,
                                            G_SIGNAL_RUN_FIRST,
                                            G_CALLBACK (on_context_available),
                                            NULL, NULL, NULL,
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
                                         NULL, NULL, NULL,
                                         G_TYPE_NONE,
                                         1,
                                         GUPNP_TYPE_CONTEXT);
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
 * NetworkManager - on its availability during run-time. If it is not available,
 * the implementation falls back to the basic Unix context manager instead.
 *
 * Equivalent to calling #gupnp_context_manager_create_full (%GSSDP_UDA_VERSION_1_0, %G_SOCKET_FAMILY_IPV4, port);
 *
 * Returns: (transfer full): A new #GUPnPContextManager object.
 *
 * Since: 0.18.0
 **/
GUPnPContextManager *
gupnp_context_manager_create (guint port)
{
        return gupnp_context_manager_create_full (GSSDP_UDA_VERSION_1_0,
                                                  G_SOCKET_FAMILY_IPV4,
                                                  port);
}

/**
 * gupnp_context_manager_create_full:
 * @uda_version: #GSSDPUDAVersion the created contexts should implement
 * (UDA 1.0 or 1.1). For %GSSDP_UDA_VERSION_UNSPECIFIED for default.
 * @family: #GSocketFamily to create the context for
 * @port: Port to create contexts for, or 0 if you don't care what port is used.
 *
 * Factory-method to create a new #GUPnPContextManager. The final type of the
 * #GUPnPContextManager depends on the compile-time selection or - in case of
 * NetworkManager - on its availability during run-time. If it is not available,
 * the implementation falls back to the basic Unix context manager instead.
 *
 * Returns: (transfer full): A new #GUPnPContextManager object.
 *
 * Since: 1.2.0
 **/
GUPnPContextManager *
gupnp_context_manager_create_full (GSSDPUDAVersion uda_version, GSocketFamily family, guint port)
{
#if defined(USE_NETWORK_MANAGER) || defined (USE_CONNMAN)
        GDBusConnection *system_bus;
#endif
        GUPnPContextManager *impl;
        GType impl_type = G_TYPE_INVALID;

#ifdef G_OS_WIN32
        impl_type = GUPNP_TYPE_WINDOWS_CONTEXT_MANAGER;
#else
#if defined(USE_NETWORK_MANAGER)
        system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

        if (system_bus != NULL && gupnp_network_manager_is_available ())
                impl_type = GUPNP_TYPE_NETWORK_MANAGER;
#elif defined(USE_CONNMAN)
        system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

       if (system_bus != NULL && gupnp_connman_manager_is_available ())
                impl_type = GUPNP_TYPE_CONNMAN_MANAGER;
#endif

        if (impl_type == G_TYPE_INVALID) {
            /* Either user requested us to use the Linux CM explicitly or we
             * are using one of the DBus managers but it's not available, so we
             * fall-back to it. */
#if defined (USE_NETLINK) || defined (HAVE_LINUX_RTNETLINK_H)
#if defined (HAVE_IFADDRS_H)
                if (gupnp_linux_context_manager_is_available ())
                        impl_type = GUPNP_TYPE_LINUX_CONTEXT_MANAGER;
                else
                    impl_type = GUPNP_TYPE_UNIX_CONTEXT_MANAGER;
#else
                impl_type = GUPNP_TYPE_LINUX_CONTEXT_MANAGER;

#endif
#elif defined (HAVE_IFADDRS_H)
                impl_type = GUPNP_TYPE_UNIX_CONTEXT_MANAGER;
#else
#error No context manager defined
#endif
        }
#endif /* G_OS_WIN32 */

        g_debug ("Using context manager implementation %s, family: %d, UDA: "
                 "%d, port: %u",
                 g_type_name (impl_type),
                 family,
                 uda_version,
                 port);
        impl = GUPNP_CONTEXT_MANAGER (g_object_new (impl_type,
                                                    "family", family,
                                                    "uda-version", uda_version,
                                                    "port", port,
                                                    NULL));

#if defined(USE_NETWORK_MANAGER) || defined(USE_CONNMAN)
        g_clear_object (&system_bus);
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
 *
 * Since: 0.20.3
 **/
void
gupnp_context_manager_rescan_control_points (GUPnPContextManager *manager)
{
        GUPnPContextManagerPrivate *priv;

        g_return_if_fail (GUPNP_IS_CONTEXT_MANAGER (manager));

        priv = gupnp_context_manager_get_instance_private (manager);

        g_ptr_array_foreach (priv->control_points,
                             (GFunc) gssdp_resource_browser_rescan,
                             NULL);
}

/**
 * gupnp_context_manager_manage_control_point:
 * @manager: A #GUPnPContextManager
 * @control_point: The #GUPnPControlPoint to be taken care of
 *
 * By calling this function, you are asking @manager to keep a reference to
 * @control_point until its associated #GUPnPContext is no longer available.
 * You usually want to call this function from your
 * [signal@GUPnP.ContextManager::context-available] handler after you create a
 * #GUPnPControlPoint object for the newly available context.
 * You usually then give up your own reference to the control point so it will be
 * automatically destroyed if its context is no longer available.
 *
 * This function is mainly useful when implementing an UPnP client.
 *
 * ```c
 * void on_context_available (GUPnPContextManager *manager, GUPnPContext *context, gpointer user_data)
 * {
 *     GUPnPControlPoint *cp = gupnp_control_point_new (context, "urn:schemas-upnp-org:device:MediaRenderer:1");
 *     gupnp_context_manager_manage_control_point (manager, cp);
 *     // Subscribe to control point's signals etc.
 *     g_object_unref (cp);
 * }
 * ```
 *
 * Since: 0.14.0
 **/
void
gupnp_context_manager_manage_control_point (GUPnPContextManager *manager,
                                            GUPnPControlPoint   *control_point)
{
        GUPnPContextManagerPrivate *priv;

        g_return_if_fail (GUPNP_IS_CONTEXT_MANAGER (manager));
        g_return_if_fail (GUPNP_IS_CONTROL_POINT (control_point));

        priv = gupnp_context_manager_get_instance_private (manager);

        GUPnPContext *ctx = GUPNP_CONTEXT (gssdp_resource_browser_get_client (
                GSSDP_RESOURCE_BROWSER (control_point)));

        GPtrArray *objects = ensure_context (priv->contexts, ctx);

        g_ptr_array_add (objects, g_object_ref (control_point));
        g_ptr_array_add (priv->control_points, control_point);

        g_object_weak_ref (G_OBJECT (control_point),
                           (GWeakNotify) g_ptr_array_remove_fast,
                           priv->control_points);
}

/**
 * gupnp_context_manager_manage_root_device:
 * @manager: A #GUPnPContextManager
 * @root_device: The #GUPnPRootDevice to be taken care of
 *
 * By calling this function, you are asking @manager to keep a reference to
 * @root_device when its associated #GUPnPContext is no longer available. You
 * usually want to call this function from
 * [signal@GUPnP.ContextManager::context-available] handler after you create a
 * #GUPnPRootDevice object for the newly available context.
 *
 * You usually then give up your own reference to the root device so it will be
 * automatically destroyed if its context is no longer available.
 *
 * This function is mainly useful when implementing an UPnP client.
 *
 * ```c
 * void on_context_available (GUPnPContextManager *manager, GUPnPContext *context, gpointer user_data)
 * {
 *     GError *error = NULL;
 *
 *     GUPnPRootDevice *rd = gupnp_root_device_new (context, "BasicLight1.xml", ".", &error);
 *     gupnp_context_manager_manage_root_device (manager, rd);
 *     // Subscribe to control point's signals etc.
 *     g_object_unref (rd);
 * }
 * ```
 * Since: 0.14.0
 **/
void
gupnp_context_manager_manage_root_device (GUPnPContextManager *manager,
                                          GUPnPRootDevice     *root_device)
{
        GUPnPContextManagerPrivate *priv;

        g_return_if_fail (GUPNP_IS_CONTEXT_MANAGER (manager));
        g_return_if_fail (GUPNP_IS_ROOT_DEVICE (root_device));

        priv = gupnp_context_manager_get_instance_private (manager);

        GUPnPContext *ctx =
                gupnp_device_info_get_context (GUPNP_DEVICE_INFO (root_device));

        GPtrArray *objects = ensure_context (priv->contexts, ctx);

        g_ptr_array_add (objects, g_object_ref (root_device));

        g_ptr_array_add (priv->root_devices, root_device);
        g_object_weak_ref (G_OBJECT (root_device),
                           (GWeakNotify) g_ptr_array_remove_fast,
                           priv->root_devices);
}

/**
 * gupnp_context_manager_get_port:(attributes org.gtk.Method.get_property=port)
 * @manager: A #GUPnPContextManager
 *
 * Get the network port associated with this context manager.
 * Returns: The network port associated with this context manager.
 *
 * Since: 0.20.0
 */
guint
gupnp_context_manager_get_port (GUPnPContextManager *manager)
{
        GUPnPContextManagerPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT_MANAGER (manager), 0);

        priv = gupnp_context_manager_get_instance_private (manager);

        return priv->port;
}

/**
 * gupnp_context_manager_get_context_filter:(attributes org.gtk.Method.get_property=context-filter)
 * @manager: A #GUPnPContextManager
 *
 * Get the #GUPnPContextFilter associated with @manager.
 *
 * Returns: (transfer none):  The #GUPnPContextFilter associated with this
 * context manager.
 *
 * Since: 1.4.0
 */
GUPnPContextFilter *
gupnp_context_manager_get_context_filter (GUPnPContextManager *manager)
{
        GUPnPContextManagerPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT_MANAGER (manager), NULL);

        priv = gupnp_context_manager_get_instance_private (manager);

        return priv->context_filter;
}

/**
 * gupnp_context_manager_get_socket_family:(attributes org.gtk.Method.get_property=family)
 * @manager: A #GUPnPContextManager
 *
 * Get the #GSocketFamily the contexts are created for. Can be
 * %G_SOCKET_FAMILY_IPV6, %G_SOCKET_FAMILY_IPV4 or %G_SOCKET_FAMILY_INVALID for
 * both
 *
 * Returns: The socket family
 * Since: 1.2.0
 */
GSocketFamily
gupnp_context_manager_get_socket_family (GUPnPContextManager *manager)
{
        GUPnPContextManagerPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT_MANAGER (manager),
                              G_SOCKET_FAMILY_INVALID);

        priv = gupnp_context_manager_get_instance_private (manager);

        return priv->family;
}

/**
 * gupnp_context_manager_get_uda_version:(attributes org.gtk.Method.get_property=uda-version)
 * @manager: A #GUPnPContextManager
 *
 * Get the UDA protocol version the contexts are implementing
 *
 * Returns: The UDA protocol version
 * Since: 1.2.0
 */
GSSDPUDAVersion
gupnp_context_manager_get_uda_version (GUPnPContextManager *manager)
{
        GUPnPContextManagerPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT_MANAGER (manager),
                              GSSDP_UDA_VERSION_UNSPECIFIED);

        priv = gupnp_context_manager_get_instance_private (manager);

        return priv->uda_version;
}
