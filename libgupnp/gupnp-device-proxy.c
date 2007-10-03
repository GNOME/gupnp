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
#include "gupnp-resource-factory.h"
#include "xml-util.h"

G_DEFINE_TYPE (GUPnPDeviceProxy,
               gupnp_device_proxy,
               GUPNP_TYPE_DEVICE_INFO);

struct _GUPnPDeviceProxyPrivate {
        GUPnPResourceFactory *factory;
};

enum {
        PROP_0,
        PROP_RESOURCE_FACTORY,
};

static GUPnPDeviceInfo *
gupnp_device_proxy_get_device (GUPnPDeviceInfo *info,
                               xmlNode         *element)
{
        GUPnPDeviceProxy *proxy, *device;
        GUPnPContext *context;
        XmlDocWrapper *doc;
        const char *location;
        const SoupUri *url_base;

        proxy = GUPNP_DEVICE_PROXY (info);

        context = gupnp_device_info_get_context (info);
        doc = _gupnp_device_info_get_document (info);
        location = gupnp_device_info_get_location (info);
        url_base = gupnp_device_info_get_url_base (info);

        device = gupnp_resource_factory_create_device_proxy
                                        (proxy->priv->factory,
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
        GUPnPDeviceProxy *proxy;
        GUPnPServiceProxy *service;
        GUPnPContext *context;
        XmlDocWrapper *doc;
        const char *location, *udn;
        const SoupUri *url_base;

        proxy = GUPNP_DEVICE_PROXY (info);

        context = gupnp_device_info_get_context (info);
        doc = _gupnp_device_info_get_document (info);
        udn = gupnp_device_info_get_udn (info);
        location = gupnp_device_info_get_location (info);
        url_base = gupnp_device_info_get_url_base (info);

        service = gupnp_resource_factory_create_service_proxy
                                        (proxy->priv->factory,
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
        proxy->priv = G_TYPE_INSTANCE_GET_PRIVATE (proxy,
                                                   GUPNP_TYPE_DEVICE_PROXY,
                                                   GUPnPDeviceProxyPrivate);
}

static void
gupnp_device_proxy_dispose (GObject *object)
{
        GUPnPDeviceProxy *device_proxy;
        GObjectClass *object_class;

        device_proxy = GUPNP_DEVICE_PROXY (object);

        if (device_proxy->priv->factory) {
                g_object_unref (device_proxy->priv->factory);
                device_proxy->priv->factory = NULL;
        }

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_device_proxy_parent_class);
        object_class->dispose (object);
}

static void
gupnp_device_proxy_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
        GUPnPDeviceProxy *proxy;

        proxy = GUPNP_DEVICE_PROXY (object);

        switch (property_id) {
        case PROP_RESOURCE_FACTORY:
                if (proxy->priv->factory != NULL)
                       g_object_unref (proxy->priv->factory);
                proxy->priv->factory = g_value_dup_object (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_device_proxy_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
        GUPnPDeviceProxy *proxy;

        proxy = GUPNP_DEVICE_PROXY (object);

        switch (property_id) {
        case PROP_RESOURCE_FACTORY:
                g_value_set_object (value, proxy->priv->factory);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_device_proxy_class_init (GUPnPDeviceProxyClass *klass)
{
        GUPnPDeviceInfoClass *info_class;
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        info_class = GUPNP_DEVICE_INFO_CLASS (klass);

        object_class->set_property = gupnp_device_proxy_set_property;
        object_class->get_property = gupnp_device_proxy_get_property;
        object_class->dispose      = gupnp_device_proxy_dispose;
        info_class->get_device     = gupnp_device_proxy_get_device;
        info_class->get_service    = gupnp_device_proxy_get_service;

        g_type_class_add_private (klass, sizeof (GUPnPDeviceProxyPrivate));

        /**
         * GUPnPDeviceProxy:resource-factory
         *
         * The resource factory to use. Set to NULL for default factory.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_RESOURCE_FACTORY,
                 g_param_spec_object ("resource-factory",
                                      "Resource Factory",
                                      "The resource factory to use",
                                      GUPNP_TYPE_RESOURCE_FACTORY,
                                      G_PARAM_CONSTRUCT |
                                      G_PARAM_READWRITE |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));
}

