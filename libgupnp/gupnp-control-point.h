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

#ifndef __GUPNP_CONTROL_POINT_H__
#define __GUPNP_CONTROL_POINT_H__

#include <libgssdp/gssdp-service-browser.h>

#include "gupnp-device-proxy.h"

G_BEGIN_DECLS

GType
gupnp_control_point_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_CONTROL_POINT \
                (gupnp_control_point_get_type ())
#define GUPNP_CONTROL_POINT(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_CONTROL_POINT, \
                 GUPnPControlPoint))
#define GUPNP_CONTROL_POINT_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_CONTROL_POINT, \
                 GUPnPControlPointClass))
#define GUPNP_IS_CONTROL_POINT(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_CONTROL_POINT))
#define GUPNP_IS_CONTROL_POINT_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_CONTROL_POINT))
#define GUPNP_CONTROL_POINT_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_CONTROL_POINT, \
                 GUPnPControlPointClass))

typedef struct _GUPnPControlPointPrivate GUPnPControlPointPrivate;

typedef struct {
        GSSDPServiceBrowser parent;

        GUPnPControlPointPrivate *priv;
} GUPnPControlPoint;

typedef struct {
        GSSDPServiceBrowserClass parent_class;

        /* signals */
        void (* device_available)   (GUPnPControlPoint *control_point,
                                     GUPnPDeviceProxy  *proxy);

        void (* device_unavailable) (GUPnPControlPoint *control_point,
                                     GUPnPDeviceProxy  *proxy);

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
} GUPnPControlPointClass;

GUPnPControlPoint *
gupnp_control_point_new          (GSSDPClient       *ssdp_client);

GList *
gupnp_control_point_list_devices (GUPnPControlPoint *control_point);

G_END_DECLS

#endif /* __GUPNP_CONTROL_POINT_H__ */
