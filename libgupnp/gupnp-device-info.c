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

#include "gupnp-device-info.h"

GType
gupnp_device_info_type (void)
{
        static GType type = 0;

        if (!type) {
                static const GTypeInfo info = {
                        sizeof (GUPnPDeviceInfoIface),
                	NULL,
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

/**
 * gupnp_device_info_get_type
 * @info: An object implementing the #GUPnPDeviceInfo interface
 *
 * Return value: The device type.
 **/
const char *
gupnp_device_info_get_type (GUPnPDeviceInfo *info)
{
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return NULL;
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
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return NULL;
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
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return NULL;
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
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return NULL;
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
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return NULL;
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
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return NULL;
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
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return NULL;
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
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return NULL;
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
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return NULL;
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
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return NULL;
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
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return NULL;
}

/**
 * gupnp_device_info_get_icon_url
 * @info: An object implementing the #GUPnPDeviceInfo interface
 * @requested_format: The requested file format
 * @requested_depth: The requested color depth
 * @cequested_width: The requested width
 * @requested_height: The requested height
 * @prefer_bigger: TRUE if a bigger, rather than a smaller icon should be
 * returned if no exact match could be found
 * @format: The location where to store the the format of the returned icon,
 * or NULL. The returned string should be freed after use
 * @depth: The location where to store the depth of the returned icon, or NULL
 * @width: The location where to store the width of the returned icon, or NULL
 * @height::The location where to store the height of the returned icon, or NULL
 *
 * Return value: A URL pointing to the icon most closely matching the
 * given criteria.
 **/
const char *
gupnp_device_info_get_icon_url (GUPnPDeviceInfo *info,
                                const char      *requested_format,
                                int              requested_depth,
                                int              requested_width,
                                int              requested_height,
                                gboolean         prefer_bigger,
                                char           **format,
                                int             *depth,
                                int             *weight,
                                int             *height)
{
        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        return NULL;
}
