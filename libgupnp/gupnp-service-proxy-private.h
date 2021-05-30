/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef GUPNP_SERVICE_PROXY_PRIVATE_H
#define GUPNP_SERVICE_PROXY_PRIVATE_H

#include "gupnp-service-proxy.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL void
gupnp_service_proxy_remove_action (GUPnPServiceProxy       *proxy,
                                   GUPnPServiceProxyAction *action);

G_END_DECLS

#endif /* GUPNP_SERVICE_PROXY_ACTION_H */
