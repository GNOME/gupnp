/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
