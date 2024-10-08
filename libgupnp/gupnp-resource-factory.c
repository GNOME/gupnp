/*
 * Copyright (C) 2007 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

/**
 * GUPnPResourceFactory:
 *
 * Associating custom Services, Devices, ServiceProxies and DeviceProxies with UPnP types.
 *
 * #GUPnPResourceFactory objects are used by [class@GUPnP.ControlPoint],
 * [class@GUPnP.DeviceProxy] and [class@GUPnP.Device] to create resource proxy and resource
 * objects.
 *
 * Register UPnP type - [alias@GObject.Type] pairs to have resource or resource proxy
 * objects created with the specified #GType whenever an object for a resource
 * of the specified UPnP type is requested. The #GType needs
 * to be derived from the relevant resource or resource proxy type (e.g.
 * a device proxy type needs to be derived from [class@GUPnP.DeviceProxy]).
 */

#define G_LOG_DOMAIN "gupnp-resource-factory"

#include <config.h>
#include <string.h>

#include "gupnp-device-info-private.h"
#include "gupnp-resource-factory-private.h"
#include "gupnp-root-device.h"

struct _GUPnPResourceFactoryPrivate {
        GHashTable *resource_type_hash;
        GHashTable *proxy_type_hash;
};
typedef struct _GUPnPResourceFactoryPrivate GUPnPResourceFactoryPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GUPnPResourceFactory,
                            gupnp_resource_factory,
                            G_TYPE_OBJECT)

static void
gupnp_resource_factory_init (GUPnPResourceFactory *factory)
{
        GUPnPResourceFactoryPrivate *priv;

        priv = gupnp_resource_factory_get_instance_private (factory);
        priv->resource_type_hash = g_hash_table_new_full (g_str_hash,
                                                          g_str_equal,
                                                          g_free,
                                                          NULL);
        priv->proxy_type_hash = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       NULL);
}

static void
gupnp_resource_factory_finalize (GObject *object)
{
    GUPnPResourceFactory *self;
    GObjectClass *object_class;
    GUPnPResourceFactoryPrivate *priv;

    self = GUPNP_RESOURCE_FACTORY (object);
    priv = gupnp_resource_factory_get_instance_private (self);

    if (priv->resource_type_hash) {
        g_hash_table_destroy (priv->resource_type_hash);
        priv->resource_type_hash = NULL;
    }

    if (priv->proxy_type_hash) {
        g_hash_table_destroy (priv->proxy_type_hash);
        priv->proxy_type_hash = NULL;
    }

    object_class = G_OBJECT_CLASS (gupnp_resource_factory_parent_class);
    object_class->finalize (object);
}

static void
gupnp_resource_factory_class_init (GUPnPResourceFactoryClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gupnp_resource_factory_finalize;
}

/**
 * gupnp_resource_factory_new:
 *
 * Create a new #GUPnPResourceFactory object.
 *
 * Return value: A #GUPnPResourceFactory object.
 **/
GUPnPResourceFactory *
gupnp_resource_factory_new (void)
{
        return g_object_new (GUPNP_TYPE_RESOURCE_FACTORY, NULL);
}

/**
 * gupnp_resource_factory_get_default:
 *
 * Get the default singleton #GUPnPResourceFactory object.
 *
 * Returns: (transfer none): A @GUPnPResourceFactory object.
 **/
GUPnPResourceFactory *
gupnp_resource_factory_get_default (void)
{
        static GUPnPResourceFactory *default_factory = NULL;

        if (g_once_init_enter (&default_factory)) {
                GUPnPResourceFactory *factory =
                        g_object_new (GUPNP_TYPE_RESOURCE_FACTORY, NULL);

                g_once_init_leave (&default_factory, factory);
        }

        return default_factory;
}

static GType
lookup_type_with_fallback (GHashTable *resource_types,
                           const char *requested_type,
                           const char *child_node,
                           xmlNode    *element,
                           GType       fallback)
{
        GType type = fallback;
        char *upnp_type = NULL;

        if (requested_type == NULL) {
                g_debug ("Looking up type from XML");
                upnp_type = xml_util_get_child_element_content_glib (element,
                                                                     child_node);
        } else {
                g_debug ("Using passed type %s", requested_type);
                upnp_type = g_strdup (requested_type);
        }


        if (upnp_type != NULL) {
                g_debug ("Found type from XML: %s", upnp_type);
                gpointer value;
                char *needle = NULL;

                value = g_hash_table_lookup (resource_types, upnp_type);

                if (value == NULL) {
                        g_debug ("Trying to use version-less type...");
                        needle = g_strrstr (upnp_type, ":");
                        if (needle != NULL) {
                                *needle = '\0';
                                g_debug ("Version-less type is %s", upnp_type);

                                value = g_hash_table_lookup (resource_types, upnp_type);
                        }
                }

                if (value != NULL) {
                        type = GPOINTER_TO_SIZE (value);
                }

                g_debug ("Will return type %s for UPnP type %s", g_type_name (type), upnp_type);
                g_free (upnp_type);
        } else {
                g_debug ("Will return fall-back type %s", upnp_type);
        }


        return type;
}


/**
 * gupnp_resource_factory_create_device_proxy:
 * @factory: A #GUPnPResourceFactory
 * @context: A #GUPnPContext
 * @doc: A #GUPnPXMLDoc
 * @element: The #xmlNode ponting to the right device element
 * @udn: The UDN of the device to create a proxy for
 * @location: The location of the device description file
 * @url_base: The URL base for this device, or %NULL if none
 *
 * Create a #GUPnPDeviceProxy for the device with element @element, as
 * read from the device description file specified by @location.
 *
 * Return value:(nullable)(transfer full): A new #GUPnPDeviceProxy.
 **/
GUPnPDeviceProxy *
gupnp_resource_factory_create_device_proxy (GUPnPResourceFactory *factory,
                                            GUPnPContext *context,
                                            GUPnPXMLDoc *doc,
                                            xmlNode *element,
                                            const char *udn,
                                            const char *location,
                                            const GUri *url_base)
{
        GUPnPDeviceProxy *proxy;
        GType proxy_type;
        GUPnPResourceFactoryPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_RESOURCE_FACTORY (factory), NULL);
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (GUPNP_IS_XML_DOC (doc), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (location != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        priv = gupnp_resource_factory_get_instance_private (factory);

        proxy_type = lookup_type_with_fallback (priv->proxy_type_hash,
                                                NULL,
                                                "deviceType",
                                                element,
                                                GUPNP_TYPE_DEVICE_PROXY);

        proxy = g_object_new (proxy_type,
                              "resource-factory", factory,
                              "context", context,
                              "location", location,
                              "udn", udn,
                              "url-base", url_base,
                              "document", doc,
                              "element", element,
                              NULL);

        return proxy;
}

/**
 * gupnp_resource_factory_create_service_proxy:
 * @factory: A #GUPnPResourceFactory
 * @context: A #GUPnPContext
 * @doc: A #GUPnPXMLDoc
 * @element: The #xmlNode pointing to the right service element
 * @location: The location of the service description file
 * @udn: The UDN of the device the service is contained in
 * @service_type: (nullable): The service type, or %NULL to use service
 * type from @element
 * @url_base: The URL base for this service, or %NULL if none
 *
 * Create a #GUPnPServiceProxy for the service with element @element, as
 * read from the service description file specified by @location.
 *
 * Return value:(nullable)(transfer full): A new #GUPnPServiceProxy.
 **/
GUPnPServiceProxy *
gupnp_resource_factory_create_service_proxy (GUPnPResourceFactory *factory,
                                             GUPnPContext *context,
                                             GUPnPXMLDoc *doc,
                                             xmlNode *element,
                                             const char *udn,
                                             const char *service_type,
                                             const char *location,
                                             const GUri *url_base)
{
        GUPnPServiceProxy *proxy;
        GType proxy_type;
        GUPnPResourceFactoryPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_RESOURCE_FACTORY (factory), NULL);
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (GUPNP_IS_XML_DOC (doc), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (location != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        priv = gupnp_resource_factory_get_instance_private (factory);

        proxy_type = lookup_type_with_fallback (priv->proxy_type_hash,
                                                service_type,
                                                "serviceType",
                                                element,
                                                GUPNP_TYPE_SERVICE_PROXY);

        proxy = g_object_new (proxy_type,
                              "context", context,
                              "location", location,
                              "udn", udn,
                              "service-type", service_type,
                              "url-base", url_base,
                              "document", doc,
                              "element", element,
                              NULL);

        return proxy;
}

/**
 * gupnp_resource_factory_create_device:
 * @factory: A #GUPnPResourceFactory
 * @context: A #GUPnPContext
 * @root_device: The #GUPnPRootDevice
 * @element: The #xmlNode ponting to the right device element
 * @udn: The UDN of the device to create a device for
 * @location: The location of the device description file
 * @url_base: The URL base for this device
 *
 * Create a #GUPnPDevice for the device with element @element, as
 * read from the device description file specified by @location.
 *
 * Return value: (nullable)(transfer full): A new #GUPnPDevice.
 **/
GUPnPDevice *
gupnp_resource_factory_create_device (GUPnPResourceFactory *factory,
                                      GUPnPContext *context,
                                      GUPnPDevice *root_device,
                                      xmlNode *element,
                                      const char *udn,
                                      const char *location,
                                      const GUri *url_base)
{
        GUPnPDevice *device;
        GType device_type;
        GUPnPResourceFactoryPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_RESOURCE_FACTORY (factory), NULL);
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (GUPNP_IS_ROOT_DEVICE (root_device), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        priv = gupnp_resource_factory_get_instance_private (factory);

        device_type = lookup_type_with_fallback (priv->resource_type_hash,
                                                 NULL,
                                                 "deviceType",
                                                 element,
                                                 GUPNP_TYPE_DEVICE);

        GUPnPXMLDoc *doc = _gupnp_device_info_get_document (
                GUPNP_DEVICE_INFO (root_device));
        device = g_object_new (device_type,
                               "resource-factory",
                               factory,
                               "context",
                               context,
                               "root-device",
                               root_device,
                               "location",
                               location,
                               "udn",
                               udn,
                               "url-base",
                               url_base,
                               "document",
                               doc,
                               "element",
                               element,
                               NULL);

        return device;
}

/**
 * gupnp_resource_factory_create_service:
 * @factory: A #GUPnPResourceFactory
 * @context: A #GUPnPContext
 * @root_device: The #GUPnPRootDevice
 * @element: The #xmlNode ponting to the right service element
 * @udn: The UDN of the device the service is contained in
 * @location: The location of the service description file
 * @url_base: The URL base for this service
 *
 * Create a #GUPnPService for the service with element @element, as
 * read from the service description file specified by @location.
 *
 * Return value: (nullable)(transfer full): A new #GUPnPService.
 **/
GUPnPService *
gupnp_resource_factory_create_service (GUPnPResourceFactory *factory,
                                       GUPnPContext *context,
                                       GUPnPDevice *root_device,
                                       xmlNode *element,
                                       const char *udn,
                                       const char *location,
                                       const GUri *url_base)
{
        GUPnPService *service;
        GType service_type;
        GUPnPResourceFactoryPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_RESOURCE_FACTORY (factory), NULL);
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (GUPNP_IS_ROOT_DEVICE (root_device), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (location != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        priv = gupnp_resource_factory_get_instance_private (factory);

        service_type = lookup_type_with_fallback (priv->resource_type_hash,
                                                  NULL,
                                                  "serviceType",
                                                  element,
                                                  GUPNP_TYPE_SERVICE);

        GUPnPXMLDoc *doc = _gupnp_device_info_get_document (
                GUPNP_DEVICE_INFO (root_device));
        service = g_object_new (service_type,
                                "context",
                                context,
                                "root-device",
                                root_device,
                                "location",
                                location,
                                "udn",
                                udn,
                                "url-base",
                                url_base,
                                "document",
                                doc,
                                "element",
                                element,
                                NULL);

        return service;
}

/**
 * gupnp_resource_factory_register_resource_type:
 * @factory: A #GUPnPResourceFactory.
 * @upnp_type: The UPnP type name of the resource.
 * @type: The requested GType assignment for the resource.
 *
 * Registers the GType @type for the resource of UPnP type @upnp_type. After
 * this call, the factory @factory will create object of GType @type each time
 * it is asked to create a resource object for UPnP type @upnp_type.
 *
 * You can either register a type for a concrete version of a device or service
 * such as urn:schemas-upnp-org:service:AVTransport:2 or version-independently,
 * urn:schemas-upnp-org:service:AVTransport. If you register for an explicit
 * version of a service, it will be an exact match.
 *
 * Note: GType @type must be a derived type of #GUPNP_TYPE_DEVICE if resource is
 * a device or #GUPNP_TYPE_SERVICE if its a service.
 **/
void
gupnp_resource_factory_register_resource_type (GUPnPResourceFactory *factory,
                                               const char           *upnp_type,
                                               GType                 type)
{
        GUPnPResourceFactoryPrivate *priv;

        priv = gupnp_resource_factory_get_instance_private (factory);

        g_hash_table_insert (priv->resource_type_hash,
                             g_strdup (upnp_type),
                             GSIZE_TO_POINTER (type));
}

/**
 * gupnp_resource_factory_unregister_resource_type:
 * @factory: A #GUPnPResourceFactory.
 * @upnp_type: The UPnP type name of the resource.
 *
 * Unregisters the GType assignment for the resource of UPnP type @upnp_type.
 *
 * Return value: %TRUE if GType assignment was removed successfully, %FALSE
 * otherwise.
 **/
gboolean
gupnp_resource_factory_unregister_resource_type
                                (GUPnPResourceFactory *factory,
                                 const char           *upnp_type)
{
        GUPnPResourceFactoryPrivate *priv;

        priv = gupnp_resource_factory_get_instance_private (factory);

        return g_hash_table_remove (priv->resource_type_hash,
                                    upnp_type);
}

/**
 * gupnp_resource_factory_register_resource_proxy_type:
 * @factory: A #GUPnPResourceFactory.
 * @upnp_type: The UPnP type name of the resource.
 * @type: The requested GType assignment for the resource proxy.
 *
 * Registers the GType @type for the proxy of resource of UPnP type @upnp_type.
 * After this call, the factory @factory will create object of GType @type each
 * time it is asked to create a resource proxy object for UPnP type @upnp_type.
 *
 * Note: GType @type must be a derived type of #GUPNP_TYPE_DEVICE_PROXY if
 * resource is a device or #GUPNP_TYPE_SERVICE_PROXY if its a service.
 **/
void
gupnp_resource_factory_register_resource_proxy_type
                                (GUPnPResourceFactory *factory,
                                 const char           *upnp_type,
                                 GType                 type)
{
        GUPnPResourceFactoryPrivate *priv;

        priv = gupnp_resource_factory_get_instance_private (factory);

        g_hash_table_insert (priv->proxy_type_hash,
                             g_strdup (upnp_type),
                             GSIZE_TO_POINTER (type));
}

/**
 * gupnp_resource_factory_unregister_resource_proxy_type:
 * @factory: A #GUPnPResourceFactory.
 * @upnp_type: The UPnP type name of the resource.
 *
 * Unregisters the GType assignment for the proxy of resource of UPnP type
 * @upnp_type.
 *
 * Return value: %TRUE if GType assignment was removed successfully, %FALSE
 * otherwise.
 **/
gboolean
gupnp_resource_factory_unregister_resource_proxy_type
                                (GUPnPResourceFactory *factory,
                                 const char           *upnp_type)
{
        GUPnPResourceFactoryPrivate *priv;

        priv = gupnp_resource_factory_get_instance_private (factory);

        return g_hash_table_remove (priv->proxy_type_hash, upnp_type);
}
