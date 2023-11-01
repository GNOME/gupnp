// SPDX-License-Identifier: LGPL-2.1-or-later

#include <config.h>

#include <libgupnp/gupnp-xml-doc.h>

#include <libgupnp/gupnp-context-private.h>
#include <libgupnp/gupnp-service-private.h>
#include <libgupnp/gupnp.h>

static GUPnPContext *
create_context (guint16 port, GError **error)
{
        return GUPNP_CONTEXT (g_initable_new (GUPNP_TYPE_CONTEXT,
                                              NULL,
                                              error,
                                              "host-ip",
                                              "127.0.0.1",
                                              "port",
                                              port,
                                              NULL));
}

typedef struct {
        GMainLoop *loop;
        SoupServerMessage *message;
} TestServiceNotificationCancelledData;

void
on_notify (SoupServer *server,
           SoupServerMessage *msg,
           const char *path,
           GHashTable *query,
           gpointer user_data)
{
        TestServiceNotificationCancelledData *data = user_data;

        // Pause message, quit mainlopp
#if SOUP_CHECK_VERSION(3, 1, 2)
        soup_server_message_pause (msg);
#else
        soup_server_pause_message (server, msg);
#endif
        data->message = msg;
        g_main_loop_quit (data->loop);
}

void
on_subscribe (GObject *source, GAsyncResult *res, gpointer user_data)
{
        GError *error = NULL;

        GBytes *data = soup_session_send_and_read_finish (SOUP_SESSION (source),
                                                          res,
                                                          &error);

        g_assert_no_error (error);
        g_clear_pointer (&data, g_bytes_unref);
}

static void
on_finished (SoupServerMessage *msg, TestServiceNotificationCancelledData *data)
{
        g_assert_cmpint (soup_server_message_get_status (msg),
                         ==,
                         SOUP_STATUS_INTERNAL_SERVER_ERROR);
        g_main_loop_quit (data->loop);
}

static SoupMessage *
prepare_subscribe_message (const char *subscription_uri, SoupServer *server)
{
        SoupMessage *msg = soup_message_new ("SUBSCRIBE", subscription_uri);

        GSList *uris = soup_server_get_uris (server);
        GUri *subscription =
                soup_uri_copy (uris->data, SOUP_URI_PATH, "/Notify", NULL);
        char *uri_string = g_uri_to_string (subscription);
        char *callback = g_strdup_printf ("<%s>", uri_string);
        g_free (uri_string);
        g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);
        g_uri_unref (subscription);

        SoupMessageHeaders *h = soup_message_get_request_headers (msg);
        soup_message_headers_append (h, "Callback", callback);
        g_free (callback);

        soup_message_headers_append (h, "NT", "upnp:event");

        return msg;
}

static void
test_service_notification_cancelled ()
{
        // Check that the notification message is cancelled correctly if the service is
        // Shut down during sending
        GUPnPContext *context = NULL;
        GError *error = NULL;
        GUPnPRootDevice *rd;
        GUPnPServiceInfo *info = NULL;

        TestServiceNotificationCancelledData data = { NULL, NULL };

        data.loop = g_main_loop_new (NULL, FALSE);

        context = create_context (0, &error);
        g_assert_no_error (error);
        g_assert (context != NULL);

        rd = gupnp_root_device_new (context,
                                    "TestDevice.xml",
                                    DATA_PATH,
                                    &error);
        g_assert_no_error (error);
        g_assert (rd != NULL);
        gupnp_root_device_set_available (rd, TRUE);

        SoupServer *server = soup_server_new (NULL, NULL);
        soup_server_add_handler (server, "/Notify", on_notify, &data, NULL);
        soup_server_listen_local (server,
                                  0,
                                  SOUP_SERVER_LISTEN_IPV4_ONLY,
                                  &error);
        g_assert_no_error (error);

        // Generate SUBSCRIBE message
        info = gupnp_device_info_get_service (
                GUPNP_DEVICE_INFO (rd),
                "urn:test-gupnp-org:service:TestService:1");
        char *url = gupnp_service_info_get_event_subscription_url (info);

        SoupMessage *msg = prepare_subscribe_message (url, server);
        SoupSession *session = soup_session_new ();
        // FIXME: Add timeout header
        soup_session_send_and_read_async (session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          on_subscribe,
                                          &data);

        g_main_loop_run (data.loop);
        g_signal_connect (data.message,
                          "finished",
                          G_CALLBACK (on_finished),
                          &data);

        g_clear_object (&info);
        g_free (url);

#if SOUP_CHECK_VERSION(3, 1, 2)
        soup_server_message_unpause (data.message);
#else
        soup_server_unpause_message (server, data.message);
#endif

        g_main_loop_run (data.loop);
        g_clear_object (&rd);
        g_clear_object (&msg);
        g_clear_object (&session);
        g_clear_object (&server);
        g_clear_object (&context);
        g_main_loop_unref (data.loop);
}

static void
on_notify_failed (GUPnPService *service,
                  GList *callbacks,
                  GError *error,
                  gpointer user_data)
{
        TestServiceNotificationCancelledData *data = user_data;

        g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED);
        g_main_loop_quit (data->loop);
}

static void
test_service_notification_remote_disappears (void)
{
        // Check that the notification message is cancelled correctly if the service is
        // Shut down during sending
        GUPnPContext *context = NULL;
        GError *error = NULL;
        GUPnPRootDevice *rd;
        GUPnPServiceInfo *info = NULL;

        TestServiceNotificationCancelledData data = { NULL, NULL };

        data.loop = g_main_loop_new (NULL, FALSE);

        context = create_context (0, &error);
        g_assert_no_error (error);
        g_assert (context != NULL);

        rd = gupnp_root_device_new (context,
                                    "TestDevice.xml",
                                    DATA_PATH,
                                    &error);
        g_assert_no_error (error);
        g_assert (rd != NULL);
        gupnp_root_device_set_available (rd, TRUE);

        // Generate SUBSCRIBE message
        info = gupnp_device_info_get_service (
                GUPNP_DEVICE_INFO (rd),
                "urn:test-gupnp-org:service:TestService:1");
        char *url = gupnp_service_info_get_event_subscription_url (info);

        g_signal_connect (info,
                          "notify-failed",
                          G_CALLBACK (on_notify_failed),
                          &data);

        SoupMessage *msg = soup_message_new ("SUBSCRIBE", url);

        SoupMessageHeaders *h = soup_message_get_request_headers (msg);
        soup_message_headers_append (h, "Callback", "<http://127.0.0.1:1312>");

        soup_message_headers_append (h, "NT", "upnp:event");

        SoupSession *session = soup_session_new ();
        // FIXME: Add timeout header
        soup_session_send_and_read_async (session,
                                          msg,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          NULL,
                                          &data);

        g_main_loop_run (data.loop);

        g_clear_object (&info);

        g_clear_object (&rd);
        g_clear_object (&msg);
        g_clear_object (&session);
        g_clear_object (&context);
        g_main_loop_unref (data.loop);
        g_free (url);
}

int
main (int argc, char *argv[])
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/service/notify/cancel",
                         test_service_notification_cancelled);

        g_test_add_func ("/service/notify/handle-remote-disappering",
                         test_service_notification_remote_disappears);

        return g_test_run ();
}
