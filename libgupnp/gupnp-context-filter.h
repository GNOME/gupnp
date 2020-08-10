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

#ifndef GUPNP_CONTEXT_FILTER_H
#define GUPNP_CONTEXT_FILTER_H

#include "gupnp-context.h"
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

GUPnPContextFilter *
gupnp_context_filter_new (void);

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
