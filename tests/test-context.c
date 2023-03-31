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
create_context (const char *localhost, guint16 port, GError **error)
{
        return GUPNP_CONTEXT (g_initable_new (GUPNP_TYPE_CONTEXT,
                                              NULL,
                                              error,
                                              "host-ip",
                                              localhost,
                                              "port",
                                              port,
                                              NULL));
}


typedef struct {
        GMainLoop *loop;
        GUPnPContext *context;
        SoupSession *session;
        char *base_uri;
} ContextTestFixture;

static void
test_fixture_setup (ContextTestFixture* tf, gconstpointer user_data)
{
        GError *error = NULL;

        tf->context = create_context ((const char *)user_data, 0, &error);

        g_assert_nonnull (tf->context);
        g_assert_no_error (error);

        tf->loop = g_main_loop_new (NULL, FALSE);
        tf->session = soup_session_new ();

        GSList *uris =
                soup_server_get_uris (gupnp_context_get_server (tf->context));
        g_assert_nonnull (uris);
        g_assert_cmpint (g_slist_length (uris), ==, 1);

        tf->base_uri = g_uri_to_string (uris->data);
        g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);
}

static gboolean
delayed_loop_quitter (gpointer user_data)
{
        g_main_loop_quit (user_data);
        return G_SOURCE_REMOVE;
}

static void
test_fixture_teardown (ContextTestFixture *tf, G_GNUC_UNUSED gconstpointer user_data)
{
        g_free (tf->base_uri);
        g_object_unref (tf->context);

        // Make sure the source teardown handlers get run so we don't confuse valgrind
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);
        g_main_loop_unref (tf->loop);
        g_object_unref (tf->session);
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
                           ContextTestFixture *tf,
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

        RangeHelper h = { tf->loop, NULL, NULL };
        soup_session_send_and_read_async (tf->session,
                                          message,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          on_message_finished,
                                          &h);

        g_main_loop_run (tf->loop);
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

        // coverity[leaked_storage]
        result = memcmp (g_mapped_file_get_contents (file) + want_start,
                         g_bytes_get_data (h.body, NULL),
                         want_length);
        g_assert_cmpint (result, ==, 0);

        g_object_unref (message);
        g_bytes_unref (h.body);
}

static void
test_gupnp_context_http_ranged_requests (ContextTestFixture *tf,
                                         G_GNUC_UNUSED gconstpointer user_data)
{
        GError *error = NULL;
        SoupMessage *message = NULL;
        char *uri = NULL;
        GMappedFile *file;
        goffset file_length = 0;

        file = g_mapped_file_new (DATA_PATH "/random4k.bin",
                                  FALSE,
                                  &error);
        g_assert (file != NULL);
        g_assert (error == NULL);
        file_length = g_mapped_file_get_length (file);

        char *new_uri =
                g_uri_resolve_relative (tf->base_uri, "random4k.bin", G_URI_FLAGS_NONE, &error);
        g_assert_no_error (error);
        uri = gupnp_context_rewrite_uri (tf->context, new_uri);
        g_free (new_uri);

        gupnp_context_host_path (tf->context,
                                 DATA_PATH "/random4k.bin",
                                 "/random4k.bin");

        /* Corner cases: First byte */
        request_range_and_compare (file, tf, uri, 0, 0);

        /* Corner cases: Last byte */
        request_range_and_compare (file,
                                   tf,
                                   uri,
                                   file_length - 1,
                                   file_length - 1);

        /* Examples from http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html */
        /* Request first 500 bytes */
        request_range_and_compare (file, tf, uri, 0, 499);

        /* Request second 500 bytes */
        request_range_and_compare (file, tf, uri, 500, 999);

        /* Request everything but the first 500 bytes */
        request_range_and_compare (file, tf, uri, 500, file_length - 1);

        /* Request the last 500 bytes */
        request_range_and_compare (file, tf, uri, file_length - 500, file_length - 1);

        /* Request the last 500 bytes by using negative requests: Range:
         * bytes: -500 */
        request_range_and_compare (file, tf, uri, -500, -1);

        /* Request the last 1k bytes by using negative requests: Range:
         * bytes: 3072- */
        request_range_and_compare (file, tf, uri, 3072, -1);

        /* Try to get 1 byte after the end of the file */
        message = soup_message_new ("GET", uri);

        RangeHelper h = { tf->loop, NULL, NULL };
        soup_message_headers_set_range (
                soup_message_get_request_headers (message),
                file_length,
                file_length);
        soup_session_send_and_read_async (tf->session,
                                          message,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          on_message_finished,
                                          &h);

        g_main_loop_run (tf->loop);

        g_assert_no_error (h.error);
        g_assert_nonnull (h.body);
        g_bytes_unref (h.body);
        g_assert_cmpint (soup_message_get_status (message),
                         ==,
                         SOUP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE);

        g_object_unref (message);

        g_free (uri);
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

        SoupServer *s = soup_server_new (NULL, NULL);
        soup_server_listen_local (server,
                                  port,
                                  SOUP_SERVER_LISTEN_IPV4_ONLY,
                                  &error);

        g_object_unref (s);

        if (error == NULL) {
                g_object_unref (server);
                // Skip the test, for some reason it is possible to bind to the
                // same TCP port twice here
                return;
        }
        g_clear_error (&error);

        g_test_expect_message (
                "gupnp-context",
                G_LOG_LEVEL_WARNING,
                "*Unable to listen*");
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
        if (error == NULL) {
                uris = soup_server_get_uris (server);

                address = g_uri_get_host (uris->data);
                port = g_uri_get_port (uris->data);

                g_test_expect_message ("gupnp-context",
                                       G_LOG_LEVEL_WARNING,
                                       "*Unable to listen*");
                context = g_initable_new (GUPNP_TYPE_CONTEXT,
                                          NULL,
                                          &error,
                                          "host-ip",
                                          address,
                                          "port",
                                          port,
                                          NULL);

                g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);

                g_test_assert_expected_messages ();
                g_assert_error (error,
                                GUPNP_SERVER_ERROR,
                                GUPNP_SERVER_ERROR_OTHER);
                g_assert_null (context);
                g_clear_error (&error);
        }
        g_object_unref (server);
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

        // Skip test if IPv6 could not be found
        if (error == NULL) {
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
}

void
test_default_handler_on_read_async (GObject *source,
                                    GAsyncResult *res,
                                    gpointer user_data)
{
        GError *error = NULL;
        GBytes *response =
                soup_session_send_and_read_finish (SOUP_SESSION (source), res, &error);
        g_assert_nonnull (response);
        g_assert_no_error (error);
        g_bytes_unref (response);
        g_main_loop_quit (user_data);
}

void
test_gupnp_context_http_default_handler (ContextTestFixture *tf,
                                         G_GNUC_UNUSED gconstpointer user_data)
{
        GError *error = NULL;
        GChecksum *checksum = g_checksum_new (G_CHECKSUM_SHA512);
        GMainLoop *loop = tf->loop;

        for (int i = 0; i < 10; i++) {
                guint32 random = g_random_int ();
                g_checksum_update (checksum,
                                   (const guchar *) &random,
                                   sizeof (random));
                char *new_uri = g_uri_resolve_relative (
                        tf->base_uri,
                        g_checksum_get_string (checksum),
                        G_URI_FLAGS_NONE,
                        &error);
                char *rewritten_uri =
                        gupnp_context_rewrite_uri (tf->context, new_uri);
                g_assert_nonnull (new_uri);
                g_assert_no_error (error);
                g_free (new_uri);

                g_debug ("Trying to get URI %s", rewritten_uri);

                SoupMessage *msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
                g_free (rewritten_uri);

                g_checksum_reset (checksum);
                soup_session_send_and_read_async (
                        tf->session,
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

        g_checksum_free (checksum);
}

void
test_default_language_on_read_async (GObject *source,
                                    GAsyncResult *res,
                                    gpointer user_data)
{
        GError *error = NULL;
        GBytes *response =
                soup_session_send_and_read_finish (SOUP_SESSION (source), res, &error);
        g_assert_no_error (error);
        g_assert_nonnull (response);
        g_bytes_unref (response);
        g_main_loop_quit (user_data);
}

void
test_gupnp_context_http_language_default (ContextTestFixture *tf,
                                          G_GNUC_UNUSED gconstpointer user_data)
{
        GError *error = NULL;

        // Default default language
        g_assert_cmpstr (gupnp_context_get_default_language (tf->context),
                         ==,
                         "en");

        gupnp_context_host_path (tf->context,
                                 DATA_PATH "/random4k.bin",
                                 "/foo");
        char *new_uri = g_uri_resolve_relative (tf->base_uri,
                                                "foo",
                                                G_URI_FLAGS_NONE,
                                                &error);

        g_assert_no_error (error);
        char *rewritten_uri = gupnp_context_rewrite_uri (tf->context, new_uri);
        g_free (new_uri);

        SoupMessage *msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        soup_session_set_accept_language (tf->session, NULL);
        g_free (rewritten_uri);

        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          test_default_language_on_read_async,
                                          tf->loop);
        g_main_loop_run (tf->loop);

        SoupMessageHeaders *hdrs = soup_message_get_response_headers (msg);
        g_assert_cmpint (soup_message_get_status (msg), ==, SOUP_STATUS_OK);
        g_assert_null (soup_message_headers_get_one (hdrs, "Content-Language"));

        soup_session_set_accept_language (tf->session, "fr");
        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          test_default_language_on_read_async,
                                          tf->loop);
        g_main_loop_run (tf->loop);

        hdrs = soup_message_get_response_headers (msg);
        g_assert_cmpint (soup_message_get_status (msg), ==, SOUP_STATUS_OK);
        g_assert_cmpstr (soup_message_headers_get_one (hdrs, "Content-Language"), ==, "en");

        g_object_unref (msg);
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
test_gupnp_context_http_language_serve_file (ContextTestFixture *tf,
                                             G_GNUC_UNUSED gconstpointer user_data)
{
        GError *error = NULL;
        DefaultCallbackData d = { .bytes = NULL, .loop = NULL };

        d.loop = tf->loop;

        gupnp_context_host_path (tf->context, DATA_PATH "/default", "/foo");
        char *new_uri =
                g_uri_resolve_relative (tf->base_uri, "foo", G_URI_FLAGS_NONE, &error);
        g_assert_no_error (error);
        char *rewritten_uri = gupnp_context_rewrite_uri (tf->context, new_uri);
        g_free (new_uri);

        SoupMessage *msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        soup_session_set_accept_language (tf->session, NULL);

        soup_session_send_and_read_async (tf->session,
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
        msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        soup_session_set_accept_language (tf->session, "de");
        soup_session_send_and_read_async (tf->session,
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
        msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        soup_session_set_accept_language (tf->session, "fr");
        soup_session_send_and_read_async (tf->session,
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
        msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        soup_session_set_accept_language (tf->session, "it");
        soup_session_send_and_read_async (tf->session,
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

        g_free (rewritten_uri);
}

void
test_gupnp_context_http_language_serve_folder (ContextTestFixture *tf,
                                               G_GNUC_UNUSED gconstpointer user_data)
{
        GError *error = NULL;
        DefaultCallbackData d = { .bytes = NULL, .loop = NULL };

        d.loop = tf->loop;

        gupnp_context_host_path (tf->context, DATA_PATH "/locale/test", "/foo");
        char *new_uri =
                g_uri_resolve_relative (tf->base_uri, "foo/", G_URI_FLAGS_NONE, &error);
        g_assert_no_error (error);
        char *rewritten_uri = gupnp_context_rewrite_uri (tf->context, new_uri);
        g_free (new_uri);

        SoupMessage *msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        soup_session_set_accept_language (tf->session, NULL);

        soup_session_send_and_read_async (tf->session,
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
        msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        soup_session_set_accept_language (tf->session, "de");
        soup_session_send_and_read_async (tf->session,
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
        msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        soup_session_set_accept_language (tf->session, "fr");
        soup_session_send_and_read_async (tf->session,
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
        msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        soup_session_set_accept_language (tf->session, "it");
        soup_session_send_and_read_async (tf->session,
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
        g_free (rewritten_uri);
}

void
test_gupnp_context_http_folder_redirect (ContextTestFixture *tf,
                                         G_GNUC_UNUSED gconstpointer user_data)
{
        GError *error = NULL;
        DefaultCallbackData d = { .bytes = NULL, .loop = NULL };

        d.loop = tf->loop;

        gupnp_context_host_path (tf->context, DATA_PATH "/locale", "/foo");

        char *new_uri = g_uri_resolve_relative (tf->base_uri,
                                                "foo",
                                                G_URI_FLAGS_NONE,
                                                &error);
        g_assert_no_error (error);
        char *rewritten_uri = gupnp_context_rewrite_uri (tf->context, new_uri);
        g_free (new_uri);

        SoupMessage *msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        g_free (rewritten_uri);

        // Do not automatically follow the redirect as we want to check if it happened
        soup_message_add_flags (msg, SOUP_MESSAGE_NO_REDIRECT);

        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (tf->loop);

        g_assert_cmpint (soup_message_get_status (msg),
                         ==,
                         SOUP_STATUS_MOVED_PERMANENTLY);
        g_assert_nonnull (d.bytes);
        g_assert_null (g_bytes_get_data (d.bytes, NULL));
        g_bytes_unref (d.bytes);
        g_object_unref (msg);
}

void
test_gupnp_context_host_for_agent (ContextTestFixture *tf,
                                   G_GNUC_UNUSED gconstpointer user_data)
{
        GError *error = NULL;
        DefaultCallbackData d = { .bytes = NULL, .loop = NULL };

        d.loop = tf->loop;

        GRegex *user_agent =
                g_regex_new ("GUPnP-Context-Test UA", 0, 0, &error);
        g_assert_no_error (error);
        g_assert_nonnull (user_agent);

        // Cannot host path for a user agent if path is not hosted generally
        g_assert_false (gupnp_context_host_path_for_agent (tf->context,
                                                           DATA_PATH "/default",
                                                           "/foo",
                                                           user_agent));

        gupnp_context_host_path (tf->context, DATA_PATH "/random4k.bin", "/foo");
        // Hosting agent-specific path must now succeed
        g_assert_true (gupnp_context_host_path_for_agent (tf->context,
                                                          DATA_PATH "/default",
                                                          "/foo",
                                                          user_agent));

        char *new_uri = g_uri_resolve_relative (tf->base_uri,
                                                "foo",
                                                G_URI_FLAGS_NONE,
                                                &error);

        g_assert_no_error (error);
        char *rewritten_uri = gupnp_context_rewrite_uri (tf->context, new_uri);
        g_free (new_uri);

        // Checking wihtout user agent - should return the 4k file
        SoupMessage *msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        soup_session_set_accept_language (tf->session, NULL);

        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (tf->loop);

        g_assert_cmpint (soup_message_get_status (msg), ==, SOUP_STATUS_OK);
        g_assert_nonnull (d.bytes);
        g_assert_nonnull (g_bytes_get_data (d.bytes, NULL));
        g_assert_cmpint (g_bytes_get_size (d.bytes), ==, 4096);
        g_bytes_unref (d.bytes);

        // Checking with user agent - should return the small file
        g_object_unref (msg);
        msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        soup_session_set_user_agent (tf->session, "GUPnP-Context-Test UA");

        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (tf->loop);

        g_assert_cmpint (soup_message_get_status (msg), ==, SOUP_STATUS_OK);
        g_assert_nonnull (d.bytes);
        g_assert_cmpmem (g_bytes_get_data (d.bytes, NULL),
                         g_bytes_get_size (d.bytes),
                         "default\n",
                         8);
        g_bytes_unref (d.bytes);

        g_object_unref (msg);

        gupnp_context_unhost_path (tf->context, "/foo");

        // there should be no file anymore for the user agent
        msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        soup_session_set_user_agent (tf->session, "GUPnP-Context-Test UA");

        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (tf->loop);

        g_assert_cmpint (soup_message_get_status (msg),
                         ==,
                         SOUP_STATUS_NOT_FOUND);
        g_assert_nonnull (d.bytes);
        g_assert_null (g_bytes_get_data(d.bytes, NULL));
        g_bytes_unref (d.bytes);

        g_object_unref (msg);
        // there should be no file anymore for the user agent
        msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        soup_session_set_user_agent (tf->session, NULL);

        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (tf->loop);

        g_assert_cmpint (soup_message_get_status (msg),
                         ==,
                         SOUP_STATUS_NOT_FOUND);
        g_assert_nonnull (d.bytes);
        g_assert_null (g_bytes_get_data(d.bytes, NULL));
        g_bytes_unref (d.bytes);
        g_free (rewritten_uri);

        g_object_unref (msg);

        g_regex_unref (user_agent);
}

void
test_gupnp_context_host_path_user_agent_cache_filled_from_request (
        ContextTestFixture *tf,
        G_GNUC_UNUSED gconstpointer user_data)
{
        const char *user_agent = "GUPnP-Context Test UA";

        GError *error = NULL;
        DefaultCallbackData d = { .bytes = NULL, .loop = NULL };

        d.loop = tf->loop;

        gupnp_context_host_path (tf->context, DATA_PATH "/default", "/foo");
        GRegex *agent = g_regex_new (user_agent, 0, 0, &error);
        g_assert_nonnull (agent);
        g_assert_no_error (error);

        gupnp_context_host_path_for_agent (tf->context,
                                           DATA_PATH "/default.de",
                                           "/foo",
                                           agent);
        char *new_uri = g_uri_resolve_relative (tf->base_uri,
                                                "foo",
                                                G_URI_FLAGS_NONE,
                                                &error);
        g_assert_no_error (error);
        char *rewritten_uri = gupnp_context_rewrite_uri (tf->context, new_uri);
        g_free (new_uri);

        SoupMessage *msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);

        // First request with user agent
        soup_session_set_user_agent (tf->session, user_agent);
        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (d.loop);
        g_assert_nonnull (d.bytes);
        g_assert_cmpmem (g_bytes_get_data (d.bytes, NULL),
                         g_bytes_get_size (d.bytes),
                         "de\n",
                         3);
        g_bytes_unref (d.bytes);
        g_object_unref (msg);

        // Second request without user agent - we should get the specific resource
        msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);

        // First request with user agent
        soup_session_set_user_agent (tf->session, NULL);
        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (d.loop);
        g_assert_nonnull (d.bytes);
        g_assert_cmpmem (g_bytes_get_data (d.bytes, NULL),
                         g_bytes_get_size (d.bytes),
                         "de\n",
                         3);
        g_bytes_unref (d.bytes);
        g_object_unref (msg);
        g_free (rewritten_uri);
        g_regex_unref (agent);
}

void
test_gupnp_context_host_path_user_agent_cache_prefilled (
        ContextTestFixture *tf,
        gconstpointer user_data)
{
        const char *user_agent = "GUPnP-Context Test UA";

        GError *error = NULL;
        DefaultCallbackData d = { .bytes = NULL, .loop = NULL };

        d.loop = tf->loop;

        gupnp_context_host_path (tf->context, DATA_PATH "/default", "/foo");
        GRegex *agent = g_regex_new (user_agent, 0, 0, &error);
        g_assert_nonnull (agent);
        g_assert_no_error (error);

        gupnp_context_host_path_for_agent (tf->context,
                                           DATA_PATH "/default.de",
                                           "/foo",
                                           agent);
        char *new_uri = g_uri_resolve_relative (tf->base_uri,
                                                "foo",
                                                G_URI_FLAGS_NONE,
                                                &error);
        g_assert_no_error (error);
        char *rewritten_uri = gupnp_context_rewrite_uri (tf->context, new_uri);
        g_free (new_uri);

        gssdp_client_add_cache_entry (GSSDP_CLIENT (tf->context),
                                      user_data,
                                      user_agent);

        SoupMessage *msg = soup_message_new (SOUP_METHOD_GET, rewritten_uri);
        soup_session_set_user_agent (tf->session, NULL);
        soup_session_send_and_read_async (tf->session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          soup_message_default_callback,
                                          &d);
        g_main_loop_run (d.loop);
        g_assert_nonnull (d.bytes);
        g_assert_cmpmem (g_bytes_get_data (d.bytes, NULL),
                         g_bytes_get_size (d.bytes),
                         "de\n",
                         3);
        g_bytes_unref (d.bytes);
        g_object_unref (msg);
        g_free (rewritten_uri);
        g_regex_unref (agent);
}

void
test_gupnp_context_host_path_invalid_methods (ContextTestFixture *tf,
                                              G_GNUC_UNUSED gconstpointer
                                                      user_data)
{
        // FIXME: CONNECT does not seem to be forwarded to the handler by libsoup,
        // causes the main loop not to quit
        char *invalid_methods[] = { "POST",    "PUT",   "DELETE", /*"CONNECT",*/
                                    "OPTIONS", "TRACE", "PATCH",  NULL };

        GError *error = NULL;
        DefaultCallbackData d = { .bytes = NULL, .loop = NULL };

        d.loop = tf->loop;

        gupnp_context_host_path (tf->context, DATA_PATH "/default", "/foo");

        char *new_uri = g_uri_resolve_relative (tf->base_uri,
                                                "foo",
                                                G_URI_FLAGS_NONE,
                                                &error);
        char *rewritten_uri = gupnp_context_rewrite_uri (tf->context, new_uri);
        g_free (new_uri);

        char **it = invalid_methods;
        while (*it != NULL) {
                g_debug ("Trying %s on %s", *it, rewritten_uri);
                SoupMessage *msg = soup_message_new (*it, rewritten_uri);
                soup_session_set_user_agent (tf->session, NULL);
                soup_session_send_and_read_async (tf->session,
                                                  msg,
                                                  G_PRIORITY_DEFAULT,
                                                  NULL,
                                                  soup_message_default_callback,
                                                  &d);
                g_main_loop_run (d.loop);
                g_assert_cmpint (soup_message_get_status (msg),
                                 ==,
                                 SOUP_STATUS_NOT_IMPLEMENTED);
                g_object_unref (msg);
                g_bytes_unref (d.bytes);
                it++;
        }
        g_free (rewritten_uri);
}


int
main (int argc, char *argv[])
{
        g_test_init (&argc, &argv, NULL);

        GPtrArray *addresses = g_ptr_array_sized_new (2);
        g_ptr_array_add (addresses, g_strdup ("127.0.0.1"));


        // Detect if there is a special network device with "proper" ip addresses to check also link-local addresses
        GUPnPContext *c = g_initable_new (GUPNP_TYPE_CONTEXT,
                                          NULL,
                                          NULL,
                                          "host-ip",
                                          "::1",
                                          NULL);

        if (c != NULL) {
                g_ptr_array_add (
                        addresses,
                        g_strdup (gssdp_client_get_host_ip (GSSDP_CLIENT (c))));
                g_object_unref (c);
        }

        // Detect if there is a special network device with "proper" ip addresses to check also link-local addresses
        c = g_initable_new (GUPNP_TYPE_CONTEXT,
                            NULL,
                            NULL,
                            "interface",
                            "gupnp0",
                            "address-family",
                            G_SOCKET_FAMILY_IPV4,
                            NULL);

        if (c != NULL) {
                g_debug ("Adding address %s from device gupnp0",
                         gssdp_client_get_host_ip (GSSDP_CLIENT (c)));
                g_ptr_array_add (
                        addresses,
                        g_strdup (gssdp_client_get_host_ip (GSSDP_CLIENT (c))));
                g_object_unref (c);
        }

        c = g_initable_new (GUPNP_TYPE_CONTEXT,
                            NULL,
                            NULL,
                            "interface",
                            "gupnp0",
                            "address-family",
                            G_SOCKET_FAMILY_IPV6,
                            NULL);

        if (c != NULL) {
                g_debug ("Adding address %s from device gupnp0",
                         gssdp_client_get_host_ip (GSSDP_CLIENT (c)));
                g_ptr_array_add (
                        addresses,
                        g_strdup (gssdp_client_get_host_ip (GSSDP_CLIENT (c))));
                g_object_unref (c);
        }

        g_ptr_array_add (addresses, NULL);

        g_test_add_func ("/context/creation/error-when-bound",
                         test_gupnp_context_error_when_bound);

        g_test_add_func ("/context/utility/rewrite_uri",
                         test_gupnp_context_rewrite_uri);


        char **data = (char **)g_ptr_array_free (addresses, FALSE);
        char **it = data;
        while (*it != NULL) {
                char *name = NULL;

                name = g_strdup_printf ("/context/http/ranged-requests/%s",
                                        *it);
                g_test_add (name,
                            ContextTestFixture,
                            *it,
                            test_fixture_setup,
                            test_gupnp_context_http_ranged_requests,
                            test_fixture_teardown);
                g_free (name);

                name = g_strdup_printf ("/context/http/default-handler/%s",
                                        *it);
                g_test_add (name,
                            ContextTestFixture,
                            *it,
                            test_fixture_setup,
                            test_gupnp_context_http_default_handler,
                            test_fixture_teardown);
                g_free (name);

                name = g_strdup_printf ("/context/http/language/default/%s",
                                        *it);
                g_test_add (name,
                            ContextTestFixture,
                            *it,
                            test_fixture_setup,
                            test_gupnp_context_http_language_default,
                            test_fixture_teardown);
                g_free (name);

                name = g_strdup_printf ("/context/http/language/serve-file/%s",
                                        *it);
                g_test_add (name,
                            ContextTestFixture,
                            *it,
                            test_fixture_setup,
                            test_gupnp_context_http_language_serve_file,
                            test_fixture_teardown);
                g_free (name);

                name = g_strdup_printf (
                        "/context/http/language/serve-folder/%s",
                        *it);
                g_test_add (name,
                            ContextTestFixture,
                            *it,
                            test_fixture_setup,
                            test_gupnp_context_http_language_serve_folder,
                            test_fixture_teardown);
                g_free (name);

                name = g_strdup_printf ("/context/http/host/folder-rewrite/%s",
                                        *it);
                g_test_add (name,
                            ContextTestFixture,
                            *it,
                            test_fixture_setup,
                            test_gupnp_context_http_folder_redirect,
                            test_fixture_teardown);
                g_free (name);

                name = g_strdup_printf ("/context/http/host/for-agent/%s", *it);
                g_test_add (name,
                            ContextTestFixture,
                            *it,
                            test_fixture_setup,
                            test_gupnp_context_host_for_agent,
                            test_fixture_teardown);
                g_free (name);

                name = g_strdup_printf ("/context/http/host/agent-cache-from-request/%s", *it);
                g_test_add (name,
                            ContextTestFixture,
                            *it,
                            test_fixture_setup,
                            test_gupnp_context_host_path_user_agent_cache_filled_from_request,
                            test_fixture_teardown);
                g_free (name);

                name = g_strdup_printf ("/context/http/host/agent-cache-from-prefill/%s", *it);
                g_test_add (name,
                            ContextTestFixture,
                            *it,
                            test_fixture_setup,
                            test_gupnp_context_host_path_user_agent_cache_prefilled,
                            test_fixture_teardown);
                g_free (name);

                name = g_strdup_printf ("/context/http/host/invalid-methods/%s", *it);
                g_test_add (name,
                            ContextTestFixture,
                            *it,
                            test_fixture_setup,
                            test_gupnp_context_host_path_invalid_methods,
                            test_fixture_teardown);
                g_free (name);

                it++;
        }

        int result = g_test_run ();

        g_strfreev (data);

        return result;
}
