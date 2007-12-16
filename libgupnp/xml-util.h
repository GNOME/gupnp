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

/* GObject wrapper for xmlDoc, so that we can use refcounting and
 * weak references. */
GType
xml_doc_wrapper_get_type (void) G_GNUC_CONST;

#define TYPE_XML_DOC_WRAPPER \
                (xml_doc_wrapper_get_type ())
#define XML_DOC_WRAPPER(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 TYPE_XML_DOC_WRAPPER, \
                 XmlDocWrapper))
#define IS_XML_DOC_WRAPPER(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 TYPE_XML_DOC_WRAPPER))

typedef struct {
        GInitiallyUnowned parent;

        xmlDoc *doc;
} XmlDocWrapper;

typedef struct {
        GInitiallyUnownedClass parent_class;
} XmlDocWrapperClass;

XmlDocWrapper *
xml_doc_wrapper_new (xmlDoc *doc);

/* Misc utilities for inspecting xmlNodes */
xmlNode *
xml_util_get_element                    (xmlNode    *node,
                                         ...) G_GNUC_NULL_TERMINATED;

xmlChar *
xml_util_get_child_element_content      (xmlNode    *node,
                                         const char *child_name);

int
xml_util_get_child_element_content_int  (xmlNode    *node,
                                         const char *child_name);

char *
xml_util_get_child_element_content_glib (xmlNode    *node,
                                         const char *child_name);

SoupUri *
xml_util_get_child_element_content_uri  (xmlNode    *node,
                                         const char *child_name,
                                         SoupUri    *base);

char *
xml_util_get_child_element_content_url  (xmlNode    *node,
                                         const char *child_name,
                                         SoupUri    *base);
xmlChar *
xml_util_get_attribute_contents         (xmlNode    *node,
                                         const char *attribute_name);

/* XML string creation helpers */
GString *
xml_util_new_string                     (void);

void
xml_util_start_element                  (GString    *xml_str,
                                         const char *element_name);

void
xml_util_end_element                    (GString    *xml_str,
                                         const char *element_name);

void
xml_util_add_content                    (GString    *xml_str,
                                         const char *content);

#endif /* __XML_UTIL_H__ */
