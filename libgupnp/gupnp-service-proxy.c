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

#include <string.h>

#include "gupnp-service-proxy.h"
#include "gupnp-service-proxy-private.h"
#include "gupnp-device-proxy-private.h"
#include "xml-util.h"

static void
gupnp_service_proxy_info_init (GUPnPServiceInfoIface *iface);

G_DEFINE_TYPE_EXTENDED (GUPnPServiceProxy,
                        gupnp_service_proxy,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUPNP_TYPE_SERVICE_INFO,
                                               gupnp_service_proxy_info_init));

struct _GUPnPServiceProxyPrivate {
        char *location;
        char *udn;

        xmlNode *element;

        gboolean subscribed;
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

static const char *
gupnp_service_proxy_get_location (GUPnPServiceInfo *info)
{
        GUPnPServiceProxy *proxy;
        
        proxy = GUPNP_SERVICE_PROXY (info);
        
        return proxy->priv->location;
}

static const char *
gupnp_service_proxy_get_udn (GUPnPServiceInfo *info)
{
        GUPnPServiceProxy *proxy;
        
        proxy = GUPNP_SERVICE_PROXY (info);
        
        return proxy->priv->udn;
}

static xmlNode *
gupnp_service_proxy_get_element (GUPnPServiceInfo *info)
{
        GUPnPServiceProxy *proxy;
        
        proxy = GUPNP_SERVICE_PROXY (info);

        return proxy->priv->element;
}

static void
gupnp_service_proxy_init (GUPnPServiceProxy *proxy)
{
        proxy->priv = G_TYPE_INSTANCE_GET_PRIVATE (proxy,
                                                   GUPNP_TYPE_SERVICE_PROXY,
                                                   GUPnPServiceProxyPrivate);
}

static void
gupnp_service_proxy_finalize (GObject *object)
{
        GUPnPServiceProxy *proxy;

        proxy = GUPNP_SERVICE_PROXY (object);

        g_free (proxy->priv->location);
        g_free (proxy->priv->udn);
}

static void
gupnp_service_proxy_class_init (GUPnPServiceProxyClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gupnp_service_proxy_finalize;
        
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

static void
gupnp_service_proxy_info_init (GUPnPServiceInfoIface *iface)
{
        iface->get_location = gupnp_service_proxy_get_location;
        iface->get_udn      = gupnp_service_proxy_get_udn;
        iface->get_element  = gupnp_service_proxy_get_element;
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
        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);

        return FALSE;
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
        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);

        return FALSE;
}

/**
 * gupnp_service_proxy_begin_action
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @callback: The callback to call when sending the action has succeeded
 * or failed
 * @user_data: User data for @callback
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
                                  ...)
{
        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), NULL);
        g_return_val_if_fail (action, NULL);
        g_return_val_if_fail (callback, NULL);

        return NULL;
}

/**
 * gupnp_service_proxy_begin_action_valist
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @callback: The callback to call when sending the action has succeeded
 * or failed
 * @user_data: User data for @callback
 * @var_args: A va_list of tuples of in parameter name, in paramater type, and 
 * in parameter value
 *
 * See gupnp_service_proxy_begin_action(); this version takes a va_list for
 * use by language bindings.
 *
 * Return value: A #GUPnPServiceProxyAction handle. This will be freed
 * automatically on @callback calling.
 **/
GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action_valist
                                   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GUPnPServiceProxyActionCallback callback,
                                    gpointer                        user_data,
                                    va_list                         var_args)
{
        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), NULL);
        g_return_val_if_fail (action, NULL);
        g_return_val_if_fail (callback, NULL);

        return NULL;
}

/**
 * gupnp_service_proxy_end_action
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 * @error: The location where to store any error, or NULL
 * @Varargs: tuples of out parameter name, out paramater type, and out parameter
 * value location, terminated with NULL
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
        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);

        return FALSE;
}

/**
 * gupnp_service_proxy_end_action_valist
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 * @error: The location where to store any error, or NULL
 * @var_args: A va_list of tuples of out parameter name, out paramater type, 
 * and out parameter value location
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
        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);

        return FALSE;
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
        g_return_if_fail (GUPNP_IS_SERVICE_PROXY (proxy));
        g_return_if_fail (action);
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
 * gupnp_service_proxy_set_subscribed
 * @proxy: A #GUPnPServiceProxy
 * @subscribed: TRUE to subscribe to this service
 *
 * (Un)subscribes to this service.
 **/
void
gupnp_service_proxy_set_subscribed (GUPnPServiceProxy *proxy,
                                    gboolean           subscribed)
{
        g_return_if_fail (GUPNP_IS_SERVICE_PROXY (proxy));

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
gupnp_service_proxy_new (xmlDoc     *doc,
                         const char *udn,
                         const char *type,
                         const char *location)
{
        GUPnPServiceProxy *proxy;

        g_return_val_if_fail (doc, NULL);
        g_return_val_if_fail (udn, NULL);
        g_return_val_if_fail (type, NULL);

        proxy = g_object_new (GUPNP_TYPE_SERVICE_PROXY,
                              NULL);

        proxy->priv->udn      = g_strdup (udn);
        proxy->priv->location = g_strdup (location);

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

        return proxy;
}

/**
 * _gupnp_service_proxy_new_from_element
 * @element: The #xmlNode ponting to the right service element
 * @location: The location of the service description file
 *
 * Return value: A #GUPnPServiceProxy for the service with element @element, as
 * read from the service description file specified by @location.
 **/
GUPnPServiceProxy *
_gupnp_service_proxy_new_from_element (xmlNode    *element,
                                       const char *location)
{
        GUPnPServiceProxy *proxy;

        g_return_val_if_fail (location, NULL);
        g_return_val_if_fail (element, NULL);

        proxy = g_object_new (GUPNP_TYPE_SERVICE_PROXY,
                              NULL);

        proxy->priv->location = g_strdup (location);

        proxy->priv->element = element;

        return proxy;
}
