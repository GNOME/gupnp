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

#ifndef __XML_UTIL_H__
#define __XML_UTIL_H__

#include <libxml/tree.h>
#include <stdarg.h>
#include <glib-object.h>

xmlNode *
xml_util_get_element               (xmlNode    *node,
                                    ...) G_GNUC_NULL_TERMINATED;

int
xml_util_node_get_content_int      (xmlNode    *node);

gboolean
xml_util_node_get_content_value    (xmlNode    *node,
                                    GValue     *value);

void
xml_util_set_child_element_content (xmlNode    *node,
                                    const char *child_name,
                                    const char *content);

xmlChar *
xml_util_get_child_element_content (xmlNode    *node,
                                    const char *child_name);

#endif /* __XML_UTIL_H__ */
