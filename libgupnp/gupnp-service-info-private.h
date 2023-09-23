// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "gupnp-service-info.h"
#include "gupnp-service-introspection.h"

G_GNUC_INTERNAL GUPnPServiceIntrospection *
gupnp_service_info_get_introspection (GUPnPServiceInfo *info);
