/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#define G_LOG_DOMAIN "gupnp-service-proxy"

#include <config.h>
#include <libsoup/soup.h>
#include <gobject/gvaluecollector.h>
#include <string.h>
#include <locale.h>
#include <errno.h>

#include <libxml/globals.h>

#include "gena-protocol.h"
#include "gupnp-context-private.h"
#include "gupnp-error-private.h"
#include "gupnp-error.h"
#include "gupnp-service-proxy.h"
#include "gupnp-service-proxy-action-private.h"
#include "gupnp-types.h"
#include "gvalue-util.h"
#include "http-headers.h"
#include "xml-util.h"

struct _GUPnPServiceProxyPrivate {
        gboolean subscribed;

        char *path; /* Path to this proxy */

        // Credentials
        char *user;
        char *password;

        char *sid; /* Subscription ID */
        GSource *subscription_timeout_src;

        guint32 seq; /* Event sequence number */

        GHashTable *notify_hash;

        // GCancellable that is used to all SoupMessages that are neither
        // notifies nor proxy calls
        GCancellable *pending_messages;

        GQueue *pending_notifies; /* Pending notifications to be sent (xmlDoc) */
        GSource *notify_idle_src; /* Idle handler src of notification emiter */
};
typedef struct _GUPnPServiceProxyPrivate GUPnPServiceProxyPrivate;

/**
 * GUPnPServiceProxy:
 *
 * Proxy class for remote services.
 *
 * #GUPnPServiceProxy sends commands to a remote UPnP service and handles
 * incoming event notifications.
 */
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
} NotifyCallbackData;

typedef struct {
        char *sid;
        guint32 seq;

        xmlDoc *doc;
} EmitNotifyData;

static void
subscribe_got_response (GObject *source, GAsyncResult *res, gpointer user_data);

static void
subscribe (GUPnPServiceProxy *proxy);
static void
unsubscribe (GUPnPServiceProxy *proxy);

static void
callback_data_free (NotifyCallbackData *data)
{
        if (data->notify)
                data->notify (data->user_data);

        g_slice_free (NotifyCallbackData, data);
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

        priv->pending_messages = g_cancellable_new ();
        priv->pending_notifies = g_queue_new ();
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
        // SoupSession *session;

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

        if (priv->pending_messages)
                g_cancellable_cancel (priv->pending_messages);
        g_clear_object (&priv->pending_messages);

        /* Cancel pending notifications */
        g_clear_pointer (&priv->notify_idle_src, g_source_destroy);

        g_queue_free_full (g_steal_pointer(&priv->pending_notifies),
                          (GDestroyNotify) emit_notify_data_free);

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

        g_clear_pointer (&priv->user, g_free);
        g_clear_pointer (&priv->password, g_free);

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

static gboolean
on_authenticate (SoupMessage *msg,
                 SoupAuth    *auth,
                 gboolean     retrying,
                 gpointer     user_data)
{
        GUPnPServiceProxy *proxy = user_data;
        GUPnPServiceProxyPrivate *priv;

        priv = gupnp_service_proxy_get_instance_private (proxy);

        if (!retrying && priv->user != NULL && priv->password != NULL) {
            soup_auth_authenticate (auth, priv->user, priv->password);
            return FALSE;
        }

        return FALSE;
}

static void
on_restarted (SoupMessage *msg, gpointer user_data)
{
        GUPnPServiceProxyAction *action = user_data;
        g_autoptr (GBytes) body = NULL;
        const char *service_type;

        service_type = gupnp_service_info_get_service_type
                                        (GUPNP_SERVICE_INFO (action->proxy));
        gupnp_service_proxy_action_serialize (action, service_type);
        body = g_string_free_to_bytes (action->msg_str);
        soup_message_set_request_body_from_bytes (msg,
                                                  "text/xml; charset=\"utf-8\"",
                                                  body);
        action->msg_str = NULL;
}

/* Begins a basic action message */
static gboolean
prepare_action_msg (GUPnPServiceProxy *proxy,
                    GUPnPServiceProxyAction *action,
                    const char *method,
                    GError **error)
{
        char *control_url, *full_action;
        const char *service_type;

        gupnp_service_proxy_action_reset (action);

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

        action->msg = soup_message_new (method, local_control_url);
        g_signal_connect_object (G_OBJECT (action->msg), "authenticate", G_CALLBACK (on_authenticate), G_OBJECT (proxy), 0);
        g_signal_connect (G_OBJECT (action->msg), "restarted", G_CALLBACK (on_restarted), action);
        g_free (local_control_url);

        SoupMessageHeaders *headers =
                soup_message_get_request_headers (action->msg);

        /* Specify action */
        full_action = g_strdup_printf ("\"%s#%s\"", service_type, action->name);

        // This is internal API, it should either be that constant or the string "M-POST"
        if (method == SOUP_METHOD_POST) {
                soup_message_headers_append (headers,
                                             "SOAPAction",
                                             full_action);
        } else {
                soup_message_headers_append (headers,
                                             "s-SOAPAction",
                                             full_action);
                soup_message_headers_append (
                        headers,
                        "Man",
                        "\"http://schemas.xmlsoap.org/soap/envelope/\"; ns=s");
        }
        g_free (full_action);

        /* Specify language */
        http_request_set_accept_language (action->msg);

        /* Accept gzip encoding */
        soup_message_headers_append (headers, "Accept-Encoding", "gzip");

        gupnp_service_proxy_action_serialize (action, service_type);

        GBytes *body = g_string_free_to_bytes (action->msg_str);

        soup_message_set_request_body_from_bytes (action->msg,
                                                  "text/xml; charset=\"utf-8\"",
                                                  body);
        g_bytes_unref (body);
        action->msg_str = NULL;

        action->proxy = proxy;
        g_object_add_weak_pointer (G_OBJECT (proxy),
                                   (gpointer *) &(action->proxy));

        return TRUE;
}

static void
gupnp_service_proxy_action_queue_task (GTask *task);

static void
action_task_got_response (GObject *source,
                          GAsyncResult *res,
                          gpointer user_data)
{
        GTask *task = G_TASK (user_data);
        GError *error = NULL;
        GUPnPServiceProxyAction *action =
                (GUPnPServiceProxyAction *) g_task_get_task_data (task);

        action->response =
                soup_session_send_and_read_finish (SOUP_SESSION (source),
                                                   res,
                                                   &error);


        if (error != NULL) {
                action->error = g_error_copy (error);
                g_task_return_error (task, error);
                g_object_unref (task);

                return;
        }

        switch (soup_message_get_status (action->msg)) {
        case SOUP_STATUS_METHOD_NOT_ALLOWED:
                if (g_str_equal (soup_message_get_method (action->msg),
                                 "POST")) {
                        g_debug ("POST returned with METHOD_NOT_ALLOWED, "
                                 "trying with M-POST");
                        g_clear_pointer (&action->response, g_bytes_unref);
                        if (!prepare_action_msg (action->proxy,
                                                 action,
                                                 "M-POST",
                                                 &error)) {

                                g_task_return_error (task, error);

                                g_object_unref (task);
                        } else {
                                gupnp_service_proxy_action_queue_task (task);
                        }

                } else {
                        g_task_return_new_error (task,
                                                 GUPNP_SERVER_ERROR,
                                                 GUPNP_SERVER_ERROR_OTHER,
                                                 "Server does not allow any POST messages");
                        g_object_unref (task);
                }
                break;

        default:
                gupnp_service_proxy_action_check_response (action);
                if (action->error != NULL) {
                        g_task_return_error (task,
                                             g_error_copy (action->error));
                } else {
                        g_task_return_pointer (task,
                                               g_task_get_task_data (task),
                                               NULL);
                }

                g_object_unref (task);

                break;
        }
}

static void
gupnp_service_proxy_action_queue_task (GTask *task)
{
        GUPnPContext *context;
        SoupSession *session;
        GUPnPServiceProxyAction *action = g_task_get_task_data (task);

        /* Send the message */
        context = gupnp_service_info_get_context (
                GUPNP_SERVICE_INFO (action->proxy));
        session = gupnp_context_get_session (context);

        soup_session_send_and_read_async (
                session,
                action->msg,
                G_PRIORITY_DEFAULT,
                g_task_get_cancellable (task),
                (GAsyncReadyCallback) action_task_got_response,
                task);
        action->pending = TRUE;
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
        NotifyCallbackData *callback_data;
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
        callback_data = g_slice_new (NotifyCallbackData);

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
 * Get a notification for anything that happens on the peer.
 *
 * @value in @callback will be of type G_TYPE_POINTER and contain the pre-parsed
 * [type@libxml2.Doc]. Do NOT free or modify this document.
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
                NotifyCallbackData *callback_data;

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
                NotifyCallbackData *callback_data;

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
                        NotifyCallbackData *callback_data = it->data;

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

        for (pending_notify = priv->pending_notifies->head;
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
        g_queue_clear_full (priv->pending_notifies,
                            (GDestroyNotify) emit_notify_data_free);

        if (resubscribe) {
                unsubscribe (proxy);
                subscribe (proxy);
        }

        priv->notify_idle_src = NULL;

        return FALSE;
}

/*
 * HTTP server received a message. Handle, if this was a NOTIFY
 * message with our SID.
 */
static void
server_handler (G_GNUC_UNUSED SoupServer *soup_server,
                SoupServerMessage *msg,
                G_GNUC_UNUSED const char *server_path,
                G_GNUC_UNUSED GHashTable *query,
                gpointer user_data)
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
        const char *method = soup_server_message_get_method (msg);

        SoupMessageHeaders *request_headers =
                soup_server_message_get_request_headers (msg);

        if (strcmp (method, GENA_METHOD_NOTIFY) != 0) {
                /* We don't implement this method */
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_NOT_IMPLEMENTED,
                                                "Method not supported");

                return;
        }

        nt = soup_message_headers_get_one (request_headers, "NT");
        nts = soup_message_headers_get_one (request_headers, "NTS");
        if (nt == NULL || nts == NULL) {
                /* Required header is missing */
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_BAD_REQUEST,
                                                "NT or NTS is missing");

                return;
        }

        if (strcmp (nt, "upnp:event") != 0 ||
            strcmp (nts, "upnp:propchange") != 0) {
                /* Unexpected header content */
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_PRECONDITION_FAILED,
                                                "Unexpected NT or NTS");

                return;
        }

        hdr = soup_message_headers_get_one (request_headers, "SEQ");
        if (hdr == NULL) {
                /* No SEQ header */
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_PRECONDITION_FAILED,
                                                "SEQ missing");

                return;
        }

        errno = 0;
        seq_parsed = strtoul (hdr, NULL, 10);
        if (errno != 0 || seq_parsed > G_MAXUINT32) {
                /* Invalid SEQ header value */
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_PRECONDITION_FAILED,
                                                "SEQ invalid");

                return;
        }

        seq = (guint32) seq_parsed;

        hdr = soup_message_headers_get_one (request_headers, "SID");
        if (hdr == NULL ||
            strlen (hdr) <= strlen ("uuid:") ||
            strncmp (hdr, "uuid:", strlen ("uuid:")) != 0) {
                /* No SID */
                soup_server_message_set_status (
                        msg,
                        SOUP_STATUS_PRECONDITION_FAILED,
                        "SID header missing or malformed");

                return;
        }

        SoupMessageBody *request_body =
                soup_server_message_get_request_body (msg);

        /* Parse the actual XML message content */
        doc = xmlReadMemory (request_body->data,
                             request_body->length,
                             NULL,
                             NULL,
                             XML_PARSE_NONET | XML_PARSE_RECOVER);
        if (doc == NULL) {
                /* Failed */
                g_warning ("Failed to parse NOTIFY message body");

                soup_server_message_set_status (
                        msg,
                        SOUP_STATUS_INTERNAL_SERVER_ERROR,
                        "Unable to parse NOTIFY message");

                return;
        }

        priv = gupnp_service_proxy_get_instance_private (proxy);
        /* Get root propertyset element */
        node = xmlDocGetRootElement (doc);
        if (node == NULL || strcmp ((char *) node->name, "propertyset") || priv->sid == NULL) {
                /* Empty or unsupported */
                xmlFreeDoc (doc);

                soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);

                return;
        }

        /*
         * Some UPnP stacks (hello, myigd/1.0) block when sending a NOTIFY, so
         * call the callbacks in an idle handler so that if the client calls the
         * device in the notify callback the server can actually respond.
         */
        emit_notify_data = emit_notify_data_new (hdr, seq, doc);

        g_queue_push_tail (priv->pending_notifies, emit_notify_data);
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
        soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
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

typedef struct {
        GUPnPServiceProxy *proxy;
        SoupMessage *msg;
} SubscriptionCallData;

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

        SoupMessageHeaders *request_headers =
                soup_message_get_request_headers (msg);

        /* Add headers */
        soup_message_headers_append (request_headers, "SID", priv->sid);

        timeout = make_timeout_header (context);
        soup_message_headers_append (request_headers, "Timeout", timeout);
        g_free (timeout);

        /* And send it off */
        session = gupnp_context_get_session (context);

        SubscriptionCallData *data = g_new0 (SubscriptionCallData, 1);
        data->msg = msg;
        data->proxy = proxy;

        soup_session_send_async (session,
                                 msg,
                                 G_PRIORITY_DEFAULT,
                                 priv->pending_messages,
                                 subscribe_got_response,
                                 data);

        return FALSE;
}

/*
 * Received subscription response.
 */
static void
subscribe_got_response (GObject *source, GAsyncResult *res, gpointer user_data)
{
        GError *error = NULL;
        GUPnPServiceProxyPrivate *priv;
        SubscriptionCallData *data = user_data;

        priv = gupnp_service_proxy_get_instance_private (data->proxy);

        GInputStream *is =
                soup_session_send_finish (SOUP_SESSION (source), res, &error);

        /* Cancelled? */
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                goto out;
        } else if (error != NULL) {
                // FIXME: Is just propagating the error the right way?
                goto hdr_err;
        }

        // We don't need the body, it should be empty anyway
        g_input_stream_close (is, NULL, NULL);
        g_object_unref (is);

        /* Remove subscription timeout */
        g_clear_pointer (&priv->subscription_timeout_src, g_source_destroy);

        /* Check whether the subscription is still wanted */
        if (!priv->subscribed) {
                goto out;
        }

        /* Reset SID */
        g_clear_pointer (&priv->sid, g_free);

        SoupStatus status = soup_message_get_status (data->msg);
        SoupMessageHeaders *response_headers =
                soup_message_get_response_headers (data->msg);

        /* Check message status */
        if (SOUP_STATUS_IS_SUCCESSFUL (status)) {
                /* Success. */
                const char *hdr;
                int timeout;

                /* Save SID. */
                hdr = soup_message_headers_get_one (response_headers, "SID");
                if (hdr == NULL) {
                        error = g_error_new
                                        (GUPNP_EVENTING_ERROR,
                                         GUPNP_EVENTING_ERROR_SUBSCRIPTION_LOST,
                                         "No SID in SUBSCRIBE response");

                        goto hdr_err;
                }

                priv->sid = g_strdup (hdr);

                /* Figure out when the subscription times out */
                hdr = soup_message_headers_get_one (response_headers,
                                                    "Timeout");
                if (hdr == NULL) {
                        g_warning ("No Timeout in SUBSCRIBE response.");

                        goto out;
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
                         * expires. We do that somewhat around the middle of the
                         * subscription period and introduce some random jitter
                         * around that, so we do not bombard the services with
                         * all the re-subscription all at once
                         */
                        int jitter =
                                g_random_int_range (-timeout / 4, timeout / 4);
                        timeout = timeout / 2 + jitter;

                        g_debug ("Service announced timeout of %s, will "
                                 "re-subscribe in %d seconds",
                                 hdr,
                                 timeout);

                        /* Add actual timeout */
                        priv->subscription_timeout_src =
                                g_timeout_source_new_seconds (timeout);
                        g_source_set_callback (priv->subscription_timeout_src,
                                               subscription_expire,
                                               data->proxy,
                                               NULL);
                        g_source_attach (priv->subscription_timeout_src,
                                         g_main_context_get_thread_default ());

                        g_source_unref (priv->subscription_timeout_src);
                }
        } else {
                GUPnPContext *context;
                SoupServer *server;

                /* Subscription failed. */
                error = g_error_new_literal (
                        GUPNP_EVENTING_ERROR,
                        GUPNP_EVENTING_ERROR_SUBSCRIPTION_FAILED,
                        soup_message_get_reason_phrase (data->msg));

hdr_err:
                /* Remove listener */
                context = gupnp_service_info_get_context (
                        GUPNP_SERVICE_INFO (data->proxy));

                server = gupnp_context_get_server (context);
                soup_server_remove_handler (server, priv->path);

                priv->subscribed = FALSE;

                g_object_notify (G_OBJECT (data->proxy), "subscribed");

                /* Emit subscription-lost */
                g_signal_emit (data->proxy,
                               signals[SUBSCRIPTION_LOST],
                               0,
                               error);

                g_error_free (error);
        }

out:
        g_object_unref (data->msg);
        g_free (user_data);
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
        GUri *uri;
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
        GUri *new_uri = soup_uri_copy (uri, SOUP_URI_PATH, priv->path, NULL);

        uri_string = g_uri_to_string_partial (new_uri, G_URI_HIDE_PASSWORD);
        g_uri_unref (new_uri);
        g_uri_unref (uri);

        delivery_url = g_strdup_printf ("<%s>", uri_string);
        g_free (uri_string);

        SoupMessageHeaders *request_headers =
                soup_message_get_request_headers (msg);

        soup_message_headers_append (request_headers, "Callback", delivery_url);
        g_free (delivery_url);

        soup_message_headers_append (request_headers, "NT", "upnp:event");

        timeout = make_timeout_header (context);
        soup_message_headers_append (request_headers, "Timeout", timeout);
        g_free (timeout);

        /* Listen for events */
        server = gupnp_context_get_server (context);

        soup_server_add_handler (server,
                                 priv->path,
                                 server_handler,
                                 proxy,
                                 NULL);

        /* And send our subscription message off */
        session = gupnp_context_get_session (context);

        SubscriptionCallData *data = g_new0 (SubscriptionCallData, 1);

        data->msg = msg;
        data->proxy = proxy;

        soup_session_send_async (session,
                                 msg,
                                 G_PRIORITY_DEFAULT,
                                 priv->pending_messages,
                                 subscribe_got_response,
                                 data);
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
                        soup_message_headers_append (
                                soup_message_get_request_headers (msg),
                                "SID",
                                priv->sid);

                        /* And queue it */
                        session = gupnp_context_get_session (context);

                        // We do not care about the result of that message
                        soup_session_send_async (
                                session,
                                msg,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                NULL,
                                NULL);
                        g_object_unref (msg);
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
        GError *error = NULL;

        g_return_if_fail (GUPNP_IS_SERVICE_PROXY (proxy));

        task = g_task_new (proxy, cancellable, callback, user_data);
        char *task_name = g_strdup_printf ("UPnP Call \"%s\"", action->name);
        g_task_set_name (task, task_name);
        g_free (task_name);
        g_task_set_task_data (
                task,
                gupnp_service_proxy_action_ref (action),
                (GDestroyNotify) gupnp_service_proxy_action_unref);

        prepare_action_msg (proxy, action, SOUP_METHOD_POST, &error);

        if (error != NULL) {
                g_task_return_error (task, error);
                g_object_unref (task);
        } else {
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
 * Returns: (nullable) (transfer none): %NULL, if the call had an error, the action otherwise.
 * Since: 1.2.0
 */
GUPnPServiceProxyAction *
gupnp_service_proxy_call_action_finish (GUPnPServiceProxy *proxy,
                                        GAsyncResult      *result,
                                        GError           **error)
{
        g_return_val_if_fail (g_task_is_valid (G_TASK (result), proxy), NULL);

        GUPnPServiceProxyAction *action =
                g_task_get_task_data (G_TASK (result));
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

        if (!prepare_action_msg (proxy, action, SOUP_METHOD_POST, error)) {
                return NULL;
        }

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));
        session = gupnp_context_get_session (context);
        action->response = soup_session_send_and_read (session,
                                                       action->msg,
                                                       cancellable,
                                                       &action->error);

        if (action->error != NULL) {
                goto out;
        }

        /* If not allowed, try again */
        if (soup_message_get_status (action->msg) ==
            SOUP_STATUS_METHOD_NOT_ALLOWED) {
                if (prepare_action_msg (proxy,
                                        action,
                                        "M-POST",
                                        &action->error)) {
                        g_clear_pointer (&action->response, g_bytes_unref);

                        action->response =
                                soup_session_send_and_read (session,
                                                            action->msg,
                                                            cancellable,
                                                            &action->error);
                }
        }

        if (action->error == NULL)
                gupnp_service_proxy_action_check_response (action);

out:

        if (action->error != NULL) {
                g_propagate_error (error, g_error_copy (action->error));

                return NULL;
        }

        g_clear_weak_pointer (&action->proxy);

        return action;
}


/**
 * gupnp_service_proxy_set_credentials:
 * @proxy: A #GUPnPServiceProxy
 * @user: user name for authentication
 * @password: user password for authentication
 *
 * Sets user and password for authentication
 *
 * Since: 1.6.4
 **/
void
gupnp_service_proxy_set_credentials (GUPnPServiceProxy *proxy,
                                     const char        *user,
                                     const char        *password)
{
  GUPnPServiceProxyPrivate *priv;

  priv = gupnp_service_proxy_get_instance_private (proxy);

  g_clear_pointer (&priv->user, g_free);
  g_clear_pointer (&priv->password, g_free);

  priv->user = g_strdup (user);
  priv->password = g_strdup (password);
}
