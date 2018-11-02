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

#ifndef GUPNP_DEVICE_PROXY_H
#define GUPNP_DEVICE_PROXY_H

#include "gupnp-device-info.h"

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
typedef struct _GUPnPDeviceProxy GUPnPDeviceProxy;
typedef struct _GUPnPDeviceProxyClass GUPnPDeviceProxyClass;

/**
 * GUPnPDeviceProxy:
 *
 * This struct contains private data only, and should be accessed using the
 * functions below.
 */
struct _GUPnPDeviceProxy {
        GUPnPDeviceInfo parent;

        GUPnPDeviceProxyPrivate *priv;
};

struct _GUPnPDeviceProxyClass {
        GUPnPDeviceInfoClass parent_class;

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
};

G_END_DECLS

#endif /* GUPNP_DEVICE_PROXY_H */
