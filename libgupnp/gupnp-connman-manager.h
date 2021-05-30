/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author: Ludovic Ferrandis <ludovic.ferrandis@intel.com>
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *                               <zeeshan.ali@nokia.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_CONNMAN_MANAGER_H
#define GUPNP_CONNMAN_MANAGER_H

#include "gupnp-context-manager.h"

G_BEGIN_DECLS

#define GUPNP_TYPE_CONNMAN_MANAGER \
                (gupnp_connman_manager_get_type ())

G_DECLARE_FINAL_TYPE (GUPnPConnmanManager,
                      gupnp_connman_manager,
                      GUPNP,
                      CONNMAN_MANAGER,
                      GUPnPContextManager)

G_GNUC_INTERNAL gboolean
gupnp_connman_manager_is_available      (void);

G_END_DECLS

#endif /* GUPNP_CONNMAN_MANAGER_H */
