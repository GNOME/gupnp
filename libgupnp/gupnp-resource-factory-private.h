/*
 * Copyright (C) 2007 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_RESOURCE_FACTORY_PRIVATE_H
#define GUPNP_RESOURCE_FACTORY_PRIVATE_H

#include "xml-util.h"
#include "gupnp-device.h"
#include "gupnp-service.h"
#include "gupnp-device-proxy.h"
#include "gupnp-service-proxy.h"
#include "gupnp-resource-factory.h"
#include "gupnp-xml-doc.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL GUPnPDeviceProxy *
gupnp_resource_factory_create_device_proxy (GUPnPResourceFactory *factory,
                                            GUPnPContext *context,
                                            GUPnPXMLDoc *doc,
                                            xmlNode *element,
                                            const char *udn,
                                            const char *location,
                                            const GUri *url_base);

G_GNUC_INTERNAL GUPnPServiceProxy *
gupnp_resource_factory_create_service_proxy (GUPnPResourceFactory *factory,
                                             GUPnPContext *context,
                                             GUPnPXMLDoc *doc,
                                             xmlNode *element,
                                             const char *udn,
                                             const char *service_type,
                                             const char *location,
                                             const GUri *url_base);

G_GNUC_INTERNAL GUPnPDevice *
gupnp_resource_factory_create_device (GUPnPResourceFactory *factory,
                                      GUPnPContext *context,
                                      GUPnPDevice *root_device,
                                      xmlNode *element,
                                      const char *udn,
                                      const char *location,
                                      const GUri *url_base);

G_GNUC_INTERNAL GUPnPService *
gupnp_resource_factory_create_service (GUPnPResourceFactory *factory,
                                       GUPnPContext *context,
                                       GUPnPDevice *root_device,
                                       xmlNode *element,
                                       const char *udn,
                                       const char *location,
                                       const GUri *url_base);

G_END_DECLS

#endif /* GUPNP_RESOURCE_FACTORY_PRIVATE_H */
