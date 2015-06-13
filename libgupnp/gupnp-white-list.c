/*
 * Copyright (C) 2013 Intel Corporation.
 *
 * Author: Ludovic Ferrandis <ludovic.ferrandis@intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
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
 * SECTION:gupnp-white-list
 * @short_description: Class for network filtering.
 *
 * #GUPnPWhiteList handles network filtering. It provides API to manage a list
 * of entries that will be used to filter networks.
 * The #GUPnPWhiteList could be enabled or not. If it's enabled but the entries
 * list is empty, it behaves as disabled.
 *
 * Since: 0.20.5
 */

#include <string.h>

#include "gupnp-white-list.h"

G_DEFINE_TYPE (GUPnPWhiteList,
               gupnp_white_list,
               G_TYPE_OBJECT);

struct _GUPnPWhiteListPrivate {
        gboolean enabled;
        GList *entries;
};

enum {
        PROP_0,
        PROP_ENABLED,
        PROP_ENTRIES
};

enum {
        ENTRY_CHANGE,
        ENABLED,
        SIGNAL_LAST
};

static void
gupnp_white_list_init (GUPnPWhiteList *list)
{
        list->priv = G_TYPE_INSTANCE_GET_PRIVATE (list,
                                                  GUPNP_TYPE_WHITE_LIST,
                                                  GUPnPWhiteListPrivate);

        list->priv->entries = NULL;
}

static void
gupnp_white_list_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
        GUPnPWhiteList *list;

        list = GUPNP_WHITE_LIST (object);

        switch (property_id) {
        case PROP_ENABLED:
                list->priv->enabled = g_value_get_boolean (value);
                break;
        case PROP_ENTRIES:
                list->priv->entries = g_value_get_pointer (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_white_list_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
        GUPnPWhiteList *list;

        list = GUPNP_WHITE_LIST (object);

        switch (property_id) {
        case PROP_ENABLED:
                g_value_set_boolean (value, list->priv->enabled);
                break;
        case PROP_ENTRIES:
                g_value_set_pointer (value, list->priv->entries);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_white_list_class_finalize (GObject *object)
{
        GUPnPWhiteList *list;
        GObjectClass *object_class;

        list = GUPNP_WHITE_LIST (object);

        g_list_free_full (list->priv->entries, g_free);
        list->priv->entries = NULL;

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_white_list_parent_class);
        object_class->finalize (object);
}

static void
gupnp_white_list_class_init (GUPnPWhiteListClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_white_list_set_property;
        object_class->get_property = gupnp_white_list_get_property;
        object_class->finalize     = gupnp_white_list_class_finalize;

        g_type_class_add_private (klass, sizeof (GUPnPWhiteListPrivate));

        /**
         * GUPnPWhiteList:enabled:
         *
         * Whether this white list is active or not.
         *
         * Since: 0.20.5
         **/
        g_object_class_install_property
                (object_class,
                 PROP_ENABLED,
                 g_param_spec_boolean
                         ("enabled",
                          "Enabled",
                          "TRUE if the white list is active.",
                          FALSE,
                          G_PARAM_CONSTRUCT |
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

        /**
         * GUPnPWhiteList:entries: (type GList(utf8))
         *
         * Whether this white list is active or not.
         *
         * Since: 0.20.5
         **/
        g_object_class_install_property
                (object_class,
                 PROP_ENTRIES,
                 g_param_spec_pointer
                         ("entries",
                          "Entries",
                          "GList of strings that compose the white list.",
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
}

/**
 * gupnp_white_list_new:
 *
 * Create a new #GUPnPWhiteList.
 * The white list is disabled by default.
 *
 * Returns: (transfer full): A new #GUPnPWhiteList object.
 *
 * Since: 0.20.5
 **/
GUPnPWhiteList *
gupnp_white_list_new (void)
{
        return g_object_new (GUPNP_TYPE_WHITE_LIST, NULL);
}

/**
 * gupnp_white_list_set_enabled:
 * @white_list: A #GUPnPWhiteList
 * @enable:  %TRUE to enable @white_list, %FALSE otherwise
 *
 * Enable or disable the #GUPnPWhiteList to perform the network filtering.
 *
 * Since: 0.20.5
 **/
void
gupnp_white_list_set_enabled (GUPnPWhiteList *white_list, gboolean enable)
{
        g_return_if_fail (GUPNP_IS_WHITE_LIST (white_list));

        white_list->priv->enabled = enable;
        g_object_notify (G_OBJECT (white_list), "enabled");
}

/**
 * gupnp_white_list_get_enabled:
 * @white_list: A #GUPnPWhiteList
 *
 * Return the status of the #GUPnPWhiteList
 *
 * Return value: %TRUE if @white_list is enabled, %FALSE otherwise.
 *
 * Since: 0.20.5
 **/
gboolean
gupnp_white_list_get_enabled (GUPnPWhiteList *white_list)
{
        g_return_val_if_fail (GUPNP_IS_WHITE_LIST (white_list), FALSE);

        return white_list->priv->enabled;
}

/**
 * gupnp_white_list_is_empty:
 * @white_list: A #GUPnPWhiteList
 *
 * Return the state of the entries list of #GUPnPWhiteList
 *
 * Return value: %TRUE if @white_list is empty, %FALSE otherwise.
 *
 * Since: 0.20.5
 **/
gboolean
gupnp_white_list_is_empty (GUPnPWhiteList *white_list)
{
        g_return_val_if_fail (GUPNP_IS_WHITE_LIST (white_list), TRUE);

        return (white_list->priv->entries == NULL);
}

/**
 * gupnp_white_list_add_entry:
 * @white_list: A #GUPnPWhiteList
 * @entry: A value used to filter network
 *
 * Add @entry in the list of valid criteria used by @white_list to
 * filter networks.
 * if @entry already exists, it won't be added a second time.
 *
 * Return value: %TRUE if @entry is added, %FALSE otherwise.
 *
 * Since: 0.20.5
 **/
gboolean
gupnp_white_list_add_entry (GUPnPWhiteList *white_list, const gchar* entry)
{
        GList *s_entry;
        GUPnPWhiteListPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_WHITE_LIST (white_list), FALSE);
        g_return_val_if_fail ((entry != NULL), FALSE);

        priv = white_list->priv;

        s_entry = g_list_find_custom (priv->entries, entry,
                                      (GCompareFunc) g_ascii_strcasecmp);

        if (s_entry == NULL) {
                priv->entries = g_list_prepend (priv->entries,
                                                g_strdup (entry));
                g_object_notify (G_OBJECT (white_list), "entries");
        }

        return (s_entry == NULL);
}

/**
 * gupnp_white_list_add_entryv:
 * @white_list: A #GUPnPWhiteList
 * @entries: (array zero-terminated=1): A %NULL-terminated list of strings
 *
 * Add a list of entries to a #GUPnPWhiteList. This is a helper function to
 * directly add a %NULL-terminated array of string usually aquired from
 * commandline args.
 *
 * Since: 0.20.8
 */
void
gupnp_white_list_add_entryv (GUPnPWhiteList *white_list, gchar **entries)
{
        gchar * const * iter = entries;

        g_return_if_fail (GUPNP_IS_WHITE_LIST (white_list));
        g_return_if_fail ((entries != NULL));

        for (; *iter != NULL; iter++)
                gupnp_white_list_add_entry (white_list, *iter);
 }

/**
 * gupnp_white_list_remove_entry:
 * @white_list: A #GUPnPWhiteList
 * @entry: A value to remove from the filter list.
 *
 * Remove @entry in the list of valid criteria used by @white_list to
 * filter networks.
 *
 * Return value: %TRUE if @entry is removed, %FALSE otherwise.
 *
 * Since: 0.20.5
 **/
gboolean
gupnp_white_list_remove_entry (GUPnPWhiteList *white_list, const gchar* entry)
{
        GList *s_entry;
        GUPnPWhiteListPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_WHITE_LIST (white_list), FALSE);
        g_return_val_if_fail ((entry != NULL), FALSE);

        priv = white_list->priv;

        s_entry = g_list_find_custom (priv->entries, entry,
                                      (GCompareFunc) g_ascii_strcasecmp);

        if (s_entry != NULL) {
                priv->entries = g_list_remove_link (priv->entries, s_entry);
                g_list_free_full (s_entry, g_free);
                g_object_notify (G_OBJECT (white_list), "entries");
        }

        return (s_entry != NULL);
}

/**
 * gupnp_white_list_get_entries:
 * @white_list: A #GUPnPWhiteList
 *
 * Get the #GList of entries that compose the white list. Do not free
 *
 * Return value: (element-type utf8) (transfer none):  a #GList of entries
 * used to filter networks, interfaces,... or %NULL.
 * Do not modify or free the list nor its elements.
 *
 * Since: 0.20.5
 **/
GList *
gupnp_white_list_get_entries (GUPnPWhiteList *white_list)
{
        g_return_val_if_fail (GUPNP_IS_WHITE_LIST (white_list), NULL);

        return white_list->priv->entries;
}

/**
 * gupnp_white_list_clear:
 * @white_list: A #GUPnPWhiteList
 *
 * Remove all entries from #GList that compose the white list.
 * The list is now empty. Even if #GUPnPWhiteList is enabled, it will have the
 * same behavior as if it was disabled.
 *
 * Since: 0.20.5
 **/
void
gupnp_white_list_clear (GUPnPWhiteList *white_list)
{
        GUPnPWhiteListPrivate *priv;

        g_return_if_fail (GUPNP_IS_WHITE_LIST(white_list));

        priv = white_list->priv;
        g_list_free_full (priv->entries, g_free);
        priv->entries = NULL;
        g_object_notify (G_OBJECT (white_list), "entries");
}

/**
 * gupnp_white_list_check_context:
 * @white_list: A #GUPnPWhiteList
 * @context: A #GUPnPContext to test.
 *
 * It will check if the @context is allowed or not. The @white_list will check
 * all its entries againt #GUPnPContext interface, host ip and network fields
 * information. This function doesn't take into account the @white_list status
 * (enabled or not).
 *
 * Return value: %TRUE if @context is matching the @white_list criterias,
 * %FALSE otherwise.
 *
 * Since: 0.20.5
 **/
gboolean
gupnp_white_list_check_context (GUPnPWhiteList *white_list,
                                GUPnPContext *context)
{
        GSSDPClient  *client;
        GList *l;
        const char *interface;
        const char *host_ip;
        const char *network;
        gboolean match = FALSE;

        g_return_val_if_fail (GUPNP_IS_WHITE_LIST (white_list), FALSE);
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), FALSE);

        client = GSSDP_CLIENT (context);

        interface = gssdp_client_get_interface (client);
        host_ip = gssdp_client_get_host_ip (client);
        network = gssdp_client_get_network (client);

        l = white_list->priv->entries;

        while (l && !match) {
                match = (interface && !strcmp (l->data, interface)) ||
                        (host_ip && !strcmp (l->data, host_ip)) ||
                        (network && !strcmp (l->data, network));

                l = l->next;
        }

        return match;
}
