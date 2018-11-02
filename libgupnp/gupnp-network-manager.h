/*
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *                               <zeeshan.ali@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
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
