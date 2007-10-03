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

#ifndef __GUPNP_RESOURCE_FACTORY_H__
#define __GUPNP_RESOURCE_FACTORY_H__

#include "xml-util.h"
#include "gupnp-device.h"
#include "gupnp-service.h"
#include "gupnp-device-proxy.h"
#include "gupnp-service-proxy.h"

G_BEGIN_DECLS

GType
gupnp_resource_factory_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_RESOURCE_FACTORY \
                (gupnp_resource_factory_get_type ())
#define GUPNP_RESOURCE_FACTORY(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_RESOURCE_FACTORY, \
                 GUPnPResourceFactory))
#define GUPNP_RESOURCE_FACTORY_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_RESOURCE_FACTORY, \
                 GUPnPResourceFactoryClass))
#define GUPNP_IS_RESOURCE_FACTORY(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_RESOURCE_FACTORY))
#define GUPNP_IS_RESOURCE_FACTORY_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_RESOURCE_FACTORY))
#define GUPNP_RESOURCE_FACTORY_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_RESOURCE_FACTORY, \
                 GUPnPResourceFactoryClass))

typedef struct {
        GObject parent;

        gpointer _gupnp_reserved;
} GUPnPResourceFactory;

typedef struct {
        GObjectClass parent_class;

        /* resource proxy creation implementations */
        GUPnPDeviceProxy  * (* create_device_proxy)
                                         (GUPnPResourceFactory *factory,
                                          GUPnPContext         *context,
                                          XmlDocWrapper        *doc,
                                          xmlNode              *element,
                                          const char           *udn,
                                          const char           *location,
                                          const SoupUri        *url_base);

        GUPnPServiceProxy * (* create_service_proxy)
                                         (GUPnPResourceFactory *factory,
                                          GUPnPContext         *context,
                                          XmlDocWrapper        *wrapper,
                                          xmlNode              *element,
                                          const char           *udn,
                                          const char           *service_type,
                                          const char           *location,
                                          const SoupUri        *url_base);

        /* resource creation implementations */
        GUPnPDevice       * (* create_device)
                                         (GUPnPResourceFactory *factory,
                                          GUPnPContext         *context,
                                          GUPnPDevice          *root_device,
                                          xmlNode              *element,
                                          const char           *udn,
                                          const char           *location,
                                          const SoupUri        *url_base);

        GUPnPService      * (* create_service)
                                         (GUPnPResourceFactory *factory,
                                          GUPnPContext         *context,
                                          GUPnPDevice          *root_device,
                                          xmlNode              *element,
                                          const char           *udn,
                                          const char           *location,
                                          const SoupUri        *url_base);

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
} GUPnPResourceFactoryClass;

GUPnPResourceFactory *
gupnp_resource_factory_get_default    (void);

GUPnPDeviceProxy *
gupnp_resource_factory_create_device_proxy
                                      (GUPnPResourceFactory *factory,
                                       GUPnPContext         *context,
                                       XmlDocWrapper        *doc,
                                       xmlNode              *element,
                                       const char           *udn,
                                       const char           *location,
                                       const SoupUri        *url_base);

GUPnPServiceProxy *
gupnp_resource_factory_create_service_proxy
                                      (GUPnPResourceFactory *factory,
                                       GUPnPContext         *context,
                                       XmlDocWrapper        *wrapper,
                                       xmlNode              *element,
                                       const char           *udn,
                                       const char           *service_type,
                                       const char           *location,
                                       const SoupUri        *url_base);

GUPnPDevice *
gupnp_resource_factory_create_device  (GUPnPResourceFactory *factory,
                                       GUPnPContext         *context,
                                       GUPnPDevice          *root_device,
                                       xmlNode              *element,
                                       const char           *udn,
                                       const char           *location,
                                       const SoupUri        *url_base);

GUPnPService *
gupnp_resource_factory_create_service (GUPnPResourceFactory *factory,
                                       GUPnPContext         *context,
                                       GUPnPDevice          *root_device,
                                       xmlNode              *element,
                                       const char           *udn,
                                       const char           *location,
                                       const SoupUri        *url_base);

G_END_DECLS

#endif /* __GUPNP_RESOURCE_FACTORY_H__ */
