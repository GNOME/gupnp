/*
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_SERVICE_INTROSPECTION_PRIVATE_H
#define GUPNP_SERVICE_INTROSPECTION_PRIVATE_H

#include <libxml/tree.h>

#include "gupnp-service-introspection.h"

G_GNUC_INTERNAL GUPnPServiceIntrospection *
gupnp_service_introspection_new (xmlDoc *scpd, GError **error);

#endif /* GUPNP_SERVICE_INTROSPECTION_PRIVATE_H */
