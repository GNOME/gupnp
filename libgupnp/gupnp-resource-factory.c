/*
 * Copyright (C) 2007 Zeeshan Ali <zeenix@gstreamer.net>
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali <zeenix@gstreamer.net>
 *         Jorn Baayen <jorn@openedhand.com>
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
 * SECTION:gupnp-resource-factory
 * @short_description: Class for resource and resource proxy object creation.
 *
 * #GUPnPResourceFactory objects are used by #GUPnPControlPoint,
 * #GUPnPDeviceProxy and #GUPnPDevice to create resource proxy and resource
 * objects. Register UPnP type - #GType pairs to have resource or resource proxy
 * objects created with the specified #GType whenever an object for a resource
 * of the specified UPnP type is requested. The #GType<!-- -->s need
 * to be derived from the relevant resource or resource proxy type (e.g.
 * a device proxy type needs to be derived from #GUPnPDeviceProxy).
 */

#include <string.h>

#include "gupnp-resource-factory-private.h"
#include "gupnp-root-device.h"

G_DEFINE_TYPE (GUPnPResourceFactory,
               gupnp_resource_factory,
               G_TYPE_OBJECT);

struct _GUPnPResourceFactoryPrivate {
        GHashTable *resource_type_hash;
        GHashTable *proxy_type_hash;
};

static void
gupnp_resource_factory_init (GUPnPResourceFactory *factory)
{
        factory->priv =
                G_TYPE_INSTANCE_GET_PRIVATE (factory,
                                             GUPNP_TYPE_RESOURCE_FACTORY,
                                             GUPnPResourceFactoryPrivate);

        factory->priv->resource_type_hash =
                        g_hash_table_new_full (g_str_hash,
                                               g_int_equal,
                                               g_free,
                                               NULL);
        factory->priv->proxy_type_hash =
                        g_hash_table_new_full (g_str_hash,
                                               g_int_equal,
                                               g_free,
                                               NULL);
}

static void
gupnp_resource_factory_class_init (GUPnPResourceFactoryClass *klass)
{
        g_type_class_add_private (klass, sizeof (GUPnPResourceFactoryPrivate));
}

/**
 * gupnp_resource_factory_new
 *
 * Return value: A new #GUPnPResourceFactory object.
 **/
GUPnPResourceFactory *
gupnp_resource_factory_new (void)
{
        return g_object_new (GUPNP_TYPE_RESOURCE_FACTORY, NULL);
}

/**
 * gupnp_resource_factory_get_default
 *
 * Return value: The default singleton #GUPnPResourceFactory object.
 **/
GUPnPResourceFactory *
gupnp_resource_factory_get_default (void)
{
        static GUPnPResourceFactory *default_factory = NULL;

        if (G_UNLIKELY (default_factory == NULL)) {
                default_factory = g_object_new (GUPNP_TYPE_RESOURCE_FACTORY,
                                                NULL);
        }

        return default_factory;
}

/**
 * gupnp_resource_factory_create_device_proxy
 * @factory: A #GUPnPResourceFactory
 * @context: A #GUPnPContext
 * @doc: An #XmlDocWrapper
 * @element: The #xmlNode ponting to the right device element
 * @udn: The UDN of the device to create a proxy for
 * @location: The location of the device description file
 * @url_base: The URL base for this device, or NULL if none
 *
 * Return value: A #GUPnPDeviceProxy for the device with element @element, as
 * read from the device description file specified by @location.
 **/
GUPnPDeviceProxy *
gupnp_resource_factory_create_device_proxy
                                (GUPnPResourceFactory *factory,
                                 GUPnPContext         *context,
                                 XmlDocWrapper        *doc,
                                 xmlNode              *element,
                                 const char           *udn,
                                 const char           *location,
                                 const SoupURI        *url_base)
{
        GUPnPDeviceProxy *proxy;
        const char       *upnp_type;
        GType             proxy_type = GUPNP_TYPE_DEVICE_PROXY;

        g_return_val_if_fail (GUPNP_IS_RESOURCE_FACTORY (factory), NULL);
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (IS_XML_DOC_WRAPPER (doc), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (location != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        upnp_type = xml_util_get_child_element_content_glib (element,
                                                             "deviceType");
        if (upnp_type) {
                gpointer value;

                value = g_hash_table_lookup (factory->priv->proxy_type_hash,
                                             upnp_type);
                if (value)
                        proxy_type = GPOINTER_TO_INT (value);
        }

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
 * gupnp_resource_factory_create_service_proxy
 * @factory: A #GUPnPResourceFactory
 * @context: A #GUPnPContext
 * @wrapper: An #XmlDocWrapper
 * @element: The #xmlNode ponting to the right service element
 * @location: The location of the service description file
 * @udn: The UDN of the device the service is contained in
 * @service_type: The service type
 * @url_base: The URL base for this service, or NULL if none
 *
 * Return value: A #GUPnPServiceProxy for the service with element @element, as
 * read from the service description file specified by @location.
 **/
GUPnPServiceProxy *
gupnp_resource_factory_create_service_proxy
                                (GUPnPResourceFactory *factory,
                                 GUPnPContext           *context,
                                 XmlDocWrapper          *wrapper,
                                 xmlNode                *element,
                                 const char             *udn,
                                 const char             *service_type,
                                 const char             *location,
                                 const SoupURI          *url_base)
{
        GUPnPServiceProxy *proxy;
        GType              proxy_type = GUPNP_TYPE_SERVICE_PROXY;

        g_return_val_if_fail (GUPNP_IS_RESOURCE_FACTORY (factory), NULL);
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (IS_XML_DOC_WRAPPER (wrapper), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (location != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        if (service_type) {
                gpointer value;

                value = g_hash_table_lookup (factory->priv->proxy_type_hash,
                                             service_type);
                if (value)
                        proxy_type = GPOINTER_TO_INT (value);
        }

        proxy = g_object_new (proxy_type,
                              "context", context,
                              "location", location,
                              "udn", udn,
                              "service-type", service_type,
                              "url-base", url_base,
                              "document", wrapper,
                              "element", element,
                              NULL);

        return proxy;
}

/**
 * gupnp_resource_factory_create_device
 * @factory: A #GUPnPResourceFactory
 * @context: A #GUPnPContext
 * @root_device: The #GUPnPRootDevice
 * @element: The #xmlNode ponting to the right device element
 * @udn: The UDN of the device to create a device for
 * @location: The location of the device description file
 * @url_base: The URL base for this device
 *
 * Return value: A #GUPnPDevice for the device with element @element, as
 * read from the device description file specified by @location.
 **/
GUPnPDevice *
gupnp_resource_factory_create_device
                                (GUPnPResourceFactory *factory,
                                 GUPnPContext         *context,
                                 GUPnPDevice          *root_device,
                                 xmlNode              *element,
                                 const char           *udn,
                                 const char           *location,
                                 const SoupURI        *url_base)
{
        GUPnPDevice *device;
        const char  *upnp_type;
        GType        device_type = GUPNP_TYPE_DEVICE;

        g_return_val_if_fail (GUPNP_IS_RESOURCE_FACTORY (factory), NULL);
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (GUPNP_IS_ROOT_DEVICE (root_device), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        upnp_type = xml_util_get_child_element_content_glib (element,
                                                             "deviceType");
        if (upnp_type) {
                gpointer value;

                value = g_hash_table_lookup (factory->priv->resource_type_hash,
                                             upnp_type);
                if (value)
                        device_type = GPOINTER_TO_INT (value);
        }

        device = g_object_new (device_type,
                               "resource-factory", factory,
                               "context", context,
                               "root-device", root_device,
                               "location", location,
                               "udn", udn,
                               "url-base", url_base,
                               "element", element,
                               NULL);

        return device;
}

/**
 * gupnp_resource_factory_create_service
 * @factory: A #GUPnPResourceFactory
 * @context: A #GUPnPContext
 * @root_device: The #GUPnPRootDevice
 * @element: The #xmlNode ponting to the right service element
 * @udn: The UDN of the device the service is contained in
 * @location: The location of the service description file
 * @url_base: The URL base for this service
 *
 * Return value: A #GUPnPService for the service with element @element, as
 * read from the service description file specified by @location.
 **/
GUPnPService *
gupnp_resource_factory_create_service
                                (GUPnPResourceFactory *factory,
                                 GUPnPContext         *context,
                                 GUPnPDevice          *root_device,
                                 xmlNode              *element,
                                 const char           *udn,
                                 const char           *location,
                                 const SoupURI        *url_base)
{
        GUPnPService *service;
        const char   *upnp_type;
        GType         service_type = GUPNP_TYPE_SERVICE;

        g_return_val_if_fail (GUPNP_IS_RESOURCE_FACTORY (factory), NULL);
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (GUPNP_IS_ROOT_DEVICE (root_device), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (location != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        upnp_type = xml_util_get_child_element_content_glib (element,
                                                             "serviceType");
        if (upnp_type) {
                gpointer value;

                value = g_hash_table_lookup (factory->priv->resource_type_hash,
                                             upnp_type);
                if (value)
                        service_type = GPOINTER_TO_INT (value);
        }

        service = g_object_new (service_type,
                                "context", context,
                                "root-device", root_device,
                                "location", location,
                                "udn", udn,
                                "url-base", url_base,
                                "element", element,
                                NULL);

        return service;
}

/**
 * gupnp_resource_factory_register_resource_type
 * @factory: A #GUPnPResourceFactory.
 * @upnp_type: The UPnP type name of the resource.
 * @type: The requested GType assignment for the resource.
 *
 * Registers the GType @type for the resource of UPnP type @upnp_type. After
 * this call, the factory @factory will create object of GType @type each time
 * it is asked to create a resource object for UPnP type @upnp_type.
 *
 * Note: GType @type must be a derived type of #GUPNP_TYPE_DEVICE if resource is
 * a device or #GUPNP_TYPE_SERVICE if its a service.
 **/
void
gupnp_resource_factory_register_resource_type (GUPnPResourceFactory *factory,
                                               const char           *upnp_type,
                                               GType                 type)
{
        g_hash_table_insert (factory->priv->resource_type_hash,
                             g_strdup (upnp_type),
                             GINT_TO_POINTER (type));
}

/**
 * gupnp_resource_factory_unregister_resource_type
 * @factory: A #GUPnPResourceFactory.
 * @upnp_type: The UPnP type name of the resource.
 *
 * Unregisters the GType assignment for the resource of UPnP type @upnp_type.
 *
 * Return value: TRUE if GType assignment was removed successfully, FALSE
 * otherwise.
 **/
gboolean
gupnp_resource_factory_unregister_resource_type
                                (GUPnPResourceFactory *factory,
                                 const char           *upnp_type)
{
        return g_hash_table_remove (factory->priv->resource_type_hash,
                                    upnp_type);
}

/**
 * gupnp_resource_factory_register_resource_proxy_type
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
        g_hash_table_insert (factory->priv->proxy_type_hash,
                             g_strdup (upnp_type),
                             GINT_TO_POINTER (type));
}

/**
 * gupnp_resource_factory_unregister_resource_proxy_type
 * @factory: A #GUPnPResourceFactory.
 * @upnp_type: The UPnP type name of the resource.
 *
 * Unregisters the GType assignment for the proxy of resource of UPnP type
 * @upnp_type.
 *
 * Return value: TRUE if GType assignment was removed successfully, FALSE
 * otherwise.
 **/
gboolean
gupnp_resource_factory_unregister_resource_proxy_type
                                (GUPnPResourceFactory *factory,
                                 const char           *upnp_type)
{
        return g_hash_table_remove (factory->priv->proxy_type_hash, upnp_type);
}
