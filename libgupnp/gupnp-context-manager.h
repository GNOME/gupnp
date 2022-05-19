/*
 * Copyright (C) 2009 Nokia Corporation.
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_CONTEXT_MANAGER_H
#define GUPNP_CONTEXT_MANAGER_H

#include "gupnp-context-filter.h"
#include "gupnp-context.h"
#include "gupnp-control-point.h"
#include "gupnp-root-device.h"
#include <glib.h>

G_BEGIN_DECLS

#define GUPNP_TYPE_CONTEXT_MANAGER \
                (gupnp_context_manager_get_type ())

G_DECLARE_DERIVABLE_TYPE (GUPnPContextManager,
                          gupnp_context_manager,
                          GUPNP,
                          CONTEXT_MANAGER,
                          GObject)

struct _GUPnPContextManagerClass {
        GObjectClass parent_class;

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
};

GUPnPContextManager *
gupnp_context_manager_create            (guint port);

GUPnPContextManager *
gupnp_context_manager_create_full       (GSSDPUDAVersion uda_version,
                                         GSocketFamily   family,
                                         guint           port);

void
gupnp_context_manager_rescan_control_points
                                        (GUPnPContextManager *manager);

void
gupnp_context_manager_manage_control_point
                                        (GUPnPContextManager *manager,
                                         GUPnPControlPoint   *control_point);

void
gupnp_context_manager_manage_root_device
                                        (GUPnPContextManager *manager,
                                         GUPnPRootDevice     *root_device);

guint
gupnp_context_manager_get_port          (GUPnPContextManager *manager);

GUPnPContextFilter *
gupnp_context_manager_get_context_filter (GUPnPContextManager *manager);

GSocketFamily
gupnp_context_manager_get_socket_family (GUPnPContextManager *manager);

GSSDPUDAVersion
gupnp_context_manager_get_uda_version   (GUPnPContextManager *manager);

G_END_DECLS

#endif /* GUPNP_CONTEXT_MANAGER_H */
