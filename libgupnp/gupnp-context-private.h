/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
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

#ifndef __GUPNP_CONTEXT_PRIVATE_H__
#define __GUPNP_CONTEXT_PRIVATE_H__

#include <libsoup/soup.h>

#include "gupnp-acl-private.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL SoupURI *
_gupnp_context_get_server_uri (GUPnPContext *context);

G_GNUC_INTERNAL void
_gupnp_context_add_server_handler_with_data (GUPnPContext *context,
                                             const char *path,
                                             AclServerHandler *data);

G_GNUC_INTERNAL gboolean
gupnp_context_ip_is_ours (GUPnPContext *context, const char *address);

G_GNUC_INTERNAL gboolean
gupnp_context_validate_host_header (GUPnPContext *context, const char *host);

gboolean
validate_host_header (const char *host_header,
                      const char *host_ip,
                      guint context_port);

G_END_DECLS

#endif /* __GUPNP_CONTEXT_PRIVATE_H__ */
