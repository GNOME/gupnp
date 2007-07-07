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

#include <string.h>

#include "xml-util.h"

xmlNode *
xml_util_get_element (xmlNode *node,
                      ...)
{
        va_list var_args;

        va_start (var_args, node);

        while (TRUE) {
                const char *arg;

                arg = va_arg (var_args, const char *);
                if (!arg)
                        break;

                for (node = node->children; node; node = node->next)
                        if (!strcmp (arg, (char *) node->name))
                                break;

                if (!node)
                        break;
        }
        
        va_end (var_args);

        return node;
}

gboolean
xml_util_node_get_content_value (xmlNode *node,
                                 GValue  *value)
{
        xmlChar *content;
        gboolean success;
        GValue tmp_value = {0, };
        int i;

        content = xmlNodeGetContent (node);
        success = TRUE;

        switch (G_VALUE_TYPE (value)) {
        case G_TYPE_STRING:
                g_value_set_string (value, (char *) content);
                break;
        case G_TYPE_INT:
                i = atoi ((char *) content);

                g_value_set_int (value, i);

                break;
        default:
                /* Try to convert */
                if (g_value_type_transformable (G_TYPE_STRING,
                                                G_VALUE_TYPE (value))) {
                        g_value_init (&tmp_value, G_TYPE_STRING);
                        g_value_set_static_string (&tmp_value,
                                                   (char *) content);

                        g_value_transform (&tmp_value, value);

                        g_value_unset (&tmp_value);

                } else if (g_value_type_transformable (G_TYPE_INT,
                                                       G_VALUE_TYPE (value))) {
                        i = atoi ((char *) content);

                        g_value_init (&tmp_value, G_TYPE_INT);
                        g_value_set_int (&tmp_value, i);

                        g_value_transform (&tmp_value, value);

                        g_value_unset (&tmp_value);

                } else {
                        g_warning ("Failed to transform integer "
                                   "value to type %s",
                                   G_VALUE_TYPE_NAME (value));

                        success = FALSE;
                }

                break;
        }

        xmlFree (content);

        return success;
}

void
xml_util_set_child_element_content (xmlNode    *node,
                                    const char *child_name,
                                    const char *content)
{
        xmlNode *child_node;

        child_node = xml_util_get_element (node,
                                           child_name,
                                           NULL);

        if (!child_node) {
                child_node = xmlNewNode (NULL, (xmlChar *) child_name);

                xmlAddChild (node, child_node);
        }

        xmlNodeSetContent (child_node, (xmlChar *) content);
}

xmlChar *
xml_util_get_child_element_content (xmlNode    *node,
                                    const char *child_name)
{
        xmlNode *child_node;

        child_node = xml_util_get_element (node,
                                           child_name,
                                           NULL);
        if (!child_node)
                return NULL;

        return xmlNodeGetContent (child_node);
}

int
xml_util_get_child_element_content_int (xmlNode    *node,
                                        const char *child_name)
{
        xmlChar *content;
        int i;

        content = xml_util_get_child_element_content (node, child_name);
        if (!content)
                return -1;

        i = atoi ((char *) content);

        xmlFree (content);

        return i;
}

char *
xml_util_get_child_element_content_glib (xmlNode    *node,
                                         const char *child_name)
{
        xmlChar *content;
        char *copy;

        content = xml_util_get_child_element_content (node, child_name);
        if (!content)
                return NULL;

        copy = g_strdup ((char *) content);

        xmlFree (content);

        return copy;
}

SoupUri *
xml_util_get_child_element_content_uri (xmlNode    *node,
                                        const char *child_name,
                                        SoupUri    *base)
{
        xmlChar *content;
        SoupUri *uri;

        content = xml_util_get_child_element_content (node, child_name);
        if (!content)
                return NULL;

        uri = soup_uri_new_with_base (base, (const char *) content);

        xmlFree (content);

        return uri;
}

char *
xml_util_get_child_element_content_url (xmlNode    *node,
                                        const char *child_name,
                                        SoupUri    *base)
{
        SoupUri *uri;
        char *url;

        uri = xml_util_get_child_element_content_uri (node, child_name, base);
        if (!uri)
                return NULL;

        url = soup_uri_to_string (uri, FALSE);

        soup_uri_free (uri);

        return url;
}

xmlChar *
xml_util_get_attribute_contents (xmlNode    *node,
                                 const char *attribute_name)
{
        xmlAttr *attribute;

        for (attribute = node->properties;
             attribute;
             attribute = attribute->next) {
                if (strcmp (attribute_name, (char *) attribute->name) == 0)
                        break;
        }

        if (attribute)
                return xmlNodeGetContent (attribute->children);
        else
                return NULL;
}

