/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
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

#include <libgssdp/gssdp-resource-browser.h>

#include "gupnp-context.h"
#include "gupnp-resource-factory.h"
#include "gupnp-device-proxy.h"
#include "gupnp-service-proxy.h"

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

/**
 * GUPnPControlPoint:
 *
 * This struct contains private data only, and should be accessed using the
 * functions below.
 */
typedef struct {
        GSSDPResourceBrowser parent;

        GUPnPControlPointPrivate *priv;
} GUPnPControlPoint;

typedef struct {
        GSSDPResourceBrowserClass parent_class;

        /* signals */
        void (* device_proxy_available)    (GUPnPControlPoint *control_point,
                                            GUPnPDeviceProxy  *proxy);

        void (* device_proxy_unavailable)  (GUPnPControlPoint *control_point,
                                            GUPnPDeviceProxy  *proxy);

        void (* service_proxy_available)   (GUPnPControlPoint *control_point,
                                            GUPnPServiceProxy *proxy);

        void (* service_proxy_unavailable) (GUPnPControlPoint *control_point,
                                            GUPnPServiceProxy *proxy);

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
} GUPnPControlPointClass;

GUPnPControlPoint *
gupnp_control_point_new                  (GUPnPContext         *context,
                                          const char           *target);

GUPnPControlPoint *
gupnp_control_point_new_full             (GUPnPContext         *context,
                                          GUPnPResourceFactory *factory,
                                          const char           *target);

GUPnPContext *
gupnp_control_point_get_context          (GUPnPControlPoint    *control_point);

const GList *
gupnp_control_point_list_device_proxies  (GUPnPControlPoint    *control_point);

const GList *
gupnp_control_point_list_service_proxies (GUPnPControlPoint    *control_point);

GUPnPResourceFactory *
gupnp_control_point_get_resource_factory (GUPnPControlPoint    *control_point);

G_END_DECLS

#endif /* __GUPNP_CONTROL_POINT_H__ */
