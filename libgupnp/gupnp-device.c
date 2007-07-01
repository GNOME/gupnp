/* 
 * Copyright (C) 2007 OpenedHand Ltd.
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
 * SECTION:gupnp-device
 * @short_description: Class for device implementations.
 *
 * #GUPnPDevice allows for retrieving a device's subdevices
 * and services. #GUPnPDevice implements the #GUPnPDeviceInfo
 * interface.
 */

#include <string.h>

#include "gupnp-device.h"
#include "gupnp-service.h"
#include "gupnp-service-private.h"
#include "xml-util.h"

static GUPnPDevice *
_gupnp_device_new_from_element (GUPnPContext *context,
                                xmlNode      *element,
                                const char   *udn,
                                const char   *location,
                                SoupUri      *url_base);

G_DEFINE_TYPE (GUPnPDevice,
               gupnp_device,
               GUPNP_TYPE_DEVICE_INFO);

static GUPnPDeviceInfo *
gupnp_device_get_device (GUPnPDeviceInfo *info,
                         xmlNode         *element)
{
        GUPnPDevice *proxy, *device;
        GUPnPContext *context;
        const char *location, *udn;
        SoupUri *url_base;

        proxy = GUPNP_DEVICE (info);
                                
        context = gupnp_device_info_get_context (info);
        udn = gupnp_device_info_get_udn (info);
        location = gupnp_device_info_get_location (info);
        url_base = gupnp_device_info_get_url_base (info);

        device = _gupnp_device_new_from_element (context,
                                                 element,
                                                 udn,
                                                 location,
                                                 url_base);

        return GUPNP_DEVICE_INFO (device);
}

static GUPnPServiceInfo *
gupnp_device_get_service (GUPnPDeviceInfo *info,
                          xmlNode         *element)
{
        GUPnPDevice *proxy;
        GUPnPService *service;
        GUPnPContext *context;
        const char *location, *udn;
        SoupUri *url_base;

        proxy = GUPNP_DEVICE (info);
                                
        context = gupnp_device_info_get_context (info);
        udn = gupnp_device_info_get_udn (info);
        location = gupnp_device_info_get_location (info);
        url_base = gupnp_device_info_get_url_base (info);

        service = _gupnp_service_new_from_element (context,
                                                   element,
                                                   udn,
                                                   location,
                                                   url_base);

        return GUPNP_SERVICE_INFO (service);
}

static void
gupnp_device_init (GUPnPDevice *proxy)
{
}

static void
gupnp_device_class_init (GUPnPDeviceClass *klass)
{
        GUPnPDeviceInfoClass *info_class;

        info_class = GUPNP_DEVICE_INFO_CLASS (klass);

        info_class->get_device  = gupnp_device_get_device;
        info_class->get_service = gupnp_device_get_service;
}

/**
 * @element: An #xmlNode pointing to a "device" element
 * @udn: The UDN of the device element to find
 **/
xmlNode *
_gupnp_device_find_element_for_udn (xmlNode    *element,
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
                                _gupnp_device_find_element_for_udn
                                        (tmp, udn);

                        if (element)
                                return element;
                }
        }

        return NULL;
}

/**
 * _gupnp_device_new_from_element
 * @context: A #GUPnPContext
 * @element: The #xmlNode ponting to the right device element
 * @udn: The UDN of the device to create a proxy for
 * @location: The location of the device description file
 * @url_base: The URL base for this device
 *
 * Return value: A #GUPnPDevice for the device with element @element, as
 * read from the device description file specified by @location.
 **/
static GUPnPDevice *
_gupnp_device_new_from_element (GUPnPContext *context,
                                xmlNode      *element,
                                const char   *udn,
                                const char   *location,
                                SoupUri      *url_base)
{
        GUPnPDevice *proxy;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        proxy = g_object_new (GUPNP_TYPE_DEVICE,
                              "context", context,
                              "location", location,
                              "udn", udn,
                              "url-base", url_base,
                              "element", element,
                              NULL);

        return proxy;
}
