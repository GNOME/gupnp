/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_XML_UTIL_H
#define GUPNP_XML_UTIL_H

#include <libxml/tree.h>
#include <libxml/parser.h>

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

G_GNUC_INTERNAL GUri *
xml_util_get_child_element_content_uri (xmlNode *node,
                                        const char *child_name,
                                        GUri *base);

G_GNUC_INTERNAL char *
xml_util_get_child_element_content_url (xmlNode *node,
                                        const char *child_name,
                                        GUri *base);
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

#endif /* GUPNP_XML_UTIL_H */
