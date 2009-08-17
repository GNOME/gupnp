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
 * and services. #GUPnPDeviceProxy implements the #GUPnPDeviceInfo interface.
 */

#include <string.h>

#include "gupnp-device-proxy.h"
#include "gupnp-device-info-private.h"
#include "gupnp-resource-factory-private.h"
#include "xml-util.h"

G_DEFINE_TYPE (GUPnPDeviceProxy,
               gupnp_device_proxy,
               GUPNP_TYPE_DEVICE_INFO);

static GUPnPDeviceInfo *
gupnp_device_proxy_get_device (GUPnPDeviceInfo *info,
                               xmlNode         *element)
{
        GUPnPDeviceProxy     *proxy, *device;
        GUPnPResourceFactory *factory;
        GUPnPContext         *context;
        GUPnPXMLDoc          *doc;
        const char           *location;
        const SoupURI        *url_base;

        proxy = GUPNP_DEVICE_PROXY (info);

        factory = gupnp_device_info_get_resource_factory (info);
        context = gupnp_device_info_get_context (info);
        doc = _gupnp_device_info_get_document (info);
        location = gupnp_device_info_get_location (info);
        url_base = gupnp_device_info_get_url_base (info);

        device = gupnp_resource_factory_create_device_proxy (factory,
                                                             context,
                                                             doc,
                                                             element,
                                                             NULL,
                                                             location,
                                                             url_base);

        return GUPNP_DEVICE_INFO (device);
}

static GUPnPServiceInfo *
gupnp_device_proxy_get_service (GUPnPDeviceInfo *info,
                                xmlNode         *element)
{
        GUPnPDeviceProxy     *proxy;
        GUPnPResourceFactory *factory;
        GUPnPServiceProxy    *service;
        GUPnPContext         *context;
        GUPnPXMLDoc          *doc;
        const char           *location, *udn;
        const SoupURI        *url_base;

        proxy = GUPNP_DEVICE_PROXY (info);

        factory = gupnp_device_info_get_resource_factory (info);
        context = gupnp_device_info_get_context (info);
        doc = _gupnp_device_info_get_document (info);
        udn = gupnp_device_info_get_udn (info);
        location = gupnp_device_info_get_location (info);
        url_base = gupnp_device_info_get_url_base (info);

        service = gupnp_resource_factory_create_service_proxy (factory,
                                                               context,
                                                               doc,
                                                               element,
                                                               udn,
                                                               NULL,
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

