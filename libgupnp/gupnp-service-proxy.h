/* 
 * Copyright (C) 2006 OpenedHand Ltd.
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

#ifndef __GUPNP_SERVICE_PROXY_H__
#define __GUPNP_SERVICE_PROXY_H__

#include <libxml/tree.h>

#include "gupnp-service-info.h"
#include "gupnp-service-proxy.h"

G_BEGIN_DECLS

GType
gupnp_service_proxy_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_SERVICE_PROXY \
                (gupnp_service_proxy_get_type ())
#define GUPNP_SERVICE_PROXY(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_SERVICE_PROXY, \
                 GUPnPServiceProxy))
#define GUPNP_SERVICE_PROXY_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_SERVICE_PROXY, \
                 GUPnPServiceProxyClass))
#define GUPNP_IS_SERVICE_PROXY(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_SERVICE_PROXY))
#define GUPNP_IS_SERVICE_PROXY_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_SERVICE_PROXY))
#define GUPNP_SERVICE_PROXY_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_SERVICE_PROXY, \
                 GUPnPServiceProxyClass))

typedef struct _GUPnPServiceProxyPrivate GUPnPServiceProxyPrivate;

typedef struct {
        GObject parent;

        GUPnPServiceProxyPrivate *priv;
} GUPnPServiceProxy;

typedef struct {
        GObjectClass parent_class;

        /* signals */
        void (* subscription_lost) (GUPnPServiceProxy *proxy,
                                    const GError      *reason);

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
} GUPnPServiceProxyClass;

typedef struct _GUPnPServiceProxyAction GUPnPServiceProxyAction;

typedef void
(* GUPnPServiceProxyActionCallback) (GUPnPServiceProxy       *proxy,
                                     GUPnPServiceProxyAction *action,
                                     gpointer                 user_data);

typedef void
(* GUPnPServiceProxyNotifyCallback) (GUPnPServiceProxy       *proxy,
                                     const char              *variable,
                                     GValue                  *value,
                                     gpointer                 user_data);

GUPnPServiceProxy *
gupnp_service_proxy_new            (xmlDoc                         *doc,
                                    const char                     *udn,
                                    const char                     *type,
                                    const char                     *location);

gboolean
gupnp_service_proxy_send_action    (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GError                        **error,
                                    ...) G_GNUC_NULL_TERMINATED;

gboolean
gupnp_service_proxy_send_action_valist
                                   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GError                        **error,
                                    va_list                         var_args);

GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GUPnPServiceProxyActionCallback callback,
                                    gpointer                        user_data,
                                    ...) G_GNUC_NULL_TERMINATED;

GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action_valist
                                   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GUPnPServiceProxyActionCallback callback,
                                    gpointer                        user_data,
                                    va_list                         var_args);

gboolean
gupnp_service_proxy_end_action     (GUPnPServiceProxy              *proxy,
                                    GUPnPServiceProxyAction        *action,
                                    GError                        **error,
                                    ...) G_GNUC_NULL_TERMINATED;

gboolean
gupnp_service_proxy_end_action_valist
                                   (GUPnPServiceProxy              *proxy,
                                    GUPnPServiceProxyAction        *action,
                                    GError                        **error,
                                    va_list                         var_args);

void
gupnp_service_proxy_cancel_action  (GUPnPServiceProxy              *proxy,
                                    GUPnPServiceProxyAction        *action);

guint
gupnp_service_proxy_add_notify     (GUPnPServiceProxy              *proxy,
                                    const char                     *variable,
                                    GUPnPServiceProxyNotifyCallback callback,
                                    gpointer                        user_data);

void
gupnp_service_proxy_remove_notify  (GUPnPServiceProxy              *proxy,
                                    guint                           id);

void
gupnp_service_proxy_set_subscribed (GUPnPServiceProxy              *proxy,
                                    gboolean                        subscribed);

gboolean
gupnp_service_proxy_get_subscribed (GUPnPServiceProxy              *proxy);

G_END_DECLS

#endif /* __GUPNP_SERVICE_PROXY_H__ */
