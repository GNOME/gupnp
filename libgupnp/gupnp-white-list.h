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

#ifndef GUPNP_WHITE_LIST_H
#define GUPNP_WHITE_LIST_H

#include <glib.h>
#include "gupnp-context.h"

G_BEGIN_DECLS

#define GUPNP_TYPE_WHITE_LIST \
                (gupnp_white_list_get_type ())

G_DECLARE_DERIVABLE_TYPE (GUPnPWhiteList,
                          gupnp_white_list,
                          GUPNP,
                          WHITE_LIST,
                          GObject)

struct _GUPnPWhiteListClass {
        GObjectClass parent_class;
};

GUPnPWhiteList *
gupnp_white_list_new            (void);

void
gupnp_white_list_set_enabled    (GUPnPWhiteList *white_list,
                                 gboolean enable);

gboolean
gupnp_white_list_get_enabled     (GUPnPWhiteList *white_list);

gboolean
gupnp_white_list_is_empty       (GUPnPWhiteList *white_list);

gboolean
gupnp_white_list_add_entry      (GUPnPWhiteList *white_list,
                                 const gchar* entry);
void
gupnp_white_list_add_entryv     (GUPnPWhiteList *white_list,
                                 gchar** entries);

gboolean
gupnp_white_list_remove_entry   (GUPnPWhiteList *white_list,
                                 const gchar* entry);

GList *
gupnp_white_list_get_entries    (GUPnPWhiteList *white_list);

void
gupnp_white_list_clear          (GUPnPWhiteList *white_list);

gboolean
gupnp_white_list_check_context  (GUPnPWhiteList *white_list,
                                 GUPnPContext *context);

G_END_DECLS

#endif /* GUPNP_WHITE_LIST_H */
