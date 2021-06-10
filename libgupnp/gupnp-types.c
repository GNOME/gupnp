/*
 * Copyright (C) 2007 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

/**
 * SECTION:gupnp-types
 * @short_description: Extra types for use when calling UPnP actions.
 *
 * These GTypes are used to marshal to and from string data to particular UPnP
 * types when invoking actions on a #GUPnPServiceProxy.
 */

#include <config.h>
#include <string.h>

#include "gupnp-types.h"
#include "gupnp-types-private.h"

static void
gupnp_string_type_to_string (const GValue *src_value,
                             GValue       *dest_value)
{
        const char *str;

        str = gupnp_value_get_string (src_value);

        g_value_set_string (dest_value, str);
}

static void
gupnp_string_to_string_type (const GValue *src_value,
                             GValue       *dest_value)
{
        const char *str;

        str = g_value_get_string (src_value);

        g_value_set_boxed (dest_value, str);
}

static GType
register_string_type (const char *name)
{
        GType type;

        type = g_boxed_type_register_static (
                                g_intern_static_string (name),
                                (GBoxedCopyFunc) g_strdup,
                                (GBoxedFreeFunc) g_free);
        g_value_register_transform_func (
                        type,
                        G_TYPE_STRING,
                        gupnp_string_type_to_string);
        g_value_register_transform_func (
                        G_TYPE_STRING,
                        type,
                        gupnp_string_to_string_type);

        return type;
}

GType
gupnp_bin_base64_get_type (void)
{
        static GType type = 0;

        if (!type)
                type = register_string_type ("GUPnPBinBase64");

        return type;
}

GType
gupnp_bin_hex_get_type (void)
{
        static GType type = 0;

        if (!type)
                type = register_string_type ("GUPnPBinHex");

        return type;
}

GType
gupnp_date_get_type (void)
{
        static GType type = 0;

        if (!type)
                type = register_string_type ("GUPnPDate");

        return type;
}

GType
gupnp_date_time_get_type (void)
{
        static GType type = 0;

        if (!type)
                type = register_string_type ("GUPnPDateTime");

        return type;
}

GType
gupnp_date_time_tz_get_type (void)
{
        static GType type = 0;

        if (!type)
                type = register_string_type ("GUPnPDateTimeTZ");

        return type;
}

GType
gupnp_time_get_type (void)
{
        static GType type = 0;

        if (!type)
                type = register_string_type ("GUPnPTime");

        return type;
}

GType
gupnp_time_tz_get_type (void)
{
        static GType type = 0;

        if (!type)
                type = register_string_type ("GUPnPTimeTZ");

        return type;
}

GType
gupnp_uri_get_type (void)
{
        static GType type = 0;

        if (!type)
                type = register_string_type ("GUPnPURI");

        return type;
}

GType
gupnp_uuid_get_type (void)
{
        static GType type = 0;

        if (!type)
                type = register_string_type ("GUPnPUUID");

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
                return GUPNP_TYPE_BIN_HEX;
        else
                return G_TYPE_INVALID;
}
