/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author: Ludovic Ferrandis <ludovic.ferrandis@intel.com>
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

#ifndef __GUPNP_CONNMAN_MANAGER_H__
#define __GUPNP_CONNMAN_MANAGER_H__

#include "gupnp-context-manager.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL GType
gupnp_connman_manager_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_CONNMAN_MANAGER \
                (gupnp_connman_manager_get_type ())
#define GUPNP_CONNMAN_MANAGER(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_CONNMAN_MANAGER, \
                 GUPnPConnmanManager))
#define GUPNP_CONNMAN_MANAGER_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_CONNMAN_MANAGER, \
                 GUPnPConnmanManagerClass))
#define GUPNP_IS_CONNMAN_MANAGER(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_CONNMAN_MANAGER))
#define GUPNP_IS_CONNMAN_MANAGER_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_CONNMAN_MANAGER))
#define GUPNP_CONNMAN_MANAGER_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_CONNMAN_MANAGER, \
                 GUPnPConnmanManagerClass))

typedef struct _GUPnPConnmanManagerPrivate GUPnPConnmanManagerPrivate;

typedef struct {
        GUPnPContextManager             parent;
        GUPnPConnmanManagerPrivate      *priv;

} GUPnPConnmanManager;

typedef struct {
        GUPnPContextManagerClass parent_class;

} GUPnPConnmanManagerClass;

G_GNUC_INTERNAL gboolean
gupnp_connman_manager_is_available      (void);

G_END_DECLS

#endif /* __GUPNP_CONNMAN_MANAGER_H__ */
