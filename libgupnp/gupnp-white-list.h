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
