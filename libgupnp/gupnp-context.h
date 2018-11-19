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

#ifndef GUPNP_CONTEXT_H
#define GUPNP_CONTEXT_H

#include <libgssdp/gssdp-client.h>
#include <libsoup/soup-server.h>
#include <libsoup/soup-session.h>

#include "gupnp-acl.h"

G_BEGIN_DECLS

#define GUPNP_TYPE_CONTEXT \
                (gupnp_context_get_type ())

G_DECLARE_DERIVABLE_TYPE (GUPnPContext, gupnp_context, GUPNP, CONTEXT, GSSDPClient)

struct _GUPnPContextClass {
        GSSDPClientClass parent_class;

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
};

GUPnPContext *
gupnp_context_new                      (const char   *iface,
                                        guint         port,
                                        GError      **error);

guint
gupnp_context_get_port                 (GUPnPContext *context);

SoupServer *
gupnp_context_get_server               (GUPnPContext *context);

SoupSession *
gupnp_context_get_session              (GUPnPContext *context);

void
gupnp_context_set_subscription_timeout (GUPnPContext *context,
                                        guint         timeout);

guint
gupnp_context_get_subscription_timeout (GUPnPContext *context);

void
gupnp_context_set_default_language     (GUPnPContext *context,
                                        const char   *language);

const char *
gupnp_context_get_default_language     (GUPnPContext *context);

void
gupnp_context_host_path                (GUPnPContext *context,
                                        const char   *local_path,
                                        const char   *server_path);

gboolean
gupnp_context_host_path_for_agent      (GUPnPContext *context,
                                        const char   *local_path,
                                        const char   *server_path,
                                        GRegex       *user_agent);

void
gupnp_context_unhost_path              (GUPnPContext *context,
                                        const char   *server_path);

GUPnPAcl *
gupnp_context_get_acl                  (GUPnPContext *context);

void
gupnp_context_set_acl                  (GUPnPContext *context,
                                        GUPnPAcl     *acl);

void
gupnp_context_add_server_handler       (GUPnPContext *context,
                                        gboolean use_acl,
                                        const char *path,
                                        SoupServerCallback callback,
                                        gpointer user_data,
                                        GDestroyNotify destroy);

void
gupnp_context_remove_server_handler    (GUPnPContext *context,
                                        const char *path);

char *
gupnp_context_rewrite_uri              (GUPnPContext *context,
                                        const char *uri);
G_END_DECLS

#endif /* GUPNP_CONTEXT_H */
