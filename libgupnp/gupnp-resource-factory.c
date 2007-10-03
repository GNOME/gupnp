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
 * @short_description: Class for resource objects creation.
 *
 * #GUPnPResourceFactory handles creation of client and server resource
 * object: device, service, device and service proxy. You can always subclass
 * the resource objects but the #GUPnPControlPoint and #GUPnPRootDevice objects
 * do not and can not create objects of your subclass. The solution to this
 * problem is to:
 *
 * <orderedlist>
 * <listitem>
 * <para>Create your own subclass of the #GUPnPResourceFactory.
 * </para>
 * </listitem>
 * <listitem>
 * <para>In your #GUPnPResourceFactory subclass, override the appropriate
 * *_create_* virtual functions with your own functions, in which you create
 * the objects of your desired type.
 * </para>
 * </listitem>
 * <listitem>
 * <para>Ask the #GUPnPControlPoint and #GUPnPRootDevice to use your custom
 * #GUPnPResourceFactory to create resource objects, either at creation time
 * using the appropriate *_new_full functions or using the "resource-factory"
 * property.
 * </para>
 * </listitem>
 * </orderedlist>
 *
 */

#include <string.h>

#include "gupnp-resource-factory.h"
#include "gupnp-root-device.h"

G_DEFINE_TYPE (GUPnPResourceFactory,
               gupnp_resource_factory,
               G_TYPE_OBJECT);

static void
gupnp_resource_factory_init (GUPnPResourceFactory *factory)
{
}

static GUPnPDeviceProxy *
create_device_proxy (GUPnPResourceFactory *factory,
                     GUPnPContext         *context,
                     XmlDocWrapper        *doc,
                     xmlNode              *element,
                     const char           *udn,
                     const char           *location,
                     const SoupUri        *url_base)
{
        GUPnPDeviceProxy *proxy;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (IS_XML_DOC_WRAPPER (doc), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (location != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        proxy = g_object_new (GUPNP_TYPE_DEVICE_PROXY,
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

static GUPnPServiceProxy *
create_service_proxy (GUPnPResourceFactory *factory,
                      GUPnPContext         *context,
                      XmlDocWrapper        *doc,
                      xmlNode              *element,
                      const char           *udn,
                      const char           *service_type,
                      const char           *location,
                      const SoupUri        *url_base)
{
        GUPnPServiceProxy *proxy;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (IS_XML_DOC_WRAPPER (doc), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (location != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        proxy = g_object_new (GUPNP_TYPE_SERVICE_PROXY,
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

static GUPnPDevice *
create_device (GUPnPResourceFactory *factory,
               GUPnPContext         *context,
               GUPnPDevice          *root_device,
               xmlNode              *element,
               const char           *udn,
               const char           *location,
               const SoupUri        *url_base)
{
        GUPnPDevice *device;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (GUPNP_IS_ROOT_DEVICE (root_device), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        device = g_object_new (GUPNP_TYPE_DEVICE,
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

static GUPnPService *
create_service (GUPnPResourceFactory *factory,
                GUPnPContext         *context,
                GUPnPDevice          *root_device,
                xmlNode              *element,
                const char           *udn,
                const char           *location,
                const SoupUri        *url_base)
{
        GUPnPService *service;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (GUPNP_IS_ROOT_DEVICE (root_device), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (location != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        service = g_object_new (GUPNP_TYPE_SERVICE,
                                "context", context,
                                "root-device", root_device,
                                "location", location,
                                "udn", udn,
                                "url-base", url_base,
                                "element", element,
                                NULL);

        return service;
}

static void
gupnp_resource_factory_class_init (GUPnPResourceFactoryClass *klass)
{
        klass->create_device_proxy  = create_device_proxy;
        klass->create_service_proxy = create_service_proxy;
        klass->create_device        = create_device;
        klass->create_service       = create_service;
}

/**
 * gupnp_resource_factory_get_default
 *
 * Return value: The default singleton #GUPnPResourceFactory object.
 *
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
                                 const SoupUri        *url_base)
{
        GUPnPResourceFactoryClass *klass;

        g_return_val_if_fail (GUPNP_IS_RESOURCE_FACTORY (factory), NULL);

        klass = GUPNP_RESOURCE_FACTORY_GET_CLASS (factory);
        if (klass->create_device != NULL) {
                return klass->create_device_proxy (factory,
                                                   context,
                                                   doc,
                                                   element,
                                                   udn,
                                                   location,
                                                   url_base);
        } else
                return NULL;
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
                                 const SoupUri          *url_base)
{
        GUPnPResourceFactoryClass *klass;

        g_return_val_if_fail (GUPNP_IS_RESOURCE_FACTORY (factory), NULL);

        klass = GUPNP_RESOURCE_FACTORY_GET_CLASS (factory);
        if (klass->create_service_proxy != NULL) {
                return klass->create_service_proxy (factory,
                                                    context,
                                                    wrapper,
                                                    element,
                                                    udn,
                                                    service_type,
                                                    location,
                                                    url_base);
        } else
                return NULL;
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
                                 const SoupUri        *url_base)
{
        GUPnPResourceFactoryClass *klass;

        g_return_val_if_fail (GUPNP_IS_RESOURCE_FACTORY (factory), NULL);

        klass = GUPNP_RESOURCE_FACTORY_GET_CLASS (factory);
        if (klass->create_device != NULL) {
                return klass->create_device (factory,
                                             context,
                                             root_device,
                                             element,
                                             udn,
                                             location,
                                             url_base);
        } else
                return NULL;
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
                                 const SoupUri        *url_base)
{
        GUPnPResourceFactoryClass *klass;

        g_return_val_if_fail (GUPNP_IS_RESOURCE_FACTORY (factory), NULL);

        klass = GUPNP_RESOURCE_FACTORY_GET_CLASS (factory);
        if (klass->create_service != NULL) {
                return klass->create_service (factory,
                                              context,
                                              root_device,
                                              element,
                                              udn,
                                              location,
                                              url_base);
        } else
                return NULL;
}

