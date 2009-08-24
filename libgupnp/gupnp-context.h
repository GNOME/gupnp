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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GUPNP_CONTEXT_H__
#define __GUPNP_CONTEXT_H__

#include <libgssdp/gssdp-client.h>
#include <libsoup/soup-server.h>
#include <libsoup/soup-session.h>

G_BEGIN_DECLS

GType
gupnp_context_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_CONTEXT \
                (gupnp_context_get_type ())
#define GUPNP_CONTEXT(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_CONTEXT, \
                 GUPnPContext))
#define GUPNP_CONTEXT_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_CONTEXT, \
                 GUPnPContextClass))
#define GUPNP_IS_CONTEXT(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_CONTEXT))
#define GUPNP_IS_CONTEXT_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_CONTEXT))
#define GUPNP_CONTEXT_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_CONTEXT, \
                 GUPnPContextClass))

typedef struct _GUPnPContextPrivate GUPnPContextPrivate;

/**
 * GUPnPContext:
 *
 * This struct contains private data only, and should be accessed using the
 * functions below.
 */
typedef struct {
        GSSDPClient parent;

        GUPnPContextPrivate *priv;
} GUPnPContext;

typedef struct {
        GSSDPClientClass parent_class;

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
} GUPnPContextClass;

GUPnPContext *
gupnp_context_new                      (GMainContext *main_context,
                                        const char   *interface,
                                        guint         port,
                                        GError      **error);

#ifndef GUPNP_DISABLE_DEPRECATED

const char *
gupnp_context_get_host_ip              (GUPnPContext *context);

#endif /* GUPNP_DISABLE_DEPRECATED */

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
gupnp_context_host_path                (GUPnPContext *context,
                                        const char   *local_path,
                                        const char   *server_path);

void
gupnp_context_unhost_path              (GUPnPContext *context,
                                        const char   *server_path);

G_END_DECLS

#endif /* __GUPNP_CONTEXT_H__ */
