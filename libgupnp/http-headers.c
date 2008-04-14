/*
 * Copyright (C) 2007, 2008 OpenedHand Ltd.
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

#include <config.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "http-headers.h"

/* Returns the language taken from the current locale, in a format
 * suitable for the HTTP Accept-Language header. */
char *
accept_language_get_header (void)
{
        char *locale, *lang;
        int dash_index;
        GString *tmp;

        locale = setlocale (LC_ALL, NULL);
        if (locale == NULL)
                return NULL;

        if (strcmp (locale, "C") == 0)
                return NULL;

        lang = g_strdup (locale);

        dash_index = http_language_from_locale (lang);

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
 * in an ordered list in UNIX locale format */
GList *
accept_language_get_locales (SoupMessage *message)
{
        const char *header;
        char **bits;
        int i, j;
        GList *locales;

        header = soup_message_headers_get (message->request_headers,
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
                        locale_from_http_language (bits[i]);

                        /* Because bits is sorted in ascending order */
                        locales = g_list_prepend (locales, bits[i]);

                        break;
                }
        }

        g_free (bits);

        return locales;
}

/* Converts @lang from HTTP language tag format into locale format.
 * Return value: The index of the '-' character. */
int
http_language_from_locale (char *lang)
{
        gboolean tolower;
        int i, dash_index;

        tolower = FALSE;
        dash_index = -1;

        for (i = 0; lang[i] != '\0'; i++) {
                switch (lang[i]) {
                case '_':
                        /* Underscores to dashes */
                        lang[i] = '-';

                        /* Lowercase country bit */
                        tolower = TRUE;

                        /* Save dash index */
                        dash_index = i;

                        break;
                case '.':
                case '@':
                        /* Terminate our string here */
                        lang[i] = '\0';

                        return dash_index;
                default:
                        if (tolower)
                                lang[i] = g_ascii_tolower (lang[i]);

                        break;
                }
        }

        return dash_index;
}

/* Converts @lang from locale format into HTTP language tag format.
 * Return value: The index of the '_' character. */
int
locale_from_http_language (char *lang)
{
        gboolean toupper;
        int i, underscore_index;

        toupper = FALSE;
        underscore_index = -1;

        for (i = 0; lang[i] != '\0'; i++) {
                switch (lang[i]) {
                case '-':
                        /* Dashes to underscores */
                        lang[i] = '_';

                        /* Uppercase country bit */
                        toupper = TRUE;

                        /* Save underscore index */
                        underscore_index = i;

                        break;
                case ';':
                        /* Terminate our string here */
                        lang[i] = '\0';

                        return underscore_index;
                default:
                        if (toupper)
                                lang[i] = g_ascii_toupper (lang[i]);

                        break;
                }
        }

        return underscore_index;
}

static char *user_agent = NULL;

static void
free_user_agent (void)
{
        g_free (user_agent);
}

/* Sets the User-Agent header on @message */
void
message_set_user_agent (SoupMessage *message)
{
        if (G_UNLIKELY (user_agent == NULL)) {
                user_agent = g_strdup_printf
                                ("%s GUPnP/" VERSION " DLNADOC/1.50",
                                 g_get_application_name ());

                g_atexit (free_user_agent);
        }

        soup_message_headers_append (message->request_headers,
				     "User-Agent",
                                     user_agent);
}
