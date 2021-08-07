/*
 * Copyright (C) 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include <config.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>

#include <libsoup/soup.h>

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

/* Sets the Accept-Language on @message with the language taken from the
 * current locale. */
void
http_request_set_accept_language (SoupMessage *message)
{
#ifdef G_OS_WIN32
        /* TODO: Use GetSystemDefaultLangID or similar */
        return;
#else
        char *locale, *lang;
        int dash_index;
        GString *tmp;


        locale = setlocale (LC_MESSAGES, NULL);

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

        SoupMessageHeaders *request_headers =
                soup_message_get_request_headers (message);

        soup_message_headers_append (request_headers,
                                     "Accept-Language",
                                     tmp->str);

        g_string_free (tmp, TRUE);
#endif
}

static double
get_quality (const char *val)
{
        val = strstr (val, ";q=");
        if (!val)
                return 1;

        val += strlen (";q=");
        return atof (val);
}

static int
sort_locales_by_quality (const char *a,
                         const char *b)
{
        const double diff = get_quality (a) - get_quality (b);

        if (diff == 0.0)
                return 0;
        else if (diff > 0)
                return -1;

        return 1;
}

/* Parses the Accept-Language header in @message, and returns its values
 * in an ordered list in UNIX locale format */
GList *
http_request_get_accept_locales (SoupMessageHeaders *request_headers)
{
        const char *header;
        char **bits;
        int i;
        GList *locales;

        header = soup_message_headers_get_one (request_headers,
                                               "Accept-Language");
        if (header == NULL)
                return NULL;

        locales = NULL;

        bits = g_strsplit (header, ",", -1);

        /* Transform to list */
        for (i = 0; bits[i] != NULL; i++) {
                bits[i] = g_strstrip (bits[i]);

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

        locales = g_list_sort (locales, (GCompareFunc) sort_locales_by_quality);

        return locales;
}

/* Set Accept-Language header according to @locale. */
void
http_response_set_content_locale (SoupMessageHeaders *response_headers,
                                  const char *locale)
{
        char *lang;

        lang = g_strdup (locale);
        http_language_from_locale (lang);

        soup_message_headers_append (response_headers,
                                     "Content-Language",
                                     lang);

        g_free (lang);
}

/* Set Content-Type header guessed from @path, @data and @data_size using
 * g_content_type_guess(). */
void
http_response_set_content_type (SoupMessageHeaders *response_headers,
                                const char *path,
                                const guchar *data,
                                gsize data_size)
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
        else if (strcmp (mime, "application/xml") == 0) {
                g_free (mime);
                mime = g_strdup ("text/xml; charset=\"utf-8\"");
        }

        soup_message_headers_append (response_headers, "Content-Type", mime);

        g_free (mime);
        g_free (content_type);
}

/* Set Content-Encoding header to gzip and append compressed body */
void
http_response_set_body_gzip (SoupServerMessage *msg,
                             const char *body,
                             const gsize length)
{
        GZlibCompressor *compressor;
        gboolean finished = FALSE;
        gsize converted = 0;

        SoupMessageBody *message_body =
                soup_server_message_get_response_body (msg);
        SoupMessageHeaders *response_headers =
                soup_server_message_get_response_headers (msg);

        soup_message_headers_append (response_headers,
                                     "Content-Encoding",
                                     "gzip");

        compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, -1);

        while (! finished) {
                GError *error = NULL;
                char buf[65536];
                gsize bytes_read = 0;
                gsize bytes_written = 0;

                switch (g_converter_convert (G_CONVERTER (compressor),
                                             body + converted,
                                             length - converted,
                                             buf, sizeof (buf),
                                             G_CONVERTER_INPUT_AT_END,
                                             &bytes_read, &bytes_written,
                                             &error)) {
                case G_CONVERTER_ERROR:
                        g_warning ("Error compressing response: %s",
                                   error->message);
                        g_error_free (error);
                        g_object_unref (compressor);
                        return;
                case G_CONVERTER_CONVERTED:
                        converted += bytes_read;
                        break;
                case G_CONVERTER_FINISHED:
                        finished = TRUE;
                        break;
                case G_CONVERTER_FLUSHED:
                        break;
                default:
                        break;
                }

                if (bytes_written)
                        soup_message_body_append (message_body,
                                                  SOUP_MEMORY_COPY,
                                                  buf,
                                                  bytes_written);
        }

        g_object_unref (compressor);
}
