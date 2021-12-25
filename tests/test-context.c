/*
 * Copyright (C) 2012 Nokia.
 *
 * Author: Jens Georg <jensg@openismus.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <config.h>

#include <string.h>

#include <libsoup/soup.h>
#include "libgupnp/gupnp.h"

static GUPnPContext *
create_context (guint16 port, GError **error) {
        return GUPNP_CONTEXT (g_initable_new (GUPNP_TYPE_CONTEXT,
                                              NULL,
                                              error,
                                              "host-ip", "127.0.0.1",
                                              "msearch-port", port,
                                              NULL));
}

typedef struct {
        GMainLoop *loop;
        GBytes *body;
        GError *error;
} RangeHelper;

static void
on_message_finished (GObject *source, GAsyncResult *res, gpointer user_data)
{
        RangeHelper *h = (RangeHelper *) user_data;

        h->body = soup_session_send_and_read_finish (SOUP_SESSION (source),
                                                     res,
                                                     &h->error);

        g_main_loop_quit (h->loop);
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

        message = soup_message_new ("GET", uri);

        SoupMessageHeaders *request_headers =
                soup_message_get_request_headers (message);

        soup_message_headers_set_range (request_headers, want_start, want_end);

        // interpretation according to SoupRange documentation
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

        RangeHelper h = { loop, NULL, NULL };
        soup_session_send_and_read_async (session,
                                          message,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          on_message_finished,
                                          &h);

        g_main_loop_run (loop);
        g_assert_no_error (h.error);
        g_assert_nonnull (h.body);

        g_assert_cmpint (soup_message_get_status (message),
                         ==,
                         SOUP_STATUS_PARTIAL_CONTENT);
        g_assert_cmpint (g_bytes_get_size (h.body), ==, want_length);
        SoupMessageHeaders *response_headers =
                soup_message_get_response_headers (message);
        got_length = soup_message_headers_get_content_length (response_headers);
        g_assert_cmpint (got_length, ==, want_length);
        soup_message_headers_get_content_range (response_headers,
                                                &got_start,
                                                &got_end,
                                                &got_length);
        g_assert_cmpint (got_start, ==, want_start);
        g_assert_cmpint (got_end, ==, want_end);
        result = memcmp (g_mapped_file_get_contents (file) + want_start,
                         g_bytes_get_data (h.body, NULL),
                         want_length);
        g_assert_cmpint (result, ==, 0);

        g_object_unref (message);
        g_bytes_unref (h.body);
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

        context = create_context (0, &error);
        g_assert (context != NULL);
        g_assert (error == NULL);
        port = gupnp_context_get_port (context);

        gupnp_context_host_path (context,
                                 DATA_PATH "/random4k.bin",
                                 "/random4k.bin");

        uri = g_strdup_printf ("http://127.0.0.1:%u/random4k.bin", port);
        g_assert (uri != NULL);

        session = soup_session_new ();

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

        RangeHelper h = { loop, NULL, NULL };
        soup_message_headers_set_range (
                soup_message_get_request_headers (message),
                file_length,
                file_length);
        soup_session_send_and_read_async (session,
                                          message,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          on_message_finished,
                                          &h);

        g_main_loop_run (loop);

        g_assert_no_error (h.error);
        g_assert_nonnull (h.body);
        g_bytes_unref (h.body);
        g_assert_cmpint (soup_message_get_status (message),
                         ==,
                         SOUP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE);

        g_object_unref (message);

        g_free (uri);
        g_object_unref (context);

        // Make sure the source teardown handlers get run so we don't confuse valgrind
        g_timeout_add (500, g_main_loop_quit, loop);
        g_main_loop_run (loop);
        g_main_loop_unref (loop);
        g_object_unref (session);
        g_mapped_file_unref (file);
}

static void
test_gupnp_context_error_when_bound ()
{
        GError *error = NULL;

        // IPv6
        SoupServer *server = soup_server_new (NULL, NULL);        
        soup_server_listen_local (server, 0, SOUP_SERVER_LISTEN_IPV4_ONLY, &error);
        g_assert_no_error (error);

        GSList *uris = soup_server_get_uris (server);

        const char *address = g_uri_get_host (uris->data);
        int port = g_uri_get_port (uris->data);

        g_test_expect_message (
                "gupnp-context",
                G_LOG_LEVEL_WARNING,
                "*Unable to listen*Could not listen*Address already in use*");
        GUPnPContext *context = g_initable_new (GUPNP_TYPE_CONTEXT,
                                                NULL,
                                                &error,
                                                "host-ip",
                                                address,
                                                "port",
                                                port,
                                                NULL);

        g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);
        g_object_unref (server);
        g_test_assert_expected_messages ();
        g_assert_error (error, GUPNP_SERVER_ERROR, GUPNP_SERVER_ERROR_OTHER);
        g_assert_null (context);
        g_clear_error (&error);

        // IPv6
        server = soup_server_new (NULL, NULL);
        soup_server_listen_local (server, 0, SOUP_SERVER_LISTEN_IPV6_ONLY, &error);
        g_assert_no_error (error);

        uris = soup_server_get_uris (server);

        address = g_uri_get_host (uris->data);
        port = g_uri_get_port (uris->data);

        g_test_expect_message (
                "gupnp-context",
                G_LOG_LEVEL_WARNING,
                "*Unable to listen*Could not listen*Address already in use*");
        context = g_initable_new (GUPNP_TYPE_CONTEXT,
                                  NULL,
                                  &error,
                                  "host-ip",
                                  address,
                                  "port",
                                  port,
                                  NULL);

        g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);
        g_object_unref (server);

        g_test_assert_expected_messages ();
        g_assert_error (error, GUPNP_SERVER_ERROR, GUPNP_SERVER_ERROR_OTHER);
        g_assert_null (context);
        g_clear_error (&error);
}

void
test_gupnp_context_rewrite_uri ()
{
        GUPnPContext *context = NULL;
        GError *error = NULL;

        // Create a v4 context
        context = g_initable_new (GUPNP_TYPE_CONTEXT,
                                  NULL,
                                  &error,
                                  "host-ip",
                                  "127.0.0.1",
                                  NULL);

        g_assert_no_error (error);
        g_assert_nonnull (context);

        char *uri = gupnp_context_rewrite_uri (context, "http://127.0.0.1");
        g_assert_cmpstr (uri, ==, "http://127.0.0.1");
        g_free (uri);

        // Rewriting a v6 uri on a v4 context should not work
        g_test_expect_message ("gupnp-context",
                               G_LOG_LEVEL_WARNING,
                               "Address*family*mismatch*");
        uri = gupnp_context_rewrite_uri (context, "http://[::1]");
        g_assert_null (uri);
        g_test_assert_expected_messages ();

        g_object_unref (context);

        // Create a v6 context
        context = g_initable_new (GUPNP_TYPE_CONTEXT,
                                  NULL,
                                  &error,
                                  "host-ip",
                                  "::1",
                                  NULL);

        g_assert_no_error (error);
        g_assert_nonnull (context);
        // Rewriting a v6 uri on a v4 context should not work
        uri = gupnp_context_rewrite_uri (context, "http://[fe80::1]");
        char *expected = g_strdup_printf (
                "http://[fe80::1%%25%d]",
                gssdp_client_get_index (GSSDP_CLIENT (context)));
        g_assert_cmpstr (uri, ==, expected);
        g_free (expected);
        g_free (uri);

        g_test_expect_message ("gupnp-context",
                               G_LOG_LEVEL_WARNING,
                               "Address*family*mismatch*");
        uri = gupnp_context_rewrite_uri (context, "http://127.0.0.1");
        g_assert_null (uri);
        g_test_assert_expected_messages ();

        g_object_unref (context);
}

void
test_gupnp_context_http_default_handler ()
{
        GError *error = NULL;
        GUPnPContext *context = create_context (0, &error);

        g_assert_no_error (error);
        g_assert_nonnull (context);
}

int main (int argc, char *argv[]) {
        g_test_init (&argc, &argv, NULL);
        g_test_add_func ("/context/http/ranged-requests",
                         test_gupnp_context_http_ranged_requests);

        g_test_add_func ("/context/creation/error-when-bound",
                         test_gupnp_context_error_when_bound);
        g_test_add_func ("/context/http/default-handler",
                         test_gupnp_context_http_default_handler);

        g_test_add_func ("/context/utility/rewrite_uri",
                         test_gupnp_context_rewrite_uri);

        g_test_run ();

        return 0;
}
