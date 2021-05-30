/*
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *                               <zeeshan.ali@nokia.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_NETWORK_MANAGER_H
#define GUPNP_NETWORK_MANAGER_H

#include "gupnp-context-manager.h"

G_BEGIN_DECLS

#define GUPNP_TYPE_NETWORK_MANAGER \
                (gupnp_network_manager_get_type ())

G_DECLARE_FINAL_TYPE (GUPnPNetworkManager,
                      gupnp_network_manager,
                      GUPNP,
                      NETWORK_MANAGER,
                      GUPnPContextManager)

G_GNUC_INTERNAL gboolean
gupnp_network_manager_is_available                      (void);

G_END_DECLS

#endif /* GUPNP_NETWORK_MANAGER_H */
