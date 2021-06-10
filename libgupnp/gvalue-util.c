/*
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include <config.h>
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

        g_return_val_if_fail (str != NULL, FALSE);

        switch (G_VALUE_TYPE (value)) {
        case G_TYPE_STRING:
                g_value_set_string (value, str);

                break;

        case G_TYPE_CHAR:
                g_value_set_schar (value, *str);

                break;

        case G_TYPE_UCHAR:
                g_value_set_uchar (value, *str);

                break;

        case G_TYPE_INT:
                g_value_set_int (value, strtol (str, NULL, 10));

                break;

        case G_TYPE_UINT:
                g_value_set_uint (value, strtoul (str, NULL, 10));

                break;

        case G_TYPE_INT64:
                g_value_set_int64 (value, g_ascii_strtoll (str, NULL, 10));

                break;

        case G_TYPE_UINT64:
                g_value_set_uint64 (value, g_ascii_strtoull (str, NULL, 10));

                break;

        case G_TYPE_LONG:
                g_value_set_long (value, strtol (str, NULL, 10));

                break;

        case G_TYPE_ULONG:
                g_value_set_ulong (value, strtoul (str, NULL, 10));

                break;

        case G_TYPE_FLOAT:
                g_value_set_float (value, g_ascii_strtod (str, NULL));

                break;

        case G_TYPE_DOUBLE:
                g_value_set_double (value, g_ascii_strtod (str, NULL));

                break;

        case G_TYPE_BOOLEAN:
                if (g_ascii_strcasecmp (str, "true") == 0 ||
                    g_ascii_strcasecmp (str, "yes") == 0)
                        g_value_set_boolean (value, TRUE);
                else if (g_ascii_strcasecmp (str, "false") == 0 ||
                         g_ascii_strcasecmp (str, "no") == 0)
                        g_value_set_boolean (value, FALSE);
                else {
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
        char buf[G_ASCII_DTOSTR_BUF_SIZE];

        switch (G_VALUE_TYPE (value)) {
        case G_TYPE_STRING:
                /* Escape and append */
                tmp = g_value_get_string (value);
                if (tmp != NULL)
                        xml_util_add_content (str, tmp);

                return TRUE;

        case G_TYPE_CHAR:
                g_string_append_c (str, g_value_get_schar (value));

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
                g_string_append (str,
                                 g_ascii_dtostr (buf,
                                                 sizeof (buf),
                                                 g_value_get_float (value)));

                return TRUE;

        case G_TYPE_DOUBLE:
                g_string_append (str,
                                 g_ascii_dtostr (buf,
                                                 sizeof (buf),
                                                 g_value_get_double (value)));

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

/* GDestroyNotify for GHashTable holding GValues.
 */
void
gvalue_free (gpointer data)
{
  GValue *value = (GValue *) data;

  g_value_unset (value);
  g_free (value);
}
