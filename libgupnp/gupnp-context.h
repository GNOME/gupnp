/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
        /*<private>*/
        /**
         * _gupnp_reserved1:(skip):
         *
         * Padding
         */
        void (* _gupnp_reserved1) (void);
        /**
         * _gupnp_reserved2:(skip):
         */
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
};

G_DEPRECATED_FOR(gupnp_context_new_for_address)
GUPnPContext *
gupnp_context_new (const char *iface, guint port, GError **error);

GUPnPContext *
gupnp_context_new_for_address (GInetAddress *addr,
                               guint16 port,
                               GSSDPUDAVersion uda_version,
                               GError **error);

GUPnPContext *
gupnp_context_new_full (const char *iface,
                        GInetAddress *addr,
                        guint16 port,
                        GSSDPUDAVersion uda_version,
                        GError **error);

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
