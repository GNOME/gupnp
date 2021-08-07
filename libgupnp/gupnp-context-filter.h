/*
 * Copyright (C) 2013 Intel Corporation.
 *
 * Author: Ludovic Ferrandis <ludovic.ferrandis@intel.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#ifndef GUPNP_CONTEXT_FILTER_H
#define GUPNP_CONTEXT_FILTER_H

#include <libgupnp/gupnp-context.h>
#include <glib.h>

G_BEGIN_DECLS

#define GUPNP_TYPE_CONTEXT_FILTER (gupnp_context_filter_get_type ())

G_DECLARE_DERIVABLE_TYPE (GUPnPContextFilter,
                          gupnp_context_filter,
                          GUPNP,
                          CONTEXT_FILTER,
                          GObject)

struct _GUPnPContextFilterClass {
        GObjectClass parent_class;
};

void
gupnp_context_filter_set_enabled (GUPnPContextFilter *context_filter,
                                  gboolean enable);

gboolean
gupnp_context_filter_get_enabled (GUPnPContextFilter *context_filter);

gboolean
gupnp_context_filter_is_empty (GUPnPContextFilter *context_filter);

gboolean
gupnp_context_filter_add_entry (GUPnPContextFilter *context_filter,
                                const gchar *entry);
void
gupnp_context_filter_add_entryv (GUPnPContextFilter *context_filter,
                                 gchar **entries);

gboolean
gupnp_context_filter_remove_entry (GUPnPContextFilter *context_filter,
                                   const gchar *entry);

GList *
gupnp_context_filter_get_entries (GUPnPContextFilter *context_filter);

void
gupnp_context_filter_clear (GUPnPContextFilter *context_filter);

gboolean
gupnp_context_filter_check_context (GUPnPContextFilter *context_filter,
                                    GUPnPContext *context);

G_END_DECLS

#endif /* GUPNP_CONTEXT_FILTER_H */
