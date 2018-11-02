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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef GUPNP_CONTROL_POINT_H
#define GUPNP_CONTROL_POINT_H

#include <libgssdp/gssdp-resource-browser.h>

#include "gupnp-context.h"
#include "gupnp-resource-factory.h"
#include "gupnp-device-proxy.h"
#include "gupnp-service-proxy.h"

G_BEGIN_DECLS

#define GUPNP_TYPE_CONTROL_POINT \
                (gupnp_control_point_get_type ())

G_DECLARE_DERIVABLE_TYPE (GUPnPControlPoint,
                          gupnp_control_point,
                          GUPNP,
                          CONTROL_POINT,
                          GSSDPResourceBrowser)

struct _GUPnPControlPointClass {
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
};

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

#endif /* GUPNP_CONTROL_POINT_H */
