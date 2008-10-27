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
#include <gio/gio.h>

#include "http-headers.h"

/* Converts @lang from HTTP language tag format into locale format.
 * Return value: The index of the '-' character. */
static int
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
static int
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

/* Parses the HTTP Range header on @message and sets:
 *
 * @have_range to %TRUE if a range was specified,
 * @offset to the requested offset (left unchanged if none specified),
 * @length to the requested length (left unchanged if none specified).
 *
 * Both @offset and @length are expected to be initialised to their default
 * values.
 *
 * Returns %TRUE on success. */
gboolean
http_request_get_range (SoupMessage *message,
                        gboolean    *have_range,
                        gsize       *offset,
                        gsize       *length)
{
        const char *header;
        char **v;

        header = soup_message_headers_get (message->request_headers, "Range");
        if (header == NULL) {
                *have_range = FALSE;

                return TRUE;
        }

        /* We have a Range header. Parse. */
        if (strncmp (header, "bytes=", 6) != 0)
                return FALSE;

        header += 6;

        v = g_strsplit (header, "-", 2);

        /* Get first byte position */
        if (v[0] != NULL && *v[0] != 0)
                *offset = atoll (v[0]);

        else {
                /* We don't support ranges without first byte position */
                g_strfreev (v);

                return FALSE;
        }

        /* Get last byte position if specified */
        if (v[1] != NULL && *v[1] != 0)
                *length = atoll (v[1]) - *offset;
        else
                *length = *length - *offset;

        *have_range = TRUE;

        /* Cleanup */
        g_strfreev (v);

        return TRUE;
}

/* Sets the Accept-Language on @message with the language taken from the
 * current locale. */
void
http_request_set_accept_language (SoupMessage *message)
{
        char *locale, *lang;
        int dash_index;
        GString *tmp;

        locale = setlocale (LC_ALL, NULL);
        if (locale == NULL)
                return;

        if (strcmp (locale, "C") == 0)
                return;

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

        soup_message_headers_append (message->request_headers,
				     "Accept-Language",
                                     tmp->str);

        g_string_free (tmp, TRUE);
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
http_request_get_accept_locales (SoupMessage *message)
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

static char *user_agent = NULL;

static void
free_user_agent (void)
{
        g_free (user_agent);
}

/* Sets the User-Agent header on @message */
void
http_request_set_user_agent (SoupMessage *message)
{
        if (G_UNLIKELY (user_agent == NULL)) {
                user_agent = g_strdup_printf
                                ("%s GUPnP/" VERSION " DLNADOC/1.50",
                                 g_get_application_name () ?: "");

                g_atexit (free_user_agent);
        }

        soup_message_headers_append (message->request_headers,
				     "User-Agent",
                                     user_agent);
}

/* Set Accept-Language header according to @locale. */
void
http_response_set_content_locale (SoupMessage *msg,
                                  const char  *locale)
{
        char *lang;

        lang = g_strdup (locale);
        http_language_from_locale (lang);

        soup_message_headers_append (msg->response_headers,
				     "Content-Language",
                                     lang);

        g_free (lang);
}

/* Set Content-Type header guessed from @path, @data and @data_size using
 * g_content_type_guess(). */
void
http_response_set_content_type (SoupMessage  *msg,
                                const char   *path,
                                const guchar *data,
                                gsize         data_size)
{
        char *content_type, *mime;

        content_type = g_content_type_guess
                                (path,
                                 data,
                                 data_size,
                                 NULL);
        mime = g_content_type_get_mime_type (content_type);
        if (mime == NULL)
                mime = g_strdup ("application/octet-stream");

        soup_message_headers_append (msg->response_headers,
                                     "Content-Type",
                                     mime);

        g_free (mime);
        g_free (content_type);
}

/* Set Content-Range header */
void
http_response_set_content_range (SoupMessage  *msg,
                                 gsize         offset,
                                 gsize         length,
                                 gsize         total)
{
        char *content_range;

        content_range = g_strdup_printf
                ("bytes %" G_GSIZE_FORMAT "-%"
                 G_GSIZE_FORMAT "/%" G_GSIZE_FORMAT,
                 offset,
                 offset + length,
                 total);

        soup_message_headers_append (msg->response_headers,
                                     "Content-Range",
                                     content_range);

        g_free (content_range);
}
