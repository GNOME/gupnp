/*
 * Copyright (C) 2013 Intel Corporation.
 *
 * Author: Ludovic Ferrandis <ludovic.ferrandis@intel.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gupnp-context-filter"

#include "gupnp-context-filter.h"

#include <string.h>

struct _GUPnPContextFilterPrivate {
        gboolean enabled;
        GHashTable *entries;
};
typedef struct _GUPnPContextFilterPrivate GUPnPContextFilterPrivate;

/**
 * GUPnPContextFilter:
 *
 * Network context filter, used by [class@GUPnP.ContextManager]
 *
 * #GUPnPContextFilter handles network filtering. It provides API to manage a
 * list of entries that will be used to positive filter networks. The #GUPnPContextFilter
 * could be enabled or not. If it's enabled but the entries list is empty, it
 * behaves as if being disabled.
 *
 * The GUPnPContextFilter is used with the [class@GUPnP.ContextManager]
 * to narrow down the contexts that are notified by it.
 *
 * Contexts can be filtered by the following criteria:
 *
 *  - Their IP addresses
 *  - The network device they will live on
 *  - The name of the network the context would join
 *
 * To add or modify a context filter, you need to retrieve the current context filter
 * from the context manger using [method@GUPnP.ContextManager.get_context_filter].
 *
 * By default, a context filter is empty and disabled.
 *
 * For example, to only react to contexts that are appearing on eth0 or when being in the WiFi network with
 * the SSID "HomeNetwork", and on IPv6 localhost, you should do:
 *
 *
 * ```c
 * GUPnPContextFilter* filter;
 *
 * filter = gupnp_context_manager_get_context_filter (manager);
 * const char *filter_entries[] = {
 *     "eth0",
 *     "HomeNetwork",
 *     "::1",
 *     NULL
 * };
 * gupnp_context_filter_add_entryv (filter, filter_entries);
 * gupnp_context_filter_set_enabled (filter, TRUE);
 * ```
 *
 * Since: 1.4.0
 */

G_DEFINE_TYPE_WITH_PRIVATE (GUPnPContextFilter,
                            gupnp_context_filter,
                            G_TYPE_OBJECT)

enum
{
        PROP_0,
        PROP_ENABLED,
        PROP_ENTRIES
};

static void
gupnp_context_filter_init (GUPnPContextFilter *list)
{
        GUPnPContextFilterPrivate *priv;

        priv = gupnp_context_filter_get_instance_private (list);

        priv->entries =
                g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
gupnp_context_filter_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
        GUPnPContextFilter *list;
        GUPnPContextFilterPrivate *priv;

        list = GUPNP_CONTEXT_FILTER (object);
        priv = gupnp_context_filter_get_instance_private (list);

        switch (property_id) {
        case PROP_ENABLED:
                gupnp_context_filter_set_enabled (list,
                                                  g_value_get_boolean (value));
                break;
        case PROP_ENTRIES: {
                g_hash_table_remove_all (priv->entries);
                GPtrArray *array = g_ptr_array_new ();
                GList *entries = g_value_get_pointer (value);
                for (GList *it = entries; it != NULL; it = g_list_next (it)) {
                        g_ptr_array_add (array, it->data);
                }
                g_ptr_array_add (array, NULL);
                gupnp_context_filter_add_entryv (list, (char **) array->pdata);
                g_ptr_array_unref (array);
        } break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_context_filter_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
        GUPnPContextFilter *list;
        GUPnPContextFilterPrivate *priv;

        list = GUPNP_CONTEXT_FILTER (object);
        priv = gupnp_context_filter_get_instance_private (list);

        switch (property_id) {
        case PROP_ENABLED:
                g_value_set_boolean (value, priv->enabled);
                break;
        case PROP_ENTRIES:
                g_value_set_pointer (value,
                                     gupnp_context_filter_get_entries (list));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_context_filter_class_finalize (GObject *object)
{
        GUPnPContextFilter *list;
        GObjectClass *object_class;
        GUPnPContextFilterPrivate *priv;

        list = GUPNP_CONTEXT_FILTER (object);
        priv = gupnp_context_filter_get_instance_private (list);

        g_clear_pointer (&priv->entries, g_hash_table_destroy);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_context_filter_parent_class);
        object_class->finalize (object);
}

static void
gupnp_context_filter_class_init (GUPnPContextFilterClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_context_filter_set_property;
        object_class->get_property = gupnp_context_filter_get_property;
        object_class->finalize = gupnp_context_filter_class_finalize;

        /**
         * GUPnPContextFilter:enabled:(attributes org.gtk.Property.get=gupnp_context_filter_get_enabled org.gtk.Property.set=gupnp_context_filter_set_enabled)
         *
         * Whether this context filter is active or not.
         *
         * Since: 1.4.0
         **/
        g_object_class_install_property (
                object_class,
                PROP_ENABLED,
                g_param_spec_boolean ("enabled",
                                      "Enabled",
                                      "TRUE if the context filter is active.",
                                      FALSE,
                                      G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
                                              G_PARAM_STATIC_STRINGS |
                                              G_PARAM_EXPLICIT_NOTIFY));

        /**
         * GUPnPContextFilter:entries: (type GList(utf8))(attributes org.gtk.Property.get=gupnp_context_filter_get_entries)
         *
         * A list of items to filter for.
         *
         * Since: 1.4.0
         **/
        g_object_class_install_property (
                object_class,
                PROP_ENTRIES,
                g_param_spec_pointer (
                        "entries",
                        "Filter entries",
                        "GList of strings that compose the context filter.",
                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                                G_PARAM_STATIC_STRINGS |
                                G_PARAM_EXPLICIT_NOTIFY));
}

/**
 * gupnp_context_filter_new:
 *
 * Create a new #GUPnPContextFilter.
 * The context filter is disabled by default.
 *
 * Returns: (transfer full): A new #GUPnPContextFilter object.
 *
 * Since: 1.4.0
 **/
GUPnPContextFilter *
gupnp_context_filter_new (void)
{
        return g_object_new (GUPNP_TYPE_CONTEXT_FILTER, NULL);
}

/**
 * gupnp_context_filter_set_enabled:(attributes org.gtk.Method.set_property=enabled)
 * @context_filter: A #GUPnPContextFilter
 * @enable:  %TRUE to enable @context_filter, %FALSE otherwise
 *
 * Enable or disable the #GUPnPContextFilter to perform the network filtering.
 *
 * Since: 1.4.0
 **/
void
gupnp_context_filter_set_enabled (GUPnPContextFilter *context_filter,
                                  gboolean enable)
{
        GUPnPContextFilterPrivate *priv;

        g_return_if_fail (GUPNP_IS_CONTEXT_FILTER (context_filter));

        priv = gupnp_context_filter_get_instance_private (context_filter);
        if (priv->enabled != enable) {
                priv->enabled = enable;
                g_object_notify (G_OBJECT (context_filter), "enabled");
        }
}

/**
 * gupnp_context_filter_get_enabled:(attributes org.gtk.Method.get_property=enabled)
 * @context_filter: A #GUPnPContextFilter
 *
 * Return the status of the #GUPnPContextFilter
 *
 * Return value: %TRUE if @context_filter is enabled, %FALSE otherwise.
 *
 * Since: 1.4.0
 **/
gboolean
gupnp_context_filter_get_enabled (GUPnPContextFilter *context_filter)
{
        GUPnPContextFilterPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT_FILTER (context_filter), FALSE);

        priv = gupnp_context_filter_get_instance_private (context_filter);

        return priv->enabled;
}

/**
 * gupnp_context_filter_is_empty:
 * @context_filter: A #GUPnPContextFilter
 *
 * Return the state of the entries list of #GUPnPContextFilter
 *
 * Return value: %TRUE if @context_filter is empty, %FALSE otherwise.
 *
 * Since: 1.4.0
 **/
gboolean
gupnp_context_filter_is_empty (GUPnPContextFilter *context_filter)
{
        GUPnPContextFilterPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT_FILTER (context_filter), TRUE);

        priv = gupnp_context_filter_get_instance_private (context_filter);

        return (g_hash_table_size (priv->entries) == 0);
}

/**
 * gupnp_context_filter_add_entry:
 * @context_filter: A #GUPnPContextFilter
 * @entry: A value used to filter network
 *
 * Add @entry in the list of valid criteria used by @context_filter to
 * filter networks.
 * if @entry already exists, it won't be added a second time.
 *
 * Return value: %TRUE if @entry is added, %FALSE otherwise.
 *
 * Since: 1.4.0
 **/
gboolean
gupnp_context_filter_add_entry (GUPnPContextFilter *context_filter,
                                const gchar *entry)
{
        GUPnPContextFilterPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT_FILTER (context_filter), FALSE);
        g_return_val_if_fail ((entry != NULL), FALSE);

        priv = gupnp_context_filter_get_instance_private (context_filter);

        if (g_hash_table_add (priv->entries, g_strdup (entry))) {
                g_object_notify (G_OBJECT (context_filter), "entries");

                return TRUE;
        }

        return FALSE;
}

/**
 * gupnp_context_filter_add_entryv:
 * @context_filter: A #GUPnPContextFilter
 * @entries: (array zero-terminated=1): A %NULL-terminated list of strings
 *
 * Add a list of entries to a #GUPnPContextFilter. This is a helper function to
 * directly add a %NULL-terminated array of string usually acquired from
 * command line arguments.
 *
 * Since: 1.4.0
 */
void
gupnp_context_filter_add_entryv (GUPnPContextFilter *context_filter,
                                 gchar **entries)
{
        gchar *const *iter = entries;
        GUPnPContextFilterPrivate *priv;

        g_return_if_fail (GUPNP_IS_CONTEXT_FILTER (context_filter));
        g_return_if_fail ((entries != NULL));

        priv = gupnp_context_filter_get_instance_private (context_filter);
        gboolean changed = FALSE;
        for (; *iter != NULL; iter++) {
                if (g_hash_table_add (priv->entries, g_strdup (*iter)))
                        changed = TRUE;
        }

        if (changed)
                g_object_notify (G_OBJECT (context_filter), "entries");
}

/**
 * gupnp_context_filter_remove_entry:
 * @context_filter: A #GUPnPContextFilter
 * @entry: A value to remove from the filter list.
 *
 * Remove @entry in the list of valid criteria used by @context_filter to
 * filter networks.
 *
 * Return value: %TRUE if @entry is removed, %FALSE otherwise.
 *
 * Since: 1.4.0
 **/
gboolean
gupnp_context_filter_remove_entry (GUPnPContextFilter *context_filter,
                                   const gchar *entry)
{
        GUPnPContextFilterPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT_FILTER (context_filter), FALSE);
        g_return_val_if_fail ((entry != NULL), FALSE);

        priv = gupnp_context_filter_get_instance_private (context_filter);

        if (g_hash_table_remove (priv->entries, entry)) {
                g_object_notify (G_OBJECT (context_filter), "entries");

                return TRUE;
        }

        return FALSE;
}

/**
 * gupnp_context_filter_get_entries:
 * @context_filter: A #GUPnPContextFilter
 *
 * Get the #GList of entries that compose the context filter. Do not free
 *
 * Return value: (element-type utf8) (transfer container)(nullable):  a #GList of entries
 * used to filter networks, interfaces,... or %NULL.
 *
 * Since: 1.4.0
 **/
GList *
gupnp_context_filter_get_entries (GUPnPContextFilter *context_filter)
{
        GUPnPContextFilterPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT_FILTER (context_filter), NULL);

        priv = gupnp_context_filter_get_instance_private (context_filter);

        return g_hash_table_get_keys (priv->entries);
}

/**
 * gupnp_context_filter_clear:
 * @context_filter: A #GUPnPContextFilter
 *
 * Remove all entries from #GList that compose the context filter.
 * The list is now empty. Even if #GUPnPContextFilter is enabled, it will have
 * the same behavior as if it was disabled.
 *
 * Since: 1.4.0
 **/
void
gupnp_context_filter_clear (GUPnPContextFilter *context_filter)
{
        GUPnPContextFilterPrivate *priv;

        g_return_if_fail (GUPNP_IS_CONTEXT_FILTER (context_filter));

        priv = gupnp_context_filter_get_instance_private (context_filter);
        g_hash_table_remove_all (priv->entries);

        g_object_notify (G_OBJECT (context_filter), "entries");
}

/**
 * gupnp_context_filter_check_context:
 * @context_filter: A #GUPnPContextFilter
 * @context: A #GUPnPContext to test.
 *
 * It will check if the @context is allowed or not. The @context_filter will
 * check all its entries against #GUPnPContext interface, host IP and network
 * fields information. This function doesn't take into account the
 * @context_filter status (enabled or not).
 *
 * Return value: %TRUE if @context is matching the @context_filter criteria,
 * %FALSE otherwise.
 *
 * Since: 1.4.0
 **/
gboolean
gupnp_context_filter_check_context (GUPnPContextFilter *context_filter,
                                    GUPnPContext *context)
{
        GSSDPClient *client;
        const char *interface;
        const char *host_ip;
        const char *network;
        GUPnPContextFilterPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT_FILTER (context_filter), FALSE);
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), FALSE);

        client = GSSDP_CLIENT (context);
        priv = gupnp_context_filter_get_instance_private (context_filter);

        interface = gssdp_client_get_interface (client);
        host_ip = gssdp_client_get_host_ip (client);
        network = gssdp_client_get_network (client);

        return g_hash_table_contains (priv->entries, interface) ||
               g_hash_table_contains (priv->entries, host_ip) ||
               g_hash_table_contains (priv->entries, network);
}
