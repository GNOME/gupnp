/* 
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
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

/**
 * SECTION:gupnp-service-proxy
 * @short_description: Proxy class for remote services.
 *
 * #GUPnPServiceProxy sends commands to a remote UPnP service and handles
 * incoming event notifications. Service proxies also implement the
 * #GUPnPServiceInfo interface.
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <libsoup/soup-soap-message.h>
#include <gobject/gvaluecollector.h>
#include <string.h>
#include <time.h>
#include <locale.h>

#include "gupnp-service-proxy.h"
#include "gupnp-service-proxy-private.h"
#include "gupnp-device-proxy-private.h"
#include "gupnp-context-private.h"
#include "gupnp-error.h"
#include "xml-util.h"
#include "gena-protocol.h"
#include "accept-language.h"

G_DEFINE_TYPE (GUPnPServiceProxy,
               gupnp_service_proxy,
               GUPNP_TYPE_SERVICE_INFO);

struct _GUPnPServiceProxyPrivate {
        gboolean subscribed;

        GList *pending_actions;

        char *path; /* Path to this proxy */

        char *sid; /* Subscription ID */
        guint subscription_timeout_id;

        int seq; /* Event sequence number */

        GHashTable *notify_hash;
};

enum {
        PROP_0,
        PROP_SUBSCRIBED
};

enum {
        SUBSCRIPTION_LOST,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _GUPnPServiceProxyAction {
        GUPnPServiceProxy *proxy;

        SoupSoapMessage *msg;

        GUPnPServiceProxyActionCallback callback;
        gpointer user_data;

        va_list var_args; /* The va_list after begin_action_valist has
                             gone through it. Used by send_action_valist(). */
};

typedef struct {
        GType type;

        GList *callbacks;
} NotifyData;

typedef struct {
        GUPnPServiceProxyNotifyCallback callback;
        gpointer user_data;
} CallbackData;

static void
subscribe_got_response (SoupMessage       *msg,
                        GUPnPServiceProxy *proxy);
static void
subscribe (GUPnPServiceProxy *proxy);
static void
unsubscribe (GUPnPServiceProxy *proxy, gboolean sync);

static void
gupnp_service_proxy_action_free (GUPnPServiceProxyAction *action)
{
        action->proxy->priv->pending_actions =
                g_list_remove (action->proxy->priv->pending_actions, action);

        g_object_unref (action->msg);

        g_slice_free (GUPnPServiceProxyAction, action);
}

static void
notify_data_free (NotifyData *data)
{
        g_list_free (data->callbacks);

        g_slice_free (NotifyData, data);
}

static void
gupnp_service_proxy_init (GUPnPServiceProxy *proxy)
{
        static int proxy_counter = 0;

        proxy->priv = G_TYPE_INSTANCE_GET_PRIVATE (proxy,
                                                   GUPNP_TYPE_SERVICE_PROXY,
                                                   GUPnPServiceProxyPrivate);

        /* Generate unique path */
        proxy->priv->path = g_strdup_printf ("/ServiceProxy%d", proxy_counter);
        proxy_counter++;

        /* Set up notify hash */
        proxy->priv->notify_hash =
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

        proxy = GUPNP_SERVICE_PROXY (object);

        switch (property_id) {
        case PROP_SUBSCRIBED:
                g_value_set_boolean (value,
                                     proxy->priv->subscribed);
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
        GObjectClass *object_class;

        proxy = GUPNP_SERVICE_PROXY (object);

        /* Cancel any pending actions */
        while (proxy->priv->pending_actions) {
                GUPnPServiceProxyAction *action;

                action = proxy->priv->pending_actions->data;
                
                gupnp_service_proxy_cancel_action (proxy, action);
        }

        /* Unsubscribe */
        if (proxy->priv->subscribed) {
                /* We force unsubscribe to be sync as we want to make sure
                 * the message is sent before the application quits. */
                unsubscribe (proxy, TRUE);

                proxy->priv->subscribed = FALSE;
        }

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_service_proxy_parent_class);
        object_class->dispose (object);
}

static void
gupnp_service_proxy_finalize (GObject *object)
{
        GUPnPServiceProxy *proxy;
        GObjectClass *object_class;

        proxy = GUPNP_SERVICE_PROXY (object);

        g_free (proxy->priv->path);

        g_hash_table_destroy (proxy->priv->notify_hash);

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

        g_type_class_add_private (klass, sizeof (GUPnPServiceProxyPrivate));

        /**
         * GUPnPServiceProxy:subscribed
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
         * GUPnPServiceProxy::subscription-lost
         * @proxy: The #GUPnPServiceProxy that received the signal
         * @error: A pointer to a #GError describing why the subscription has
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
 * gupnp_service_proxy_send_action
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @error: The location where to store any error, or NULL
 * @Varargs: tuples of in parameter name, in paramater type, and in parameter
 * value, followed by NULL, and then tuples of out paramater name,
 * out parameter type, and out parameter value location, terminated with NULL
 *
 * Sends action @action with parameters @Varargs to the service exposed by
 * @proxy synchronously.
 *
 * Return value: TRUE if sending the action was succesful.
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
        ret = gupnp_service_proxy_send_action_valist (proxy,
                                                      action,
                                                      error,
                                                      var_args);
        va_end (var_args);

        return ret;
}

static void
stop_main_loop (GUPnPServiceProxy       *proxy,
                GUPnPServiceProxyAction *action,
                gpointer                 user_data)
{
        g_main_loop_quit ((GMainLoop *) user_data);
}

/**
 * gupnp_service_proxy_send_action_valist
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @error: The location where to store any error, or NULL
 * @var_args: va_list of tuples of in parameter name, in paramater type, and in
 * parameter value, followed by NULL, and then tuples of out paramater name,
 * out parameter type, and out parameter value location
 *
 * See gupnp_service_proxy_send_action(); this version takes a va_list for
 * use by language bindings.
 *
 * Return value: TRUE if sending the action was succesful.
 **/
gboolean
gupnp_service_proxy_send_action_valist (GUPnPServiceProxy *proxy,
                                        const char        *action,
                                        GError           **error,
                                        va_list            var_args)
{
        GUPnPContext *context;
        GMainContext *main_context;
        GMainLoop *main_loop;
        GUPnPServiceProxyAction *handle;
        
        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));
        main_context = gssdp_client_get_main_context (GSSDP_CLIENT (context));
        main_loop = g_main_loop_new (main_context, TRUE);

        handle = gupnp_service_proxy_begin_action_valist (proxy,
                                                          action,
                                                          stop_main_loop,
                                                          main_loop,
                                                          error,
                                                          var_args);
        if (!handle) {
                g_main_loop_unref (main_loop);

                return FALSE;
        }

        /* Loop till we get a reply (or time out) */
        if (g_main_loop_is_running (main_loop))
                g_main_loop_run (main_loop);

        g_main_loop_unref (main_loop);

        if (!gupnp_service_proxy_end_action_valist (proxy,
                                                    handle,
                                                    error,
                                                    handle->var_args))
                return FALSE;

        return TRUE;
}

/**
 * gupnp_service_proxy_begin_action
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @callback: The callback to call when sending the action has succeeded
 * or failed
 * @user_data: User data for @callback
 * @error: The location where to store any error, or NULL
 * @Varargs: tuples of in parameter name, in paramater type, and in parameter
 * value, terminated with NULL
 *
 * Sends action @action with parameters @Varargs to the service exposed by
 * @proxy asynchronously, reporting the result by calling @callback.
 *
 * Return value: A #GUPnPServiceProxyAction handle. This will be freed
 * automatically on @callback calling.
 **/
GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action (GUPnPServiceProxy              *proxy,
                                  const char                     *action,
                                  GUPnPServiceProxyActionCallback callback,
                                  gpointer                        user_data,
                                  GError                        **error,
                                  ...)
{
        va_list var_args;
        GUPnPServiceProxyAction *ret;

        va_start (var_args, error);
        ret = gupnp_service_proxy_begin_action_valist (proxy,
                                                       action,
                                                       callback,
                                                       user_data,
                                                       error,
                                                       var_args);
        va_end (var_args);

        return ret;
}

/* Received response to action message */
static void
action_got_response (SoupMessage             *msg,
                     GUPnPServiceProxyAction *action)
{
        if (msg->status == SOUP_STATUS_METHOD_NOT_ALLOWED) {
                /* Not allowed .. */
                const char *full_action;
                GUPnPContext *context;
                SoupSession *session;

                /* Retry with M-POST */
                msg->method = "M-POST";

                soup_message_add_header
                        (msg->request_headers,
                         "Man",
                         "\"http://schemas.xmlsoap.org/soap/envelope/\"; ns=s");

                /* Rename "SOAPAction" to "s-SOAPAction" */
                full_action = soup_message_get_header (msg->request_headers,
                                                       "SOAPAction");
                soup_message_add_header (msg->request_headers,
                                         "s-SOAPAction",
                                         full_action);
                soup_message_remove_header (msg->request_headers,
                                            "SOAPAction");

                /* And re-queue */
                context = gupnp_service_info_get_context
                                (GUPNP_SERVICE_INFO (action->proxy));
                session = _gupnp_context_get_session (context);

                soup_session_requeue_message (session, msg);
        } else {
                /* Success: Call callback */
                action->callback (action->proxy, action, action->user_data);
        }
}

/**
 * gupnp_service_proxy_begin_action_valist
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @callback: The callback to call when sending the action has succeeded
 * or failed
 * @user_data: User data for @callback
 * @error: The location where to store any error, or NULL
 * @var_args: A va_list of tuples of in parameter name, in paramater type, and 
 * in parameter value
 *
 * See gupnp_service_proxy_begin_action(); this version takes a va_list for
 * use by language bindings.
 *
 * Return value: A #GUPnPServiceProxyAction handle, or NULL on error. This will
 * be freed automatically on @callback calling.
 **/
GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action_valist
                                   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GUPnPServiceProxyActionCallback callback,
                                    gpointer                        user_data,
                                    GError                        **error,
                                    va_list                         var_args)
{
        char *control_url, *service_type, *full_action, *lang;
        const char *arg_name;
        SoupSoapMessage *msg;
        GUPnPServiceProxyAction *ret;
        GUPnPContext *context;
        SoupSession *session;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), NULL);
        g_return_val_if_fail (action, NULL);
        g_return_val_if_fail (callback, NULL);

        /* Create message */
        control_url = gupnp_service_info_get_control_url
                                        (GUPNP_SERVICE_INFO (proxy));
        msg = soup_soap_message_new (SOUP_METHOD_POST,
				     control_url,
				     FALSE, NULL, NULL, NULL);
        g_free (control_url);

        g_assert (msg != NULL);

        /* Specify language */
        lang = accept_language_get_header ();
        if (lang) {
                soup_message_add_header (SOUP_MESSAGE (msg)->request_headers,
                                         "Accept-Language",
                                         lang);

                g_free (lang);
        }

        /* Specify action */
        service_type = gupnp_service_info_get_service_type
                                        (GUPNP_SERVICE_INFO (proxy));
        full_action = g_strdup_printf ("\"%s#%s\"", service_type, action);
        soup_message_add_header (SOUP_MESSAGE (msg)->request_headers,
				 "SOAPAction",
                                 full_action);
        g_free (full_action);

        /* Fill envelope */
	soup_soap_message_start_envelope (msg);
	soup_soap_message_start_body (msg);

        /* Action element */
        soup_soap_message_start_element (msg,
                                         action,
                                         "u",
                                         service_type);

        g_free (service_type);

        /* Arguments */
        arg_name = va_arg (var_args, const char *);
        while (arg_name) {
                GType arg_type;
                GValue value = { 0, }, transformed_value = { 0, };
                char *collect_error = NULL;
                const char *str;

                arg_type = va_arg (var_args, GType);
                g_value_init (&value, arg_type);

                G_VALUE_COLLECT (&value, var_args, 0, &collect_error);
                if (collect_error) {
                        g_warning ("Error collecting value: %s\n",
                                   collect_error);

                        g_free (collect_error);
                        
                        /* we purposely leak the value here, it might not be
	                 * in a sane state if an error condition occoured
	                 */

                        g_object_unref (msg);

                        return NULL;
                }

                g_value_init (&transformed_value, G_TYPE_STRING);
                if (!g_value_transform (&value, &transformed_value)) {
                        g_warning ("Failed to transform value of type %s "
                                   "to a string", g_type_name (arg_type));
                        
                        g_value_unset (&value);
                        g_value_unset (&transformed_value);

                        g_object_unref (msg);

                        return NULL;
                }

                soup_soap_message_start_element (msg,
                                                 arg_name,
                                                 NULL,
                                                 NULL);

                str = g_value_get_string (&transformed_value);
                soup_soap_message_write_string (msg, str); 

                soup_soap_message_end_element (msg);

                g_value_unset (&value);
                g_value_unset (&transformed_value);
                
                arg_name = va_arg (var_args, const char *);
        }

        soup_soap_message_end_element (msg);
        
	soup_soap_message_end_body (msg);
	soup_soap_message_end_envelope (msg);
        soup_soap_message_persist (msg);

        /* Create action structure */
        ret = g_slice_new (GUPnPServiceProxyAction);

        ret->proxy = proxy;

        ret->msg = msg;

        ret->callback = callback;
        ret->user_data = user_data;

        proxy->priv->pending_actions =
                g_list_prepend (proxy->priv->pending_actions, ret);

        /* We keep our own reference to the message as well */
        g_object_ref (msg);

        /* Send the message */
        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));
        session = _gupnp_context_get_session (context);

        soup_session_queue_message (session,
                                    SOUP_MESSAGE (msg),
                                    (SoupMessageCallbackFn)
                                        action_got_response,
                                    ret);

        /* Save the current position in the va_list for send_action_valist() */
        ret->var_args = var_args;

        return ret;
}

/**
 * gupnp_service_proxy_end_action
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 * @error: The location where to store any error, or NULL
 * @Varargs: tuples of out parameter name, out paramater type, and out parameter
 * value location, terminated with NULL. The out parameter values should be
 * freed after use
 *
 * Retrieves the result of @action. The out parameters in @Varargs will be 
 * filled in, and ff an error occurred, @error will be set.
 *
 * Return value: TRUE on success.
 **/
gboolean
gupnp_service_proxy_end_action (GUPnPServiceProxy       *proxy,
                                GUPnPServiceProxyAction *action,
                                GError                 **error,
                                ...)
{
        va_list var_args;
        gboolean ret;

        va_start (var_args, error);
        ret = gupnp_service_proxy_end_action_valist (proxy,
                                                     action,
                                                     error,
                                                     var_args);
        va_end (var_args);

        return ret;
}

static void
set_error_literal (GError    **error,
                   GQuark      error_quark,
                   int         code,
                   const char *message)
{
        if (error == NULL)
                return;

        if (*error == NULL) {
                *error = g_error_new_literal (error_quark,
                                              code,
                                              message);
        } else
                g_warning ("Error already set.");
}

/* Convert errorCode to GUPnPControlError */
static int
convert_error_code (int code)
{
        switch (code) {
        case GUPNP_CONTROL_ERROR_INVALID_ACTION:
                return code;
        case GUPNP_CONTROL_ERROR_INVALID_ARGS:
                return code;
        case GUPNP_CONTROL_ERROR_OUT_OF_SYNC:
                return code;
        case GUPNP_CONTROL_ERROR_ACTION_FAILED:
                return code;
        default:
                if (code >= 600 && code < 700)
                        return GUPNP_CONTROL_ERROR_UPNP_FORUM_DEFINED;
                else if (code >= 700 && code < 800)
                        return GUPNP_CONTROL_ERROR_DEVICE_TYPE_DEFINED;
                else if (code >= 800 && code < 900)
                        return GUPNP_CONTROL_ERROR_VENDOR_DEFINED;
                else {
                        g_warning ("Unrecognised error code: %d", code);

                        return GUPNP_CONTROL_ERROR_VENDOR_DEFINED;
                }
        }
}

/**
 * gupnp_service_proxy_end_action_valist
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 * @error: The location where to store any error, or NULL
 * @var_args: A va_list of tuples of out parameter name, out paramater type, 
 * and out parameter value location. The out parameter values should be
 * freed after use
 *
 * See gupnp_service_proxy_end_action(); this version takes a va_list for
 * use by language bindings.
 *
 * Return value: TRUE on success.
 **/
gboolean
gupnp_service_proxy_end_action_valist (GUPnPServiceProxy       *proxy,
                                       GUPnPServiceProxyAction *action,
                                       GError                 **error,
                                       va_list                  var_args)
{
        SoupMessage *soup_msg;
        SoupSoapResponse *response;
        SoupSoapParameter *param;
        const char *arg_name;
        int code;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);

        soup_msg = SOUP_MESSAGE (action->msg);

        /* Check for errors */
        switch (soup_msg->status_code) {
        case SOUP_STATUS_OK:
        case SOUP_STATUS_INTERNAL_SERVER_ERROR:
                break;
        default:
                /* Convert SoupStatus to GUPnPServerError */
                switch (soup_msg->status_code) {
                case SOUP_STATUS_NOT_IMPLEMENTED:
                        code = GUPNP_SERVER_ERROR_NOT_IMPLEMENTED;
                        break;
                case SOUP_STATUS_NOT_FOUND:
                        code = GUPNP_SERVER_ERROR_NOT_FOUND;
                        break;
                default:
                        code = GUPNP_SERVER_ERROR_OTHER;
                        break;
                }

                set_error_literal (error,
                                   GUPNP_SERVER_ERROR,
                                   code,
                                   soup_msg->reason_phrase);
                
                gupnp_service_proxy_action_free (action);

                return FALSE;
        }

        /* Parse response */
	response = soup_soap_message_parse_response (action->msg);
	if (!response) {
                if (soup_msg->status_code == SOUP_STATUS_OK) {
                        g_set_error (error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                     "Could not parse SOAP response");
                } else {
                        set_error_literal
                                    (error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INTERNAL_SERVER_ERROR,
                                     soup_msg->reason_phrase);
                }
                
                gupnp_service_proxy_action_free (action);

                return FALSE;
	}

        /* Check whether we have a Fault */
        if (soup_msg->status_code == SOUP_STATUS_INTERNAL_SERVER_ERROR) {
                SoupSoapParameter *child;
                char *desc;

                param = soup_soap_response_get_first_parameter_by_name
                                                        (response, "detail");
                if (param) {
                        param = soup_soap_parameter_get_first_child_by_name
                                                        (param, "UPnPError");
                }

                if (!param) {
                        g_set_error (error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                     "Invalid Fault");

                        gupnp_service_proxy_action_free (action);

                        g_object_unref (response);

                        return FALSE;
                }

                /* Code */
                child = soup_soap_parameter_get_first_child_by_name
                                                (param, "errorCode");
                if (child) {
                        code = soup_soap_parameter_get_int_value (child);

                        code = convert_error_code (code);
                } else {
                        g_set_error (error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                     "Invalid Fault");

                        gupnp_service_proxy_action_free (action);

                        g_object_unref (response);

                        return FALSE;
                }

                /* Description */
                child = soup_soap_parameter_get_first_child_by_name
                                                (param, "errorDescription");
                if (child)
                        desc = soup_soap_parameter_get_string_value (child);
                else
                        desc = g_strdup (soup_msg->reason_phrase);

                set_error_literal (error,
                                   GUPNP_CONTROL_ERROR,
                                   code,
                                   desc);

                g_free (desc);
                
                /* Cleanup */
                gupnp_service_proxy_action_free (action);

                g_object_unref (response);

                return FALSE;
        }

        /* Arguments */
        arg_name = va_arg (var_args, const char *);
        while (arg_name) {
                GType arg_type;
                GValue value = { 0, }, int_value = { 0, };
                char *copy_error = NULL;
                char *str;
                int i;

                param = soup_soap_response_get_first_parameter_by_name
                                                        (response, arg_name);
                if (!param) {
                        g_set_error (error,
                                     GUPNP_CONTROL_ERROR,
                                     GUPNP_CONTROL_ERROR_INVALID_ARGS,
                                     "Could not find variable \"%s\" in "
                                     "response",
                                     arg_name);

                        gupnp_service_proxy_action_free (action);

                        g_object_unref (response);

                        return FALSE;
                }

                arg_type = va_arg (var_args, GType);

                g_value_init (&value, arg_type);

                switch (arg_type) {
                case G_TYPE_STRING:
                        str = soup_soap_parameter_get_string_value (param);

                        g_value_set_string_take_ownership (&value, str);

                        break;
                default:
                        i = soup_soap_parameter_get_int_value (param);

                        g_value_init (&int_value, G_TYPE_INT);
                        g_value_set_int (&int_value, i);

                        if (!g_value_transform (&int_value, &value)) {
                                g_warning ("Failed to transform integer "
                                           "value to type %s",
                                           g_type_name (arg_type));

                                g_value_unset (&int_value);
                                g_value_unset (&value);

                                gupnp_service_proxy_action_free (action);

                                g_object_unref (response);

                                return FALSE;
                        }

                        g_value_unset (&int_value);

                        break;
                }
                
                G_VALUE_LCOPY (&value, var_args, 0, &copy_error);

                g_value_unset (&value);

                if (copy_error) {
                        g_warning ("Error copying value: %s", copy_error);

                        g_free (copy_error);
                        
                        gupnp_service_proxy_action_free (action);

                        g_object_unref (response);

                        return FALSE;
                }

                arg_name = va_arg (var_args, const char *);
        }

        /* Cleanup */
        gupnp_service_proxy_action_free (action);

        g_object_unref (response);

        return TRUE;
}

/**
 * gupnp_service_proxy_cancel_action
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 *
 * Cancels @action, freeing the @action handle.
 **/
void
gupnp_service_proxy_cancel_action (GUPnPServiceProxy       *proxy,
                                   GUPnPServiceProxyAction *action)
{
        GUPnPContext *context;
        SoupSession *session;

        g_return_if_fail (GUPNP_IS_SERVICE_PROXY (proxy));
        g_return_if_fail (action);

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));
        session = _gupnp_context_get_session (context);
        
        soup_session_cancel_message (session, SOUP_MESSAGE (action->msg));

        gupnp_service_proxy_action_free (action);
}

/**
 * gupnp_service_proxy_add_notify
 * @proxy: A #GUPnPServiceProxy
 * @variable: The variable to add notification for
 * @type: The type of the variable
 * @callback: The callback to call when @variable changes
 * @user_data: User data for @callback
 *
 * Sets up @callback to be called whenever a change notification for 
 * @variable is recieved.
 *
 * Return value: TRUE on success.
 **/
gboolean
gupnp_service_proxy_add_notify (GUPnPServiceProxy              *proxy,
                                const char                     *variable,
                                GType                           type,
                                GUPnPServiceProxyNotifyCallback callback,
                                gpointer                        user_data)
{
        NotifyData *data;
        CallbackData *callback_data;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (variable, FALSE);
        g_return_val_if_fail (type, FALSE);
        g_return_val_if_fail (callback, FALSE);

        /* See if we already have notifications set up for this variable */
        data = g_hash_table_lookup (proxy->priv->notify_hash, variable);
        if (data == NULL) {
                /* No, create one */
                data = g_slice_new (NotifyData);

                data->type       = type;
                data->callbacks  = NULL;

                g_hash_table_insert (proxy->priv->notify_hash,
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

        data->callbacks = g_list_append (data->callbacks, callback_data);

        return TRUE;
}

/**
 * gupnp_service_proxy_remove_notify
 * @proxy: A #GUPnPServiceProxy
 * @variable: The variable to add notification for
 * @callback: The callback to call when @variable changes
 * @user_data: User data for @callback
 *
 * Cancels the variable change notification for @callback and @user_data.
 *
 * Return value: TRUE on success.
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

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (variable, FALSE);
        g_return_val_if_fail (callback, FALSE);

        /* Look up NotifyData for variable */
        data = g_hash_table_lookup (proxy->priv->notify_hash, variable);
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
                        g_slice_free (CallbackData, callback_data);

                        data->callbacks =
                                g_list_delete_link (data->callbacks, l);
                        if (data->callbacks == NULL) {
                                /* No callbacks left: Remove from hash */
                                g_hash_table_remove (proxy->priv->notify_hash,
                                                     data);
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
 * HTTP server received a message. Handle, if this was a NOTIFY
 * message with our SID.
 **/
static void
server_handler (SoupServerContext *server_context,
                SoupMessage       *msg,
                gpointer           user_data)
{
        GUPnPServiceProxy *proxy;
        const char *hdr;
        int seq;
        xmlDoc *doc;
        xmlNode *node;

        proxy = GUPNP_SERVICE_PROXY (user_data);

        if (strcmp (msg->method, GENA_METHOD_NOTIFY) != 0) {
                /* We don't implement this method */
                soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);

                return;
        }

        hdr = soup_message_get_header (msg->request_headers, "NT");
        if (hdr == NULL || strcmp (hdr, "upnp:event") != 0) {
                /* Proper NT header lacking */
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        hdr = soup_message_get_header (msg->request_headers, "NTS");
        if (hdr == NULL || strcmp (hdr, "upnp:propchange") != 0) {
                /* Proper NTS header lacking */
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        hdr = soup_message_get_header (msg->request_headers, "SID");
        if (hdr == NULL || strcmp (hdr, proxy->priv->sid) != 0) {
                /* No SID or not ours */
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        hdr = soup_message_get_header (msg->request_headers, "SEQ");
        if (hdr == NULL) {
                /* No SEQ header */
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        seq = atoi (hdr);
        if (seq > proxy->priv->seq) {
                /* Oops, we missed a notify. Resubscribe .. */
                unsubscribe (proxy, FALSE);
                subscribe (proxy);

                /* Message was OK otherwise */
                soup_message_set_status (msg, SOUP_STATUS_OK);

                return;
        }

        /* Increment our own event sequence number */
        if (proxy->priv->seq < G_MAXINT32)
                proxy->priv->seq++;
        else
                proxy->priv->seq = 1;

        /* Parse the actual XML message content */
        doc = xmlParseMemory (msg->request.body,
                              msg->request.length);
        if (doc == NULL) {
                /* Failed */
                g_warning ("Failed to parse NOTIFY message body");

                soup_message_set_status (msg,
                                         SOUP_STATUS_INTERNAL_SERVER_ERROR);

                return;
        }

        /* Get root propertyset element */
        node = xmlDocGetRootElement (doc);
        if (node == NULL || strcmp ((char *) node->name, "propertyset")) {
                /* Empty or unsupported */
                xmlFreeDoc (doc);

                soup_message_set_status (msg, SOUP_STATUS_OK);

                return;
        }

        /* Iterate over all provided properties */
        for (node = node->children; node; node = node->next) {
                xmlNode *var_node;

                if (strcmp ((char *) node->name, "property") != 0)
                        continue;

                /* property */
                for (var_node = node->children; var_node;
                     var_node = var_node->next) {
                        NotifyData *data;
                        GValue value = {0, };
                        GList *l;

                        data = g_hash_table_lookup (proxy->priv->notify_hash,
                                                    var_node->name);
                        if (data == NULL)
                                continue;

                        /* Make a GValue of the desired type */
                        g_value_init (&value, data->type);

                        if (!xml_util_node_get_content_value (var_node,
                                                              &value)) {
                                g_value_unset (&value);

                                continue;
                        }

                        /* Call callbacks */
                        for (l = data->callbacks; l; l = l->next) {
                                CallbackData *callback_data;

                                callback_data = l->data;

                                callback_data->callback
                                        (proxy,
                                         (const char *) var_node->name,
                                         &value,
                                         callback_data->user_data);
                        }

                        /* Cleanup */
                        g_value_unset (&value);
                }
        }

        xmlFreeDoc (doc);

        /* Everything went OK */
        soup_message_set_status (msg, SOUP_STATUS_OK);
}

/**
 * Generates a timeout header for the subscription timeout specified
 * in our GUPnPContext.
 **/
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

/**
 * Subscription expired.
 **/
static gboolean
subscription_expire (gpointer user_data)
{
        GUPnPServiceProxy *proxy;
        GUPnPContext *context;
        SoupMessage *msg;
        SoupSession *session;
        char *sub_url, *timeout;

        proxy = GUPNP_SERVICE_PROXY (user_data);

        /* Reset timeout ID */
        proxy->priv->subscription_timeout_id = 0;

        /* Send renewal message */
        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));

        /* Create subscription message */
        sub_url = gupnp_service_info_get_event_subscription_url
                                                (GUPNP_SERVICE_INFO (proxy));
        msg = soup_message_new (GENA_METHOD_SUBSCRIBE, sub_url);
        g_free (sub_url);

        g_assert (msg != NULL);

        /* Add headers */
        soup_message_add_header (msg->request_headers,
                                 "SID",
                                 proxy->priv->sid);

        timeout = make_timeout_header (context);
        soup_message_add_header (msg->request_headers,
                                 "Timeout",
                                 timeout);
        g_free (timeout);

        /* And send it off */
        session = _gupnp_context_get_session (context);

        soup_session_queue_message (session,
                                    msg,
                                    (SoupMessageCallbackFn)
                                        subscribe_got_response,
                                    proxy);

        return FALSE;
}

/**
 * Received subscription response.
 **/
static void
subscribe_got_response (SoupMessage       *msg,
                        GUPnPServiceProxy *proxy)
{
        /* Reset SID */
        g_free (proxy->priv->sid);
        proxy->priv->sid = NULL;

        /* Check message status */
        if (SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
                /* Success. */
                GUPnPContext *context;
                SoupServer *server;
                const char *hdr;
                int timeout;

                /* Save SID. */
                hdr = soup_message_get_header (msg->response_headers, "SID");
                if (hdr == NULL) {
                        GError *error;

                        proxy->priv->subscribed = FALSE;

                        g_object_notify (G_OBJECT (proxy), "subscribed");

                        /* Emit subscription-lost */
                        error = g_error_new
                                        (GUPNP_EVENTING_ERROR,
                                         GUPNP_EVENTING_ERROR_SUBSCRIPTION_LOST,
                                         "No SID in SUBSCRIBE response");

                        g_signal_emit (proxy,
                                       signals[SUBSCRIPTION_LOST],
                                       0,
                                       error);

                        g_error_free (error);

                        return;
                }

                proxy->priv->sid = g_strdup (hdr);

                /* Connect to 'server-message-received' signal */
                context = gupnp_service_info_get_context
                                                (GUPNP_SERVICE_INFO (proxy));

                server = _gupnp_context_get_server (context);
                soup_server_add_handler (server,
                                         proxy->priv->path,
                                         NULL,
                                         server_handler,
                                         NULL,
                                         proxy);

                /* Figure out when the subscription times out */
                hdr = soup_message_get_header (msg->response_headers,
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

                        /* Add actual timeout */
                        proxy->priv->subscription_timeout_id =
                                g_timeout_add (timeout * 1000,
                                               subscription_expire,
                                               proxy);
                }
        } else {
                GError *error;

                /* Subscription failed. */
                proxy->priv->subscribed = FALSE;

                g_object_notify (G_OBJECT (proxy), "subscribed");

                /* Emit subscription-lost */
                error = g_error_new (GUPNP_EVENTING_ERROR,
                                     GUPNP_EVENTING_ERROR_SUBSCRIPTION_FAILED,
                                     "(Re)subscription failed: %d",
                                     msg->status_code);

                g_signal_emit (proxy,
                               signals[SUBSCRIPTION_LOST],
                               0,
                               error);

                g_error_free (error);
        }
}

/**
 * Subscribe to this service.
 **/
static void
subscribe (GUPnPServiceProxy *proxy)
{
        GUPnPContext *context;
        SoupMessage *msg;
        SoupSession *session;
        const char *server_url;
        char *sub_url, *delivery_url, *timeout;

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));

        /* Create subscription message */
        sub_url = gupnp_service_info_get_event_subscription_url
                                                (GUPNP_SERVICE_INFO (proxy));
        msg = soup_message_new (GENA_METHOD_SUBSCRIBE, sub_url);
        g_free (sub_url);

        g_assert (msg != NULL);

        /* Add headers */
        server_url = _gupnp_context_get_server_url (context);
        delivery_url = g_strdup_printf ("<%s%s>",
                                        server_url,
                                        proxy->priv->path);
        soup_message_add_header (msg->request_headers,
                                 "Callback",
                                 delivery_url);
        g_free (delivery_url);

        soup_message_add_header (msg->request_headers,
                                 "NT",
                                 "upnp:event");

        timeout = make_timeout_header (context);
        soup_message_add_header (msg->request_headers,
                                 "Timeout",
                                 timeout);
        g_free (timeout);

        /* And send it off */
        session = _gupnp_context_get_session (context);

        soup_session_queue_message (session,
                                    msg,
                                    (SoupMessageCallbackFn)
                                        subscribe_got_response,
                                    proxy);
}

/**
 * Unsubscribe from this service.
 **/
static void
unsubscribe (GUPnPServiceProxy *proxy, gboolean sync)
{
        GUPnPContext *context;
        SoupMessage *msg;
        SoupSession *session;
        SoupServer *server;
        char *sub_url;

        if (proxy->priv->sid == NULL)
                return; /* No SID: nothing to unsubscribe */

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));

        /* Create unsubscription message */
        sub_url = gupnp_service_info_get_event_subscription_url
                                                (GUPNP_SERVICE_INFO (proxy));
        msg = soup_message_new (GENA_METHOD_UNSUBSCRIBE, sub_url);
        g_free (sub_url);

        g_assert (msg != NULL);

        /* Add headers */
        soup_message_add_header (msg->request_headers,
                                 "SID",
                                 proxy->priv->sid);

        /* And send it off */
        session = _gupnp_context_get_session (context);

        if (sync)
                soup_session_send_message (session, msg);
        else
                soup_session_queue_message (session, msg, NULL, NULL);
        
        /* Reset SID */
        g_free (proxy->priv->sid);
        proxy->priv->sid = NULL;

        /* Reset sequence number */
        proxy->priv->seq = 0;

        /* Remove subscription timeout */
        g_source_remove (proxy->priv->subscription_timeout_id);
        proxy->priv->subscription_timeout_id = 0;

        /* Remove server handler */
        server = _gupnp_context_get_server (context);
        soup_server_remove_handler (server, proxy->priv->path);
}

/**
 * gupnp_service_proxy_set_subscribed
 * @proxy: A #GUPnPServiceProxy
 * @subscribed: TRUE to subscribe to this service
 *
 * (Un)subscribes to this service.
 *
 * Note that the relevant messages are not immediately sent but queued.
 * If you want to unsubcribe from this service because the application
 * is quitting, rely on automatic synchronised unsubscription on object
 * destruction instead.
 **/
void
gupnp_service_proxy_set_subscribed (GUPnPServiceProxy *proxy,
                                    gboolean           subscribed)
{
        g_return_if_fail (GUPNP_IS_SERVICE_PROXY (proxy));

        if (proxy->priv->subscribed == subscribed)
                return;

        if (subscribed)
                subscribe (proxy);
        else
                unsubscribe (proxy, FALSE);

        proxy->priv->subscribed = subscribed;

        g_object_notify (G_OBJECT (proxy), "subscribed");
}

/**
 * gupnp_service_proxy_get_subscribed
 * @proxy: A #GUPnPServiceProxy
 *
 * Return value: TRUE if we are subscribed to this service.
 **/
gboolean
gupnp_service_proxy_get_subscribed (GUPnPServiceProxy *proxy)
{
        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);

        return proxy->priv->subscribed;
}

/**
 * @element: An #xmlNode pointing to a "device" element
 * @type: The type of the service element to find
 **/
static xmlNode *
find_service_element_for_type (xmlNode    *element,
                               const char *type)
{
        xmlNode *tmp;

        tmp = xml_util_get_element (element,
                                    "serviceList",
                                    NULL);
        if (tmp) {
                for (tmp = tmp->children; tmp; tmp = tmp->next) {
                        xmlNode *type_element;

                        type_element = xml_util_get_element (tmp,
                                                             "serviceType",
                                                             NULL);
                        if (type_element) {
                                xmlChar *content;
                                gboolean match;

                                content = xmlNodeGetContent (type_element);
                                
                                match = !strcmp (type, (char *) content);

                                xmlFree (content);

                                if (match)
                                        return tmp;
                        }
                }
        }

        return NULL;
}

/**
 * _gupnp_service_proxy_new_from_doc
 * @context: A #GUPnPContext
 * @doc: A device description document
 * @udn: The UDN of the device the service is contained in
 * @type: The type of the service to create a proxy for
 * @location: The location of the service description file
 *
 * Return value: A #GUPnPServiceProxy for the service with type @type from
 * the device with UDN @udn, as read from the device description file specified
 * by @doc.
 **/
GUPnPServiceProxy *
_gupnp_service_proxy_new_from_doc (GUPnPContext *context,
                                   xmlDoc       *doc,
                                   const char   *udn,
                                   const char   *type,
                                   const char   *location)
{
        GUPnPServiceProxy *proxy;
        xmlNode *root_element, *element;
        SoupUri *url_base;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (doc != NULL, NULL);
        g_return_val_if_fail (type != NULL, NULL);
        g_return_val_if_fail (location != NULL, NULL);

        root_element = xml_util_get_element ((xmlNode *) doc,
                                             "root",
                                             NULL);

        element = xml_util_get_element (root_element,
                                        "device",
                                        NULL);
        
        if (element) {
                element = _gupnp_device_proxy_find_element_for_udn (element,
                                                                    udn);

                if (element) {
                        element = find_service_element_for_type (element,
                                                                 type);
                }
        }

        if (!element) {
                g_warning ("Device description does not contain service "
                           "with type \"%s\".", type);

                return NULL;
        }

        /* Save the URL base, if any */
        url_base = xml_util_get_child_element_content_uri (root_element,
                                                           "URLBase",
                                                           NULL);
        if (!url_base)
                url_base = soup_uri_new (location);

        proxy = g_object_new (GUPNP_TYPE_SERVICE_PROXY,
                              "context", context,
                              "location", location,
                              "udn", udn,
                              "url-base", url_base,
                              "element", element,
                              NULL);

        return proxy;
}

/**
 * _gupnp_service_proxy_new_from_element
 * @context: A #GUPnPContext
 * @element: The #xmlNode ponting to the right service element
 * @location: The location of the service description file
 * @udn: The UDN of the device the service is contained in
 * @url_base: The URL base for this service, or NULL if none
 *
 * Return value: A #GUPnPServiceProxy for the service with element @element, as
 * read from the service description file specified by @location.
 **/
GUPnPServiceProxy *
_gupnp_service_proxy_new_from_element (GUPnPContext *context,
                                       xmlNode      *element,
                                       const char   *udn,
                                       const char   *location,
                                       SoupUri      *url_base)
{
        GUPnPServiceProxy *proxy;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (location != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        proxy = g_object_new (GUPNP_TYPE_SERVICE_PROXY,
                              "context", context,
                              "location", location,
                              "udn", udn,
                              "url-base", url_base,
                              "element", element,
                              NULL);

        return proxy;
}
