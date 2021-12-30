/*
 * Copyright (C) 2013 Intel Corporation.
 *
 * Author: Ludovic Ferrandis <ludovic.ferrandis@intel.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

/**
 * SECTION:gupnp-white-list
 * @short_description: Deprecated network filtering
 */

#include <config.h>
#include <string.h>

#include "gupnp-white-list.h"
#include "gupnp-context-filter.h"

/**
 * GUPnPWhiteList:
 *
 * Utility class for context filtering in the context manager.
 *
 * It provides API to manage a list
 * of entries that will be used to filter networks.
 * The #GUPnPWhiteList could be enabled or not. If it's enabled but the entries
 * list is empty, it behaves as disabled.
 *
 * Since: 0.20.5
 * Deprecated: 1.4.0: Use #GUPnPContextFilter
 */

/**
 * gupnp_white_list_new:
 *
 * Create a new #GUPnPWhiteList.
 * The white list is disabled by default.
 *
 * Returns: (transfer full): A new #GUPnPWhiteList object.
 *
 * Since: 0.20.5
 * Deprecated: 1.4.0: Use gupnp_context_filter_new() instead.
 **/
GUPnPWhiteList *
gupnp_white_list_new (void)
{
        return g_object_new (GUPNP_TYPE_CONTEXT_FILTER, NULL);
}

/**
 * gupnp_white_list_set_enabled:
 * @white_list: A #GUPnPWhiteList
 * @enable:  %TRUE to enable @white_list, %FALSE otherwise
 *
 * Enable or disable the #GUPnPWhiteList to perform the network filtering.
 *
 * Deprecated: 1.4.0: Use gupnp_context_filter_set_enabled() instead.
 */
void
gupnp_white_list_set_enabled (GUPnPWhiteList *white_list, gboolean enable)
{
        g_return_if_fail (GUPNP_IS_CONTEXT_FILTER (white_list));

        gupnp_context_filter_set_enabled (GUPNP_CONTEXT_FILTER (white_list),
                                          enable);
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
 * Deprecated: 1.4.0: Use gupnp_context_filter_get_enabled() instead.
 */
gboolean
gupnp_white_list_get_enabled (GUPnPWhiteList *white_list)
{
        g_return_val_if_fail (GUPNP_IS_CONTEXT_FILTER (white_list), FALSE);

        return gupnp_context_filter_get_enabled (GUPNP_CONTEXT_FILTER (white_list));
}

/**
 * gupnp_white_list_is_empty:
 * @white_list: A #GUPnPWhiteList
 *
 * Return the state of the entries list of #GUPnPWhiteList
 *
 * Return value: %TRUE if @white_list is empty, %FALSE otherwise.
 *
 * Deprecated: 1.4.0: Use gupnp_context_filter_is_empty() instead.
 */
gboolean
gupnp_white_list_is_empty (GUPnPWhiteList *white_list)
{
        g_return_val_if_fail (GUPNP_IS_CONTEXT_FILTER (white_list), TRUE);

        return gupnp_context_filter_is_empty (
                GUPNP_CONTEXT_FILTER (white_list));
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
 * Deprecated: 1.4.0: Use gupnp_context_filter_add_entry() instead.
 */
gboolean
gupnp_white_list_add_entry (GUPnPWhiteList *white_list, const gchar* entry)
{
        g_return_val_if_fail (GUPNP_IS_CONTEXT_FILTER (white_list), FALSE);
        g_return_val_if_fail (entry != NULL, FALSE);

        return gupnp_context_filter_add_entry (
                GUPNP_CONTEXT_FILTER (white_list),
                entry);
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
 * Deprecated: 1.4.0: Use gupnp_context_filter_add_entryv() instead.
 */
void
gupnp_white_list_add_entryv (GUPnPWhiteList *white_list, gchar **entries)
{
        g_return_if_fail (GUPNP_IS_CONTEXT_FILTER (white_list));
        g_return_if_fail ((entries != NULL));

        gupnp_context_filter_add_entryv (GUPNP_CONTEXT_FILTER (white_list),
                                         entries);
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
 * Deprecated: 1.4.0: Use gupnp_context_filter_remove_entry() instead.
 */
gboolean
gupnp_white_list_remove_entry (GUPnPWhiteList *white_list, const gchar* entry)
{
        g_return_val_if_fail (GUPNP_IS_CONTEXT_FILTER (white_list), FALSE);
        g_return_val_if_fail ((entry != NULL), FALSE);

        return gupnp_context_filter_remove_entry (
                GUPNP_CONTEXT_FILTER (white_list),
                entry);
}

/**
 * gupnp_white_list_get_entries:
 * @white_list: A #GUPnPWhiteList
 *
 * Get the #GList of entries that compose the white list. Do not free
 *
 * Return value: (nullable)(element-type utf8) (transfer none):  a #GList of entries
 * used to filter networks, interfaces,... or %NULL.
 * Do not modify or free the list nor its elements.
 *
 * Since: 0.20.5
 * Deprecated: 1.4.0: Use gupnp_context_filter_get_entries() instead.
 */
GList *
gupnp_white_list_get_entries (GUPnPWhiteList *white_list)
{
        g_return_val_if_fail (GUPNP_IS_CONTEXT_FILTER (white_list), NULL);
        return gupnp_context_filter_get_entries (
                GUPNP_CONTEXT_FILTER (white_list));
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
 * Deprecated: 1.4.0: Use gupnp_context_filter_clear() instead.
 */
void
gupnp_white_list_clear (GUPnPWhiteList *white_list)
{
        g_return_if_fail (GUPNP_IS_CONTEXT_FILTER(white_list));

        gupnp_context_filter_clear (GUPNP_CONTEXT_FILTER (white_list));
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
 * Deprecated: 1.4.0: Use gupnp_context_filter_check_context() instead.
 */
gboolean
gupnp_white_list_check_context (GUPnPWhiteList *white_list,
                                GUPnPContext *context)
{
        return gupnp_context_filter_check_context (
                GUPNP_CONTEXT_FILTER (white_list),
                context);
}
