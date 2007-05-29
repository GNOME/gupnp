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
 * SECTION:gupnp-device-proxy
 * @short_description: Proxy class for remote devices.
 *
 * #GUPnPServiceProxy allows for retrieving proxies for a device's subdevices
 * and services. Device proxies also implement the #GUPnPDeviceInfo
 * interface.
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

static GUPnPDeviceInfo *
gupnp_device_proxy_get_device (GUPnPDeviceInfo *info,
                               xmlNode         *element)
{
        GUPnPDeviceProxy *proxy, *device;
        GUPnPContext *context;
        const char *location, *udn;

        proxy = GUPNP_DEVICE_PROXY (info);
                                
        context = gupnp_device_info_get_context (info);
        udn = gupnp_device_info_get_udn (info);
        location = gupnp_device_info_get_location (info);

        device = _gupnp_device_proxy_new_from_element (context,
                                                       element,
                                                       udn,
                                                       location,
                                                       proxy->priv->url_base);

        return GUPNP_DEVICE_INFO (device);
}

static GUPnPServiceInfo *
gupnp_device_proxy_get_service (GUPnPDeviceInfo *info,
                                xmlNode         *element)
{
        GUPnPDeviceProxy *proxy;
        GUPnPServiceProxy *service;
        GUPnPContext *context;
        const char *location, *udn;

        proxy = GUPNP_DEVICE_PROXY (info);
                                
        context = gupnp_device_info_get_context (info);
        udn = gupnp_device_info_get_udn (info);
        location = gupnp_device_info_get_location (info);

        service = _gupnp_service_proxy_new_from_element (context,
                                                         element,
                                                         udn,
                                                         location,
                                                         proxy->priv->url_base);

        return GUPNP_SERVICE_INFO (service);
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
        info_class->get_device   = gupnp_device_proxy_get_device;
        info_class->get_service  = gupnp_device_proxy_get_service;
       
        g_type_class_add_private (klass, sizeof (GUPnPDeviceProxyPrivate));
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
