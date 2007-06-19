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
 * #GUPnPDeviceProxy allows for retrieving proxies for a device's subdevices
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
                                      const char    *url_base);

G_DEFINE_TYPE (GUPnPDeviceProxy,
               gupnp_device_proxy,
               GUPNP_TYPE_DEVICE_INFO);

static GUPnPDeviceInfo *
gupnp_device_proxy_get_device (GUPnPDeviceInfo *info,
                               xmlNode         *element)
{
        GUPnPDeviceProxy *proxy, *device;
        GUPnPContext *context;
        const char *location, *udn, *url_base;

        proxy = GUPNP_DEVICE_PROXY (info);
                                
        context = gupnp_device_info_get_context (info);
        udn = gupnp_device_info_get_udn (info);
        location = gupnp_device_info_get_location (info);
        url_base = gupnp_device_info_get_url_base (info);

        device = _gupnp_device_proxy_new_from_element (context,
                                                       element,
                                                       udn,
                                                       location,
                                                       url_base);

        return GUPNP_DEVICE_INFO (device);
}

static GUPnPServiceInfo *
gupnp_device_proxy_get_service (GUPnPDeviceInfo *info,
                                xmlNode         *element)
{
        GUPnPDeviceProxy *proxy;
        GUPnPServiceProxy *service;
        GUPnPContext *context;
        const char *location, *udn, *url_base;

        proxy = GUPNP_DEVICE_PROXY (info);
                                
        context = gupnp_device_info_get_context (info);
        udn = gupnp_device_info_get_udn (info);
        location = gupnp_device_info_get_location (info);
        url_base = gupnp_device_info_get_url_base (info);

        service = _gupnp_service_proxy_new_from_element (context,
                                                         element,
                                                         udn,
                                                         location,
                                                         url_base);

        return GUPNP_SERVICE_INFO (service);
}

static void
gupnp_device_proxy_init (GUPnPDeviceProxy *proxy)
{
}

static void
gupnp_device_proxy_class_init (GUPnPDeviceProxyClass *klass)
{
        GUPnPDeviceInfoClass *info_class;

        info_class = GUPNP_DEVICE_INFO_CLASS (klass);

        info_class->get_device  = gupnp_device_proxy_get_device;
        info_class->get_service = gupnp_device_proxy_get_service;
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
 * _gupnp_device_proxy_new_from_doc
 * @context: A #GUPnPContext
 * @doc: A device description document
 * @udn: The UDN of the device to create a proxy for.
 * @location: The location of the device description file
 *
 * Return value: A #GUPnPDeviceProxy for the device with UDN @udn, as read
 * from the device description @doc.
 **/
GUPnPDeviceProxy *
_gupnp_device_proxy_new_from_doc (GUPnPContext *context,
                                  xmlDoc       *doc,
                                  const char   *udn,
                                  const char   *location)
{
        GUPnPDeviceProxy *proxy;
        xmlNode *element, *url_base_element;
        xmlChar *url_base = NULL;

        element = xml_util_get_element ((xmlNode *) doc,
                                        "root",
                                        "device",
                                        NULL);

        if (element) {
                element = _gupnp_device_proxy_find_element_for_udn (element,
                                                                    udn);
        }

        if (!element) {
                g_warning ("Device description does not contain device "
                           "with UDN \"%s\".", udn);

                return NULL;
        }

        /* Save the URL base, if any */
        url_base_element =
                xml_util_get_element ((xmlNode *) doc,
                                      "root",
                                      "URLBase",
                                      NULL);
        if (url_base_element != NULL)
                url_base = xmlNodeGetContent (url_base_element);

        proxy = g_object_new (GUPNP_TYPE_DEVICE_PROXY,
                              "context", context,
                              "location", location,
                              "udn", udn,
                              "url-base", (const char *) url_base,
                              "element", element,
                              NULL);

        if (url_base)
                xmlFree (url_base);

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
_gupnp_device_proxy_new_from_element (GUPnPContext *context,
                                      xmlNode      *element,
                                      const char   *udn,
                                      const char   *location,
                                      const char   *url_base)
{
        GUPnPDeviceProxy *proxy;

        g_return_val_if_fail (element, NULL);

        proxy = g_object_new (GUPNP_TYPE_DEVICE_PROXY,
                              "context", context,
                              "location", location,
                              "udn", udn,
                              "url-base", url_base,
                              "element", element,
                              NULL);

        return proxy;
}
