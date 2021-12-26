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
test_default_handler_on_read_async (GObject *source,
                                    GAsyncResult *res,
                                    gpointer user_data)
{
        GError *error = NULL;
        GBytes *response =
                soup_session_send_and_read_finish (source, res, &error);
        g_assert_nonnull (response);
        g_assert_no_error (error);
        g_bytes_unref (response);
        g_main_loop_quit (user_data);
}

void
test_gupnp_context_http_default_handler ()
{
        GError *error = NULL;
        GUPnPContext *context = create_context (0, &error);
        g_assert_no_error (error);
        g_assert_nonnull (context);

        GChecksum *checksum = g_checksum_new (G_CHECKSUM_SHA512);
        GMainLoop *loop = g_main_loop_new (NULL, FALSE);

        GSList *uris =
                soup_server_get_uris (gupnp_context_get_server (context));
        SoupSession *session = soup_session_new ();
        for (int i = 0; i < 10; i++) {
                guint32 random = g_random_int ();
                g_checksum_update (checksum,
                                   (const char *) &random,
                                   sizeof (random));
                char *base = g_uri_to_string (uris->data);
                char *new_uri = g_uri_resolve_relative (
                        base,
                        g_checksum_get_string (checksum),
                        G_URI_FLAGS_NONE,
                        &error);
                g_debug ("Trying to get URI %s", new_uri);
                g_free (base);
                g_assert_nonnull (new_uri);
                g_assert_no_error (error);

                SoupMessage *msg = soup_message_new (SOUP_METHOD_GET, new_uri);
                g_free (new_uri);

                g_checksum_reset (checksum);
                soup_session_send_and_read_async (
                        session,
                        msg,
                        G_PRIORITY_DEFAULT,
                        NULL,
                        test_default_handler_on_read_async,
                        loop);
                g_main_loop_run (loop);
                g_assert_cmpint (soup_message_get_status (msg),
                                 ==,
                                 SOUP_STATUS_NOT_FOUND);
                g_object_unref (msg);
        }
        g_slist_free_full (uris, g_uri_unref);
        g_checksum_free (checksum);
        g_object_unref (context);
        // Make sure the source teardown handlers get run so we don't confuse valgrind
        g_timeout_add (500, g_main_loop_quit, loop);
        g_main_loop_run (loop);
        g_main_loop_unref (loop);
        g_object_unref (session);
}

void
test_default_language_on_read_async (GObject *source,
                                    GAsyncResult *res,
                                    gpointer user_data)
{
        GError *error = NULL;
        GBytes *response =
                soup_session_send_and_read_finish (source, res, &error);
        g_assert_no_error (error);
        g_assert_nonnull (response);
        g_bytes_unref (response);
        g_main_loop_quit (user_data);
}

void
test_gupnp_context_http_language_default ()
{
        GError *error = NULL;
        GUPnPContext *context = create_context (0, &error);
        GMainLoop *loop = g_main_loop_new (NULL, FALSE);

        // Default default language
        g_assert_cmpstr (gupnp_context_get_default_language (context),
                         ==,
                         "en");

        GSList *uris =
                soup_server_get_uris (gupnp_context_get_server (context));
        gupnp_context_host_path (context, DATA_PATH "/random4k.bin", "/foo");
        SoupSession *session = soup_session_new ();
        char *base = g_uri_to_string (uris->data);
        char *new_uri =
                g_uri_resolve_relative (base, "foo", G_URI_FLAGS_NONE, &error);
        g_free (base);

        SoupMessage *msg = soup_message_new (SOUP_METHOD_GET, new_uri);
        soup_session_set_accept_language (session, NULL);
        g_free (new_uri);

        soup_session_send_and_read_async (session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          test_default_language_on_read_async,
                                          loop);
        g_main_loop_run (loop);

        SoupMessageHeaders *hdrs = soup_message_get_response_headers (msg);
        g_assert_cmpint (soup_message_get_status (msg), ==, SOUP_STATUS_OK);
        g_assert_null (soup_message_headers_get_one (hdrs, "Content-Language"));

        soup_session_set_accept_language (session, "fr");
        soup_session_send_and_read_async (session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          test_default_language_on_read_async,
                                          loop);
        g_main_loop_run (loop);

        hdrs = soup_message_get_response_headers (msg);
        g_assert_cmpint (soup_message_get_status (msg), ==, SOUP_STATUS_OK);
        g_assert_cmpstr (soup_message_headers_get_one (hdrs, "Content-Language"), ==, "en");

        g_object_unref (msg);
        g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);

        g_object_unref (context);

        // Make sure the source teardown handlers get run so we don't confuse valgrind
        g_timeout_add (500, (GSourceFunc)g_main_loop_quit, loop);
        g_main_loop_run (loop);
        g_main_loop_unref (loop);
        g_object_unref (session);
}

typedef struct _DefaultCallbackData {
        GBytes *bytes;
        GMainLoop *loop;
} DefaultCallbackData;

void
soup_message_default_callback (GObject *source,
                               GAsyncResult *res,
                               gpointer user_data)
{
        DefaultCallbackData *d = user_data;
        GError *error = NULL;
        d->bytes = soup_session_send_and_read_finish (SOUP_SESSION (source),
                                                      res,
                                                      &error);
        g_assert_no_error (error);
        g_assert_nonnull (d->bytes);
        g_main_loop_quit (d->loop);
}

void
test_gupnp_context_http_language_serve_file ()
{
        GError *error = NULL;
        GUPnPContext *context = create_context (0, &error);
        DefaultCallbackData d = { .bytes = NULL, .loop = NULL };

        d.loop = g_main_loop_new (NULL, FALSE);

        GSList *uris =
                soup_server_get_uris (gupnp_context_get_server (context));
        gupnp_context_host_path (context, DATA_PATH "/default", "/foo");
        SoupSession *session = soup_session_new ();
        char *base = g_uri_to_string (uris->data);
        char *new_uri =
                g_uri_resolve_relative (base, "foo", G_URI_FLAGS_NONE, &error);
        g_free (base);

        SoupMessage *msg = soup_message_new (SOUP_METHOD_GET, new_uri);
        soup_session_set_accept_language (session, NULL);

        soup_session_send_and_read_async (session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (d.loop);

        SoupMessageHeaders *hdrs = soup_message_get_response_headers (msg);
        g_assert_cmpint (soup_message_get_status (msg), ==, SOUP_STATUS_OK);
        g_assert_null (soup_message_headers_get_one (hdrs, "Content-Language"));
        g_assert_nonnull (d.bytes);
        g_assert_cmpmem (g_bytes_get_data (d.bytes, NULL),
                         g_bytes_get_size (d.bytes),
                         "default\n",
                         8);
        g_bytes_unref (d.bytes);

        g_object_unref (msg);
        msg = soup_message_new (SOUP_METHOD_GET, new_uri);
        soup_session_set_accept_language (session, "de");
        soup_session_send_and_read_async (session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (d.loop);

        hdrs = soup_message_get_response_headers (msg);
        g_assert_cmpint (soup_message_get_status (msg), ==, SOUP_STATUS_OK);
        g_assert_cmpstr (
                soup_message_headers_get_one (hdrs, "Content-Language"),
                ==,
                "de");
        g_assert_nonnull (d.bytes);
        g_assert_cmpmem (g_bytes_get_data (d.bytes, NULL),
                         g_bytes_get_size (d.bytes),
                         "de\n",
                         3);
        g_bytes_unref (d.bytes);


        g_object_unref (msg);
        msg = soup_message_new (SOUP_METHOD_GET, new_uri);
        soup_session_set_accept_language (session, "fr");
        soup_session_send_and_read_async (session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (d.loop);

        hdrs = soup_message_get_response_headers (msg);
        g_assert_cmpint (soup_message_get_status (msg), ==, SOUP_STATUS_OK);
        g_assert_cmpstr (
                soup_message_headers_get_one (hdrs, "Content-Language"),
                ==,
                "fr");
        g_assert_nonnull (d.bytes);
        g_assert_cmpmem (g_bytes_get_data (d.bytes, NULL),
                         g_bytes_get_size (d.bytes),
                         "fr\n",
                         3);
        g_bytes_unref (d.bytes);

        g_object_unref (msg);
        msg = soup_message_new (SOUP_METHOD_GET, new_uri);
        soup_session_set_accept_language (session, "it");
        soup_session_send_and_read_async (session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (d.loop);

        hdrs = soup_message_get_response_headers (msg);
        g_assert_cmpint (soup_message_get_status (msg), ==, SOUP_STATUS_OK);
        g_assert_cmpstr (
                soup_message_headers_get_one (hdrs, "Content-Language"),
                ==,
                "en");
        g_assert_nonnull (d.bytes);
        g_assert_cmpmem (g_bytes_get_data (d.bytes, NULL),
                         g_bytes_get_size (d.bytes),
                         "default\n",
                         8);
        g_bytes_unref (d.bytes);

        g_object_unref (msg);

        g_free (new_uri);
        g_object_unref (context);
        g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);

        // Make sure the source teardown handlers get run so we don't confuse valgrind
        g_timeout_add (500, (GSourceFunc)g_main_loop_quit, d.loop);
        g_main_loop_run (d.loop);
        g_main_loop_unref (d.loop);
        g_object_unref (session);
}

int main (int argc, char *argv[]) {
        g_test_init (&argc, &argv, NULL);
        g_test_add_func ("/context/http/ranged-requests",
                         test_gupnp_context_http_ranged_requests);

        g_test_add_func ("/context/creation/error-when-bound",
                         test_gupnp_context_error_when_bound);
        g_test_add_func ("/context/http/default-handler",
                         test_gupnp_context_http_default_handler);

        g_test_add_func ("/context/http/language/default",
                         test_gupnp_context_http_language_default);

        g_test_add_func ("/context/http/language/serve-file",
                         test_gupnp_context_http_language_serve_file);

        g_test_add_func ("/context/utility/rewrite_uri",
                         test_gupnp_context_rewrite_uri);

        g_test_run ();

        return 0;
}
