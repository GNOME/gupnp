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

#ifndef __GUPNP_SERVICE_PROXY_H__
#define __GUPNP_SERVICE_PROXY_H__

#include "gupnp-error.h"
#include "gupnp-service-info.h"

G_BEGIN_DECLS

GType
gupnp_service_proxy_get_type (void) G_GNUC_CONST;

GType
gupnp_service_proxy_action_get_type (void) G_GNUC_CONST;

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
typedef struct _GUPnPServiceProxy GUPnPServiceProxy;
typedef struct _GUPnPServiceProxyClass GUPnPServiceProxyClass;

/**
 * GUPnPServiceProxy:
 *
 * This struct contains private data only, and should be accessed using the
 * functions below.
 */
struct _GUPnPServiceProxy {
        GUPnPServiceInfo parent;

        GUPnPServiceProxyPrivate *priv;
};

struct _GUPnPServiceProxyClass {
        GUPnPServiceInfoClass parent_class;

        /* signals */
        void (* subscription_lost) (GUPnPServiceProxy *proxy,
                                    const GError      *reason);

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
};

/**
 * GUPnPServiceProxyAction:
 *
 * Opaque structure for holding in-progress action data.
 **/
typedef struct _GUPnPServiceProxyAction GUPnPServiceProxyAction;

/**
 * GUPnPServiceProxyActionCallback:
 * @proxy: The #GUPnPServiceProxy @action is called from
 * @action: The #GUPnPServiceProxyAction in progress
 * @user_data: User data
 *
 * Callback notifying that @action on @proxy has returned and
 * gupnp_service_proxy_end_action() etc can be called.
 **/
typedef void (* GUPnPServiceProxyActionCallback) (
                                     GUPnPServiceProxy       *proxy,
                                     GUPnPServiceProxyAction *action,
                                     gpointer                 user_data);

/**
 * GUPnPServiceProxyNotifyCallback:
 * @proxy: The #GUPnPServiceProxy the notification originates from
 * @variable: The name of the variable being notified
 * @value: The #GValue of the variable being notified
 * @user_data: User data
 *
 * Callback notifying that the state variable @variable on @proxy has changed to
 * @value.
 **/
typedef void (* GUPnPServiceProxyNotifyCallback) (GUPnPServiceProxy *proxy,
                                                  const char        *variable,
                                                  GValue            *value,
                                                  gpointer           user_data);

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

#ifndef GUPNP_DISABLE_DEPRECATED
gboolean
gupnp_service_proxy_send_action_hash
                                   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GError                        **error,
                                    GHashTable                     *in_hash,
                                    GHashTable                     *out_hash) G_GNUC_DEPRECATED;
#endif

gboolean
gupnp_service_proxy_send_action_list (GUPnPServiceProxy *proxy,
                                      const char        *action,
                                      GError           **error,
                                      GList             *in_names,
                                      GList             *in_values,
                                      GList             *out_names,
                                      GList             *out_types,
                                      GList            **out_values);

gboolean
gupnp_service_proxy_send_action_list_gi (GUPnPServiceProxy *proxy,
                                         const char        *action,
                                         GList             *in_names,
                                         GList             *in_values,
                                         GList             *out_names,
                                         GList             *out_types,
                                         GList            **out_values,
                                         GError           **error);
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

GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action_list
                                   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GList                          *in_names,
                                    GList                          *in_values,
                                    GUPnPServiceProxyActionCallback callback,
                                    gpointer                        user_data);

#ifndef GUPNP_DISABLE_DEPRECATED
GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action_hash
                                   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GUPnPServiceProxyActionCallback callback,
                                    gpointer                        user_data,
                                    GHashTable                     *hash) G_GNUC_DEPRECATED;
#endif

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

gboolean
gupnp_service_proxy_end_action_list
                                  (GUPnPServiceProxy       *proxy,
                                   GUPnPServiceProxyAction *action,
                                   GError                  **error,
                                   GList                   *out_names,
                                   GList                   *out_types,
                                   GList                  **out_values);

gboolean
gupnp_service_proxy_end_action_list_gi
                                  (GUPnPServiceProxy       *proxy,
                                   GUPnPServiceProxyAction *action,
                                   GList                   *out_names,
                                   GList                   *out_types,
                                   GList                  **out_values,
                                   GError                  **error);

gboolean
gupnp_service_proxy_end_action_hash_gi
                                   (GUPnPServiceProxy              *proxy,
                                    GUPnPServiceProxyAction        *action,
                                    GHashTable                     *hash,
                                    GError                        **error);

gboolean
gupnp_service_proxy_end_action_hash
                                   (GUPnPServiceProxy              *proxy,
                                    GUPnPServiceProxyAction        *action,
                                    GError                        **error,
                                    GHashTable                     *hash);

void
gupnp_service_proxy_cancel_action  (GUPnPServiceProxy              *proxy,
                                    GUPnPServiceProxyAction        *action);

gboolean
gupnp_service_proxy_add_notify     (GUPnPServiceProxy              *proxy,
                                    const char                     *variable,
                                    GType                           type,
                                    GUPnPServiceProxyNotifyCallback callback,
                                    gpointer                        user_data);

gboolean
gupnp_service_proxy_add_notify_full (GUPnPServiceProxy              *proxy,
                                     const char                     *variable,
                                     GType                           type,
                                     GUPnPServiceProxyNotifyCallback callback,
                                     gpointer                        user_data,
                                     GDestroyNotify                  notify);

gboolean
gupnp_service_proxy_add_raw_notify (GUPnPServiceProxy              *proxy,
                                    GUPnPServiceProxyNotifyCallback callback,
                                    gpointer                        user_data,
                                    GDestroyNotify                  notify);

gboolean
gupnp_service_proxy_remove_notify  (GUPnPServiceProxy              *proxy,
                                    const char                     *variable,
                                    GUPnPServiceProxyNotifyCallback callback,
                                    gpointer                        user_data);

gboolean
gupnp_service_proxy_remove_raw_notify
                                   (GUPnPServiceProxy              *proxy,
                                    GUPnPServiceProxyNotifyCallback callback,
                                    gpointer                        user_data);

void
gupnp_service_proxy_set_subscribed (GUPnPServiceProxy              *proxy,
                                    gboolean                        subscribed);

gboolean
gupnp_service_proxy_get_subscribed (GUPnPServiceProxy              *proxy);


G_END_DECLS

#endif /* __GUPNP_SERVICE_PROXY_H__ */
