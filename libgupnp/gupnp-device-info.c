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
 * @short_description: Base abstract class for querying device information.
 *
 * The #GUPnPDeviceInfo base abstract class provides methods for querying
 * device information.
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

        SoupUri *url_base;

        xmlNode *element;
};

enum {
        PROP_0,
        PROP_CONTEXT,
        PROP_LOCATION,
        PROP_UDN,
        PROP_URL_BASE,
        PROP_ELEMENT
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
                info->priv->location =
                        g_value_dup_string (value);
                break;
        case PROP_UDN:
                info->priv->udn =
                        g_value_dup_string (value);
                break;
        case PROP_URL_BASE:
                info->priv->url_base = 
                        g_value_get_pointer (value);

                if (info->priv->url_base)
                        info->priv->url_base =
                                soup_uri_copy (info->priv->url_base);

                break;
        case PROP_ELEMENT:
                info->priv->element =
                        g_value_get_pointer (value);

                if (!info->priv->udn) {
                        info->priv->udn =
                                xml_util_get_child_element_content_glib
                                        (info->priv->element, "UDN");
                }

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
        case PROP_URL_BASE:
                g_value_set_pointer (value,
                                     info->priv->url_base);
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

        soup_uri_free (info->priv->url_base);
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
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

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
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

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
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPDeviceInfo:url-base
         *
         * The URL base (#SoupUri).
         **/
        g_object_class_install_property
                (object_class,
                 PROP_URL_BASE,
                 g_param_spec_pointer ("url-base",
                                       "URL base",
                                       "The URL base",
                                       G_PARAM_READWRITE |
                                       G_PARAM_CONSTRUCT_ONLY |
                                       G_PARAM_STATIC_NAME |
                                       G_PARAM_STATIC_NICK |
                                       G_PARAM_STATIC_BLURB));

        /**
         * GUPnPDeviceInfo:element
         *
         * Private property.
         *
         * Stability: Private
         **/
        g_object_class_install_property
                (object_class,
                 PROP_ELEMENT,
                 g_param_spec_pointer ("element",
                                       "Element",
                                       "The XML element related to this "
                                       "device",
                                       G_PARAM_WRITABLE |
                                       G_PARAM_CONSTRUCT_ONLY |
                                       G_PARAM_STATIC_NAME |
                                       G_PARAM_STATIC_NICK |
                                       G_PARAM_STATIC_BLURB));
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
 * gupnp_device_info_get_url_base
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: The URL base.
 **/
SoupUri *
gupnp_device_info_get_url_base (GUPnPDeviceInfo *info)
{
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return info->priv->url_base;
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

/**
 * gupnp_device_info_get_device_type
 * @info: A #GUPnPDeviceInfo
 *
 * Return value: The UPnP device type, or NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_device_type (GUPnPDeviceInfo *info)
{
        return xml_util_get_child_element_content_glib (info->priv->element,
                                                        "deviceType");
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
        return xml_util_get_child_element_content_glib (info->priv->element,
                                                        "friendlyName");
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
        return xml_util_get_child_element_content_glib (info->priv->element,
                                                        "manufacturer");
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
        return xml_util_get_child_element_content_url (info->priv->element,
                                                       "manufacturerURL",
                                                       info->priv->url_base);
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
        return xml_util_get_child_element_content_glib (info->priv->element,
                                                        "modelDescription");
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
        return xml_util_get_child_element_content_glib (info->priv->element,
                                                        "modelName");
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
        return xml_util_get_child_element_content_glib (info->priv->element,
                                                        "modelDescription");
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
        return xml_util_get_child_element_content_url (info->priv->element,
                                                       "modelURL",
                                                       info->priv->url_base);
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
        return xml_util_get_child_element_content_glib (info->priv->element,
                                                        "serialNumber");
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
        return xml_util_get_child_element_content_glib (info->priv->element,
                                                        "UPC");
}

typedef struct {
        xmlChar *mime_type;
        int      width;
        int      height;
        int      depth;
        xmlChar *url;

        int      weight;
} Icon;

static Icon *
icon_parse (GUPnPDeviceInfo *info, xmlNode *element)
{
        Icon *icon;

        icon = g_slice_new0 (Icon);

        icon->mime_type = xml_util_get_child_element_content      (element,
                                                                  "mimetype");
        icon->width     = xml_util_get_child_element_content_int (element,
                                                                  "width");
        icon->height    = xml_util_get_child_element_content_int (element,
                                                                  "height");
        icon->depth     = xml_util_get_child_element_content_int (element,
                                                                  "depth");
        icon->url       = xml_util_get_child_element_content     (element,
                                                                  "url");

        return icon;
}

static void
icon_free (Icon *icon)
{
        if (icon->mime_type)
                xmlFree (icon->mime_type);

        if (icon->url)
                xmlFree (icon->url);

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
        GList *icons, *l;
        xmlNode *element;
        Icon *icon, *closest;
        char *ret;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);
                
        /* List available icons */
        icons = NULL;

        element = xml_util_get_element (info->priv->element,
                                        "iconList",
                                        NULL);

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
                if (mime_type) {
                        if (icon->mime_type) {
                                *mime_type = g_strdup
                                                ((char *) icon->mime_type);
                        } else
                                *mime_type = NULL;
                }

                if (depth)
                        *depth = icon->depth;
                if (width)
                        *width = icon->width;
                if (height)
                        *height = icon->height;

                if (icon->url) {
                        SoupUri *uri;

                        uri = soup_uri_new_with_base (info->priv->url_base,
                                                      (const char *) icon->url);
                        ret = soup_uri_to_string (uri, FALSE);
                        soup_uri_free (uri);
                } else
                        ret = NULL;
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

        devices = NULL;

        element = xml_util_get_element (info->priv->element,
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
        GList *device_types;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        device_types = NULL;

        element = xml_util_get_element (info->priv->element,
                                        "deviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("device", (char *) element->name)) {
                        char *type;

                        type = xml_util_get_child_element_content_glib
                                                   (element, "deviceType");
                        if (!type)
                                continue;

                        device_types = g_list_prepend (device_types, type);
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

        device = NULL;

        element = xml_util_get_element (info->priv->element,
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

        services = NULL;

        element = xml_util_get_element (info->priv->element,
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
        GList *service_types;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        service_types = NULL;

        element = xml_util_get_element (info->priv->element,
                                        "serviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("service", (char *) element->name)) {
                        char *type;

                        type = xml_util_get_child_element_content_glib
                                                   (element, "serviceType");
                        if (!type)
                                continue;

                        service_types = g_list_prepend (service_types, type);
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

        service = NULL;

        element = xml_util_get_element (info->priv->element,
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
