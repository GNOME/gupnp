/*
 * Copyright (C) 2007 Zeeshan Ali <zeenix@gstreamer.net>
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali <zeenix@gstreamer.net>
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

#include "gupnp-types.h"

static void
gupnp_type_to_string (const GValue *src_value,
                      GValue       *dest_value)
{
        const char *str;

        str = gupnp_value_get_string (src_value);

        g_value_set_string (dest_value, str);
}

static void
gupnp_string_to_type (const GValue *src_value,
                      GValue       *dest_value)
{
        const char *str;

        str = g_value_get_string (src_value);

        g_value_set_boxed (dest_value, str);
}

GType
gupnp_xml_chunk_get_type (void)
{
        static GType type = 0;

        if (!type) {
                type = g_boxed_type_register_static (
                                g_intern_static_string ("GUPnPXMLChunk"),
                                (GBoxedCopyFunc) g_strdup,
                                (GBoxedFreeFunc) g_free);
                g_value_register_transform_func (
                                type,
                                G_TYPE_STRING,
                                gupnp_type_to_string);
                g_value_register_transform_func (
                                G_TYPE_STRING,
                                type,
                                gupnp_string_to_type);
        }

        return type;
}

GType
gupnp_bin_base64_get_type (void)
{
        static GType type = 0;

        if (!type) {
                type = g_boxed_type_register_static (
                                g_intern_static_string ("GUPnPBinBase64"),
                                (GBoxedCopyFunc) g_strdup,
                                (GBoxedFreeFunc) g_free);
                g_value_register_transform_func (
                                type,
                                G_TYPE_STRING,
                                gupnp_type_to_string);
                g_value_register_transform_func (
                                G_TYPE_STRING,
                                type,
                                gupnp_string_to_type);
        }

        return type;
}

GType
gupnp_bin_hex_get_type (void)
{
        static GType type = 0;

        if (!type) {
                type = g_boxed_type_register_static (
                                g_intern_static_string ("GUPnPBinHex"),
                                (GBoxedCopyFunc) g_strdup,
                                (GBoxedFreeFunc) g_free);
                g_value_register_transform_func (
                                type,
                                G_TYPE_STRING,
                                gupnp_type_to_string);
                g_value_register_transform_func (
                                G_TYPE_STRING,
                                type,
                                gupnp_string_to_type);
        }

        return type;
}

GType
gupnp_date_get_type (void)
{
        static GType type = 0;

        if (!type) {
                type = g_boxed_type_register_static (
                                g_intern_static_string ("GUPnPDate"),
                                (GBoxedCopyFunc) g_strdup,
                                (GBoxedFreeFunc) g_free);
                g_value_register_transform_func (
                                type,
                                G_TYPE_STRING,
                                gupnp_type_to_string);
                g_value_register_transform_func (
                                G_TYPE_STRING,
                                type,
                                gupnp_string_to_type);
        }

        return type;
}

GType
gupnp_date_time_get_type (void)
{
        static GType type = 0;

        if (!type) {
                type = g_boxed_type_register_static (
                                g_intern_static_string ("GUPnPDateTime"),
                                (GBoxedCopyFunc) g_strdup,
                                (GBoxedFreeFunc) g_free);
                g_value_register_transform_func (
                                type,
                                G_TYPE_STRING,
                                gupnp_type_to_string);
                g_value_register_transform_func (
                                G_TYPE_STRING,
                                type,
                                gupnp_string_to_type);
        }

        return type;
}

GType
gupnp_date_time_tz_get_type (void)
{
        static GType type = 0;

        if (!type) {
                type = g_boxed_type_register_static (
                                g_intern_static_string ("GUPnPDateTimeTZ"),
                                (GBoxedCopyFunc) g_strdup,
                                (GBoxedFreeFunc) g_free);
                g_value_register_transform_func (
                                type,
                                G_TYPE_STRING,
                                gupnp_type_to_string);
                g_value_register_transform_func (
                                G_TYPE_STRING,
                                type,
                                gupnp_string_to_type);
        }

        return type;
}

GType
gupnp_time_get_type (void)
{
        static GType type = 0;

        if (!type) {
                type = g_boxed_type_register_static (
                                g_intern_static_string ("GUPnPTime"),
                                (GBoxedCopyFunc) g_strdup,
                                (GBoxedFreeFunc) g_free);
                g_value_register_transform_func (
                                type,
                                G_TYPE_STRING,
                                gupnp_type_to_string);
                g_value_register_transform_func (
                                G_TYPE_STRING,
                                type,
                                gupnp_string_to_type);
        }

        return type;
}

GType
gupnp_time_tz_get_type (void)
{
        static GType type = 0;

        if (!type) {
                type = g_boxed_type_register_static (
                                g_intern_static_string ("GUPnPTimeTZ"),
                                (GBoxedCopyFunc) g_strdup,
                                (GBoxedFreeFunc) g_free);
                g_value_register_transform_func (
                                type,
                                G_TYPE_STRING,
                                gupnp_type_to_string);
                g_value_register_transform_func (
                                G_TYPE_STRING,
                                type,
                                gupnp_string_to_type);
        }

        return type;
}

GType
gupnp_uri_get_type (void)
{
        static GType type = 0;

        if (!type) {
                type = g_boxed_type_register_static (
                                g_intern_static_string ("GUPnPURI"),
                                (GBoxedCopyFunc) g_strdup,
                                (GBoxedFreeFunc) g_free);
                g_value_register_transform_func (
                                type,
                                G_TYPE_STRING,
                                gupnp_type_to_string);
                g_value_register_transform_func (
                                G_TYPE_STRING,
                                type,
                                gupnp_string_to_type);
        }

        return type;
}

GType
gupnp_uuid_get_type (void)
{
        static GType type = 0;

        if (!type) {
                type = g_boxed_type_register_static (
                                g_intern_static_string ("GUPnPUUID"),
                                (GBoxedCopyFunc) g_strdup,
                                (GBoxedFreeFunc) g_free);
                g_value_register_transform_func (
                                type,
                                G_TYPE_STRING,
                                gupnp_type_to_string);
                g_value_register_transform_func (
                                G_TYPE_STRING,
                                type,
                                gupnp_string_to_type);
        }

        return type;
}

GType
gupnp_data_type_to_gtype (const char *data_type)
{
        if (g_ascii_strcasecmp ("UUID", data_type) == 0)
                return GUPNP_TYPE_UUID;
        else if (g_ascii_strcasecmp ("URI", data_type) == 0)
                return GUPNP_TYPE_URI;
        else if (g_ascii_strcasecmp ("time.tz", data_type) == 0)
                return GUPNP_TYPE_TIME_TZ;
        else if (g_ascii_strcasecmp ("dateTime.tz", data_type) == 0)
                return GUPNP_TYPE_DATE_TIME_TZ;
        else if (g_ascii_strcasecmp ("dateTime", data_type) == 0)
                return GUPNP_TYPE_DATE_TIME;
        else if (g_ascii_strcasecmp ("date", data_type) == 0)
                return GUPNP_TYPE_DATE;
        else if (g_ascii_strcasecmp ("time", data_type) == 0)
                return GUPNP_TYPE_TIME;
        else if (g_ascii_strcasecmp ("bin.base64", data_type) == 0)
                return GUPNP_TYPE_BIN_BASE64;
        else if (g_ascii_strcasecmp ("bin.hex", data_type) == 0)
                return GUPNP_TYPE_BIN_BASE64;
        else
                return G_TYPE_INVALID;
}
