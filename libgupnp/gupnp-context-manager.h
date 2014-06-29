/*
 * Copyright (C) 2009 Nokia Corporation.
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
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

#ifndef __GUPNP_CONTEXT_MANAGER_H__
#define __GUPNP_CONTEXT_MANAGER_H__

#include <glib.h>
#include "gupnp-context.h"
#include "gupnp-root-device.h"
#include "gupnp-control-point.h"
#include "gupnp-white-list.h"

G_BEGIN_DECLS

GType
gupnp_context_manager_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_CONTEXT_MANAGER \
                (gupnp_context_manager_get_type ())
#define GUPNP_CONTEXT_MANAGER(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_CONTEXT_MANAGER, \
                 GUPnPContextManager))
#define GUPNP_CONTEXT_MANAGER_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_CONTEXT_MANAGER, \
                 GUPnPContextManagerClass))
#define GUPNP_IS_CONTEXT_MANAGER(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_CONTEXT_MANAGER))
#define GUPNP_IS_CONTEXT_MANAGER_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_CONTEXT_MANAGER))
#define GUPNP_CONTEXT_MANAGER_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_CONTEXT_MANAGER, \
                 GUPnPContextManagerClass))

typedef struct _GUPnPContextManagerPrivate GUPnPContextManagerPrivate;
typedef struct _GUPnPContextManager GUPnPContextManager;
typedef struct _GUPnPContextManagerClass GUPnPContextManagerClass;

/**
 * GUPnPContextManager:
 *
 * This struct contains private data only, and should be accessed using the
 * functions below.
 */
struct _GUPnPContextManager {
        GObject parent;

        GUPnPContextManagerPrivate *priv;
};

struct _GUPnPContextManagerClass {
        GObjectClass parent_class;

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
};


#ifndef GUPNP_DISABLE_DEPRECATED
GUPnPContextManager *
gupnp_context_manager_new              (GMainContext *main_context,
                                        guint         port);
#endif

GUPnPContextManager *
gupnp_context_manager_create           (guint port);

void
gupnp_context_manager_rescan_control_points
                                       (GUPnPContextManager *manager);

void
gupnp_context_manager_manage_control_point
                                       (GUPnPContextManager     *manager,
                                        GUPnPControlPoint       *control_point);

void
gupnp_context_manager_manage_root_device
                                       (GUPnPContextManager     *manager,
                                        GUPnPRootDevice         *root_device);

guint
gupnp_context_manager_get_port         (GUPnPContextManager *manager);

GUPnPWhiteList *
gupnp_context_manager_get_white_list   (GUPnPContextManager *manager);

G_END_DECLS

#endif /* __GUPNP_CONTEXT_MANAGER_H__ */
