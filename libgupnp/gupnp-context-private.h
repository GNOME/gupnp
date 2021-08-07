/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_CONTEXT_PRIVATE_H
#define GUPNP_CONTEXT_PRIVATE_H

#include <libsoup/soup.h>

#include "gupnp-acl-private.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL GUri *
_gupnp_context_get_server_uri (GUPnPContext *context);

G_GNUC_INTERNAL void
_gupnp_context_add_server_handler_with_data (GUPnPContext *context,
                                             const char *path,
                                             AclServerHandler *data);

G_GNUC_INTERNAL GUri *
gupnp_context_rewrite_uri_to_uri (GUPnPContext *context, const char *uri);

G_GNUC_INTERNAL gboolean
gupnp_context_validate_host_header (GUPnPContext *context, const char *host);

gboolean
validate_host_header (const char *host_header,
                      const char *host_ip,
                      guint context_port);

G_END_DECLS

#endif /* GUPNP_CONTEXT_PRIVATE_H */
