/*
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_DEVICE_INFO_PRIVATE_H
#define GUPNP_DEVICE_INFO_PRIVATE_H

#include "gupnp-device-info.h"
#include "gupnp-xml-doc.h"

G_GNUC_INTERNAL GUPnPXMLDoc *
_gupnp_device_info_get_document (GUPnPDeviceInfo *info);

#endif /* GUPNP_DEVICE_INFO_PRIVATE_H */
