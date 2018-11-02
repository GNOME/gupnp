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

#define GUPNP_TYPE_DEVICE_PROXY \
                (gupnp_device_proxy_get_type ())

G_DECLARE_DERIVABLE_TYPE (GUPnPDeviceProxy,
                          gupnp_device_proxy,
                          GUPNP,
                          DEVICE_PROXY,
                          GUPnPDeviceInfo)

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
