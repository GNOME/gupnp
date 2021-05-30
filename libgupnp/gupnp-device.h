/*
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_DEVICE_H
#define GUPNP_DEVICE_H

#include "gupnp-device-info.h"

G_BEGIN_DECLS

#define GUPNP_TYPE_DEVICE (gupnp_device_get_type ())

G_DECLARE_DERIVABLE_TYPE (GUPnPDevice,
                          gupnp_device,
                          GUPNP,
                          DEVICE,
                          GUPnPDeviceInfo)

struct _GUPnPDeviceClass {
        GUPnPDeviceInfoClass parent_class;

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
};

G_END_DECLS

#endif /* GUPNP_DEVICE_H */
