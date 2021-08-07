/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include <config.h>
#include <string.h>

#include "gvalue-util.h"
#include "xml-util.h"

/* libxml DOM interface helpers */
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
                        if (!g_strcmp0 (arg, (char *) node->name))
                                break;

                if (!node)
                        break;
        }

        va_end (var_args);

        return node;
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

GUri *
xml_util_get_child_element_content_uri (xmlNode *node,
                                        const char *child_name,
                                        GUri *base)
{
        xmlChar *content;
        GUri *uri;

        content = xml_util_get_child_element_content (node, child_name);
        if (!content)
                return NULL;

        if (base != NULL)
                uri = g_uri_parse_relative (base,
                                            (const char *) content,
                                            G_URI_FLAGS_NONE,
                                            NULL);
        else
                uri = g_uri_parse ((const char *) content,
                                   G_URI_FLAGS_NONE,
                                   NULL);

        xmlFree (content);

        return uri;
}

char *
xml_util_get_child_element_content_url (xmlNode *node,
                                        const char *child_name,
                                        GUri *base)
{
        GUri *uri;
        char *url;

        uri = xml_util_get_child_element_content_uri (node, child_name, base);
        if (!uri)
                return NULL;

        url = g_uri_to_string_partial (uri, G_URI_HIDE_PASSWORD);
        g_uri_unref (uri);

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

/**
 * xml_util_real_node:
 * @node: a %xmlNodePtr
 *
 * Finds the first "real" node (ie, not a comment or whitespace) at or
 * after @node at its level in the tree.
 *
 * Return: a node, or %NULL
 *
 * (Taken from libsoup)
 **/
xmlNode *
xml_util_real_node (xmlNode *node)
{
        while (node &&
               (node->type == XML_COMMENT_NODE || xmlIsBlankNode (node)))
                node = node->next;
        return node;
}

/* XML string creation helpers */

#define INITIAL_XML_STR_SIZE 100 /* Initial xml string size in bytes */

GString *
xml_util_new_string (void)
{
        return g_string_sized_new (INITIAL_XML_STR_SIZE);
}

void
xml_util_start_element (GString    *xml_str,
                        const char *element_name)
{
        g_string_append_c (xml_str, '<');
        g_string_append (xml_str, element_name);
        g_string_append_c (xml_str, '>');
}

void
xml_util_end_element (GString    *xml_str,
                      const char *element_name)
{
        g_string_append (xml_str, "</");
        g_string_append (xml_str, element_name);
        g_string_append_c (xml_str, '>');
}

void
xml_util_add_content (GString    *xml_str,
                      const char *content)
{
        /* Modified from GLib gmarkup.c */
        const gchar *p;

        p = content;

        while (*p) {
                const gchar *next;
                next = g_utf8_next_char (p);

                switch (*p) {
                case '&':
                        g_string_append (xml_str, "&amp;");
                        break;

                case '<':
                        g_string_append (xml_str, "&lt;");
                        break;

                case '>':
                        g_string_append (xml_str, "&gt;");
                        break;

                case '"':
                        g_string_append (xml_str, "&quot;");
                        break;

                default:
                        g_string_append_len (xml_str, p, next - p);
                        break;
                }

                p = next;
        }
}
