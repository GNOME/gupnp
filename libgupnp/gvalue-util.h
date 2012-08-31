/*
 * Copyright (C) 2007 OpenedHand Ltd.
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GVALUE_UTIL_H__
#define __GVALUE_UTIL_H__

#include <glib-object.h>
#include <libxml/tree.h>

G_GNUC_INTERNAL gboolean
gvalue_util_set_value_from_string      (GValue       *value,
                                        const char   *str);

G_GNUC_INTERNAL gboolean
gvalue_util_set_value_from_xml_node    (GValue       *value,
                                        xmlNode      *node);

G_GNUC_INTERNAL gboolean
gvalue_util_value_append_to_xml_string (const GValue *value,
                                        GString      *str);

#endif /* __GVALUE_UTIL_H__ */
