/*
 * Copyright (C) 2007, 2008 OpenedHand Ltd.
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef GUPNP_ROOT_DEVICE_H
#define GUPNP_ROOT_DEVICE_H

#include <libxml/tree.h>

#include <libgssdp/gssdp-resource-group.h>

#include "gupnp-context.h"
#include "gupnp-device.h"
#include "gupnp-resource-factory.h"
#include "gupnp-xml-doc.h"

G_BEGIN_DECLS

#define GUPNP_TYPE_ROOT_DEVICE \
                (gupnp_root_device_get_type ())

G_DECLARE_DERIVABLE_TYPE (GUPnPRootDevice,
                          gupnp_root_device,
                          GUPNP,
                          ROOT_DEVICE,
                          GUPnPDevice)


struct _GUPnPRootDeviceClass {
        GUPnPDeviceClass parent_class;

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
};

GUPnPRootDevice *
gupnp_root_device_new             (GUPnPContext         *context,
                                   const char           *description_path,
                                   const char           *description_dir,
                                   GError              **error);

GUPnPRootDevice *
gupnp_root_device_new_full        (GUPnPContext         *context,
                                   GUPnPResourceFactory *factory,
                                   GUPnPXMLDoc          *description_doc,
                                   const char           *description_path,
                                   const char           *description_dir,
                                   GError              **error);

void
gupnp_root_device_set_available   (GUPnPRootDevice      *root_device,
                                   gboolean              available);

gboolean
gupnp_root_device_get_available   (GUPnPRootDevice      *root_device);

const char *
gupnp_root_device_get_relative_location
                                  (GUPnPRootDevice      *root_device);

const char *
gupnp_root_device_get_description_path
                                  (GUPnPRootDevice      *root_device);

const char *
gupnp_root_device_get_description_dir
                                  (GUPnPRootDevice      *root_device);

GSSDPResourceGroup *
gupnp_root_device_get_ssdp_resource_group
                                  (GUPnPRootDevice      *root_device);

G_END_DECLS

#endif /* GUPNP_ROOT_DEVICE_H */
