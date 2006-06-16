/* 
 * Copyright (C) 2006 OpenedHand Ltd.
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

#include <string.h>

#include "gupnp-device-info.h"
#include "xml-util.h"

typedef enum {
        QUARK_DEVICE_TYPE,
        QUARK_FRIENDLY_NAME,
        QUARK_MANUFACTURER,
        QUARK_MANUFACTURER_URL,
        QUARK_MODEL_DESCRIPTION,
        QUARK_MODEL_NAME,
        QUARK_MODEL_NUMBER,
        QUARK_MODEL_URL,
        QUARK_SERIAL_NUMBER,
        QUARK_UDN,
        QUARK_UPC,
        QUARK_LAST
} Quark;

static GQuark quarks[QUARK_LAST];

static void
gupnp_device_info_base_init (gpointer class)
{
        static gboolean initialized = FALSE;

        if (!initialized) {
                quarks[QUARK_DEVICE_TYPE] =
                        g_quark_from_static_string
                                ("gupnp-device-info-device-type");
                quarks[QUARK_FRIENDLY_NAME] =
                        g_quark_from_static_string
                                ("gupnp-device-info-friendly-name");
                quarks[QUARK_MANUFACTURER] =
                        g_quark_from_static_string
                                ("gupnp-device-info-manufacturer");
                quarks[QUARK_MANUFACTURER_URL] =
                        g_quark_from_static_string
                                ("gupnp-device-info-manufacturer-url");
                quarks[QUARK_MODEL_DESCRIPTION] =
                        g_quark_from_static_string
                                ("gupnp-device-info-model-description");
                quarks[QUARK_MODEL_NAME] =
                        g_quark_from_static_string
                                ("gupnp-device-info-model-name");
                quarks[QUARK_MODEL_NUMBER] =
                        g_quark_from_static_string
                                ("gupnp-device-info-model-number");
                quarks[QUARK_MODEL_URL] =
                        g_quark_from_static_string
                                ("gupnp-device-info-model-url");
                quarks[QUARK_SERIAL_NUMBER] =
                        g_quark_from_static_string
                                ("gupnp-device-info-serial-number");
                quarks[QUARK_UDN] =
                        g_quark_from_static_string
                                ("gupnp-device-info-udn");
                quarks[QUARK_UPC] =
                        g_quark_from_static_string
                                ("gupnp-device-info-upc");
                
                initialized = TRUE;
        }
}

GType
gupnp_device_info_get_type (void)
{
        static GType type = 0;

        if (!type) {
                static const GTypeInfo info = {
                        sizeof (GUPnPDeviceInfoIface),
                	gupnp_device_info_base_init,
                	NULL,
                	NULL,
                	NULL,
                	NULL,
                	0,
                	0,
                	NULL
                };

                type = g_type_register_static (G_TYPE_INTERFACE,
                                               "GUPnPDeviceInfo",
				               &info,
                                               0);
        }

        return type;
}

/**
 * gupnp_device_info_get_location
 * @info: An object implementing the #GUPnPDeviceInfo interface
 *
 * Return value: The location of the device description file.
 **/
const char *
gupnp_device_info_get_location (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoIface *iface;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        iface = GUPNP_DEVICE_INFO_GET_IFACE (info);

        g_return_val_if_fail (iface->get_location, NULL);
        return iface->get_location (info);
}

static const char *
get_property (GUPnPDeviceInfo *info,
              const char      *element_name,
              Quark            quark)
{
        xmlChar *value;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        value = g_object_get_qdata (G_OBJECT (info), quarks[quark]);
        if (!value) {
                GUPnPDeviceInfoIface *iface;
                xmlDoc *doc;
                xmlNode *node;
                
                iface = GUPNP_DEVICE_INFO_GET_IFACE (info);

                g_return_val_if_fail (iface->get_doc, NULL);
                doc = iface->get_doc (info);
        
                node = xml_util_get_element ((xmlNode *) doc,
                                             "root",
                                             "device",
                                             element_name,
                                             NULL);

                if (node)
                        value = xmlNodeGetContent (node);
                else {
                        /* So that g_object_get_qdata() does not return NULL */
                        value = xmlStrdup ((xmlChar *) "");
                }

                g_object_set_qdata_full (G_OBJECT (info),
                                         quarks[quark],
                                         value,
                                         xmlFree);
        }

        return (const char *) value;
}

/**
 * gupnp_device_info_get_device_type
 * @info: An object implementing the #GUPnPDeviceInfo interface
 *
 * Return value: The UPnP device type.
 **/
const char *
gupnp_device_info_get_device_type (GUPnPDeviceInfo *info)
{
        return get_property (info, "deviceType", QUARK_DEVICE_TYPE);
}

/**
 * gupnp_device_info_get_friendly_name
 * @info: An object implementing the #GUPnPDeviceInfo interface
 *
 * Return value: The friendly name.
 **/
const char *
gupnp_device_info_get_friendly_name (GUPnPDeviceInfo *info)
{
        return get_property (info, "friendlyName", QUARK_FRIENDLY_NAME);
}

/**
 * gupnp_device_info_get_manufacturer
 * @info: An object implementing the #GUPnPDeviceInfo interface
 *
 * Return value: The manufacturer.
 **/
const char *
gupnp_device_info_get_manufacturer (GUPnPDeviceInfo *info)
{
        return get_property (info, "manufacturer", QUARK_MANUFACTURER);
}

/**
 * gupnp_device_info_get_manufacturer_url
 * @info: An object implementing the #GUPnPDeviceInfo interface
 *
 * Return value: A URL pointing to the manufacturers website.
 **/
const char *
gupnp_device_info_get_manufacturer_url (GUPnPDeviceInfo *info)
{
        return get_property (info, "manufacturerURL", QUARK_MANUFACTURER_URL);
}

/**
 * gupnp_device_info_get_model_description
 * @info: An object implementing the #GUPnPDeviceInfo interface
 *
 * Return value: The description of the device model.
 **/
const char *
gupnp_device_info_get_model_description (GUPnPDeviceInfo *info)
{
        return get_property (info, "modelDescription", QUARK_MODEL_DESCRIPTION);
}

/**
 * gupnp_device_info_get_model_name
 * @info: An object implementing the #GUPnPDeviceInfo interface
 *
 * Return value: The name of the device model.
 **/
const char *
gupnp_device_info_get_model_name (GUPnPDeviceInfo *info)
{
        return get_property (info, "modelName", QUARK_MODEL_NAME);
}

/**
 * gupnp_device_info_get_model_number
 * @info: An object implementing the #GUPnPDeviceInfo interface
 *
 * Return value: The model number.
 **/
const char *
gupnp_device_info_get_model_number (GUPnPDeviceInfo *info)
{
        return get_property (info, "modelDescription", QUARK_MODEL_DESCRIPTION);
}

/**
 * gupnp_device_info_get_model_url
 * @info: An object implementing the #GUPnPDeviceInfo interface
 *
 * Return value: A URL pointing to the device models website.
 **/
const char *
gupnp_device_info_get_model_url (GUPnPDeviceInfo *info)
{
        return get_property (info, "modelURL", QUARK_MODEL_URL);
}

/**
 * gupnp_device_info_get_serial_number
 * @info: An object implementing the #GUPnPDeviceInfo interface
 *
 * Return value: The serial number.
 **/
const char *
gupnp_device_info_get_serial_number (GUPnPDeviceInfo *info)
{
        return get_property (info, "serialNumber", QUARK_SERIAL_NUMBER);
}

/**
 * gupnp_device_info_get_udn
 * @info: An object implementing the #GUPnPDeviceInfo interface
 *
 * Return value: The UDN.
 **/
const char *
gupnp_device_info_get_udn (GUPnPDeviceInfo *info)
{
        return get_property (info, "UDN", QUARK_UDN);
}

/**
 * gupnp_device_info_get_upc
 * @info: An object implementing the #GUPnPDeviceInfo interface
 *
 * Return value: The UPC.
 **/
const char *
gupnp_device_info_get_upc (GUPnPDeviceInfo *info)
{
        return get_property (info, "UPC", QUARK_UPC);
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
icon_parse (xmlNode *node)
{
        Icon *icon;
        xmlNode *prop;

        icon = g_slice_new0 (Icon);

        prop = xml_util_get_element (node, "mimetype", NULL);
        if (prop)
                icon->mime_type = xmlNodeGetContent (prop);

        prop = xml_util_get_element (node, "width", NULL);
        if (prop)
                icon->width = xml_util_node_get_content_int (prop);
        else
                icon->width = -1;
        
        prop = xml_util_get_element (node, "height", NULL);
        if (prop)
                icon->height = xml_util_node_get_content_int (prop);
        else
                icon->width = -1;
        
        prop = xml_util_get_element (node, "depth", NULL);
        if (prop)
                icon->depth = xml_util_node_get_content_int (prop);
        else
                icon->width = -1;
        
        prop = xml_util_get_element (node, "url", NULL);
        if (prop)
                icon->url = xmlNodeGetContent (prop);

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
 * @info: An object implementing the #GUPnPDeviceInfo interface
 * @requested_mime_type: The requested file format, or NULL for any
 * @requested_depth: The requested color depth, or -1 for any
 * @cequested_width: The requested width, or -1 for any
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
        GUPnPDeviceInfoIface *iface;
        xmlDoc *doc;
        xmlNode *node;
        GList *icons, *l;
        Icon *icon, *closest;
        char *ret;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);
                
        iface = GUPNP_DEVICE_INFO_GET_IFACE (info);

        g_return_val_if_fail (iface->get_doc, NULL);
        doc = iface->get_doc (info);

        /* List available icons */
        icons = NULL;

        node = xml_util_get_element ((xmlNode *) doc,
                                     "root",
                                     "device",
                                     "iconList",
                                     NULL);

        for (node = node->children; node; node = node->next) {
                if (!strcmp ("icon", (char *) node->name)) {
                        gboolean mime_type_ok;

                        icon = icon_parse (node);

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
