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

#include "gvalue-util.h"
#include "xml-util.h"

G_DEFINE_TYPE (XmlDocWrapper,
               xml_doc_wrapper,
               G_TYPE_INITIALLY_UNOWNED);

static void
xml_doc_wrapper_init (XmlDocWrapper *wrapper)
{
        /* Empty */
}

static void
xml_doc_wrapper_finalize (GObject *object)
{
        XmlDocWrapper *wrapper;

        wrapper = XML_DOC_WRAPPER (object);

        xmlFreeDoc (wrapper->doc);
}

static void
xml_doc_wrapper_class_init (XmlDocWrapperClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = xml_doc_wrapper_finalize;
}

/* Takes ownership of @doc */
XmlDocWrapper *
xml_doc_wrapper_new (xmlDoc *doc)
{
        XmlDocWrapper *wrapper;

        g_return_val_if_fail (doc != NULL, NULL);

        wrapper = g_object_new (TYPE_XML_DOC_WRAPPER, NULL);

        wrapper->doc = doc;

        return wrapper;
}

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

        content = xmlNodeGetContent (node);

        success = gvalue_util_set_value_from_string (value, (char *) content);

        xmlFree (content);

        return success;
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

