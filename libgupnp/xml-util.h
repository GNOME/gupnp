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
#include <libsoup/soup-uri.h>
#include <stdarg.h>
#include <glib-object.h>

/* Misc utilities for inspecting xmlNodes */
G_GNUC_INTERNAL xmlNode *
xml_util_get_element                    (xmlNode    *node,
                                         ...) G_GNUC_NULL_TERMINATED;

G_GNUC_INTERNAL xmlChar *
xml_util_get_child_element_content      (xmlNode    *node,
                                         const char *child_name);

G_GNUC_INTERNAL int
xml_util_get_child_element_content_int  (xmlNode    *node,
                                         const char *child_name);

G_GNUC_INTERNAL char *
xml_util_get_child_element_content_glib (xmlNode    *node,
                                         const char *child_name);

G_GNUC_INTERNAL SoupURI *
xml_util_get_child_element_content_uri  (xmlNode    *node,
                                         const char *child_name,
                                         SoupURI    *base);

G_GNUC_INTERNAL char *
xml_util_get_child_element_content_url  (xmlNode    *node,
                                         const char *child_name,
                                         SoupURI    *base);
G_GNUC_INTERNAL xmlChar *
xml_util_get_attribute_contents         (xmlNode    *node,
                                         const char *attribute_name);

G_GNUC_INTERNAL xmlNode *
xml_util_real_node                      (xmlNode    *node);

/* XML string creation helpers */
G_GNUC_INTERNAL GString *
xml_util_new_string                     (void);

G_GNUC_INTERNAL void
xml_util_start_element                  (GString    *xml_str,
                                         const char *element_name);

G_GNUC_INTERNAL void
xml_util_end_element                    (GString    *xml_str,
                                         const char *element_name);

G_GNUC_INTERNAL void
xml_util_add_content                    (GString    *xml_str,
                                         const char *content);

#endif /* __XML_UTIL_H__ */
