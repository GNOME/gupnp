/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

/**
 * SECTION:gupnp-service-proxy
 * @short_description: Proxy class for remote services.
 *
 * #GUPnPServiceProxy sends commands to a remote UPnP service and handles
 * incoming event notifications. #GUPnPServiceProxy implements the
 * #GUPnPServiceInfo interface.
 */

#include <config.h>
#include <libsoup/soup.h>
#include <gobject/gvaluecollector.h>
#include <string.h>
#include <locale.h>
#include <errno.h>

#include "gupnp-service-proxy.h"
#include "gupnp-service-proxy-action-private.h"
#include "gupnp-context-private.h"
#include "gupnp-error.h"
#include "gupnp-types.h"
#include "gena-protocol.h"
#include "http-headers.h"
#include "gvalue-util.h"

struct _GUPnPServiceProxyPrivate {
        gboolean subscribed;

        GList *pending_actions;

        char *path; /* Path to this proxy */

        char *sid; /* Subscription ID */
        GSource *subscription_timeout_src;

        guint32 seq; /* Event sequence number */

        GHashTable *notify_hash;

        GList *pending_messages; /* Pending SoupMessages from this proxy */

        GList *pending_notifies; /* Pending notifications to be sent (xmlDoc) */
        GSource *notify_idle_src; /* Idle handler src of notification emiter */
};
typedef struct _GUPnPServiceProxyPrivate GUPnPServiceProxyPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GUPnPServiceProxy,
                            gupnp_service_proxy,
                            GUPNP_TYPE_SERVICE_INFO)

enum {
        PROP_0,
        PROP_SUBSCRIBED
};

enum {
        SUBSCRIPTION_LOST,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef struct {
        GType type;       /* type of the variable this notification is for */

        GList *callbacks; /* list of CallbackData */
        GList *next_emit; /* pointer into callbacks as which callback to call next*/
} NotifyData;

typedef struct {
        GUPnPServiceProxyNotifyCallback callback;
        GDestroyNotify notify;
        gpointer user_data;
} CallbackData;

typedef struct {
        char *sid;
        guint32 seq;

        xmlDoc *doc;
} EmitNotifyData;

static void
subscribe_got_response (SoupSession       *session,
                        SoupMessage       *msg,
                        GUPnPServiceProxy *proxy);
static void
subscribe (GUPnPServiceProxy *proxy);
static void
unsubscribe (GUPnPServiceProxy *proxy);

void
gupnp_service_proxy_remove_action (GUPnPServiceProxy       *proxy,
                                   GUPnPServiceProxyAction *action)
{
        GUPnPServiceProxyPrivate *priv;

        priv = gupnp_service_proxy_get_instance_private (proxy);

        priv->pending_actions = g_list_remove (priv->pending_actions,
                                               action);
}

static void
callback_data_free (CallbackData *data)
{
        if (data->notify)
                data->notify (data->user_data);

        g_slice_free (CallbackData, data);
}

static void
notify_data_free (NotifyData *data)
{
        g_list_free_full (data->callbacks,
                          (GDestroyNotify) callback_data_free);

        g_slice_free (NotifyData, data);
}

/* Steals doc reference */
static EmitNotifyData *
emit_notify_data_new (const char *sid,
                      guint32     seq,
                      xmlDoc     *doc)
{
        EmitNotifyData *data;

        data = g_slice_new (EmitNotifyData);

        data->sid = g_strdup (sid);
        data->seq = seq;
        data->doc = doc;

        return data;
}

static void
emit_notify_data_free (EmitNotifyData *data)
{
        g_free (data->sid);
        xmlFreeDoc (data->doc);

        g_slice_free (EmitNotifyData, data);
}

static void
gupnp_service_proxy_init (GUPnPServiceProxy *proxy)
{
        static int proxy_counter = 0;
        GUPnPServiceProxyPrivate *priv;

        /* Generate unique path */
        priv = gupnp_service_proxy_get_instance_private (proxy);
        priv->path = g_strdup_printf ("/ServiceProxy%d", proxy_counter);
        proxy_counter++;

        /* Set up notify hash */
        priv->notify_hash =
                g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       (GDestroyNotify) notify_data_free);
}

static void
gupnp_service_proxy_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
        GUPnPServiceProxy *proxy;

        proxy = GUPNP_SERVICE_PROXY (object);

        switch (property_id) {
        case PROP_SUBSCRIBED:
                gupnp_service_proxy_set_subscribed
                                (proxy, g_value_get_boolean (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_service_proxy_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
        GUPnPServiceProxy *proxy;
        GUPnPServiceProxyPrivate *priv;

        proxy = GUPNP_SERVICE_PROXY (object);
        priv = gupnp_service_proxy_get_instance_private (proxy);

        switch (property_id) {
        case PROP_SUBSCRIBED:
                g_value_set_boolean (value, priv->subscribed);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_service_proxy_dispose (GObject *object)
{
        GUPnPServiceProxy *proxy;
        GUPnPServiceProxyPrivate *priv;
        GObjectClass *object_class;
        GUPnPContext *context;
        SoupSession *session;

        proxy = GUPNP_SERVICE_PROXY (object);
        priv = gupnp_service_proxy_get_instance_private (proxy);

        /* Unsubscribe */
        if (priv->subscribed) {
                unsubscribe (proxy);

                priv->subscribed = FALSE;
        }

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));

        /* Remove server handler */
        if (context) {
            SoupServer *server;

            server = gupnp_context_get_server (context);
            soup_server_remove_handler (server, priv->path);
        }

        /* Cancel pending actions */
        while (priv->pending_actions) {
                GUPnPServiceProxyAction *action;

                action = priv->pending_actions->data;
                priv->pending_actions =
                        g_list_delete_link (priv->pending_actions,
                                            priv->pending_actions);

                g_cancellable_cancel (action->cancellable);
        }

        /* Cancel pending messages */
        if (context)
                session = gupnp_context_get_session (context);
        else
                session = NULL; /* Not the first time dispose is called. */

        while (priv->pending_messages) {
                SoupMessage *msg;

                msg = priv->pending_messages->data;

                soup_session_cancel_message (session,
                                             msg,
                                             SOUP_STATUS_CANCELLED);

                priv->pending_messages =
                        g_list_delete_link (priv->pending_messages,
                                            priv->pending_messages);
        }

        /* Cancel pending notifications */
        g_clear_pointer (&priv->notify_idle_src, g_source_destroy);

        g_list_free_full (priv->pending_notifies,
                          (GDestroyNotify) emit_notify_data_free);
        priv->pending_notifies = NULL;

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_service_proxy_parent_class);
        object_class->dispose (object);
}

static void
gupnp_service_proxy_finalize (GObject *object)
{
        GUPnPServiceProxy *proxy;
        GUPnPServiceProxyPrivate *priv;
        GObjectClass *object_class;

        proxy = GUPNP_SERVICE_PROXY (object);
        priv = gupnp_service_proxy_get_instance_private (proxy);

        g_free (priv->path);

        g_hash_table_destroy (priv->notify_hash);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_service_proxy_parent_class);
        object_class->finalize (object);
}

static void
gupnp_service_proxy_class_init (GUPnPServiceProxyClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_service_proxy_set_property;
        object_class->get_property = gupnp_service_proxy_get_property;
        object_class->dispose      = gupnp_service_proxy_dispose;
        object_class->finalize     = gupnp_service_proxy_finalize;

        /**
         * GUPnPServiceProxy:subscribed:
         *
         * Whether we are subscribed to this service.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_SUBSCRIBED,
                 g_param_spec_boolean ("subscribed",
                                       "Subscribed",
                                       "Whether we are subscribed to this "
                                       "service",
                                       FALSE,
                                       G_PARAM_READWRITE |
                                       G_PARAM_STATIC_NAME |
                                       G_PARAM_STATIC_NICK |
                                       G_PARAM_STATIC_BLURB));

        /**
         * GUPnPServiceProxy::subscription-lost:
         * @proxy: The #GUPnPServiceProxy that received the signal
         * @error: (type GError):A pointer to a #GError describing why the subscription has
         * been lost
         *
         * Emitted whenever the subscription to this service has been lost due
         * to an error condition.
         **/
        signals[SUBSCRIPTION_LOST] =
                g_signal_new ("subscription-lost",
                              GUPNP_TYPE_SERVICE_PROXY,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GUPnPServiceProxyClass,
                                               subscription_lost),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_POINTER);
}

/**
 * gupnp_service_proxy_send_action:
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @error: (inout)(optional)(nullable): The location where to store any error, or %NULL
 * @...: tuples of in parameter name, in parameter type, and in parameter
 * value, followed by %NULL, and then tuples of out parameter name,
 * out parameter type, and out parameter value location, terminated with %NULL
 *
 * Sends action @action with parameters @Varargs to the service exposed by
 * @proxy synchronously. If an error occurred, @error will be set. In case of
 * an UPnPError the error code will be the same in @error.
 *
 * Return value: %TRUE if sending the action was succesful.
 *
 * Deprecated: 1.2.0: Use gupnp_service_proxy_action_new() and
 * gupnp_service_proxy_call_action()
 **/
gboolean
gupnp_service_proxy_send_action (GUPnPServiceProxy *proxy,
                                 const char        *action,
                                 GError           **error,
                                 ...)
{
        va_list var_args;
        gboolean ret;

        va_start (var_args, error);
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        ret = gupnp_service_proxy_send_action_valist (proxy,
                                                      action,
                                                      error,
                                                      var_args);
        G_GNUC_END_IGNORE_DEPRECATIONS
        va_end (var_args);

        return ret;
}

/**
 * gupnp_service_proxy_send_action_valist:
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @error: (inout)(optional)(nullable): The location where to store any error, or %NULL
 * @var_args: va_list of tuples of in parameter name, in parameter type, and in
 * parameter value, followed by %NULL, and then tuples of out parameter name,
 * out parameter type, and out parameter value location
 *
 * See gupnp_service_proxy_send_action().
 *
 * Return value: %TRUE if sending the action was succesful.
 *
 * Deprecated: 1.2.0
 **/
gboolean
gupnp_service_proxy_send_action_valist (GUPnPServiceProxy *proxy,
                                        const char        *action_name,
                                        GError           **error,
                                        va_list            var_args)
{
        GList *in_names = NULL, *in_values = NULL;
        GHashTable *out_hash = NULL;
        va_list var_args_copy;
        gboolean result;
        GError *local_error;
        GUPnPServiceProxyAction *handle;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action_name, FALSE);


        VAR_ARGS_TO_IN_LIST (var_args, in_names, in_values);
        G_VA_COPY (var_args_copy, var_args);
        out_hash = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          gvalue_free);
        VAR_ARGS_TO_OUT_HASH_TABLE (var_args, out_hash);

        handle = gupnp_service_proxy_action_new_from_list (action_name, in_names, in_values);

        if (gupnp_service_proxy_call_action (proxy, handle, NULL, error) == NULL) {
                result = FALSE;
                goto out;
        }

        result = gupnp_service_proxy_action_get_result_hash (handle,
                                                             out_hash,
                                                             &local_error);

        if (local_error == NULL) {
                OUT_HASH_TABLE_TO_VAR_ARGS (out_hash, var_args_copy);
        } else {
                g_propagate_error (error, local_error);
        }
out:
        gupnp_service_proxy_action_unref (handle);
        va_end (var_args_copy);
        g_list_free_full (in_names, g_free);
        g_list_free_full (in_values, gvalue_free);
        g_hash_table_unref (out_hash);

        return result;
}

/**
 * gupnp_service_proxy_send_action_list:
 * @proxy: (transfer none): A #GUPnPServiceProxy
 * @action: An action
 * @in_names: (element-type utf8) (transfer none): #GList of 'in' parameter
 * names (as strings)
 * @in_values: (element-type GValue) (transfer none): #GList of values (as
 * #GValue) that line up with @in_names
 * @out_names: (element-type utf8) (transfer none): #GList of 'out' parameter
 * names (as strings)
 * @out_types: (element-type GType) (transfer none): #GList of types (as #GType)
 * that line up with @out_names
 * @out_values: (element-type GValue) (transfer full) (out): #GList of values
 * (as #GValue) that line up with @out_names and @out_types.
 * @error: (inout)(optional)(nullable): The location where to store any error, or %NULL
 *
 * The synchronous variant of #gupnp_service_proxy_begin_action_list and
 * #gupnp_service_proxy_end_action_list.
 *
 * Return value: %TRUE if sending the action was succesful.
 *
 * Deprecated: 1.2.0: Use gupnp_service_proxy_action_new_from_list() and gupnp_service_proxy_call_action()
 *
 **/
gboolean
gupnp_service_proxy_send_action_list (GUPnPServiceProxy *proxy,
                                      const char        *action,
                                      GList             *in_names,
                                      GList             *in_values,
                                      GList             *out_names,
                                      GList             *out_types,
                                      GList            **out_values,
                                      GError           **error)
{
        GUPnPServiceProxyAction *handle;
        gboolean result = FALSE;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);

        handle = gupnp_service_proxy_action_new_from_list (action, in_names, in_values);

        if (gupnp_service_proxy_call_action (proxy, handle, NULL, error) == NULL) {
                result = FALSE;

                goto out;
        }

        result = gupnp_service_proxy_action_get_result_list (handle,
                                                             out_names,
                                                             out_types,
                                                             out_values,
                                                             error);
out:
        gupnp_service_proxy_action_unref (handle);

        return result;
}

static void
on_legacy_async_callback (GObject *source, GAsyncResult *res, gpointer user_data)
{
        GError *error = NULL;
        GUPnPServiceProxyAction *action;

        gupnp_service_proxy_call_action_finish (GUPNP_SERVICE_PROXY (source), res, &error);
        action = (GUPnPServiceProxyAction *) user_data;

        /* Do not perform legacy call-back if action is cancelled, to comply with the old implementation */
        if (action->callback != NULL &&
            !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                if (error != NULL)
                        g_propagate_error (&action->error, error);
                action->callback (action->proxy, action, action->user_data);
        }

        g_clear_error (&error);
}

/**
 * gupnp_service_proxy_begin_action:
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @callback: (scope async): The callback to call when sending the action has succeeded
 * or failed
 * @user_data: User data for @callback
 * @...: tuples of in parameter name, in parameter type, and in parameter
 * value, terminated with %NULL
 *
 * Sends action @action with parameters @Varargs to the service exposed by
 * @proxy asynchronously, calling @callback on completion. From @callback, call
 * gupnp_service_proxy_end_action() to check for errors, to retrieve return
 * values, and to free the #GUPnPServiceProxyAction.
 *
 * Returns: (transfer none): A #GUPnPServiceProxyAction handle. This will be freed when
 * gupnp_service_proxy_cancel_action() or
 * gupnp_service_proxy_end_action_valist().
 *
 * Deprecated: 1.2.0: Use gupnp_service_proxy_action_new() and
 * gupnp_service_proxy_call_action_async()
 **/
GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action (GUPnPServiceProxy              *proxy,
                                  const char                     *action,
                                  GUPnPServiceProxyActionCallback callback,
                                  gpointer                        user_data,
                                  ...)
{
        GUPnPServiceProxyAction *ret;
        GList *in_names = NULL;
        GList *in_values = NULL;
        va_list var_args;

        va_start (var_args, user_data);
        VAR_ARGS_TO_IN_LIST (var_args, in_names, in_values);
        va_end (var_args);

        ret = gupnp_service_proxy_action_new_from_list (action, in_names, in_values);
        g_list_free_full (in_names, g_free);
        g_list_free_full (in_values, gvalue_free);

        ret->callback = callback;
        ret->user_data = user_data;

        gupnp_service_proxy_call_action_async (proxy,
                                               ret,
                                               NULL,
                                               on_legacy_async_callback,
                                               ret);

        return ret;
}

static void
on_action_cancelled (GCancellable *cancellable, gpointer user_data)
{
        GUPnPServiceProxyAction *action = (GUPnPServiceProxyAction *) user_data;

        GUPnPContext *context;
        SoupSession *session;

        if (action->msg != NULL && action->proxy != NULL) {
                context = gupnp_service_info_get_context
                                        (GUPNP_SERVICE_INFO (action->proxy));
                session = gupnp_context_get_session (context);

                soup_session_cancel_message (session,
                                             action->msg,
                                             SOUP_STATUS_CANCELLED);
                action->cancellable_connection_id = 0;
        }
}



/* Begins a basic action message */
static gboolean
prepare_action_msg (GUPnPServiceProxy *proxy,
                    GUPnPServiceProxyAction *action,
                    GError **error)
{
        char *control_url, *full_action;
        const char *service_type;

        /* Make sure we have a service type */
        service_type = gupnp_service_info_get_service_type
                                        (GUPNP_SERVICE_INFO (proxy));
        if (service_type == NULL) {
                g_propagate_error (error,
                                   g_error_new (GUPNP_SERVER_ERROR,
                                                GUPNP_SERVER_ERROR_OTHER,
                                                "No service type defined"));

                return FALSE;
        }

        /* Create message */
        control_url = gupnp_service_info_get_control_url
                                        (GUPNP_SERVICE_INFO (proxy));

        if (control_url == NULL) {
                g_propagate_error (
                        error,
                        g_error_new (GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_URL,
                                     "No valid control URL defined"));

                return FALSE;
        }

        GUPnPContext *context = NULL;
        char *local_control_url = NULL;

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));

        local_control_url = gupnp_context_rewrite_uri (context, control_url);
        g_free (control_url);

        g_clear_object (&action->msg);
        action->msg = soup_message_new (SOUP_METHOD_POST, local_control_url);
        g_free (local_control_url);

        /* Specify action */
        full_action = g_strdup_printf ("\"%s#%s\"", service_type, action->name);
        soup_message_headers_append (action->msg->request_headers,
                                     "SOAPAction",
                                     full_action);
        g_free (full_action);

        /* Specify language */
        http_request_set_accept_language (action->msg);

        /* Accept gzip encoding */
        soup_message_headers_append (action->msg->request_headers,
                                     "Accept-Encoding", "gzip");

        gupnp_service_proxy_action_serialize (action, service_type);

        soup_message_set_request (action->msg,
                                  "text/xml; charset=\"utf-8\"",
                                  SOUP_MEMORY_TAKE,
                                  action->msg_str->str,
                                  action->msg_str->len);

        g_string_free (action->msg_str, FALSE);
        action->msg_str = NULL;

        return TRUE;
}

static void
update_message_after_not_allowed (SoupMessage *msg)
{
        const char *full_action;

        /* Retry with M-POST */
        msg->method = "M-POST";

        soup_message_headers_append
                        (msg->request_headers,
                         "Man",
                         "\"http://schemas.xmlsoap.org/soap/envelope/\"; ns=s");

        /* Rename "SOAPAction" to "s-SOAPAction" */
        full_action = soup_message_headers_get_one
                        (msg->request_headers,
                         "SOAPAction");
        soup_message_headers_append (msg->request_headers,
                                     "s-SOAPAction",
                                     full_action);
        soup_message_headers_remove (msg->request_headers,
                                     "SOAPAction");
}

static void
gupnp_service_proxy_action_queue_task (GTask *task);

static void
action_task_got_response (SoupSession *session,
                          SoupMessage *msg,
                          gpointer    user_data)
{
        GTask *task = G_TASK (user_data);
        GUPnPServiceProxyAction *action = (GUPnPServiceProxyAction *) g_task_get_task_data (task);

        // Re-send the message with method M-POST and s-SOAPAction
        if (msg->status_code == SOUP_STATUS_METHOD_NOT_ALLOWED &&
            g_str_equal ("POST", msg->method)) {
                update_message_after_not_allowed (msg);
                gupnp_service_proxy_action_queue_task (task);

                return;
        }

        if (action->cancellable != NULL && action->cancellable_connection_id != 0) {
                g_cancellable_disconnect (action->cancellable,
                                          action->cancellable_connection_id);
                action->cancellable_connection_id = 0;
        }

        if (msg->status_code == SOUP_STATUS_CANCELLED) {
                g_task_return_new_error (task,
                                         G_IO_ERROR,
                                         G_IO_ERROR_CANCELLED,
                                         "Action message was cancelled");
        } else if (SOUP_STATUS_IS_TRANSPORT_ERROR (msg->status_code)) {
                g_task_return_new_error (task,
                                         GUPNP_SERVER_ERROR,
                                         GUPNP_SERVER_ERROR_OTHER,
                                         "%s",
                                         msg->reason_phrase);
        } else  {
                g_task_return_pointer (task,
                                       g_task_get_task_data (task),
                                       NULL);
        }

        g_object_unref (task);
}

static void
gupnp_service_proxy_action_queue_task (GTask *task)
{
        GUPnPContext *context;
        SoupSession *session;
        GUPnPServiceProxyAction *action = g_task_get_task_data (task);

        /* We need to keep our own reference to the message as well,
         * in order for send_action() to work. */
        g_object_ref (action->msg);

        /* Send the message */
        context = gupnp_service_info_get_context
                                (GUPNP_SERVICE_INFO (action->proxy));
        session = gupnp_context_get_session (context);

        soup_session_queue_message (session,
                                    action->msg,
                                    (SoupSessionCallback) action_task_got_response,
                                    task);
        action->pending = TRUE;
}

/**
 * gupnp_service_proxy_begin_action_valist:
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @callback: (scope async) : The callback to call when sending the action has succeeded
 * or failed
 * @user_data: User data for @callback
 * @var_args: A va_list of tuples of in parameter name, in parameter type, and
 * in parameter value
 *
 * See gupnp_service_proxy_begin_action().
 *
 * Returns: (transfer none): A #GUPnPServiceProxyAction handle. This will
 * be freed when calling gupnp_service_proxy_cancel_action() or
 * gupnp_service_proxy_end_action_valist().
 *
 * Deprecated: 1.2.0
 **/
GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action_valist
                                   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GUPnPServiceProxyActionCallback callback,
                                    gpointer                        user_data,
                                    va_list                         var_args)
{
        GUPnPServiceProxyAction *ret;
        GList *in_names = NULL;
        GList *in_values = NULL;

        VAR_ARGS_TO_IN_LIST (var_args, in_names, in_values);

        ret = gupnp_service_proxy_action_new_from_list (action, in_names, in_values);
        g_list_free_full (in_names, g_free);
        g_list_free_full (in_values, gvalue_free);

        ret->callback = callback;
        ret->user_data = user_data;

        gupnp_service_proxy_call_action_async (proxy,
                                               ret,
                                               NULL,
                                               on_legacy_async_callback,
                                               ret);

        return ret;

}

/**
 * gupnp_service_proxy_begin_action_list:
 * @proxy: (transfer none): A #GUPnPServiceProxy
 * @action: An action
 * @in_names: (element-type utf8) (transfer none): #GList of 'in' parameter
 * names (as strings)
 * @in_values: (element-type GValue) (transfer none): #GList of values (as
 * #GValue) that line up with @in_names
 * @callback: (scope async) : The callback to call when sending the action has succeeded or
 * failed
 * @user_data: User data for @callback
 *
 * A variant of #gupnp_service_proxy_begin_action that takes lists of
 * in-parameter names, types and values.
 *
 * Return value: (transfer none): A #GUPnPServiceProxyAction handle. This will
 * be freed when calling gupnp_service_proxy_cancel_action() or
 * gupnp_service_proxy_end_action_list().
 *
 * Since: 0.14.0
 **/
GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action_list
                                   (GUPnPServiceProxy               *proxy,
                                    const char                      *action,
                                    GList                           *in_names,
                                    GList                           *in_values,
                                    GUPnPServiceProxyActionCallback  callback,
                                    gpointer                         user_data)
{
        GUPnPServiceProxyAction *ret;

        ret = gupnp_service_proxy_action_new_from_list (action, in_names, in_values);

        ret->callback = callback;
        ret->user_data = user_data;

        gupnp_service_proxy_call_action_async (proxy,
                                               ret,
                                               NULL,
                                               on_legacy_async_callback,
                                               ret);

        return ret;
}

/**
 * gupnp_service_proxy_end_action:
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 * @error: (inout)(optional)(nullable): The location where to store any error, or %NULL
 * @...: tuples of out parameter name, out parameter type, and out parameter
 * value location, terminated with %NULL. The out parameter values should be
 * freed after use
 *
 * Retrieves the result of @action. The out parameters in @Varargs will be
 * filled in, and if an error occurred, @error will be set. In case of
 * an UPnP error the error code will be the same in @error.
 *
 * Return value: %TRUE on success.
 **/
gboolean
gupnp_service_proxy_end_action (GUPnPServiceProxy       *proxy,
                                GUPnPServiceProxyAction *action,
                                GError                 **error,
                                ...)
{
        va_list var_args;
        gboolean ret;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);

        if (action->error) {
                g_propagate_error (error, action->error);
                gupnp_service_proxy_action_unref (action);

                return FALSE;
        }


        va_start (var_args, error);
        ret = gupnp_service_proxy_action_get_result_valist (action, error, var_args);
        va_end (var_args);

        gupnp_service_proxy_action_unref (action);

        return ret;
}

/**
 * gupnp_service_proxy_end_action_valist:
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 * @error: (inout)(optional)(nullable): The location where to store any error, or %NULL
 * @var_args: A va_list of tuples of out parameter name, out parameter type,
 * and out parameter value location. The out parameter values should be
 * freed after use
 *
 * See gupnp_service_proxy_end_action().
 *
 * Return value: %TRUE on success.
 **/
gboolean
gupnp_service_proxy_end_action_valist (GUPnPServiceProxy       *proxy,
                                       GUPnPServiceProxyAction *action,
                                       GError                 **error,
                                       va_list                  var_args)
{
        gboolean result;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);

        if (action->error) {
                g_propagate_error (error, action->error);
                gupnp_service_proxy_action_unref (action);

                return FALSE;
        }

        result = gupnp_service_proxy_action_get_result_valist (action,
                                                               error,
                                                               var_args);
        gupnp_service_proxy_action_unref (action);

        return result;
}

/**
 * gupnp_service_proxy_end_action_list:
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 * @error:(inout)(optional)(nullable): The location where to store any error, or %NULL
 * @out_names: (element-type utf8) (transfer none): #GList of 'out' parameter
 * names (as strings)
 * @out_types: (element-type GType) (transfer none): #GList of types (as #GType)
 * that line up with @out_names
 * @out_values: (element-type GValue) (transfer full) (out): #GList of values
 * (as #GValue) that line up with @out_names and @out_types.
 *
 * A variant of #gupnp_service_proxy_end_action that takes lists of
 * out-parameter names, types and place-holders for values. The returned list
 * in @out_values must be freed using #g_list_free and each element in it using
 * #g_value_unset and #g_slice_free.
 *
 * Return value : %TRUE on success.
 *
 **/
gboolean
gupnp_service_proxy_end_action_list (GUPnPServiceProxy       *proxy,
                                     GUPnPServiceProxyAction *action,
                                     GList                   *out_names,
                                     GList                   *out_types,
                                     GList                  **out_values,
                                     GError                 **error)
{
        gboolean result;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);

        /* Check for saved error from begin_action() */
        if (action->error) {
                g_propagate_error (error, action->error);
                gupnp_service_proxy_action_unref (action);

                return FALSE;
        }

        result = gupnp_service_proxy_action_get_result_list (action,
                                                             out_names,
                                                             out_types,
                                                             out_values,
                                                             error);
        gupnp_service_proxy_action_unref (action);

        return result;
}

/**
 * gupnp_service_proxy_end_action_hash:
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 * @error:(inout)(optional)(nullable): The location where to store any error, or %NULL
 * @hash: (element-type utf8 GValue) (inout) (transfer none): A #GHashTable of
 * out parameter name and initialised #GValue pairs
 *
 * See gupnp_service_proxy_end_action(); this version takes a #GHashTable for
 * runtime generated parameter lists.
 *
 * Return value: %TRUE on success.
 *
 **/
gboolean
gupnp_service_proxy_end_action_hash
                                   (GUPnPServiceProxy              *proxy,
                                    GUPnPServiceProxyAction        *action,
                                    GHashTable                     *hash,
                                    GError                        **error)
{
        gboolean result;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);

        /* Check for saved error from begin_action() */
        if (action->error) {
                g_propagate_error (error, action->error);
                gupnp_service_proxy_action_unref (action);

                return FALSE;
        }


        result = gupnp_service_proxy_action_get_result_hash (action,
                                                             hash,
                                                             error);
        gupnp_service_proxy_action_unref (action);

        return result;
}

/**
 * gupnp_service_proxy_cancel_action:
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 *
 * Cancels @action, freeing the @action handle.
 *
 * Deprecated: 1.2.0: Use the #GCancellable passed to
 * gupnp_service_proxy_call_action_async() or gupnp_service_proxy_call_action()
 **/
void
gupnp_service_proxy_cancel_action (GUPnPServiceProxy       *proxy,
                                   GUPnPServiceProxyAction *action)
{
        g_return_if_fail (GUPNP_IS_SERVICE_PROXY (proxy));
        g_return_if_fail (action);

        if (action->cancellable != NULL) {
                g_cancellable_cancel (action->cancellable);
        }

        gupnp_service_proxy_action_unref (action);
}

/**
 * gupnp_service_proxy_add_notify: (skip)
 * @proxy: A #GUPnPServiceProxy
 * @variable: The variable to add notification for
 * @type: The type of the variable
 * @callback: (scope async): The callback to call when @variable changes
 * @user_data: User data for @callback
 *
 * Sets up @callback to be called whenever a change notification for
 * @variable is recieved.
 *
 * Return value: %TRUE on success.
 **/
gboolean
gupnp_service_proxy_add_notify (GUPnPServiceProxy              *proxy,
                                const char                     *variable,
                                GType                           type,
                                GUPnPServiceProxyNotifyCallback callback,
                                gpointer                        user_data)

{
        return gupnp_service_proxy_add_notify_full (proxy,
                                                    variable,
                                                    type,
                                                    callback,
                                                    user_data,
                                                    NULL);
}

/**
 * gupnp_service_proxy_add_notify_full: (rename-to gupnp_service_proxy_add_notify)
 * @proxy: A #GUPnPServiceProxy
 * @variable: The variable to add notification for
 * @type: The type of the variable
 * @callback: (scope notified): The callback to call when @variable changes
 * @user_data: User data for @callback
 * @notify: (allow-none): Function to call when the notification is removed, or %NULL
 *
 * Sets up @callback to be called whenever a change notification for
 * @variable is recieved.
 *
 * Since: 0.20.12
 *
 * Return value: %TRUE on success.
 **/
gboolean
gupnp_service_proxy_add_notify_full (GUPnPServiceProxy              *proxy,
                                     const char                     *variable,
                                     GType                           type,
                                     GUPnPServiceProxyNotifyCallback callback,
                                     gpointer                        user_data,
                                     GDestroyNotify                  notify)
{
        NotifyData *data;
        CallbackData *callback_data;
        GUPnPServiceProxyPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (variable, FALSE);
        g_return_val_if_fail (type, FALSE);
        g_return_val_if_fail (callback, FALSE);

        priv = gupnp_service_proxy_get_instance_private (proxy);

        /* See if we already have notifications set up for this variable */
        data = g_hash_table_lookup (priv->notify_hash, variable);
        if (data == NULL) {
                /* No, create one */
                data = g_slice_new (NotifyData);

                data->type       = type;
                data->callbacks  = NULL;
                data->next_emit   = NULL;

                g_hash_table_insert (priv->notify_hash,
                                     g_strdup (variable),
                                     data);
        } else {
                /* Yes, check that everything is sane .. */
                if (data->type != type) {
                        g_warning ("A notification already exists for %s, but "
                                   "has type %s, not %s.",
                                   variable,
                                   g_type_name (data->type),
                                   g_type_name (type));

                        return FALSE;
                }
        }

        /* Append callback */
        callback_data = g_slice_new (CallbackData);

        callback_data->callback  = callback;
        callback_data->user_data = user_data;
        callback_data->notify    = notify;

        data->callbacks = g_list_append (data->callbacks, callback_data);

        if (data->next_emit == NULL)
                data->next_emit = g_list_last (data->callbacks);

        return TRUE;
}

/**
 * gupnp_service_proxy_add_raw_notify:
 * @proxy: A #GUPnPServiceProxy
 * @callback: (scope notified): The callback to call when the peer issues any
 * variable notification.
 * @user_data: User data for @callback
 * @notify: (allow-none): A #GDestroyNotify for @user_data
 *
 * Get a notification for anything that happens on the peer. @value in
 * @callback will be of type #G_TYPE_POINTER and contain the pre-parsed
 * #xmlDoc. Do NOT free or modify this document.
 *
 * Return value: %TRUE on success.
 *
 * Since: 0.20.12
 **/
gboolean
gupnp_service_proxy_add_raw_notify (GUPnPServiceProxy              *proxy,
                                    GUPnPServiceProxyNotifyCallback callback,
                                    gpointer                        user_data,
                                    GDestroyNotify                  notify)
{
        return gupnp_service_proxy_add_notify_full (proxy,
                                                    "*",
                                                    G_TYPE_NONE,
                                                    callback,
                                                    user_data,
                                                    notify);
}


/**
 * gupnp_service_proxy_remove_notify:
 * @proxy: A #GUPnPServiceProxy
 * @variable: The variable to add notification for
 * @callback: (scope call): The callback to call when @variable changes
 * @user_data: User data for @callback
 *
 * Cancels the variable change notification for @callback and @user_data.
 *
 * Up to version 0.20.9 this function must not be called directlya or
 * indirectly from a #GUPnPServiceProxyNotifyCallback associated with this
 * service proxy, even if it is for another variable. In later versions such
 * calls are allowed.
 *
 * Return value: %TRUE on success.
 **/
gboolean
gupnp_service_proxy_remove_notify (GUPnPServiceProxy              *proxy,
                                   const char                     *variable,
                                   GUPnPServiceProxyNotifyCallback callback,
                                   gpointer                        user_data)
{
        NotifyData *data;
        gboolean found;
        GList *l;
        GUPnPServiceProxyPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (variable, FALSE);
        g_return_val_if_fail (callback, FALSE);

        priv = gupnp_service_proxy_get_instance_private (proxy);

        /* Look up NotifyData for variable */
        data = g_hash_table_lookup (priv->notify_hash, variable);
        if (data == NULL) {
                g_warning ("No notifications found for variable %s",
                           variable);

                return FALSE;
        }

        /* Look up our callback-user_data pair */
        found = FALSE;

        for (l = data->callbacks; l; l = l->next) {
                CallbackData *callback_data;

                callback_data = l->data;

                if (callback_data->callback  == callback &&
                    callback_data->user_data == user_data) {

                        /* Gotcha! */
                        callback_data_free (callback_data);

                        if (data->next_emit == l)
                                data->next_emit = data->next_emit->next;

                        data->callbacks =
                                g_list_delete_link (data->callbacks, l);
                        if (data->callbacks == NULL) {
                                /* No callbacks left: Remove from hash */
                                g_hash_table_remove (priv->notify_hash,
                                                     variable);
                        }

                        found = TRUE;

                        break;
                }
        }

        if (found == FALSE)
                g_warning ("No such callback-user_data pair was found");

        return found;
}

/**
 * gupnp_service_proxy_remove_raw_notify:
 * @proxy: A #GUPnPServiceProxy
 * @callback: (scope call): The callback to call when @variable changes
 * @user_data: User data for @callback
 *
 * Cancels the variable change notification for @callback and @user_data.
 *
 * This function must not be called directly or indirectly from a
 * #GUPnPServiceProxyNotifyCallback associated with this service proxy, even
 * if it is for another variable.
 *
 * Return value: %TRUE on success.
 **/
gboolean
gupnp_service_proxy_remove_raw_notify (GUPnPServiceProxy              *proxy,
                                       GUPnPServiceProxyNotifyCallback callback,
                                       gpointer                        user_data)
{
        return gupnp_service_proxy_remove_notify (proxy,
                                                  "*",
                                                  callback,
                                                  user_data);
}

static void
emit_notification (GUPnPServiceProxy *proxy,
                   xmlNode           *var_node)
{
        NotifyData *data;
        GValue value = {0, };
        GList *l;
        GUPnPServiceProxyPrivate *priv;

        priv = gupnp_service_proxy_get_instance_private (proxy);

        data = g_hash_table_lookup (priv->notify_hash, var_node->name);
        if (data == NULL)
                return;

        /* Make a GValue of the desired type */
        g_value_init (&value, data->type);

        if (!gvalue_util_set_value_from_xml_node (&value, var_node)) {
                g_value_unset (&value);

                return;
        }

        /* Call callbacks. Note that data->next_emit may change if
         * callback calls remove_notify() or add_notify() */
        for (l = data->callbacks; l; l = data->next_emit) {
                CallbackData *callback_data;

                callback_data = l->data;
                data->next_emit = l->next;

                callback_data->callback (proxy,
                                         (const char *) var_node->name,
                                         &value,
                                         callback_data->user_data);
        }

        /* Cleanup */
        g_value_unset (&value);
}

static void
emit_notifications_for_doc (GUPnPServiceProxy *proxy,
                            xmlDoc            *doc)
{
        NotifyData *data = NULL;
        xmlNode *node;
        GUPnPServiceProxyPrivate *priv;

        node = xmlDocGetRootElement (doc);

        /* Iterate over all provided properties */
        for (node = node->children; node; node = node->next) {
                xmlNode *var_node;

                /* Although according to the UPnP specs, there should be only
                 * one variable node inside a 'property' node, we still need to
                 * entertain the possibility of multiple variables inside it to
                 * be compatible with implementations using older GUPnP.
                 */
                for (var_node = node->children;
                     var_node;
                     var_node = var_node->next) {
                        if (strcmp ((char *) node->name, "property") == 0)
                                emit_notification (proxy, var_node);
                }
        }

        priv = gupnp_service_proxy_get_instance_private (proxy);
        data = g_hash_table_lookup (priv->notify_hash, "*");
        if (data != NULL) {
                GValue value = {0, };
                GList *it = NULL;

                g_value_init (&value, G_TYPE_POINTER);
                g_value_set_pointer (&value, doc);

                for (it = data->callbacks; it != NULL; it = it->next) {
                        CallbackData *callback_data = it->data;

                        callback_data->callback (proxy,
                                                 "*",
                                                 &value,
                                                 callback_data->user_data);
                }

                g_value_unset (&value);
        }
}

/* Emit pending notifications. See comment below on why we do this. */
static gboolean
emit_notifications (gpointer user_data)
{
        GUPnPServiceProxy *proxy = user_data;
        GUPnPServiceProxyPrivate *priv;
        GList *pending_notify;
        gboolean resubscribe = FALSE;

        g_assert (user_data);

        priv = gupnp_service_proxy_get_instance_private (proxy);
        if (priv->sid == NULL)
                /* No SID */
                if (G_LIKELY (priv->subscribed))
                        /* subscription in progress, delay emision! */
                        return TRUE;

        for (pending_notify = priv->pending_notifies;
             pending_notify != NULL;
             pending_notify = pending_notify->next) {
                EmitNotifyData *emit_notify_data;

                emit_notify_data = pending_notify->data;

                if (emit_notify_data->seq > priv->seq) {
                        /* Error procedure on missed event according to
                         * UDA 1.0, section 4.2, §5:
                         * Re-subscribe to get a new SID and SEQ */
                        resubscribe = TRUE;

                        break;
                }

                /* Increment our own event sequence number */
                /* UDA 1.0, section 4.2, §3: To prevent overflow, SEQ is set to
                 * 1, NOT 0, when encountering G_MAXUINT32. SEQ == 0 always
                 * indicates the initial event message. */
                if (priv->seq < G_MAXUINT32)
                        priv->seq++;
                else
                        priv->seq = 1;

                if (G_LIKELY (priv->sid != NULL &&
                              strcmp (emit_notify_data->sid,
                                      priv->sid) == 0))
                        /* Our SID, entertain! */
                        emit_notifications_for_doc (proxy,
                                                    emit_notify_data->doc);
        }

        /* Cleanup */
        g_list_free_full (priv->pending_notifies,
                          (GDestroyNotify) emit_notify_data_free);
        priv->pending_notifies = NULL;

        priv->notify_idle_src = NULL;

        if (resubscribe) {
                unsubscribe (proxy);
                subscribe (proxy);
        }

        return FALSE;
}

/*
 * HTTP server received a message. Handle, if this was a NOTIFY
 * message with our SID.
 */
static void
server_handler (G_GNUC_UNUSED SoupServer        *soup_server,
                SoupMessage                     *msg,
                G_GNUC_UNUSED const char        *server_path,
                G_GNUC_UNUSED GHashTable        *query,
                G_GNUC_UNUSED SoupClientContext *soup_client,
                gpointer                         user_data)
{
        GUPnPServiceProxy *proxy;
        GUPnPServiceProxyPrivate *priv;
        const char *hdr, *nt, *nts;
        guint32 seq;
        guint64 seq_parsed;
        xmlDoc *doc;
        xmlNode *node;
        EmitNotifyData *emit_notify_data;

        proxy = GUPNP_SERVICE_PROXY (user_data);

        if (strcmp (msg->method, GENA_METHOD_NOTIFY) != 0) {
                /* We don't implement this method */
                soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);

                return;
        }

        nt = soup_message_headers_get_one (msg->request_headers, "NT");
        nts = soup_message_headers_get_one (msg->request_headers, "NTS");
        if (nt == NULL || nts == NULL) {
                /* Required header is missing */
                soup_message_set_status (msg, SOUP_STATUS_BAD_REQUEST);

                return;
        }

        if (strcmp (nt, "upnp:event") != 0 ||
            strcmp (nts, "upnp:propchange") != 0) {
                /* Unexpected header content */
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        hdr = soup_message_headers_get_one (msg->request_headers, "SEQ");
        if (hdr == NULL) {
                /* No SEQ header */
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        errno = 0;
        seq_parsed = strtoul (hdr, NULL, 10);
        if (errno != 0 || seq_parsed > G_MAXUINT32) {
                /* Invalid SEQ header value */
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        seq = (guint32) seq_parsed;

        hdr = soup_message_headers_get_one (msg->request_headers, "SID");
        if (hdr == NULL ||
            strlen (hdr) <= strlen ("uuid:") ||
            strncmp (hdr, "uuid:", strlen ("uuid:")) != 0) {
                /* No SID */
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        /* Parse the actual XML message content */
        doc = xmlRecoverMemory (msg->request_body->data,
                                msg->request_body->length);
        if (doc == NULL) {
                /* Failed */
                g_warning ("Failed to parse NOTIFY message body");

                soup_message_set_status (msg,
                                         SOUP_STATUS_INTERNAL_SERVER_ERROR);

                return;
        }

        priv = gupnp_service_proxy_get_instance_private (proxy);
        /* Get root propertyset element */
        node = xmlDocGetRootElement (doc);
        if (node == NULL || strcmp ((char *) node->name, "propertyset") || priv->sid == NULL) {
                /* Empty or unsupported */
                xmlFreeDoc (doc);

                soup_message_set_status (msg, SOUP_STATUS_OK);

                return;
        }

        /*
         * Some UPnP stacks (hello, myigd/1.0) block when sending a NOTIFY, so
         * call the callbacks in an idle handler so that if the client calls the
         * device in the notify callback the server can actually respond.
         */
        emit_notify_data = emit_notify_data_new (hdr, seq, doc);

        priv->pending_notifies =
                g_list_append (priv->pending_notifies, emit_notify_data);
        if (!priv->notify_idle_src) {
                priv->notify_idle_src = g_idle_source_new();
                g_source_set_callback (priv->notify_idle_src,
                                       emit_notifications,
                                       proxy, NULL);
                g_source_attach (priv->notify_idle_src,
                                 g_main_context_get_thread_default ());

                g_source_unref (priv->notify_idle_src);
        }

        /* Everything went OK */
        soup_message_set_status (msg, SOUP_STATUS_OK);
}

/*
 * Generates a timeout header for the subscription timeout specified
 * in our GUPnPContext.
 */
static char *
make_timeout_header (GUPnPContext *context)
{
        guint timeout;

        timeout = gupnp_context_get_subscription_timeout (context);
        if (timeout > 0)
                return g_strdup_printf ("Second-%d", timeout);
        else
                return g_strdup ("infinite");
}

/*
 * Subscription expired.
 */
static gboolean
subscription_expire (gpointer user_data)
{
        GUPnPServiceProxy *proxy;
        GUPnPServiceProxyPrivate *priv;
        GUPnPContext *context;
        SoupMessage *msg;
        SoupSession *session;
        char *sub_url, *timeout, *local_sub_url;

        proxy = GUPNP_SERVICE_PROXY (user_data);
        priv = gupnp_service_proxy_get_instance_private (proxy);

        /* Reset timeout ID */
        priv->subscription_timeout_src = NULL;

        g_return_val_if_fail (priv->sid != NULL, FALSE);

        /* Send renewal message */
        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));

        /* Create subscription message */
        sub_url = gupnp_service_info_get_event_subscription_url
                                                (GUPNP_SERVICE_INFO (proxy));

        local_sub_url = gupnp_context_rewrite_uri (context, sub_url);
        g_free (sub_url);

        msg = soup_message_new (GENA_METHOD_SUBSCRIBE, local_sub_url);
        g_free (local_sub_url);

        g_return_val_if_fail (msg != NULL, FALSE);

        /* Add headers */
        soup_message_headers_append (msg->request_headers,
                                    "SID",
                                    priv->sid);

        timeout = make_timeout_header (context);
        soup_message_headers_append (msg->request_headers,
                                     "Timeout",
                                     timeout);
        g_free (timeout);

        /* And send it off */
        priv->pending_messages =
                g_list_prepend (priv->pending_messages, msg);

        session = gupnp_context_get_session (context);

        soup_session_queue_message (session,
                                    msg,
                                    (SoupSessionCallback)
                                        subscribe_got_response,
                                    proxy);

        return FALSE;
}

/*
 * Received subscription response.
 */
static void
subscribe_got_response (G_GNUC_UNUSED SoupSession *session,
                        SoupMessage               *msg,
                        GUPnPServiceProxy         *proxy)
{
        GError *error;
        GUPnPServiceProxyPrivate *priv;

        /* Cancelled? */
        if (msg->status_code == SOUP_STATUS_CANCELLED)
                return;

        /* Remove from pending messages list */
        priv = gupnp_service_proxy_get_instance_private (proxy);
        priv->pending_messages = g_list_remove (priv->pending_messages, msg);

        /* Remove subscription timeout */
        if (priv->subscription_timeout_src) {
                g_source_destroy (priv->subscription_timeout_src);
                priv->subscription_timeout_src = NULL;
        }

        /* Check whether the subscription is still wanted */
        if (!priv->subscribed)
                return;

        /* Reset SID */
        g_free (priv->sid);
        priv->sid = NULL;

        /* Check message status */
        if (SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
                /* Success. */
                const char *hdr;
                int timeout;

                /* Save SID. */
                hdr = soup_message_headers_get_one (msg->response_headers,
                                                    "SID");
                if (hdr == NULL) {
                        error = g_error_new
                                        (GUPNP_EVENTING_ERROR,
                                         GUPNP_EVENTING_ERROR_SUBSCRIPTION_LOST,
                                         "No SID in SUBSCRIBE response");

                        goto hdr_err;
                }

                priv->sid = g_strdup (hdr);

                /* Figure out when the subscription times out */
                hdr = soup_message_headers_get_one (msg->response_headers,
                                                    "Timeout");
                if (hdr == NULL) {
                        g_warning ("No Timeout in SUBSCRIBE response.");

                        return;
                }

                if (strncmp (hdr, "Second-", strlen ("Second-")) == 0) {
                        /* We have a finite timeout */
                        timeout = atoi (hdr + strlen ("Second-"));

                        if (timeout < 0) {
                                g_warning ("Invalid time-out specified. "
                                           "Assuming default value of %d.",
                                           GENA_DEFAULT_TIMEOUT);

                                timeout = GENA_DEFAULT_TIMEOUT;
                        }

                        /* We want to resubscribe before the subscription
                         * expires. */
                        timeout = g_random_int_range (1, timeout / 2);

                        /* Add actual timeout */
                        priv->subscription_timeout_src =
                                g_timeout_source_new_seconds (timeout);
                        g_source_set_callback
                                (priv->subscription_timeout_src,
                                 subscription_expire,
                                 proxy, NULL);
                        g_source_attach (priv->subscription_timeout_src,
                                         g_main_context_get_thread_default ());

                        g_source_unref (priv->subscription_timeout_src);
                }
        } else {
                GUPnPContext *context;
                SoupServer *server;

                /* Subscription failed. */
                error = g_error_new_literal
                                (GUPNP_EVENTING_ERROR,
                                 GUPNP_EVENTING_ERROR_SUBSCRIPTION_FAILED,
                                 msg->reason_phrase);

hdr_err:
                /* Remove listener */
                context = gupnp_service_info_get_context
                                        (GUPNP_SERVICE_INFO (proxy));

                server = gupnp_context_get_server (context);
                soup_server_remove_handler (server, priv->path);

                priv->subscribed = FALSE;

                g_object_notify (G_OBJECT (proxy), "subscribed");

                /* Emit subscription-lost */
                g_signal_emit (proxy,
                               signals[SUBSCRIPTION_LOST],
                               0,
                               error);

                g_error_free (error);
        }
}

/*
 * Subscribe to this service.
 */
static void
subscribe (GUPnPServiceProxy *proxy)
{
        GUPnPContext *context;
        GUPnPServiceProxyPrivate *priv;
        SoupMessage *msg;
        SoupSession *session;
        SoupServer *server;
        SoupURI *uri;
        char *uri_string;
        char *sub_url, *delivery_url, *timeout;

        /* Remove subscription timeout */
        priv = gupnp_service_proxy_get_instance_private (proxy);
        if (priv->subscription_timeout_src) {
                g_source_destroy (priv->subscription_timeout_src);
                priv->subscription_timeout_src = NULL;
        }

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));

        /* Create subscription message */
        sub_url = gupnp_service_info_get_event_subscription_url
                                                (GUPNP_SERVICE_INFO (proxy));

        msg = NULL;
        if (sub_url != NULL) {
                char *local_sub_url = NULL;

                local_sub_url = gupnp_context_rewrite_uri (context, sub_url);
                g_free (sub_url);

                msg = soup_message_new (GENA_METHOD_SUBSCRIBE, local_sub_url);
                g_free (local_sub_url);
        }

        if (msg == NULL) {
                GError *error;

                /* Subscription failed. */
                priv->subscribed = FALSE;

                g_object_notify (G_OBJECT (proxy), "subscribed");

                /* Emit subscription-lost */
                error = g_error_new (GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_URL,
                                     "No valid subscription URL defined");

                g_signal_emit (proxy,
                               signals[SUBSCRIPTION_LOST],
                               0,
                               error);

                g_error_free (error);

                return;
        }

        /* Add headers */
        uri = _gupnp_context_get_server_uri (context);
        soup_uri_set_path (uri, priv->path);
        uri_string = soup_uri_to_string (uri, FALSE);
        soup_uri_free (uri);
        delivery_url = g_strdup_printf ("<%s>", uri_string);
        g_free (uri_string);

        soup_message_headers_append (msg->request_headers,
                                     "Callback",
                                     delivery_url);
        g_free (delivery_url);

        soup_message_headers_append (msg->request_headers,
                                     "NT",
                                     "upnp:event");

        timeout = make_timeout_header (context);
        soup_message_headers_append (msg->request_headers,
                                     "Timeout",
                                     timeout);
        g_free (timeout);

        /* Listen for events */
        server = gupnp_context_get_server (context);

        soup_server_add_handler (server,
                                 priv->path,
                                 server_handler,
                                 proxy,
                                 NULL);

        /* And send our subscription message off */
        priv->pending_messages =
                g_list_prepend (priv->pending_messages, msg);

        session = gupnp_context_get_session (context);

        soup_session_queue_message (session,
                                    msg,
                                    (SoupSessionCallback)
                                        subscribe_got_response,
                                    proxy);
}

/*
 * Unsubscribe from this service.
 */
static void
unsubscribe (GUPnPServiceProxy *proxy)
{
        GUPnPContext *context;
        GUPnPServiceProxyPrivate *priv;
        SoupSession *session;
        SoupServer *server;

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));
        priv = gupnp_service_proxy_get_instance_private (proxy);

        /* Remove server handler */
        server = gupnp_context_get_server (context);
        soup_server_remove_handler (server, priv->path);

        if (priv->sid != NULL) {
                SoupMessage *msg;
                char *sub_url, *local_sub_url;

                /* Create unsubscription message */
                sub_url = gupnp_service_info_get_event_subscription_url
                                                (GUPNP_SERVICE_INFO (proxy));

                local_sub_url = gupnp_context_rewrite_uri (context, sub_url);
                g_free (sub_url);

                msg = soup_message_new (GENA_METHOD_UNSUBSCRIBE, local_sub_url);
                g_free (local_sub_url);

                if (msg != NULL) {
                        /* Add headers */
                        soup_message_headers_append (msg->request_headers,
                                                     "SID",
                                                     priv->sid);

                        /* And queue it */
                        session = gupnp_context_get_session (context);

                        soup_session_queue_message (session, msg, NULL, NULL);
                }

                /* Reset SID */
                g_free (priv->sid);
                priv->sid = NULL;

                /* Reset sequence number */
                priv->seq = 0;
        }

        /* Remove subscription timeout */
        if (priv->subscription_timeout_src) {
                g_source_destroy (priv->subscription_timeout_src);
                priv->subscription_timeout_src = NULL;
        }
}

/**
 * gupnp_service_proxy_set_subscribed:
 * @proxy: A #GUPnPServiceProxy
 * @subscribed: %TRUE to subscribe to this service
 *
 * (Un)subscribes to this service.
 *
 * <note>The relevant messages are not immediately sent but queued.
 * If you want to unsubcribe from this service because the application
 * is quitting, rely on automatic synchronised unsubscription on object
 * destruction instead.</note>
 **/
void
gupnp_service_proxy_set_subscribed (GUPnPServiceProxy *proxy,
                                    gboolean           subscribed)
{
        GUPnPServiceProxyPrivate *priv;

        g_return_if_fail (GUPNP_IS_SERVICE_PROXY (proxy));

        priv = gupnp_service_proxy_get_instance_private (proxy);
        if (priv->subscribed == subscribed)
                return;

        priv->subscribed = subscribed;

        if (subscribed)
                subscribe (proxy);
        else
                unsubscribe (proxy);

        g_object_notify (G_OBJECT (proxy), "subscribed");
}

/**
 * gupnp_service_proxy_get_subscribed:
 * @proxy: A #GUPnPServiceProxy
 *
 * Returns if we are subscribed to this service.
 *
 * Return value: %TRUE if we are subscribed to this service, otherwise %FALSE.
 **/
gboolean
gupnp_service_proxy_get_subscribed (GUPnPServiceProxy *proxy)
{
        GUPnPServiceProxyPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);

        priv = gupnp_service_proxy_get_instance_private (proxy);

        return priv->subscribed;
}

/**
 * gupnp_service_proxy_call_action_async:
 * @proxy: (transfer none): A #GUPnPServiceProxy
 * @action: (transfer none): A #GUPnPServiceProxyAction to call
 * @cancellable: (allow-none): A #GCancellable which can be used to cancel the
 * current action call
 * @callback: (scope async): A #GAsyncReadyCallback to call when the action is
 * finished.
 * @user_data: (closure): User data for @callback
 *
 * Start a call on the remote UPnP service using the pre-configured @action.
 * Use gupnp_service_proxy_call_action_finish() in the @callback to finalize
 * the call and gupnp_service_proxy_action_get_result(),
 * gupnp_service_proxy_action_get_result_hash() or
 * gupnp_service_proxy_action_get_result_list() to extract the result of the
 * remote call.
 * Since: 1.2.0
 */
void
gupnp_service_proxy_call_action_async (GUPnPServiceProxy       *proxy,
                                       GUPnPServiceProxyAction *action,
                                       GCancellable            *cancellable,
                                       GAsyncReadyCallback     callback,
                                       gpointer                user_data)
{
        GTask *task;
        GUPnPServiceProxyPrivate *priv;
        GError *error = NULL;

        g_return_if_fail (GUPNP_IS_SERVICE_PROXY (proxy));

        priv = gupnp_service_proxy_get_instance_private (proxy);
        task = g_task_new (proxy, cancellable, callback, user_data);
        char *task_name = g_strdup_printf ("UPnP Call \"%s\"", action->name);
        g_task_set_name (task, task_name);
        g_free (task_name);
        g_task_set_task_data (task,
                              gupnp_service_proxy_action_ref (action),
                              (GDestroyNotify) gupnp_service_proxy_action_unref);

        prepare_action_msg (proxy, action, &error);

        if (error != NULL) {
                g_task_return_error (task, error);
                g_object_unref (task);
        } else {
                action->proxy = proxy;
                g_object_add_weak_pointer (G_OBJECT (proxy), (gpointer *)&(action->proxy));

                priv->pending_actions = g_list_prepend (priv->pending_actions, action);

                if (cancellable != NULL) {
                        action->cancellable = g_object_ref (cancellable);
                } else {
                        action->cancellable = g_cancellable_new ();
                }
                action->cancellable_connection_id = g_cancellable_connect (action->cancellable,
                                                                           G_CALLBACK (on_action_cancelled),
                                                                           action,
                                                                           NULL);

                gupnp_service_proxy_action_queue_task (task);
        }
}

/**
 * gupnp_service_proxy_call_action_finish:
 * @proxy: a #GUPnPServiceProxy
 * @result: a #GAsyncResult
 * @error: (inout)(optional)(nullable): Return location for a #GError, or %NULL
 *
 * Finish an asynchronous call initiated with
 * gupnp_service_proxy_call_action_async().
 *
 * Note: This will only signalize transport errors to the caller, such as the action being cancelled
 * or lost connection etc. SOAP call errors are only returned by gupnp_service_proxy_action_get() and such.
 *
 * Returns: (nullable) (transfer none): %NULL, if the call had an error, the action otherwise.
 * Since: 1.2.0
 */
GUPnPServiceProxyAction *
gupnp_service_proxy_call_action_finish (GUPnPServiceProxy *proxy,
                                        GAsyncResult      *result,
                                        GError           **error)
{
        g_return_val_if_fail (g_task_is_valid (G_TASK (result), proxy), NULL);

        GUPnPServiceProxyAction *action = g_task_get_task_data (G_TASK (result));
        gupnp_service_proxy_remove_action (action->proxy, action);
        g_clear_weak_pointer (&action->proxy);
        action->pending = FALSE;

        return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * gupnp_service_proxy_call_action:
 * @proxy: (transfer none): A #GUPnPServiceProxy
 * @action: (transfer none): An action
 * @cancellable: (allow-none): A #GCancellable which can be used to cancel the
 * current action call
 * @error: (inout)(optional)(nullable): Return location for a #GError, or %NULL.
 *
 * Synchronously call the @action on the remote UPnP service.
 *
 * Returns: (nullable)(transfer none): %NULL on error, @action if successful.
 * Since: 1.2.0
 */
GUPnPServiceProxyAction *
gupnp_service_proxy_call_action (GUPnPServiceProxy       *proxy,
                                 GUPnPServiceProxyAction *action,
                                 GCancellable            *cancellable,
                                 GError                 **error)
{
        GUPnPContext *context = NULL;
        SoupSession *session = NULL;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), NULL);
        g_return_val_if_fail (!action->pending, NULL);

        if (!prepare_action_msg (proxy, action, error)) {
                return NULL;
        }

        if (cancellable != NULL) {
                action->cancellable = g_object_ref (cancellable);
        } else {
                action->cancellable = g_cancellable_new ();
        }
        action->cancellable_connection_id = g_cancellable_connect (action->cancellable,
                                                                   G_CALLBACK (on_action_cancelled),
                                                                   action,
                                                                   NULL);

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));
        session = gupnp_context_get_session (context);
        soup_session_send_message (session, action->msg);

        g_cancellable_disconnect (action->cancellable,
                                  action->cancellable_connection_id);
        action->cancellable_connection_id = 0;
        g_clear_object (&action->cancellable);

        /* If not allowed, try again */
        if (action->msg->status_code == SOUP_STATUS_METHOD_NOT_ALLOWED) {
                update_message_after_not_allowed (action->msg);
                soup_session_send_message (session, action->msg);
        }

        if (action->msg->status_code == SOUP_STATUS_CANCELLED) {
                g_propagate_error (
                        error,
                        g_error_new (G_IO_ERROR,
                                     G_IO_ERROR_CANCELLED,
                                     "Action message was cancelled"));

                return NULL;
        }

        return action;
}
