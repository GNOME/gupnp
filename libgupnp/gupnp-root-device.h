/*
 * Copyright (C) 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
                                   const char           *description_folder,
                                   GError              **error);

GUPnPRootDevice *
gupnp_root_device_new_full        (GUPnPContext         *context,
                                   GUPnPResourceFactory *factory,
                                   GUPnPXMLDoc          *description_doc,
                                   const char           *description_path,
                                   const char           *description_folder,
                                   GError              **error);

void
gupnp_root_device_set_available   (GUPnPRootDevice      *root_device,
                                   gboolean              available);

gboolean
gupnp_root_device_get_available   (GUPnPRootDevice      *root_device);

const char *
gupnp_root_device_get_description_document_name
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
