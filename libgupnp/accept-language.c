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

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "accept-language.h"

/* Returns the language taken from the current locale, in a format
 * suitable for the HTTP Accept-Language header. */
char *
accept_language_get_current (void)
{
        char *locale, *lang;
        int i, dash_index;
        GString *tmp;
        
        locale = setlocale (LC_ALL, NULL);
        if (locale == NULL)
                return NULL;

        if (strcmp (locale, "C") == 0)
                return NULL;

        lang = g_strdup (locale);

        i = dash_index = 0;
        while (lang[i] != '\0') {
                switch (lang[i]) {
                case '_':
                        dash_index = i;
                        lang[i] = '-';
                        break;
                case '.':
                case '@':
                        lang[i] = '\0';
                        break;
                default:
                        lang[i] = g_ascii_tolower (lang[i]);
                        break;
                }

                if (lang[i] == '\0')
                        break;

                i++;
        }

        tmp = g_string_new (lang);
        g_string_append (tmp, ";q=1");
                
        /* Append preference for basic (non-country specific) language version
         * if applicable */
        if (dash_index > 0) {
                g_string_append (tmp, ", ");

                lang[dash_index] = '\0';
                g_string_append (tmp, lang);
                g_string_append (tmp, ";q=0.5");
        }

        g_free (lang);

        return g_string_free (tmp, FALSE);
}

static double
get_quality (char *val)
{
        val = strstr (val, ";q=");
        if (!val)
                return 1;

        val += strlen (";q=");
        return atof (val);
}

/* Parses the Accept-Language header in @message, and returns its values
 * in an ordered list in locale format */
GList *
accept_language_get (SoupMessage *message)
{
        const char *header;
        char **bits;
        int i, j;
        gboolean toupper;
        GList *locales;
        
        header = soup_message_get_header (message->request_headers,
                                          "Accept-Language");
        if (header == NULL)
                return NULL;

        locales = NULL;

        bits = g_strsplit (header, ",", -1);
        
        /* Sort by quality using insertion sort */
        bits[0] = g_strstrip (bits[0]);
        for (i = 1; bits[i] != NULL; i++) {
                char *value;
                double value_q;

                value = g_strstrip (bits[i]);
                value_q = get_quality (value);

                /* INSERT (bits, i, value) */
                j = i - 1;
                while (j >= 0 && get_quality (bits[j]) > value_q) {
                        bits[j + 1] = bits[j];
                        j--;
                }
                bits[j + 1] = value;
        }

        /* Transform to list */
        for (i = 0; bits[i] != NULL; i++) {
                switch (bits[i][0]) {
                case '\0':
                        /* Empty */
                case '*':
                        /* Wildcard: ignore */
                        g_free (bits[i]);

                        break;
                default:
                        toupper = FALSE;

                        for (j = 0; bits[i][j] != '\0'; j++) {
                                switch (bits[i][j]) {
                                case '-':
                                        /* Dashes to underscores */
                                        bits[i][j] = '_';

                                        /* Uppercase country bit */
                                        toupper = TRUE;

                                        break;
                                case ';':
                                        /* Terminate our string here */
                                        bits[i][j] = '\0';

                                        break;
                                default:
                                        if (toupper)
                                                bits[i][j] = g_ascii_toupper
                                                                (bits[i][j]);

                                        break;
                                }
                        }

                        /* Because bits is sorted in ascending order */
                        locales = g_list_prepend (locales, bits[i]);

                        break;
                }
        }
        g_free (bits);

        return locales;
}
