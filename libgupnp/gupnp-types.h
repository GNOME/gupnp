/*
 * Copyright (C) 2007 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_TYPES_H
#define GUPNP_TYPES_H

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * GUPNP_TYPE_BIN_BASE64:
 *
 * A string type containing Base-64 encoded binary data.
 */
#define GUPNP_TYPE_BIN_BASE64 \
                (gupnp_bin_base64_get_type ())
/**
 * GUPNP_TYPE_BIN_HEX:
 *
 * A string type containing binary hexadecimal encoded binary data.
 */
#define GUPNP_TYPE_BIN_HEX \
                (gupnp_bin_hex_get_type ())
/**
 * GUPNP_TYPE_DATE:
 *
 * A string type representing a date in ISO 8601 format with no time or timezone.
 */
#define GUPNP_TYPE_DATE \
                (gupnp_date_get_type ())
/**
 * GUPNP_TYPE_DATE_TIME:
 *
 * A string type representing a date in ISO 8601 format with optional time but no timezone.
 */
#define GUPNP_TYPE_DATE_TIME \
                (gupnp_date_time_get_type ())
/**
 * GUPNP_TYPE_DATE_TIME_TZ:
 *
 * A string type representing a date in ISO 8601 format with optional time and timezone.
 */
#define GUPNP_TYPE_DATE_TIME_TZ \
                (gupnp_date_time_tz_get_type ())
/**
 * GUPNP_TYPE_TIME:
 *
 * A string type representing a time in ISO 8601 format with no date or timezone.
 */
#define GUPNP_TYPE_TIME \
                (gupnp_time_get_type ())
/**
 * GUPNP_TYPE_TIME_TZ:
 *
 * A string type representing a time in ISO 8601 format with optional timezone and no date.
 */
#define GUPNP_TYPE_TIME_TZ \
                (gupnp_time_tz_get_type ())
/**
 * GUPNP_TYPE_URI:
 *
 * A string type representing an Universal Resource Indentifier.
 */
#define GUPNP_TYPE_URI \
                (gupnp_uri_get_type ())
/**
 * GUPNP_TYPE_UUID:
 *
 * A Universally Unique ID represented as a hexadecimal-encoded string.
 */
#define GUPNP_TYPE_UUID \
                (gupnp_uuid_get_type ())

GType
gupnp_bin_base64_get_type (void) G_GNUC_CONST; /* string */

GType
gupnp_bin_hex_get_type (void) G_GNUC_CONST; /* string */

GType
gupnp_date_get_type (void) G_GNUC_CONST; /* string */

GType
gupnp_date_time_get_type (void) G_GNUC_CONST; /* string */

GType
gupnp_date_time_tz_get_type (void) G_GNUC_CONST; /* string */

GType
gupnp_time_get_type (void) G_GNUC_CONST; /* string */

GType
gupnp_time_tz_get_type (void) G_GNUC_CONST; /* string */

GType
gupnp_uri_get_type (void) G_GNUC_CONST; /* string */

GType
gupnp_uuid_get_type (void) G_GNUC_CONST; /* string */

/**
 * gupnp_value_get_xml_node:
 * @value: a [GLib.Value]
 *
 * Helper macro to get the xmlNode* from a `GValue`
 */
#define gupnp_value_get_xml_node( value ) \
        (xmlNode *) g_value_get_boxed ((value))

/**
 * gupnp_value_get_string:
 * @value: a [GLib.Value]
 *
 * Helper macro to get a char* from a `GValue`
 */
#define gupnp_value_get_string( value ) \
        (const char *) g_value_get_boxed ((value))

G_END_DECLS

#endif /* GUPNP_TYPES_H */
