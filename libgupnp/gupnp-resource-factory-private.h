/*
 * Copyright (C) 2007 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
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

#ifndef __GUPNP_RESOURCE_FACTORY_PRIVATE_H__
#define __GUPNP_RESOURCE_FACTORY_PRIVATE_H__

#include "xml-util.h"
#include "gupnp-device.h"
#include "gupnp-service.h"
#include "gupnp-device-proxy.h"
#include "gupnp-service-proxy.h"
#include "gupnp-resource-factory.h"
#include "gupnp-xml-doc.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL GUPnPDeviceProxy *
gupnp_resource_factory_create_device_proxy
                                      (GUPnPResourceFactory *factory,
                                       GUPnPContext         *context,
                                       GUPnPXMLDoc          *doc,
                                       xmlNode              *element,
                                       const char           *udn,
                                       const char           *location,
                                       const SoupURI        *url_base);

G_GNUC_INTERNAL GUPnPServiceProxy *
gupnp_resource_factory_create_service_proxy
                                      (GUPnPResourceFactory *factory,
                                       GUPnPContext         *context,
                                       GUPnPXMLDoc          *doc,
                                       xmlNode              *element,
                                       const char           *udn,
                                       const char           *service_type,
                                       const char           *location,
                                       const SoupURI        *url_base);

G_GNUC_INTERNAL GUPnPDevice *
gupnp_resource_factory_create_device  (GUPnPResourceFactory *factory,
                                       GUPnPContext         *context,
                                       GUPnPDevice          *root_device,
                                       xmlNode              *element,
                                       const char           *udn,
                                       const char           *location,
                                       const SoupURI        *url_base);

G_GNUC_INTERNAL GUPnPService *
gupnp_resource_factory_create_service (GUPnPResourceFactory *factory,
                                       GUPnPContext         *context,
                                       GUPnPDevice          *root_device,
                                       xmlNode              *element,
                                       const char           *udn,
                                       const char           *location,
                                       const SoupURI        *url_base);

G_END_DECLS

#endif /* __GUPNP_RESOURCE_FACTORY_PRIVATE_H__ */
