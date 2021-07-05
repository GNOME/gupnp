/*
 * Copyright (C) 2013 Intel Corporation.
 *
 * Author: Ludovic Ferrandis <ludovic.ferrandis@intel.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_WHITE_LIST_H
#define GUPNP_WHITE_LIST_H

#include <glib.h>
#include <libgupnp/gupnp-context-filter.h>
#include <libgupnp/gupnp-context.h>

G_BEGIN_DECLS

#define GUPNP_TYPE_WHITE_LIST (gupnp_context_filter_get_type ())
#define GUPNP_IS_WHITE_LIST GUPNP_IS_CONTEXT_FILTER
typedef GUPnPContextFilter GUPnPWhiteList;

G_GNUC_DEPRECATED_FOR(gupnp_context_filter_new) GUPnPWhiteList *
gupnp_white_list_new            (void);

G_GNUC_DEPRECATED_FOR(gupnp_context_filter_set_enabled) void
gupnp_white_list_set_enabled    (GUPnPWhiteList *white_list,
                                 gboolean enable);

G_GNUC_DEPRECATED_FOR(gupnp_context_filter_get_enabled) gboolean
gupnp_white_list_get_enabled     (GUPnPWhiteList *white_list);

G_GNUC_DEPRECATED_FOR(gupnp_context_filter_is_empty) gboolean
gupnp_white_list_is_empty       (GUPnPWhiteList *white_list);

G_GNUC_DEPRECATED_FOR(gupnp_context_filter_add_entry) gboolean
gupnp_white_list_add_entry      (GUPnPWhiteList *white_list,
                                 const gchar* entry);
G_GNUC_DEPRECATED_FOR(gupnp_context_filter_add_entryv) void
gupnp_white_list_add_entryv     (GUPnPWhiteList *white_list,
                                 gchar** entries);

G_GNUC_DEPRECATED_FOR(gupnp_context_filter_remove_entry) gboolean
gupnp_white_list_remove_entry   (GUPnPWhiteList *white_list,
                                 const gchar* entry);

G_GNUC_DEPRECATED_FOR(gupnp_context_filter_get_entries) GList *
gupnp_white_list_get_entries    (GUPnPWhiteList *white_list);

G_GNUC_DEPRECATED_FOR(gupnp_context_filter_clear) void
gupnp_white_list_clear          (GUPnPWhiteList *white_list);

G_GNUC_DEPRECATED_FOR(gupnp_context_filter_check_context) gboolean
gupnp_white_list_check_context  (GUPnPWhiteList *white_list,
                                 GUPnPContext *context);

G_END_DECLS

#endif /* GUPNP_WHITE_LIST_H */
