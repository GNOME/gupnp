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

#ifndef __GUPNP_TYPES_H__
#define __GUPNP_TYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GUPNP_TYPE_BIN_BASE64 \
                (gupnp_bin_base64_get_type ())
#define GUPNP_TYPE_BIN_HEX \
                (gupnp_bin_hex_get_type ())
#define GUPNP_TYPE_DATE \
                (gupnp_date_get_type ())
#define GUPNP_TYPE_DATE_TIME \
                (gupnp_date_time_get_type ())
#define GUPNP_TYPE_DATE_TIME_TZ \
                (gupnp_date_time_tz_get_type ())
#define GUPNP_TYPE_TIME \
                (gupnp_time_get_type ())
#define GUPNP_TYPE_TIME_TZ \
                (gupnp_time_tz_get_type ())
#define GUPNP_TYPE_URI \
                (gupnp_uri_get_type ())
#define GUPNP_TYPE_UUID \
                (gupnp_uuid_get_type ())

GType
gupnp_bin_base64_get_type (void) G_GNUC_CONST;

GType
gupnp_bin_hex_get_type (void) G_GNUC_CONST;

GType
gupnp_date_get_type (void) G_GNUC_CONST;

GType
gupnp_date_time_get_type (void) G_GNUC_CONST;

GType
gupnp_date_time_tz_get_type (void) G_GNUC_CONST;

GType
gupnp_fixed_14_4_get_type (void) G_GNUC_CONST;

GType
gupnp_time_get_type (void) G_GNUC_CONST;

GType
gupnp_time_tz_get_type (void) G_GNUC_CONST;

GType
gupnp_uri_get_type (void) G_GNUC_CONST;

GType
gupnp_uuid_get_type (void) G_GNUC_CONST;

GType
gupnp_data_type_to_gtype (const char *data_type);

G_END_DECLS

#endif /* __GUPNP_TYPES_H__ */
