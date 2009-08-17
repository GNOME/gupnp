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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GUPNP_ROOT_DEVICE_H__
#define __GUPNP_ROOT_DEVICE_H__

#include <libxml/tree.h>

#include "gupnp-context.h"
#include "gupnp-device.h"
#include "gupnp-resource-factory.h"
#include "gupnp-xml-doc.h"

G_BEGIN_DECLS

GType
gupnp_root_device_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_ROOT_DEVICE \
                (gupnp_root_device_get_type ())
#define GUPNP_ROOT_DEVICE(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_ROOT_DEVICE, \
                 GUPnPRootDevice))
#define GUPNP_ROOT_DEVICE_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_ROOT_DEVICE, \
                 GUPnPRootDeviceClass))
#define GUPNP_IS_ROOT_DEVICE(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_ROOT_DEVICE))
#define GUPNP_IS_ROOT_DEVICE_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_ROOT_DEVICE))
#define GUPNP_ROOT_DEVICE_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_ROOT_DEVICE, \
                 GUPnPRootDeviceClass))

typedef struct _GUPnPRootDevicePrivate GUPnPRootDevicePrivate;

/**
 * GUPnPRootDevice:
 *
 * This struct contains private data only, and should be accessed using the
 * functions below.
 */
typedef struct {
        GUPnPDevice parent;

        GUPnPRootDevicePrivate *priv;
} GUPnPRootDevice;

typedef struct {
        GUPnPDeviceClass parent_class;

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
} GUPnPRootDeviceClass;

GUPnPRootDevice *
gupnp_root_device_new             (GUPnPContext         *context,
                                   const char           *description_path,
                                   const char           *description_dir);

GUPnPRootDevice *
gupnp_root_device_new_full        (GUPnPContext         *context,
                                   GUPnPResourceFactory *factory,
                                   GUPnPXMLDoc          *description_doc,
                                   const char           *description_path,
                                   const char           *description_dir);

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

G_END_DECLS

#endif /* __GUPNP_ROOT_DEVICE_H__ */
