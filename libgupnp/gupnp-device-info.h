/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
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

#ifndef __GUPNP_DEVICE_INFO_H__
#define __GUPNP_DEVICE_INFO_H__

#include <glib-object.h>
#include <libxml/tree.h>
#include <libsoup/soup-uri.h>

#include "gupnp-context.h"
#include "gupnp-service-info.h"
#include "gupnp-resource-factory.h"

G_BEGIN_DECLS

GType
gupnp_device_info_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_DEVICE_INFO \
                (gupnp_device_info_get_type ())
#define GUPNP_DEVICE_INFO(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_DEVICE_INFO, \
                 GUPnPDeviceInfo))
#define GUPNP_DEVICE_INFO_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_DEVICE_INFO, \
                 GUPnPDeviceInfoClass))
#define GUPNP_IS_DEVICE_INFO(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_DEVICE_INFO))
#define GUPNP_IS_DEVICE_INFO_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_DEVICE_INFO))
#define GUPNP_DEVICE_INFO_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_DEVICE_INFO, \
                 GUPnPDeviceInfoClass))

typedef struct _GUPnPDeviceInfoPrivate GUPnPDeviceInfoPrivate;

/**
 * GUPnPDeviceInfo:
 *
 * This struct contains private data only, and should be accessed using the
 * functions below.
 */
typedef struct {
        GObject parent;

        GUPnPDeviceInfoPrivate *priv;
} GUPnPDeviceInfo;

typedef struct {
        GObjectClass parent_class;

        /* vtable */
        xmlNode          * (* get_element) (GUPnPDeviceInfo *info);
        GUPnPDeviceInfo  * (* get_device)  (GUPnPDeviceInfo *info,
                                            xmlNode         *element);
        GUPnPServiceInfo * (* get_service) (GUPnPDeviceInfo *info,
                                            xmlNode         *element);

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
} GUPnPDeviceInfoClass;


GUPnPContext *
gupnp_device_info_get_context            (GUPnPDeviceInfo *info);

const char *
gupnp_device_info_get_location           (GUPnPDeviceInfo *info);

const SoupURI *
gupnp_device_info_get_url_base           (GUPnPDeviceInfo *info);

const char *
gupnp_device_info_get_udn                (GUPnPDeviceInfo *info);

const char *
gupnp_device_info_get_device_type        (GUPnPDeviceInfo *info);

char *
gupnp_device_info_get_friendly_name      (GUPnPDeviceInfo *info);

char *
gupnp_device_info_get_manufacturer       (GUPnPDeviceInfo *info);

char *
gupnp_device_info_get_manufacturer_url   (GUPnPDeviceInfo *info);

char *
gupnp_device_info_get_model_description  (GUPnPDeviceInfo *info);

char *
gupnp_device_info_get_model_name         (GUPnPDeviceInfo *info);

char *
gupnp_device_info_get_model_number       (GUPnPDeviceInfo *info);

char *
gupnp_device_info_get_model_url          (GUPnPDeviceInfo *info);

char *
gupnp_device_info_get_serial_number      (GUPnPDeviceInfo *info);

char *
gupnp_device_info_get_upc                (GUPnPDeviceInfo *info);

char *
gupnp_device_info_get_icon_url           (GUPnPDeviceInfo *info,
                                          const char      *requested_mime_type,
                                          int              requested_depth,
                                          int              requested_width,
                                          int              requested_height,
                                          gboolean         prefer_bigger,
                                          char           **mime_type,
                                          int             *depth,
                                          int             *width,
                                          int             *height);

char *
gupnp_device_info_get_presentation_url   (GUPnPDeviceInfo *info);

GList *
gupnp_device_info_list_dlna_capabilities (GUPnPDeviceInfo *info);

char *
gupnp_device_info_get_description_value  (GUPnPDeviceInfo *info,
                                          const char      *element);
GList *
gupnp_device_info_list_devices           (GUPnPDeviceInfo *info);

GList *
gupnp_device_info_list_device_types      (GUPnPDeviceInfo *info);

GUPnPDeviceInfo *
gupnp_device_info_get_device             (GUPnPDeviceInfo *info,
                                          const char      *type);

GList *
gupnp_device_info_list_services          (GUPnPDeviceInfo *info);

GList *
gupnp_device_info_list_service_types     (GUPnPDeviceInfo *info);

GUPnPServiceInfo *
gupnp_device_info_get_service            (GUPnPDeviceInfo *info,
                                          const char      *type);

GUPnPResourceFactory *
gupnp_device_info_get_resource_factory   (GUPnPDeviceInfo *device_info);

G_END_DECLS

#endif /* __GUPNP_DEVICE_INFO_H__ */
