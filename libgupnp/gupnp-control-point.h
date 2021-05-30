/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
