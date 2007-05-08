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

#include <string.h>

#include "gupnp-device-proxy.h"
#include "gupnp-device-proxy-private.h"
#include "gupnp-service-proxy-private.h"
#include "xml-util.h"

static GUPnPDeviceProxy *
_gupnp_device_proxy_new_from_element (GUPnPContext  *context,
                                      xmlNode       *element,
                                      const char    *udn,
                                      const char    *location,
                                      const xmlChar *url_base);

G_DEFINE_TYPE (GUPnPDeviceProxy,
               gupnp_device_proxy,
               GUPNP_TYPE_DEVICE_INFO);

struct _GUPnPDeviceProxyPrivate {
        xmlNode *element;

        xmlChar *url_base;
};

static void
gupnp_device_proxy_finalize (GObject *object)
{
        GUPnPDeviceProxy *proxy;
        
        proxy = GUPNP_DEVICE_PROXY (object);

        if (proxy->priv->element)
                xmlFree (proxy->priv->element);
}

static xmlNode *
gupnp_device_proxy_get_element (GUPnPDeviceInfo *info)
{
        GUPnPDeviceProxy *proxy;
        
        proxy = GUPNP_DEVICE_PROXY (info);

        return proxy->priv->element;
}

static const char *
gupnp_device_proxy_get_url_base (GUPnPDeviceInfo *info)
{
        GUPnPDeviceProxy *proxy;
        
        proxy = GUPNP_DEVICE_PROXY (info);

        return (const char *) proxy->priv->url_base;
}

static void
gupnp_device_proxy_init (GUPnPDeviceProxy *proxy)
{
        proxy->priv = G_TYPE_INSTANCE_GET_PRIVATE (proxy,
                                                   GUPNP_TYPE_DEVICE_PROXY,
                                                   GUPnPDeviceProxyPrivate);
}

static void
gupnp_device_proxy_class_init (GUPnPDeviceProxyClass *klass)
{
        GObjectClass *object_class;
        GUPnPDeviceInfoClass *info_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gupnp_device_proxy_finalize;

        info_class = GUPNP_DEVICE_INFO_CLASS (klass);

        info_class->get_element  = gupnp_device_proxy_get_element;
        info_class->get_url_base = gupnp_device_proxy_get_url_base;
       
        g_type_class_add_private (klass, sizeof (GUPnPDeviceProxyPrivate));
}

/**
 * gupnp_device_proxy_list_devices
 * @proxy: A #GUPnPDeviceProxy
 *
 * Return value: A #GList of #GUPnPDeviceProxy objects representing the
 * devices directly contained in @proxy. The returned list should be
 * g_list_free()'d and the elements should be g_object_unref()'d.
 **/
GList *
gupnp_device_proxy_list_devices (GUPnPDeviceProxy *proxy)
{
        GList *devices;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_PROXY (proxy), NULL);

        devices = NULL;

        element = xml_util_get_element (proxy->priv->element,
                                        "deviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("device", (char *) element->name)) {
                        GUPnPContext *context;
                        const char *location, *udn;
                        GUPnPDeviceProxy *child;
                                
                        context = gupnp_device_info_get_context
                                                (GUPNP_DEVICE_INFO (proxy));
                        location = gupnp_device_info_get_location
                                                (GUPNP_DEVICE_INFO (proxy));
                        udn = gupnp_device_info_get_udn
                                                (GUPNP_DEVICE_INFO (proxy));

                        child = _gupnp_device_proxy_new_from_element
                                                (context,
                                                 element,
                                                 udn,
                                                 location,
                                                 proxy->priv->url_base);

                        devices = g_list_prepend (devices, child);
                }
        }

        return devices;
}

/**
 * gupnp_device_proxy_list_device_types
 * @proxy: A #GUPnPDeviceProxy
 *
 * Return value: A #GList of strings representing the types of the devices
 * directly contained in @proxy. The returned list should be g_list_free()'d
 * and the elements should be g_free()'d.
 **/
GList *
gupnp_device_proxy_list_device_types (GUPnPDeviceProxy *proxy)
{
        GList *device_types;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_PROXY (proxy), NULL);

        device_types = NULL;

        element = xml_util_get_element (proxy->priv->element,
                                        "deviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("device", (char *) element->name)) {
                        xmlNode *type_element;
                        xmlChar *type;

                        type_element = xml_util_get_element (element,
                                                             "deviceType",
                                                             NULL);
                        if (!type_element)
                                continue;

                        type = xmlNodeGetContent (type_element);
                        if (!type)
                                continue;

                        device_types =
                                g_list_prepend (device_types,
                                                g_strdup ((char *) type));
                        xmlFree (type);
                }
        }

        return device_types;
}

/**
 * gupnp_device_proxy_get_device
 * @proxy: A #GUPnPDeviceProxy
 * @type: The type of the device to be retrieved.
 *
 * Return value: The device with type @type directly contained in @proxy as
 * a #GUPnPDeviceProxy object, or NULL if no such device was found.
 **/
GUPnPDeviceProxy *
gupnp_device_proxy_get_device (GUPnPDeviceProxy *proxy,
                               const char       *type)
{
        GUPnPDeviceProxy *child;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_PROXY (proxy), NULL);
        g_return_val_if_fail (type, NULL);

        child = NULL;

        element = xml_util_get_element (proxy->priv->element,
                                        "deviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("device", (char *) element->name)) {
                        xmlNode *type_element;
                        xmlChar *type_str;

                        type_element = xml_util_get_element (element,
                                                             "deviceType",
                                                             NULL);
                        if (!type_element)
                                continue;

                        type_str = xmlNodeGetContent (type_element);
                        if (!type_str)
                                continue;

                        if (!strcmp (type, (char *) type_str)) {
                                GUPnPContext *context;
                                const char *location, *udn;
                                
                                context = gupnp_device_info_get_context
                                                (GUPNP_DEVICE_INFO (proxy));
                                location = gupnp_device_info_get_location
                                                (GUPNP_DEVICE_INFO (proxy));
                                udn = gupnp_device_info_get_udn
                                                (GUPNP_DEVICE_INFO (proxy));

                                child = _gupnp_device_proxy_new_from_element
                                                       (context,
                                                        element,
                                                        udn,
                                                        location,
                                                        proxy->priv->url_base);
                        }

                        xmlFree (type_str);

                        if (child)
                                break;
                }
        }

        return child;
}

/**
 * gupnp_device_proxy_list_services
 * @proxy: A #GUPnPDeviceProxy
 *
 * Return value: A #GList of #GUPnPServiceProxy objects representing the
 * services directly contained in @proxy. The returned list should be
 * g_list_free()'d and the elements should be g_object_unref()'d.
 **/
GList *
gupnp_device_proxy_list_services (GUPnPDeviceProxy *proxy)
{
        GList *services;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_PROXY (proxy), NULL);

        services = NULL;

        element = xml_util_get_element (proxy->priv->element,
                                        "serviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("service", (char *) element->name)) {
                        GUPnPContext *context;
                        const char *location, *udn;
                        GUPnPServiceProxy *service;

                        context = gupnp_device_info_get_context
                                                (GUPNP_DEVICE_INFO (proxy));
                        location = gupnp_device_info_get_location
                                                (GUPNP_DEVICE_INFO (proxy));
                        udn = gupnp_device_info_get_udn
                                                (GUPNP_DEVICE_INFO (proxy));

                        service = _gupnp_service_proxy_new_from_element
                                                (context,
                                                 element,
                                                 udn,
                                                 location,
                                                 proxy->priv->url_base);

                        services = g_list_prepend (services, service);
                }
        }

        return services;
}

/**
 * gupnp_device_proxy_list_service_types
 * @proxy: A #GUPnPDeviceProxy
 *
 * Return value: A #GList of strings representing the types of the services
 * directly contained in @proxy. The returned list should be g_list_free()'d
 * and the elements should be g_free()'d.
 **/
GList *
gupnp_device_proxy_list_service_types (GUPnPDeviceProxy *proxy)
{
        GList *service_types;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_PROXY (proxy), NULL);

        service_types = NULL;

        element = xml_util_get_element (proxy->priv->element,
                                        "serviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("service", (char *) element->name)) {
                        xmlNode *type_element;
                        xmlChar *type;

                        type_element = xml_util_get_element (element,
                                                             "serviceType",
                                                             NULL);
                        if (!type_element)
                                continue;

                        type = xmlNodeGetContent (type_element);
                        if (!type)
                                continue;

                        service_types =
                                g_list_prepend (service_types,
                                                g_strdup ((char *) type));
                        xmlFree (type);
                }
        }

        return service_types;
}

/**
 * gupnp_device_proxy_get_service
 * @proxy: A #GUPnPDeviceProxy
 * @type: The type of the service to be retrieved.
 *
 * Return value: The service with type @type directly contained in @proxy as
 * a #GUPnPServiceProxy object, or NULL if no such service was found.
 **/
GUPnPServiceProxy *
gupnp_device_proxy_get_service (GUPnPDeviceProxy *proxy,
                                const char       *type)
{
        GUPnPServiceProxy *service;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_PROXY (proxy), NULL);
        g_return_val_if_fail (type, NULL);

        service = NULL;

        element = xml_util_get_element (proxy->priv->element,
                                        "serviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("service", (char *) element->name)) {
                        xmlNode *type_element;
                        xmlChar *type_str;

                        type_element = xml_util_get_element (element,
                                                             "serviceType",
                                                             NULL);
                        if (!type_element)
                                continue;

                        type_str = xmlNodeGetContent (type_element);
                        if (!type_str)
                                continue;

                        if (!strcmp (type, (char *) type_str)) {
                                GUPnPContext *context;
                                const char *location, *udn;
                                GUPnPServiceProxy *service;

                                context = gupnp_device_info_get_context
                                                (GUPNP_DEVICE_INFO (proxy));
                                location = gupnp_device_info_get_location
                                                (GUPNP_DEVICE_INFO (proxy));
                                udn = gupnp_device_info_get_udn
                                                (GUPNP_DEVICE_INFO (proxy));

                                service = _gupnp_service_proxy_new_from_element
                                                        (context,
                                                         element,
                                                         udn,
                                                         location,
                                                         proxy->priv->url_base);
                        }

                        xmlFree (type_str);

                        if (service)
                                break;
                }
        }

        return service;
}

/**
 * @element: An #xmlNode pointing to a "device" element
 * @udn: The UDN of the device element to find
 **/
xmlNode *
_gupnp_device_proxy_find_element_for_udn (xmlNode    *element,
                                          const char *udn)
{
        xmlNode *tmp;

        tmp = xml_util_get_element (element,
                                    "UDN",
                                    NULL);
        if (tmp) {
                xmlChar *content;
                gboolean match;

                content = xmlNodeGetContent (tmp);
                
                match = !strcmp (udn, (char *) content);

                xmlFree (content);

                if (match)
                        return element;
        }

        tmp = xml_util_get_element (element,
                                    "deviceList",
                                    NULL);
        if (tmp) {
                /* Recurse into children */
                for (tmp = tmp->children; tmp; tmp = tmp->next) {
                        element =
                                _gupnp_device_proxy_find_element_for_udn
                                        (tmp, udn);

                        if (element)
                                return element;
                }
        }

        return NULL;
}

/**
 * gupnp_device_proxy_new
 * @context: A #GUPnPContext
 * @doc: A device description document
 * @udn: The UDN of the device to create a proxy for.
 * @location: The location of the device description file
 *
 * Return value: A #GUPnPDeviceProxy for the device with UDN @udn, as read
 * from the device description @doc.
 **/
GUPnPDeviceProxy *
gupnp_device_proxy_new (GUPnPContext *context,
                        xmlDoc       *doc,
                        const char   *udn,
                        const char   *location)
{
        GUPnPDeviceProxy *proxy;
        xmlNode *url_base_element;

        proxy = g_object_new (GUPNP_TYPE_DEVICE_PROXY,
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
        }

        if (!proxy->priv->element) {
                g_warning ("Device description does not contain device "
                           "with UDN \"%s\".", udn);

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
 * _gupnp_device_proxy_new_from_element
 * @context: A #GUPnPContext
 * @element: The #xmlNode ponting to the right device element
 * @udn: The UDN of the device to create a proxy for
 * @location: The location of the device description file
 * @url_base: The URL base for this device, or NULL if none
 *
 * Return value: A #GUPnPDeviceProxy for the device with element @element, as
 * read from the device description file specified by @location.
 **/
static GUPnPDeviceProxy *
_gupnp_device_proxy_new_from_element (GUPnPContext  *context,
                                      xmlNode       *element,
                                      const char    *udn,
                                      const char    *location,
                                      const xmlChar *url_base)
{
        GUPnPDeviceProxy *proxy;

        g_return_val_if_fail (element, NULL);

        proxy = g_object_new (GUPNP_TYPE_DEVICE_PROXY,
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
