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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <libsoup/soup-soap-message.h>
#include <gobject/gvaluecollector.h>
#include <string.h>
#include <time.h>

#include "gupnp-service-proxy.h"
#include "gupnp-service-proxy-private.h"
#include "gupnp-device-proxy-private.h"
#include "gupnp-context-private.h"
#include "xml-util.h"
#include "gena-protocol.h"

G_DEFINE_TYPE (GUPnPServiceProxy,
               gupnp_service_proxy,
               GUPNP_TYPE_SERVICE_INFO);

struct _GUPnPServiceProxyPrivate {
        xmlNode *element;

        xmlChar *url_base;

        gboolean subscribed;

        GList *pending_actions;

        char *sid; /* Subscription ID */
        guint subscription_timeout_id;
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

static void
subscribe_got_response (SoupMessage       *msg,
                        GUPnPServiceProxy *proxy);
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

static xmlNode *
gupnp_service_proxy_get_element (GUPnPServiceInfo *info)
{
        GUPnPServiceProxy *proxy;
        
        proxy = GUPNP_SERVICE_PROXY (info);

        return proxy->priv->element;
}

static const char *
gupnp_service_proxy_get_url_base (GUPnPServiceInfo *info)
{
        GUPnPServiceProxy *proxy;
        
        proxy = GUPNP_SERVICE_PROXY (info);

        return (const char *) proxy->priv->url_base;
}

static void
gupnp_service_proxy_init (GUPnPServiceProxy *proxy)
{
        proxy->priv = G_TYPE_INSTANCE_GET_PRIVATE (proxy,
                                                   GUPNP_TYPE_SERVICE_PROXY,
                                                   GUPnPServiceProxyPrivate);
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
}

static void
gupnp_service_proxy_finalize (GObject *object)
{
        GUPnPServiceProxy *proxy;

        proxy = GUPNP_SERVICE_PROXY (object);

        if (proxy->priv->url_base)
                xmlFree (proxy->priv->url_base);
}

static void
gupnp_service_proxy_class_init (GUPnPServiceProxyClass *klass)
{
        GObjectClass *object_class;
        GUPnPServiceInfoClass *info_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_service_proxy_set_property;
        object_class->get_property = gupnp_service_proxy_get_property;
        object_class->dispose      = gupnp_service_proxy_dispose;
        object_class->finalize     = gupnp_service_proxy_finalize;

        info_class = GUPNP_SERVICE_INFO_CLASS (klass);
        
        info_class->get_element  = gupnp_service_proxy_get_element;
        info_class->get_url_base = gupnp_service_proxy_get_url_base;
        
        g_type_class_add_private (klass, sizeof (GUPnPServiceProxyPrivate));

        g_object_class_install_property
                (object_class,
                 PROP_SUBSCRIBED,
                 g_param_spec_boolean ("subscribed",
                                       "Subscribed",
                                       "TRUE if we are subscribed to the "
                                       "service",
                                       FALSE,
                                       G_PARAM_READWRITE));
                                       
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
 * Return value: TRUE if sending the action was succesfull.
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
 * Return value: TRUE if sending the action was succesfull.
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
        main_loop = g_main_loop_new (main_context, FALSE);

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

static void
action_got_response (SoupMessage             *msg,
                     GUPnPServiceProxyAction *action)
{
        action->callback (action->proxy, action, action->user_data);
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
        char *control_url, *service_type, *full_action;
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
                        g_set_error (error,
                                     GUPNP_ERROR_QUARK,
                                     0,
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
                        g_set_error (error,
                                     GUPNP_ERROR_QUARK,
                                     0,
                                     "Failed to transform value of type %s "
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
        const char *arg_name;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);

        /* Check for errors */
        soup_msg = SOUP_MESSAGE (action->msg);
	if (!SOUP_STATUS_IS_SUCCESSFUL (soup_msg->status_code)) {
                /* XXX full error handling */
                g_set_error (error,
                             GUPNP_ERROR_QUARK,
                             soup_msg->status_code,
                             soup_msg->reason_phrase);
                
                gupnp_service_proxy_action_free (action);

                return FALSE;
	}

        /* Parse response */
	response = soup_soap_message_parse_response (action->msg);
	if (!response) {
                g_set_error (error,
                             GUPNP_ERROR_QUARK,
                             0,
                             "Could not parse SOAP response");
                
                gupnp_service_proxy_action_free (action);

                return FALSE;
	}

        /* Arguments */
        arg_name = va_arg (var_args, const char *);
        while (arg_name) {
                GType arg_type;
                GValue value = { 0, }, int_value = { 0, };
                SoupSoapParameter *param;
                char *copy_error = NULL;
                char *str;
                int i;

                param = soup_soap_response_get_first_parameter_by_name
                                                        (response, arg_name);
                if (!param) {
                        g_set_error (error,
                                     GUPNP_ERROR_QUARK,
                                     0,
                                     "Could not find variable '%s' in response",
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
                                g_set_error (error,
                                             GUPNP_ERROR_QUARK,
                                             0,
                                             "Failed to transform string value "
                                             "to type %s",
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
                if (copy_error) {
                        g_set_error (error,
                                     GUPNP_ERROR_QUARK,
                                     0,
                                     copy_error);

                        g_free (copy_error);

                        g_value_unset (&value);
                        
                        gupnp_service_proxy_action_free (action);

                        g_object_unref (response);

                        return FALSE;
                }

                g_value_unset (&value);
                
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
 * @callback: The callback to call when @variable changes
 * @user_data: User data for @callback
 *
 * Sets up @callback to be called whenever a change notification for 
 * @variable is recieved.
 *
 * Return value: A new notification ID.
 **/
guint
gupnp_service_proxy_add_notify (GUPnPServiceProxy              *proxy,
                                const char                     *variable,
                                GUPnPServiceProxyNotifyCallback callback,
                                gpointer                        user_data)
{
        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), 0);
        g_return_val_if_fail (variable, 0);
        g_return_val_if_fail (callback, 0);

        return 0;
}

/**
 * gupnp_service_proxy_remove_notify
 * @proxy: A #GUPnPServiceProxy
 * @id: A notification ID
 *
 * Cancels the variable change notification with ID @id.
 **/
void
gupnp_service_proxy_remove_notify (GUPnPServiceProxy *proxy,
                                   guint              id)
{
        g_return_if_fail (GUPNP_IS_SERVICE_PROXY (proxy));
        g_return_if_fail (id > 0);
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
                const char *hdr;
                int timeout = 0;

                /* Save SID. */
                hdr = soup_message_get_header (msg->response_headers, "SID");
                if (hdr == NULL) {
                        g_error ("No SID in SUBSCRIBE response");

                        proxy->priv->subscribed = FALSE;

                        g_object_notify (G_OBJECT (proxy), "subscribed");

                        return;
                }

                proxy->priv->sid = g_strdup (hdr);

                /* Figure out when the subscription times out */
                hdr = soup_message_get_header (msg->response_headers, "Date");
                if (hdr != NULL) {
                        struct tm gen_date;
                        time_t gen_time, cur_time;

                        /* RFC 1123 date */
                        strptime (hdr,
                                  "%a, %d %b %Y %T %z",
                                  &gen_date);
                        
                        gen_time = mktime (&gen_date);
                        cur_time = time (NULL);

                        /* Negatively offset the timeout by the difference
                         * in times */
                        if (gen_time < cur_time)
                                timeout = gen_time - cur_time;
                }

                hdr = soup_message_get_header (msg->response_headers,
                                               "Timeout");
                if (hdr == NULL) {
                        g_error ("No Timeout in SUBSCRIBE response");
                        return;
                }

                if (strncmp (hdr, "Second-", strlen ("Second-")) == 0) {
                        /* We have a finite timeout */
                        timeout += atoi (hdr + strlen ("Second-"));
                        if (timeout < 0) {
                                g_warning ("Date specified in SUBSCRIBE "
                                           "response is too far in the past. "
                                           "Assuming default time-out of %d.",
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
                /* Subscription failed. */
                g_warning ("Subscription failed: %d", msg->status_code);

                proxy->priv->subscribed = FALSE;

                g_object_notify (G_OBJECT (proxy), "subscribed");
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
        delivery_url = g_build_path ("/", server_url, GENA_NOTIFY_PATH, NULL);
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

        /* Remove subscription timeout */
        g_source_remove (proxy->priv->subscription_timeout_id);
        proxy->priv->subscription_timeout_id = 0;
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
 * gupnp_service_proxy_new
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
gupnp_service_proxy_new (GUPnPContext *context,
                         xmlDoc       *doc,
                         const char   *udn,
                         const char   *type,
                         const char   *location)
{
        GUPnPServiceProxy *proxy;
        xmlNode *url_base_element;

        g_return_val_if_fail (doc, NULL);
        g_return_val_if_fail (type, NULL);

        proxy = g_object_new (GUPNP_TYPE_SERVICE_PROXY,
                              "context", context,
                              "location", location,
                              "udn", udn,
                              NULL);

        proxy->priv->element =
                xml_util_get_element ((xmlNode *) doc,
                                      "root",
                                      "device",
                                      NULL);
        
        if (proxy->priv->element) {
                proxy->priv->element =
                        _gupnp_device_proxy_find_element_for_udn
                                (proxy->priv->element, udn);

                if (proxy->priv->element) {
                        proxy->priv->element = 
                                find_service_element_for_type
                                        (proxy->priv->element, type);
                }
        }

        if (!proxy->priv->element) {
                g_warning ("Device description does not contain service "
                           "with type \"%s\".", type);

                g_object_unref (proxy);
                proxy = NULL;
        }

        /* Save the URL base, if any */
        url_base_element =
                xml_util_get_element ((xmlNode *) doc,
                                      "root",
                                      "URLBase",
                                      NULL);
        if (url_base_element != NULL)
                proxy->priv->url_base = xmlNodeGetContent (url_base_element);
        else
                proxy->priv->url_base = NULL;

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
_gupnp_service_proxy_new_from_element (GUPnPContext  *context,
                                       xmlNode       *element,
                                       const char    *udn,
                                       const char    *location,
                                       const xmlChar *url_base)
{
        GUPnPServiceProxy *proxy;

        g_return_val_if_fail (element, NULL);

        proxy = g_object_new (GUPNP_TYPE_SERVICE_PROXY,
                              "context", context,
                              "location", location,
                              "udn", udn,
                              NULL);

        proxy->priv->element = element;

        if (url_base != NULL)
                proxy->priv->url_base = xmlStrdup (url_base);
        else
                proxy->priv->url_base = NULL;

        return proxy;
}
