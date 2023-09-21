/*
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#define G_LOG_DOMAIN "gupnp-device"

#include <config.h>
#include <string.h>

#include "gupnp-device.h"
#include "gupnp-resource-factory-private.h"
#include "gupnp-root-device.h"
#include "gupnp-service.h"

struct _GUPnPDevicePrivate {
        GUPnPRootDevice *root_device;
};
typedef struct _GUPnPDevicePrivate GUPnPDevicePrivate;

/**
 * GUPnPDevice:
 *
 * Base class for UPnP device implementations.
 *
 * #GUPnPDevice allows for retrieving a device's sub-devices
 * and services. #GUPnPDevice implements the #GUPnPDeviceInfo
 * interface.
 */
G_DEFINE_TYPE_WITH_PRIVATE (GUPnPDevice,
                            gupnp_device,
                            GUPNP_TYPE_DEVICE_INFO)


enum {
        PROP_0,
        PROP_ROOT_DEVICE
};

static GUPnPDeviceInfo *
gupnp_device_get_device (GUPnPDeviceInfo *info,
                         xmlNode         *element)
{
        GUPnPDevice          *device;
        GUPnPDevicePrivate   *priv;
        GUPnPResourceFactory *factory;
        GUPnPContext         *context;
        GUPnPDevice          *root_device;
        const char           *location;
        const GUri *url_base;

        device = GUPNP_DEVICE (info);
        priv = gupnp_device_get_instance_private (device);

        root_device = GUPNP_IS_ROOT_DEVICE (device) ? device :
                      GUPNP_DEVICE (priv->root_device);
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
        GUPnPDevicePrivate   *priv;
        GUPnPService         *service;
        GUPnPResourceFactory *factory;
        GUPnPContext         *context;
        GUPnPDevice          *root_device;
        const char *location;
        const char *udn;
        const GUri *url_base;

        device = GUPNP_DEVICE (info);
        priv = gupnp_device_get_instance_private (device);

        root_device = GUPNP_IS_ROOT_DEVICE (device) ? device :
                      GUPNP_DEVICE (priv->root_device);
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
        GUPnPDevicePrivate   *priv;

        device = GUPNP_DEVICE (object);
        priv = gupnp_device_get_instance_private (device);

        switch (property_id) {
        case PROP_ROOT_DEVICE:
                priv->root_device = g_value_get_object (value);

                /* This can be NULL in which case *this* is the root device */
                if (priv->root_device) {
                        GUPnPRootDevice **dev = &(priv->root_device);

                        g_object_add_weak_pointer
                                (G_OBJECT (priv->root_device),
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
        GUPnPDevicePrivate   *priv;

        device = GUPNP_DEVICE (object);
        priv = gupnp_device_get_instance_private (device);

        switch (property_id) {
        case PROP_ROOT_DEVICE:
                g_value_set_object (value, priv->root_device);
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
        GUPnPDevicePrivate   *priv;

        device = GUPNP_DEVICE (object);
        priv = gupnp_device_get_instance_private (device);

        if (priv->root_device) {
                GUPnPRootDevice **dev = &(priv->root_device);

                g_object_remove_weak_pointer
                        (G_OBJECT (priv->root_device),
                         (gpointer *) dev);

                priv->root_device = NULL;
        }

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_device_parent_class);
        object_class->dispose (object);
}

static void
gupnp_device_init (GUPnPDevice *device)
{
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

        info_class->create_device_instance  = gupnp_device_get_device;
        info_class->create_service_instance = gupnp_device_get_service;

        /**
         * GUPnPDevice:root-device:
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
