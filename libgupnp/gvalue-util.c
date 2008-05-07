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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <stdlib.h>

#include "gvalue-util.h"
#include "xml-util.h"
#include "gupnp-types.h"

gboolean
gvalue_util_set_value_from_string (GValue     *value,
                                   const char *str)
{
        GValue tmp_value = {0, };
        int i;
        long l;
        double d;

        g_return_val_if_fail (str != NULL, FALSE);

        switch (G_VALUE_TYPE (value)) {
        case G_TYPE_STRING:
                g_value_set_string (value, str);

                break;

        case G_TYPE_CHAR:
                g_value_set_char (value, *str);

                break;

        case G_TYPE_UCHAR:
                g_value_set_uchar (value, *str);

                break;

        case G_TYPE_INT:
                i = atoi (str);
                g_value_set_int (value, i);

                break;

        case G_TYPE_UINT:
                i = atoi (str);
                g_value_set_uint (value, (guint) i);

                break;

        case G_TYPE_INT64:
                i = atol (str);
                g_value_set_int64 (value, (gint64) i);

                break;

        case G_TYPE_UINT64:
                i = atol (str);
                g_value_set_uint64 (value, (guint64) i);

                break;

        case G_TYPE_LONG:
                l = atol (str);
                g_value_set_long (value, l);

                break;

        case G_TYPE_ULONG:
                l = atol (str);
                g_value_set_ulong (value, (gulong) l);

                break;

        case G_TYPE_FLOAT:
                d = atof (str);
                g_value_set_float (value, (float) d);

                break;

        case G_TYPE_DOUBLE:
                d = atof (str);
                g_value_set_double (value, d);

                break;

        case G_TYPE_BOOLEAN:
                if (g_ascii_strcasecmp (str, "true") == 0 ||
                    g_ascii_strcasecmp (str, "yes") == 0)
                        g_value_set_boolean (value, TRUE);
                else if (g_ascii_strcasecmp (str, "false") == 0 ||
                         g_ascii_strcasecmp (str, "no") == 0)
                        g_value_set_boolean (value, FALSE);
                else {
                        int i;

                        i = atoi (str);
                        g_value_set_boolean (value, i ? TRUE : FALSE);
                }

                break;

        default:
                /* Try to convert */
                if (g_value_type_transformable (G_TYPE_STRING,
                                                G_VALUE_TYPE (value))) {
                        g_value_init (&tmp_value, G_TYPE_STRING);
                        g_value_set_static_string (&tmp_value, str);

                        g_value_transform (&tmp_value, value);

                        g_value_unset (&tmp_value);

                } else if (g_value_type_transformable (G_TYPE_INT,
                                                       G_VALUE_TYPE (value))) {
                        i = atoi (str);

                        g_value_init (&tmp_value, G_TYPE_INT);
                        g_value_set_int (&tmp_value, i);

                        g_value_transform (&tmp_value, value);

                        g_value_unset (&tmp_value);

                } else {
                        g_warning ("Failed to transform integer "
                                   "value to type %s",
                                   G_VALUE_TYPE_NAME (value));

                        return FALSE;
                }

                break;
        }

        return TRUE;
}

gboolean
gvalue_util_set_value_from_xml_node (GValue  *value,
                                     xmlNode *node)
{
        xmlChar *str;
        int ret;

        str = xmlNodeGetContent (node);

        ret = gvalue_util_set_value_from_string (value, (const char *) str);

        xmlFree (str);

        return ret;
}

gboolean
gvalue_util_value_append_to_xml_string (const GValue *value,
                                        GString      *str)
{
        GValue transformed_value = {0, };
        const char *tmp;

        switch (G_VALUE_TYPE (value)) {
        case G_TYPE_STRING:
                /* Escape and append */
                tmp = g_value_get_string (value);
                if (tmp != NULL)
                        xml_util_add_content (str, tmp);

                return TRUE;

        case G_TYPE_CHAR:
                g_string_append_c (str, g_value_get_char (value));

                return TRUE;

        case G_TYPE_UCHAR:
                g_string_append_c (str, g_value_get_uchar (value));

                return TRUE;

        case G_TYPE_INT:
                g_string_append_printf (str, "%d", g_value_get_int (value));

                return TRUE;

        case G_TYPE_UINT:
                g_string_append_printf (str, "%u", g_value_get_uint (value));

                return TRUE;

        case G_TYPE_INT64:
                g_string_append_printf (str, "%"G_GINT64_FORMAT,
                                        g_value_get_int64 (value));

                return TRUE;

        case G_TYPE_UINT64:
                g_string_append_printf (str, "%"G_GUINT64_FORMAT,
                                        g_value_get_uint64 (value));

                return TRUE;

        case G_TYPE_LONG:
                g_string_append_printf (str, "%ld", g_value_get_long (value));

                return TRUE;

        case G_TYPE_ULONG:
                g_string_append_printf (str, "%lu", g_value_get_ulong (value));

                return TRUE;

        case G_TYPE_FLOAT:
                g_string_append_printf (str, "%f", g_value_get_float (value));

                return TRUE;

        case G_TYPE_DOUBLE:
                g_string_append_printf (str, "%g", g_value_get_double (value));

                return TRUE;

        case G_TYPE_BOOLEAN:
                /* We don't want to convert to "TRUE"/"FALSE" */
                if (g_value_get_boolean (value))
                        g_string_append_c (str, '1');
                else
                        g_string_append_c (str, '0');

                return TRUE;

        default:
                /* Try to convert */
                if (g_value_type_transformable (G_VALUE_TYPE (value),
                                                G_TYPE_STRING)) {
                        g_value_init (&transformed_value, G_TYPE_STRING);

                        g_value_transform (value, &transformed_value);

                        tmp = g_value_get_string (&transformed_value);
                        if (tmp != NULL)
                                xml_util_add_content (str, tmp);

                        g_value_unset (&transformed_value);

                        return TRUE;
                } else {
                        g_warning ("Failed to transform value of type %s "
                                   "to a string", G_VALUE_TYPE_NAME (value));

                        return FALSE;
                }
        }
}
