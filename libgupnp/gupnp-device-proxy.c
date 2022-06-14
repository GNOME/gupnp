/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#define G_LOG_DOMAIN "gupnp-device-proxy"

#include <config.h>
#include <string.h>

#include "gupnp-device-proxy.h"
#include "gupnp-device-info-private.h"
#include "gupnp-resource-factory-private.h"
#include "xml-util.h"

/**
 * GUPnPDeviceProxy:
 *
 * Interaction with remote UPnP devices.
 *
 * #GUPnPDeviceProxy allows for retrieving proxies for a device's sub-devices
 * and services. It implements the [class@GUPnP.DeviceInfo] abstract class.
 */
G_DEFINE_TYPE (GUPnPDeviceProxy,
               gupnp_device_proxy,
               GUPNP_TYPE_DEVICE_INFO)


static GUPnPDeviceInfo *
gupnp_device_proxy_get_device (GUPnPDeviceInfo *info,
                               xmlNode         *element)
{
        GUPnPDeviceProxy     *device;
        GUPnPResourceFactory *factory;
        GUPnPContext         *context;
        GUPnPXMLDoc          *doc;
        const char           *location;
        const GUri *url_base;

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
        GUPnPResourceFactory *factory;
        GUPnPServiceProxy    *service;
        GUPnPContext         *context;
        GUPnPXMLDoc          *doc;
        const char           *location, *udn;
        const GUri *url_base;

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
gupnp_device_proxy_init (G_GNUC_UNUSED GUPnPDeviceProxy *proxy)
{
}

static void
gupnp_device_proxy_class_init (GUPnPDeviceProxyClass *klass)
{
        GUPnPDeviceInfoClass *info_class;

        info_class = GUPNP_DEVICE_INFO_CLASS (klass);

        info_class->create_device_instance  = gupnp_device_proxy_get_device;
        info_class->create_service_instance = gupnp_device_proxy_get_service;
}

