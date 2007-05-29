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

/**
 * SECTION:gupnp-device-info
 * @short_description: Interface for querying device information.
 *
 * The #GUPnPDeviceInfo interface provides methods for querying device
 * information.
 */

#include <string.h>

#include "gupnp-device-info.h"
#include "xml-util.h"

G_DEFINE_ABSTRACT_TYPE (GUPnPDeviceInfo,
                        gupnp_device_info,
                        G_TYPE_OBJECT);

struct _GUPnPDeviceInfoPrivate {
        GUPnPContext *context;

        char *location;
        char *udn;
};

enum {
        PROP_0,
        PROP_CONTEXT,
        PROP_LOCATION,
        PROP_UDN
};

static void
gupnp_device_info_init (GUPnPDeviceInfo *info)
{
        info->priv = G_TYPE_INSTANCE_GET_PRIVATE (info,
                                                  GUPNP_TYPE_DEVICE_INFO,
                                                  GUPnPDeviceInfoPrivate);
}

static void
gupnp_device_info_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        GUPnPDeviceInfo *info;

        info = GUPNP_DEVICE_INFO (object);

        switch (property_id) {
        case PROP_CONTEXT:
                info->priv->context =
                        g_object_ref (g_value_get_object (value));
                break;
        case PROP_LOCATION:
                g_free (info->priv->location);
                info->priv->location =
                        g_value_dup_string (value);
                break;
        case PROP_UDN:
                g_free (info->priv->udn);
                info->priv->udn =
                        g_value_dup_string (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_device_info_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        GUPnPDeviceInfo *info;

        info = GUPNP_DEVICE_INFO (object);

        switch (property_id) {
        case PROP_CONTEXT:
                g_value_set_object (value,
                                    info->priv->context);
                break;
        case PROP_LOCATION:
                g_value_set_string (value,
                                    info->priv->location);
                break;
        case PROP_UDN:
                g_value_set_string (value,
                                    info->priv->udn);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_device_info_dispose (GObject *object)
{
        GUPnPDeviceInfo *info;

        info = GUPNP_DEVICE_INFO (object);

        if (info->priv->context) {
                g_object_unref (info->priv->context);
                info->priv->context = NULL;
        }
}

static void
gupnp_device_info_finalize (GObject *object)
{
        GUPnPDeviceInfo *info;

        info = GUPNP_DEVICE_INFO (object);

        g_free (info->priv->location);
        g_free (info->priv->udn);
}

static void
gupnp_device_info_class_init (GUPnPDeviceInfoClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_device_info_set_property;
        object_class->get_property = gupnp_device_info_get_property;
        object_class->dispose      = gupnp_device_info_dispose;
        object_class->finalize     = gupnp_device_info_finalize;

        g_type_class_add_private (klass, sizeof (GUPnPDeviceInfoPrivate));

        /**
         * GUPnPDeviceInfo:context
         *
         * The #GUPnPContext to use.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_CONTEXT,
                 g_param_spec_object ("context",
                                      "Context",
                                      "The GUPnPContext",
                                      GUPNP_TYPE_CONTEXT,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY));

        /**
         * GUPnPDeviceInfo:location
         *
         * The location of the device description file.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_LOCATION,
                 g_param_spec_string ("location",
                                      "Location",
                                      "The location of the device description "
                                      "file",
                                      NULL,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY));

        /**
         * GUPnPDeviceInfo:udn
         *
         * The UDN of this device.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_UDN,
                 g_param_spec_string ("udn",
                                      "UDN",
                                      "The UDN",
                                      NULL,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY));
}

/**
 * gupnp_device_info_get_context
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: The associated #GUPnPContext.
 **/
GUPnPContext *
gupnp_device_info_get_context (GUPnPDeviceInfo *info)
{
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return info->priv->context;
}

/**
 * gupnp_device_info_get_location
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: The location of the device description file.
 **/
const char *
gupnp_device_info_get_location (GUPnPDeviceInfo *info)
{
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return info->priv->location;
}

/**
 * gupnp_device_info_get_udn
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: The UDN.
 **/
const char *
gupnp_device_info_get_udn (GUPnPDeviceInfo *info)
{
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return info->priv->udn;
}

static char *
get_property (GUPnPDeviceInfo *info,
              const char      *element_name)
{
        GUPnPDeviceInfoClass *class;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        class = GUPNP_DEVICE_INFO_GET_CLASS (info);

        g_return_val_if_fail (class->get_element, NULL);
        element = class->get_element (info);

        element = xml_util_get_element (element, element_name, NULL);

        if (element) {
                xmlChar *value;
                char *ret;
                
                /* Make glib memmanaged */
                value = xmlNodeGetContent (element);
                ret = g_strdup ((char *) value);
                xmlFree (value);

                return ret;
        } else
                return NULL;
}

/**
 * gupnp_device_info_get_device_type
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: The UPnP device type, or NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_device_type (GUPnPDeviceInfo *info)
{
        return get_property (info, "deviceType");
}

/**
 * gupnp_device_info_get_friendly_name
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: The friendly name, or NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_friendly_name (GUPnPDeviceInfo *info)
{
        return get_property (info, "friendlyName");
}

/**
 * gupnp_device_info_get_manufacturer
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: The manufacturer, or NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_manufacturer (GUPnPDeviceInfo *info)
{
        return get_property (info, "manufacturer");
}

/**
 * gupnp_device_info_get_manufacturer_url
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: A URL pointing to the manufacturers website, or NULL.
 * g_free() after use.
 **/
char *
gupnp_device_info_get_manufacturer_url (GUPnPDeviceInfo *info)
{
        return get_property (info, "manufacturerURL");
}

/**
 * gupnp_device_info_get_model_description
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: The description of the device model, or NULL. g_free() after
 * use.
 **/
char *
gupnp_device_info_get_model_description (GUPnPDeviceInfo *info)
{
        return get_property (info, "modelDescription");
}

/**
 * gupnp_device_info_get_model_name
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: The name of the device model, or NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_model_name (GUPnPDeviceInfo *info)
{
        return get_property (info, "modelName");
}

/**
 * gupnp_device_info_get_model_number
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: The model number, or NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_model_number (GUPnPDeviceInfo *info)
{
        return get_property (info, "modelDescription");
}

/**
 * gupnp_device_info_get_model_url
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: A URL pointing to the device models website, or NULL.
 * g_free() after use.
 **/
char *
gupnp_device_info_get_model_url (GUPnPDeviceInfo *info)
{
        return get_property (info, "modelURL");
}

/**
 * gupnp_device_info_get_serial_number
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: The serial number, or NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_serial_number (GUPnPDeviceInfo *info)
{
        return get_property (info, "serialNumber");
}

/**
 * gupnp_device_info_get_upc
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: The UPC, or NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_upc (GUPnPDeviceInfo *info)
{
        return get_property (info, "UPC");
}

typedef struct {
        xmlChar *mime_type;
        int      width;
        int      height;
        int      depth;
        char    *url;

        int      weight;
} Icon;

static Icon *
icon_parse (GUPnPDeviceInfo *info, xmlNode *element)
{
        Icon *icon;
        xmlNode *prop;

        icon = g_slice_new0 (Icon);

        prop = xml_util_get_element (element, "mimetype", NULL);
        if (prop)
                icon->mime_type = xmlNodeGetContent (prop);

        prop = xml_util_get_element (element, "width", NULL);
        if (prop)
                icon->width = xml_util_node_get_content_int (prop);
        else
                icon->width = -1;
        
        prop = xml_util_get_element (element, "height", NULL);
        if (prop)
                icon->height = xml_util_node_get_content_int (prop);
        else
                icon->width = -1;
        
        prop = xml_util_get_element (element, "depth", NULL);
        if (prop)
                icon->depth = xml_util_node_get_content_int (prop);
        else
                icon->width = -1;
        
        prop = xml_util_get_element (element, "url", NULL);
        if (prop) {
                xmlChar *url;

                url = xmlNodeGetContent (prop);
                if (url) {
                        GUPnPDeviceInfoClass *class;
                        const char *url_base;

                        class = GUPNP_DEVICE_INFO_GET_CLASS (info);

                        url_base = class->get_url_base (info);

                        if (url_base != NULL) {
                                icon->url = g_build_path ("/",
                                                          url_base,
                                                          (const char *) url,
                                                          NULL);
                        } else
                                icon->url = g_strdup ((const char *) url);

                        xmlFree (url);
                }
        }

        return icon;
}

static void
icon_free (Icon *icon)
{
        if (icon->mime_type)
                xmlFree (icon->mime_type);

        g_free (icon->url);

        g_slice_free (Icon, icon);
}

/**
 * gupnp_device_info_get_icon_url
 * @info: A #GUPnPDeviceInfo
 * @requested_mime_type: The requested file format, or NULL for any
 * @requested_depth: The requested color depth, or -1 for any
 * @requested_width: The requested width, or -1 for any
 * @requested_height: The requested height, or -1 for any
 * @prefer_bigger: TRUE if a bigger, rather than a smaller icon should be
 * returned if no exact match could be found
 * @mime_type: The location where to store the the format of the returned icon,
 * or NULL. The returned string should be freed after use
 * @depth: The location where to store the depth of the returned icon, or NULL
 * @width: The location where to store the width of the returned icon, or NULL
 * @height: The location where to store the height of the returned icon, or NULL
 *
 * Return value: A URL pointing to the icon most closely matching the
 * given criteria, or NULL. If @requested_mime_type is set, only icons with
 * this mime type will be returned. If @requested_depth is set, only icons with 
 * this or lower depth will be returned. If @requested_width and/or
 * @requested_height are set, only icons that are this size or smaller are
 * returned, unless @prefer_bigger is set, in which case the next biggest icon 
 * will be returned. The returned strings should be freed.
 **/
char *
gupnp_device_info_get_icon_url (GUPnPDeviceInfo *info,
                                const char      *requested_mime_type,
                                int              requested_depth,
                                int              requested_width,
                                int              requested_height,
                                gboolean         prefer_bigger,
                                char           **mime_type,
                                int             *depth,
                                int             *width,
                                int             *height)
{
        GUPnPDeviceInfoClass *class;
        xmlNode *element;
        GList *icons, *l;
        Icon *icon, *closest;
        char *ret;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);
                
        class = GUPNP_DEVICE_INFO_GET_CLASS (info);

        g_return_val_if_fail (class->get_element, NULL);
        element = class->get_element (info);

        /* List available icons */
        icons = NULL;

        element = xml_util_get_element (element, "iconList", NULL);

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("icon", (char *) element->name)) {
                        gboolean mime_type_ok;

                        icon = icon_parse (info, element);

                        if (requested_mime_type) {
                                mime_type_ok =
                                        !strcmp (requested_mime_type,
                                                 (char *) icon->mime_type);
                        } else
                                mime_type_ok = TRUE;

                        if (requested_depth >= 0)
                                icon->weight = requested_depth - icon->depth;

                        /* Filter out icons with incorrect mime type or
                         * incorrect depth. */
                        if (mime_type_ok && icon->weight >= 0) {
                                if (requested_width >= 0) {
                                        if (prefer_bigger) {
                                                icon->weight +=
                                                        icon->width -
                                                        requested_width;
                                        } else {
                                                icon->weight +=
                                                        requested_width -
                                                        icon->width;
                                        }
                                }
                                
                                if (requested_height >= 0) {
                                        if (prefer_bigger) {
                                                icon->weight +=
                                                        icon->height -
                                                        requested_height;
                                        } else {
                                                icon->weight +=
                                                        requested_height -
                                                        icon->height;
                                        }
                                }
                                
                                icons = g_list_prepend (icons, icon);
                        } else
                                icon_free (icon);
                }
        }

        /* Find closest match */
        closest = NULL;
        for (l = icons; l; l = l->next) {
                icon = l->data;

                /* Look between icons with positive weight first */
                if (icon->weight >= 0) {
                        if (!closest || icon->weight < closest->weight)
                                closest = icon;
                }
        }

        if (!closest) {
                icon = l->data;

                /* No icons with positive weight, look at ones with
                 * negative weight */
                if (!closest || icon->weight > closest->weight)
                        closest = icon;
        }

        /* Fill in return values */
        if (closest) {
                if (mime_type)
                        *mime_type = g_strdup ((char *) icon->mime_type);
                if (depth)
                        *depth = icon->depth;
                if (width)
                        *width = icon->width;
                if (height)
                        *height = icon->height;

                ret = g_strdup ((char *) icon->url);
        } else {
                if (mime_type)
                        *mime_type = NULL;
                if (depth)
                        *depth = -1;
                if (width)
                        *width = -1;
                if (height)
                        *height = -1;

                ret = NULL;
        }

        /* Cleanup */
        while (icons) {
                icon_free (icons->data);
                icons = g_list_delete_link (icons, icons);
        }

        return ret;
}

/**
 * gupnp_device_info_list_devices
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: A #GList of objects implementing #GUPnPDeviceInfo representing
 * the devices directly contained in @info. The returned list should be
 * g_list_free()'d and the elements should be g_object_unref()'d.
 **/
GList *
gupnp_device_info_list_devices (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoClass *class;
        GList *devices;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        class = GUPNP_DEVICE_INFO_GET_CLASS (info);

        g_return_val_if_fail (class->get_device, NULL);

        g_return_val_if_fail (class->get_element, NULL);
        element = class->get_element (info);

        devices = NULL;

        element = xml_util_get_element (element,
                                        "deviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("device", (char *) element->name)) {
                        GUPnPDeviceInfo *child;

                        child = class->get_device (info, element);
                        devices = g_list_prepend (devices, child);
                }
        }

        return devices;
}

/**
 * gupnp_device_info_list_device_types
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: A #GList of strings representing the types of the devices
 * directly contained in @info. The returned list should be g_list_free()'d
 * and the elements should be g_free()'d.
 **/
GList *
gupnp_device_info_list_device_types (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoClass *class;
        GList *device_types;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        class = GUPNP_DEVICE_INFO_GET_CLASS (info);

        g_return_val_if_fail (class->get_element, NULL);
        element = class->get_element (info);

        device_types = NULL;

        element = xml_util_get_element (element,
                                        "deviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("device", (char *) element->name)) {
                        xmlNode *type_element;
                        xmlChar *type;

                        type_element = xml_util_get_element (element,
                                                             "deviceType",
                                                             NULL);
                        if (!type_element)
                                continue;

                        type = xmlNodeGetContent (type_element);
                        if (!type)
                                continue;

                        device_types =
                                g_list_prepend (device_types,
                                                g_strdup ((char *) type));
                        xmlFree (type);
                }
        }

        return device_types;
}

/**
 * gupnp_device_info_get_device
 * @info: A #GUPnPDeviceInfo
 * @type: The type of the device to be retrieved.
 *
 * Return value: The service with type @type directly contained in @info as
 * an object implementing #GUPnPDeviceInfo object, or NULL if no such device
 * was found. The returned object should be unreffed when done.
 **/
GUPnPDeviceInfo *
gupnp_device_info_get_device (GUPnPDeviceInfo *info,
                              const char      *type)
{
        GUPnPDeviceInfoClass *class;
        GUPnPDeviceInfo *device;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        class = GUPNP_DEVICE_INFO_GET_CLASS (info);

        g_return_val_if_fail (class->get_device, NULL);

        g_return_val_if_fail (class->get_element, NULL);
        element = class->get_element (info);

        device = NULL;

        element = xml_util_get_element (element,
                                        "deviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("device", (char *) element->name)) {
                        xmlNode *type_element;
                        xmlChar *type_str;

                        type_element = xml_util_get_element (element,
                                                             "deviceType",
                                                             NULL);
                        if (!type_element)
                                continue;

                        type_str = xmlNodeGetContent (type_element);
                        if (!type_str)
                                continue;

                        if (!strcmp (type, (char *) type_str))
                                device = class->get_device (info, element);

                        xmlFree (type_str);

                        if (device)
                                break;
                }
        }

        return device;
}

/**
 * gupnp_device_info_list_services
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: A #GList of objects implementing #GUPnPServiceInfo representing
 * the services directly contained in @info. The returned list should be
 * g_list_free()'d and the elements should be g_object_unref()'d.
 **/
GList *
gupnp_device_info_list_services (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoClass *class;
        GList *services;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        class = GUPNP_DEVICE_INFO_GET_CLASS (info);

        g_return_val_if_fail (class->get_service, NULL);

        g_return_val_if_fail (class->get_element, NULL);
        element = class->get_element (info);

        services = NULL;

        element = xml_util_get_element (element,
                                        "serviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("service", (char *) element->name)) {
                        GUPnPServiceInfo *service;

                        service = class->get_service (info, element);
                        services = g_list_prepend (services, service);
                }
        }

        return services;
}

/**
 * gupnp_device_info_list_service_types
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: A #GList of strings representing the types of the services
 * directly contained in @info. The returned list should be g_list_free()'d
 * and the elements should be g_free()'d.
 **/
GList *
gupnp_device_info_list_service_types (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoClass *class;
        GList *service_types;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        class = GUPNP_DEVICE_INFO_GET_CLASS (info);

        g_return_val_if_fail (class->get_element, NULL);
        element = class->get_element (info);

        service_types = NULL;

        element = xml_util_get_element (element,
                                        "serviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("service", (char *) element->name)) {
                        xmlNode *type_element;
                        xmlChar *type;

                        type_element = xml_util_get_element (element,
                                                             "serviceType",
                                                             NULL);
                        if (!type_element)
                                continue;

                        type = xmlNodeGetContent (type_element);
                        if (!type)
                                continue;

                        service_types =
                                g_list_prepend (service_types,
                                                g_strdup ((char *) type));
                        xmlFree (type);
                }
        }

        return service_types;
}

/**
 * gupnp_device_info_get_service
 * @info: A #GUPnPDeviceInfo
 * @type: The type of the service to be retrieved.
 *
 * Return value: The service with type @type directly contained in @info as
 * an object implementing #GUPnPServiceInfo object, or NULL if no such device
 * was found. The returned object should be unreffed when done.
 **/
GUPnPServiceInfo *
gupnp_device_info_get_service (GUPnPDeviceInfo *info,
                               const char      *type)
{
        GUPnPDeviceInfoClass *class;
        GUPnPServiceInfo *service;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        class = GUPNP_DEVICE_INFO_GET_CLASS (info);

        g_return_val_if_fail (class->get_service, NULL);

        g_return_val_if_fail (class->get_element, NULL);
        element = class->get_element (info);

        service = NULL;

        element = xml_util_get_element (element,
                                        "serviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("service", (char *) element->name)) {
                        xmlNode *type_element;
                        xmlChar *type_str;

                        type_element = xml_util_get_element (element,
                                                             "serviceType",
                                                             NULL);
                        if (!type_element)
                                continue;

                        type_str = xmlNodeGetContent (type_element);
                        if (!type_str)
                                continue;

                        if (!strcmp (type, (char *) type_str))
                                service = class->get_service (info, element);

                        xmlFree (type_str);

                        if (service)
                                break;
                }
        }

        return service;
}
