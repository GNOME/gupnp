/*
 * Copyright (C) 2012 Nokia.
 *
 * Author: Jens Georg <jensg@openismus.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <libsoup/soup.h>
#include "libgupnp/gupnp.h"

static void
on_message_finished (G_GNUC_UNUSED SoupSession *session,
                     G_GNUC_UNUSED SoupMessage *message,
                     gpointer                   user_data) {
        GMainLoop *loop = (GMainLoop*) user_data;

        g_main_loop_quit (loop);
}

static void
request_range_and_compare (GMappedFile *file,
                           SoupSession *session,
                           GMainLoop   *loop,
                           const char  *uri,
                           goffset      want_start,
                           goffset      want_end)
{
        SoupMessage *message = NULL;
        goffset      want_length = 0, full_length = 0;
        goffset      got_start = 0, got_end = 0, got_length = 0;
        int          result = 0;

        full_length = g_mapped_file_get_length (file);

        /* interpretation according to SoupRange documentation */
        if (want_end == -1) {
                if (want_start < 0)
                        want_length = -want_start;
                else
                        want_length = full_length - want_start;
        } else
                want_length = want_end - want_start + 1;

        message = soup_message_new ("GET", uri);
        g_object_ref (message);

        soup_message_headers_set_range (message->request_headers,
                                        want_start,
                                        want_end);

        /* interpretation according to SoupRange documentation */
        if (want_end == -1) {
                if (want_start < 0) {
                        want_length = -want_start;
                        want_start = full_length + want_start;
                        want_end = want_start + want_length - 1;
                }
                else {
                        want_length = full_length - want_start;
                        want_end = full_length - 1;
                }
        } else
                want_length = want_end - want_start + 1;


        soup_session_queue_message (session,
                                    message,
                                    on_message_finished,
                                    loop);

        g_main_loop_run (loop);
        g_assert_cmpint (message->status_code, ==, SOUP_STATUS_PARTIAL_CONTENT);
        g_assert_cmpint (message->response_body->length, ==, want_length);
        got_length = soup_message_headers_get_content_length
                                        (message->response_headers);
        g_assert_cmpint (got_length, ==, want_length);
        soup_message_headers_get_content_range (message->response_headers,
                                                &got_start,
                                                &got_end,
                                                &got_length);
        g_assert_cmpint (got_start, ==, want_start);
        g_assert_cmpint (got_end, ==, want_end);
        result = memcmp (g_mapped_file_get_contents (file) + want_start,
                         message->response_body->data,
                         want_length);
        g_assert_cmpint (result, ==, 0);

        g_object_unref (message);

        message = soup_message_new ("GET", uri);
        g_object_ref (message);
}

static void
test_gupnp_context_http_ranged_requests (void)
{
        GUPnPContext *context = NULL;
        GError *error = NULL;
        SoupSession *session = NULL;
        SoupMessage *message = NULL;
        guint port = 0;
        char *uri = NULL;
        GMainLoop *loop;
        GMappedFile *file;
        goffset file_length = 0;

        loop = g_main_loop_new (NULL, FALSE);
        g_assert (loop != NULL);

        file = g_mapped_file_new (DATA_PATH "/random4k.bin",
                                  FALSE,
                                  &error);
        g_assert (file != NULL);
        g_assert (error == NULL);
        file_length = g_mapped_file_get_length (file);

        context = gupnp_context_new (NULL,
                                     "lo",
                                     0,
                                     &error);
        g_assert (context != NULL);
        g_assert (error == NULL);
        port = gupnp_context_get_port (context);

        gupnp_context_host_path (context,
                                 DATA_PATH "/random4k.bin",
                                 "/random4k.bin");

        uri = g_strdup_printf ("http://127.0.0.1:%u/random4k.bin", port);
        g_assert (uri != NULL);

        session = soup_session_async_new ();

        /* Corner cases: First byte */
        request_range_and_compare (file, session, loop, uri, 0, 0);

        /* Corner cases: Last byte */
        request_range_and_compare (file,
                                   session,
                                   loop,
                                   uri,
                                   file_length - 1,
                                   file_length - 1);

        /* Examples from http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html */
        /* Request first 500 bytes */
        request_range_and_compare (file, session, loop, uri, 0, 499);

        /* Request second 500 bytes */
        request_range_and_compare (file, session, loop, uri, 500, 999);

        /* Request everything but the first 500 bytes */
        request_range_and_compare (file, session, loop, uri, 500, file_length - 1);

        /* Request the last 500 bytes */
        request_range_and_compare (file, session, loop, uri, file_length - 500, file_length - 1);

        /* Request the last 500 bytes by using negative requests: Range:
         * bytes: -500 */
        request_range_and_compare (file, session, loop, uri, -500, -1);

        /* Request the last 1k bytes by using negative requests: Range:
         * bytes: 3072- */
        request_range_and_compare (file, session, loop, uri, 3072, -1);

        /* Try to get 1 byte after the end of the file */
        message = soup_message_new ("GET", uri);
        g_object_ref (message);

        soup_message_headers_set_range (message->request_headers,
                                        file_length,
                                        file_length);
        soup_session_queue_message (session,
                                    message,
                                    on_message_finished,
                                    loop);

        g_main_loop_run (loop);
        g_assert_cmpint (message->status_code, ==, SOUP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE);

        g_object_unref (message);

        /* Try with inverted arguments */
        message = soup_message_new ("GET", uri);
        g_object_ref (message);

        soup_message_headers_set_range (message->request_headers, 499, 0);
        soup_session_queue_message (session,
                                    message,
                                    on_message_finished,
                                    loop);

        g_main_loop_run (loop);
        g_assert_cmpint (message->status_code, ==, SOUP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE);

        g_object_unref (message);

        g_free (uri);
        g_object_unref (context);
        g_main_loop_unref (loop);
        g_mapped_file_unref (file);
}

int main (int argc, char *argv[]) {
#if !GLIB_CHECK_VERSION(2,35,0)
        g_type_init ();
#endif
        g_test_init (&argc, &argv, NULL);
        g_test_add_func ("/context/http/ranged-requests",
                         test_gupnp_context_http_ranged_requests);

        g_test_run ();

        return 0;
}
