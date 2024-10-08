/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_SERVICE_PROXY_H
#define GUPNP_SERVICE_PROXY_H

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
        /**
         * subscription_lost:
         *
         * Test
         */
        void (* subscription_lost) (GUPnPServiceProxy *proxy,
                                    const GError      *reason);

#ifndef GOBJECT_INTROSPECTION_SKIP
        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
#endif
};

/**
 * GUPnPServiceProxyAction:
 *
 * Opaque structure for holding in-progress action data.
 **/
typedef struct _GUPnPServiceProxyAction GUPnPServiceProxyAction;

G_DECLARE_FINAL_TYPE (GUPnPServiceProxyActionIter,
                      gupnp_service_proxy_action_iter,
                      GUPNP,
                      SERVICE_PROXY_ACTION_ITER,
                      GObject);
/**
 * GUPnPServiceProxyActionIter:
 *
 * An opaque object representing an iterator over the out parameters of an action
 * Since: 1.6.6
 **/

/**
 * GUPnPServiceProxyActionCallback:
 * @proxy: The #GUPnPServiceProxy @action is called from
 * @action: The #GUPnPServiceProxyAction in progress
 * @user_data: User data
 *
 * Callback notifying that @action on @proxy has returned and
 * gupnp_service_proxy_end_action() etc can be called.
 * Deprecated: 1.2.0
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
gupnp_service_proxy_action_new_plain (const char *action);

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

GUPnPServiceProxyAction *
gupnp_service_proxy_action_add_argument (GUPnPServiceProxyAction *action,
                                         const char *name,
                                         const GValue *value);

gboolean
gupnp_service_proxy_action_set (GUPnPServiceProxyAction *action,
                                const char *key,
                                const GValue *value,
                                GError **error);

GUPnPServiceProxyActionIter *
gupnp_service_proxy_action_iterate (GUPnPServiceProxyAction *action,
                                    GError **error);

gboolean
gupnp_service_proxy_action_iter_next (GUPnPServiceProxyActionIter *self);

const char *
gupnp_service_proxy_action_iter_get_name (GUPnPServiceProxyActionIter *self);

gboolean
gupnp_service_proxy_action_iter_get_value (GUPnPServiceProxyActionIter *self,
                                           GValue *value);

gboolean
gupnp_service_proxy_action_iter_get_value_as (GUPnPServiceProxyActionIter *self,
                                              GType type,
                                              GValue *value);

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

void
gupnp_service_proxy_set_credentials (GUPnPServiceProxy *proxy,
                                     const char        *user,
                                     const char        *password);

G_END_DECLS

#endif /* GUPNP_SERVICE_PROXY_H */
