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
#include "gupnp-root-device.h"
#include "gupnp-resource-factory-private.h"
#include "xml-util.h"

G_DEFINE_TYPE (GUPnPDevice,
               gupnp_device,
               GUPNP_TYPE_DEVICE_INFO);

struct _GUPnPDevicePrivate {
        GUPnPRootDevice *root_device;
};

enum {
        PROP_0,
        PROP_ROOT_DEVICE
};

static GUPnPDeviceInfo *
gupnp_device_get_device (GUPnPDeviceInfo *info,
                         xmlNode         *element)
{
        GUPnPDevice          *device;
        GUPnPResourceFactory *factory;
        GUPnPContext         *context;
        GUPnPDevice          *root_device;
        const char           *location;
        const SoupURI        *url_base;

        device = GUPNP_DEVICE (info);

        root_device = GUPNP_IS_ROOT_DEVICE (device) ? device :
                      GUPNP_DEVICE (device->priv->root_device);
        if (root_device == NULL) {
                g_warning ("Root device not found.");

                return NULL;
        }

        factory = gupnp_device_info_get_resource_factory (info);
        context = gupnp_device_info_get_context (info);
        location = gupnp_device_info_get_location (info);
        url_base = gupnp_device_info_get_url_base (info);

        device = gupnp_resource_factory_create_device (factory,
                                                       context,
                                                       root_device,
                                                       element,
                                                       NULL,
                                                       location,
                                                       url_base);

        return GUPNP_DEVICE_INFO (device);
}

static GUPnPServiceInfo *
gupnp_device_get_service (GUPnPDeviceInfo *info,
                          xmlNode         *element)
{
        GUPnPDevice          *device;
        GUPnPService         *service;
        GUPnPResourceFactory *factory;
        GUPnPContext         *context;
        GUPnPDevice          *root_device;
        const char           *location, *udn;
        const SoupURI        *url_base;

        device = GUPNP_DEVICE (info);

        root_device = GUPNP_IS_ROOT_DEVICE (device) ? device :
                      GUPNP_DEVICE (device->priv->root_device);
        if (root_device == NULL) {
                g_warning ("Root device not found.");

                return NULL;
        }

        factory = gupnp_device_info_get_resource_factory (info);
        context = gupnp_device_info_get_context (info);
        udn = gupnp_device_info_get_udn (info);
        location = gupnp_device_info_get_location (info);
        url_base = gupnp_device_info_get_url_base (info);

        service = gupnp_resource_factory_create_service (factory,
                                                         context,
                                                         root_device,
                                                         element,
                                                         udn,
                                                         location,
                                                         url_base);

        return GUPNP_SERVICE_INFO (service);
}

static void
gupnp_device_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
        GUPnPDevice *device;

        device = GUPNP_DEVICE (object);

        switch (property_id) {
        case PROP_ROOT_DEVICE:
                device->priv->root_device = g_value_get_object (value);

                /* This can be NULL in which case *this* is the root device */
                if (device->priv->root_device) {
                        GUPnPRootDevice **dev = &(device->priv->root_device);

                        g_object_add_weak_pointer
                                (G_OBJECT (device->priv->root_device),
                                 (gpointer *) dev);
                }

                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_device_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
        GUPnPDevice *device;

        device = GUPNP_DEVICE (object);

        switch (property_id) {
        case PROP_ROOT_DEVICE:
                g_value_set_object (value, device->priv->root_device);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_device_dispose (GObject *object)
{
        GUPnPDevice *device;
        GObjectClass *object_class;

        device = GUPNP_DEVICE (object);

        if (device->priv->root_device) {
                GUPnPRootDevice **dev = &(device->priv->root_device);

                g_object_remove_weak_pointer
                        (G_OBJECT (device->priv->root_device),
                         (gpointer *) dev);

                device->priv->root_device = NULL;
        }

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_device_parent_class);
        object_class->dispose (object);
}

static void
gupnp_device_init (GUPnPDevice *device)
{
        device->priv = G_TYPE_INSTANCE_GET_PRIVATE (device,
                                                    GUPNP_TYPE_DEVICE,
                                                    GUPnPDevicePrivate);
}

static void
gupnp_device_class_init (GUPnPDeviceClass *klass)
{
        GObjectClass *object_class;
        GUPnPDeviceInfoClass *info_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_device_set_property;
        object_class->get_property = gupnp_device_get_property;
        object_class->dispose      = gupnp_device_dispose;

        info_class = GUPNP_DEVICE_INFO_CLASS (klass);

        info_class->get_device  = gupnp_device_get_device;
        info_class->get_service = gupnp_device_get_service;

        g_type_class_add_private (klass, sizeof (GUPnPDevicePrivate));

        /**
         * GUPnPDevice:root-device
         *
         * The containing #GUPnPRootDevice, or NULL if this is the root
         * device.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_ROOT_DEVICE,
                 g_param_spec_object ("root-device",
                                      "Root device",
                                      "The GUPnPRootDevice",
                                      GUPNP_TYPE_ROOT_DEVICE,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

}
