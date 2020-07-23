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

#ifndef GUPNP_SERVICE_PROXY_H
#define GUPNP_SERVICE_PROXY_H

#include "gupnp-error.h"
#include "gupnp-service-info.h"

G_BEGIN_DECLS

GType
gupnp_service_proxy_action_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_SERVICE_PROXY \
                (gupnp_service_proxy_get_type ())

G_DECLARE_DERIVABLE_TYPE (GUPnPServiceProxy,
                          gupnp_service_proxy,
                          GUPNP,
                          SERVICE_PROXY,
                          GUPnPServiceInfo)

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

G_DEPRECATED gboolean
gupnp_service_proxy_send_action    (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GError                        **error,
                                    ...) G_GNUC_NULL_TERMINATED;

G_DEPRECATED gboolean
gupnp_service_proxy_send_action_valist
                                   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GError                        **error,
                                    va_list                         var_args);

G_DEPRECATED gboolean
gupnp_service_proxy_send_action_list (GUPnPServiceProxy *proxy,
                                      const char        *action,
                                      GList             *in_names,
                                      GList             *in_values,
                                      GList             *out_names,
                                      GList             *out_types,
                                      GList            **out_values,
                                      GError           **error);

G_DEPRECATED GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GUPnPServiceProxyActionCallback callback,
                                    gpointer                        user_data,
                                    ...) G_GNUC_NULL_TERMINATED;

G_DEPRECATED GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action_valist
                                   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GUPnPServiceProxyActionCallback callback,
                                    gpointer                        user_data,
                                    va_list                         var_args);

G_DEPRECATED GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action_list
                                   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GList                          *in_names,
                                    GList                          *in_values,
                                    GUPnPServiceProxyActionCallback callback,
                                    gpointer                        user_data);

G_DEPRECATED gboolean
gupnp_service_proxy_end_action     (GUPnPServiceProxy              *proxy,
                                    GUPnPServiceProxyAction        *action,
                                    GError                        **error,
                                    ...) G_GNUC_NULL_TERMINATED;

G_DEPRECATED gboolean
gupnp_service_proxy_end_action_valist
                                   (GUPnPServiceProxy              *proxy,
                                    GUPnPServiceProxyAction        *action,
                                    GError                        **error,
                                    va_list                         var_args);

G_DEPRECATED gboolean
gupnp_service_proxy_end_action_list
                                  (GUPnPServiceProxy       *proxy,
                                   GUPnPServiceProxyAction *action,
                                   GList                   *out_names,
                                   GList                   *out_types,
                                   GList                  **out_values,
                                   GError                  **error);

G_DEPRECATED gboolean
gupnp_service_proxy_end_action_hash
                                   (GUPnPServiceProxy              *proxy,
                                    GUPnPServiceProxyAction        *action,
                                    GHashTable                     *hash,
                                    GError                        **error);

G_DEPRECATED void
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

/* New action API */

GUPnPServiceProxyAction *
gupnp_service_proxy_action_new (const char *action,
                                ...) G_GNUC_NULL_TERMINATED;

GUPnPServiceProxyAction *
gupnp_service_proxy_action_new_from_list (const char *action,
                                          GList      *in_names,
                                          GList      *in_values);

GUPnPServiceProxyAction *
gupnp_service_proxy_action_ref (GUPnPServiceProxyAction *action);

void
gupnp_service_proxy_action_unref (GUPnPServiceProxyAction *action);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GUPnPServiceProxyAction,
                               gupnp_service_proxy_action_unref)

gboolean
gupnp_service_proxy_action_get_result (GUPnPServiceProxyAction *action,
                                       GError                 **error,
                                       ...) G_GNUC_NULL_TERMINATED;

gboolean
gupnp_service_proxy_action_get_result_list (GUPnPServiceProxyAction *action,
                                            GList                   *out_names,
                                            GList                   *out_types,
                                            GList                  **out_values,
                                            GError                 **error);
gboolean
gupnp_service_proxy_action_get_result_hash (GUPnPServiceProxyAction *action,
                                            GHashTable              *out_hash,
                                            GError                 **error);


void
gupnp_service_proxy_call_action_async (GUPnPServiceProxy       *proxy,
                                       GUPnPServiceProxyAction *action,
                                       GCancellable            *cancellable,
                                       GAsyncReadyCallback     callback,
                                       gpointer                user_data);


GUPnPServiceProxyAction *
gupnp_service_proxy_call_action_finish (GUPnPServiceProxy *proxy,
                                        GAsyncResult      *result,
                                        GError           **error);

GUPnPServiceProxyAction *
gupnp_service_proxy_call_action (GUPnPServiceProxy       *proxy,
                                 GUPnPServiceProxyAction *action,
                                 GCancellable            *cancellable,
                                 GError                 **error);

G_END_DECLS

#endif /* GUPNP_SERVICE_PROXY_H */
