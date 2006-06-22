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

#ifndef __GUPNP_DEVICE_PROXY_H__
#define __GUPNP_DEVICE_PROXY_H__

#include <libxml/tree.h>

#include "gupnp-context.h"
#include "gupnp-device-info.h"
#include "gupnp-service-proxy.h"

G_BEGIN_DECLS

GType
gupnp_device_proxy_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_DEVICE_PROXY \
                (gupnp_device_proxy_get_type ())
#define GUPNP_DEVICE_PROXY(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_DEVICE_PROXY, \
                 GUPnPDeviceProxy))
#define GUPNP_DEVICE_PROXY_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_DEVICE_PROXY, \
                 GUPnPDeviceProxyClass))
#define GUPNP_IS_DEVICE_PROXY(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_DEVICE_PROXY))
#define GUPNP_IS_DEVICE_PROXY_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_DEVICE_PROXY))
#define GUPNP_DEVICE_PROXY_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_DEVICE_PROXY, \
                 GUPnPDeviceProxyClass))

typedef struct _GUPnPDeviceProxyPrivate GUPnPDeviceProxyPrivate;

typedef struct {
        GObject parent;

        GUPnPDeviceProxyPrivate *priv;
} GUPnPDeviceProxy;

typedef struct {
        GObjectClass parent_class;

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
} GUPnPDeviceProxyClass;

GUPnPDeviceProxy *
gupnp_device_proxy_new                (GUPnPContext     *context,
                                       xmlDoc           *doc,
                                       const char       *udn,
                                       const char       *location);

GUPnPContext *
gupnp_device_proxy_get_context        (GUPnPDeviceProxy *proxy);

GList *
gupnp_device_proxy_list_devices       (GUPnPDeviceProxy *proxy);

GList *
gupnp_device_proxy_list_device_types  (GUPnPDeviceProxy *proxy);

GUPnPDeviceProxy *
gupnp_device_proxy_get_device         (GUPnPDeviceProxy *proxy,
                                       const char       *type);

GList *
gupnp_device_proxy_list_services      (GUPnPDeviceProxy *proxy);

GList *
gupnp_device_proxy_list_service_types (GUPnPDeviceProxy *proxy);

GUPnPServiceProxy *
gupnp_device_proxy_get_service        (GUPnPDeviceProxy *proxy,
                                       const char       *type);

G_END_DECLS

#endif /* __GUPNP_DEVICE_PROXY_H__ */