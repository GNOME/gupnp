/*
 * Copyright (C) 2007 OpenedHand Ltd.
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

#ifndef __GUPNP_DEVICE_H__
#define __GUPNP_DEVICE_H__

#include "gupnp-device-info.h"

G_BEGIN_DECLS

GType
gupnp_device_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_DEVICE \
                (gupnp_device_get_type ())
#define GUPNP_DEVICE(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_DEVICE, \
                 GUPnPDevice))
#define GUPNP_DEVICE_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_DEVICE, \
                 GUPnPDeviceClass))
#define GUPNP_IS_DEVICE(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_DEVICE))
#define GUPNP_IS_DEVICE_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_DEVICE))
#define GUPNP_DEVICE_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_DEVICE, \
                 GUPnPDeviceClass))

typedef struct _GUPnPDevicePrivate GUPnPDevicePrivate;
typedef struct _GUPnPDevice GUPnPDevice;
typedef struct _GUPnPDeviceClass GUPnPDeviceClass;

/**
 * GUPnPDevice:
 *
 * This struct contains private data only, and should be accessed using the
 * functions below.
 */
struct _GUPnPDevice {
        GUPnPDeviceInfo parent;

        GUPnPDevicePrivate *priv;
};

struct _GUPnPDeviceClass {
        GUPnPDeviceInfoClass parent_class;

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
};

G_END_DECLS

#endif /* __GUPNP_DEVICE_H__ */
