/* 
 * Copyright (C) 2006 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GUPNP_SERVICE_INFO_H__
#define __GUPNP_SERVICE_INFO_H__

#include <glib-object.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

GType
gupnp_service_info_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_SERVICE_INFO \
                (gupnp_service_info_type ())
#define GUPNP_SERVICE_INFO(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_SERVICE_INFO, \
                 GUPnPServiceInfo))
#define GUPNP_IS_SERVICE_INFO(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_SERVICE_INFO))
#define GUPNP_SERVICE_INFO_GET_IFACE(obj) \
                (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
                 GUPNP_TYPE_SERVICE_INFO, \
                 GUPnPServiceInfoIface))

typedef struct _GUPnPServiceInfo GUPnPServiceInfo; /* Dummy typedef */

typedef struct {
        GTypeInterface iface;

        /* vtable */
        xmlNode * (* get_element) (GUPnPServiceInfo *info);

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
} GUPnPServiceInfoIface;

const char *
gupnp_service_info_get_type                   (GUPnPServiceInfo *info);

const char *
gupnp_service_info_get_id                     (GUPnPServiceInfo *info);

const char *
gupnp_service_info_get_scpd_url               (GUPnPServiceInfo *info);

const char *
gupnp_service_info_get_control_url            (GUPnPServiceInfo *info);

const char *
gupnp_service_info_get_event_subscription_url (GUPnPServiceInfo *info);

G_END_DECLS

#endif /* __GUPNP_SERVICE_INFO_H__ */
