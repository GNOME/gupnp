/*
 * Copyright (C) 2021 Jens Georg.
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <config.h>

#include "libgupnp/gupnp.h"
#include "libgupnp/gupnp-service-private.h"

#include <string.h>
#include <libsoup/soup.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

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
        GUPnPContext *server_context;
        GUPnPContext *client_context;
        GUPnPRootDevice *rd;
        GUPnPServiceInfo *service;
        GUPnPControlPoint *cp;
        GUPnPServiceProxy *proxy;
        gpointer payload;
} ProxyTestFixture;

static gboolean
test_on_timeout (gpointer user_data)
{
        g_print ("Timeout in %s\n", (const char *) user_data);
        g_assert_not_reached ();

        return FALSE;
}

static void
test_run_loop (GMainLoop *loop, const char *name)
{
        guint timeout_id = 0;
        int timeout = 2;

        const char *timeout_str = g_getenv ("GUPNP_TEST_TIMEOUT");
        if (timeout_str != NULL) {
                long t = atol (timeout_str);
                if (t != 0)
                        timeout = t;
        }

        timeout_id = g_timeout_add_seconds (timeout,
                                            test_on_timeout,
                                            (gpointer) name);
        g_main_loop_run (loop);
        g_source_remove (timeout_id);
}

static void
on_proxy_available (G_GNUC_UNUSED GUPnPControlPoint *cp,
                    GUPnPServiceProxy *proxy,
                    gpointer user_data)
{
        ProxyTestFixture *tf = user_data;

        tf->proxy = g_object_ref (proxy);
        g_main_loop_quit (tf->loop);
}

static void
test_fixture_setup (ProxyTestFixture *tf, gconstpointer user_data)
{
        GError *error = NULL;

        tf->loop = g_main_loop_new (NULL, FALSE);
        g_assert_nonnull (tf->loop);

        // Create server part
        tf->server_context =
                create_context ((const char *) user_data, 0, &error);
        g_assert_nonnull (tf->server_context);
        g_assert_no_error (error);
        GUPnPResourceFactory *factory = gupnp_resource_factory_new ();
        tf->rd = gupnp_root_device_new_full (tf->server_context,
                                             factory,
                                             NULL,
                                             "TestDevice.xml",
                                             DATA_PATH,
                                             &error);
        g_object_unref (factory);
        g_assert_no_error (error);
        g_assert_nonnull (tf->rd);
        tf->service = gupnp_device_info_get_service (
                GUPNP_DEVICE_INFO (tf->rd),
                "urn:test-gupnp-org:service:TestService:1");

        // Create client part
        tf->client_context =
                create_context ((const char *) user_data, 0, &error);

        g_assert_nonnull (tf->client_context);
        g_assert_no_error (error);
        tf->cp = gupnp_control_point_new (
                tf->client_context,
                "urn:test-gupnp-org:service:TestService:1");

        gulong id = g_signal_connect (tf->cp,
                                      "service-proxy-available",
                                      G_CALLBACK (on_proxy_available),
                                      tf);
        gupnp_root_device_set_available (tf->rd, TRUE);
        gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (tf->cp),
                                           TRUE);
        test_run_loop (tf->loop, "Test fixture setup");
        g_signal_handler_disconnect (tf->cp, id);
}

static gboolean
delayed_loop_quitter (gpointer user_data)
{
        g_main_loop_quit (user_data);
        return G_SOURCE_REMOVE;
}

static void
test_fixture_teardown (ProxyTestFixture *tf, gconstpointer user_data)
{
        g_clear_object (&tf->proxy);
        g_object_unref (tf->cp);
        g_object_unref (tf->client_context);

        g_object_unref (tf->service);
        g_object_unref (tf->rd);
        g_object_unref (tf->server_context);

        // Make sure the source teardown handlers get run so we don't confuse valgrind
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);
        g_main_loop_unref (tf->loop);
}

// Test that calls a remote function and does not even connect to any callback
// Is is mainly useful to check in combination with ASAN/Valgrind to make sure
// that nothing gets leaked on the way
void
test_fire_and_forget (ProxyTestFixture *tf,
                      G_GNUC_UNUSED gconstpointer user_data)
{
        // Run fire and forget for action that does not have any kind of arguments
        GUPnPServiceProxyAction *action =
                gupnp_service_proxy_action_new ("Ping", NULL);

        gupnp_service_proxy_call_action_async (tf->proxy,
                                               action,
                                               NULL,
                                               NULL,
                                               NULL);
        gupnp_service_proxy_action_unref (action);

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);

        // Run fire and forget for a more complex action
        action = gupnp_service_proxy_action_new ("Browse",
                                                 "ObjectID",
                                                 G_TYPE_STRING,
                                                 "0",
                                                 "BrowseFlag",
                                                 G_TYPE_STRING,
                                                 "BrowseDirectChildren",
                                                 "Filter",
                                                 G_TYPE_STRING,
                                                 "res,dc:date,res@size",
                                                 "StartingIndex",
                                                 G_TYPE_UINT,
                                                 0,
                                                 "RequestedCount",
                                                 G_TYPE_UINT,
                                                 0,
                                                 "SortCriteria",
                                                 G_TYPE_STRING,
                                                 "",
                                                 NULL);

        gupnp_service_proxy_call_action_async (tf->proxy,
                                               action,
                                               NULL,
                                               NULL,
                                               NULL);
        gupnp_service_proxy_action_unref (action);

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);
}

void
on_test_async_call_ping_success (G_GNUC_UNUSED GUPnPService *service,
                                 G_GNUC_UNUSED GUPnPServiceAction *action,
                                 G_GNUC_UNUSED gpointer user_data)
{
        gupnp_service_action_return_success (action);
}

void
on_test_async_call (GObject *source, GAsyncResult *res, gpointer user_data)
{
        GError *error = NULL;
        g_assert_nonnull (user_data);

        gupnp_service_proxy_call_action_finish (GUPNP_SERVICE_PROXY (source),
                                                res,
                                                &error);
        g_assert_no_error (error);

        ProxyTestFixture *tf = (ProxyTestFixture *) user_data;
        g_main_loop_quit (tf->loop);
}

void
test_async_call (ProxyTestFixture *tf, G_GNUC_UNUSED gconstpointer user_data)
{
        // Run fire and forget for action that does not have any kind of arguments
        GUPnPServiceProxyAction *action =
                gupnp_service_proxy_action_new ("Ping", NULL);

        g_signal_connect (tf->service,
                          "action-invoked::Ping",
                          G_CALLBACK (on_test_async_call_ping_success),
                          tf);

        g_signal_connect (tf->service,
                          "action-invoked::Browse",
                          G_CALLBACK (on_test_async_call_ping_success),
                          tf);

        gupnp_service_proxy_call_action_async (tf->proxy,
                                               action,
                                               NULL,
                                               on_test_async_call,
                                               tf);
        gupnp_service_proxy_action_unref (action);
        test_run_loop(tf->loop, g_test_get_path());

        // Run fire and forget for a more complex action
        action = gupnp_service_proxy_action_new ("Browse",
                                                 "ObjectID",
                                                 G_TYPE_STRING,
                                                 "0",
                                                 "BrowseFlag",
                                                 G_TYPE_STRING,
                                                 "BrowseDirectChildren",
                                                 "Filter",
                                                 G_TYPE_STRING,
                                                 "res,dc:date,res@size",
                                                 "StartingIndex",
                                                 G_TYPE_UINT,
                                                 0,
                                                 "RequestedCount",
                                                 G_TYPE_UINT,
                                                 0,
                                                 "SortCriteria",
                                                 G_TYPE_STRING,
                                                 "",
                                                 NULL);

        gupnp_service_proxy_call_action_async (tf->proxy,
                                               action,
                                               NULL,
                                               on_test_async_call,
                                               tf);
        gupnp_service_proxy_action_unref (action);
        test_run_loop (tf->loop, g_test_get_path ());

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);
}

void
on_test_async_call_ping_delay (G_GNUC_UNUSED GUPnPService *service,
                               G_GNUC_UNUSED GUPnPServiceAction *action,
                               gpointer user_data)
{
        g_debug ("=> Ping delay");
        ProxyTestFixture *tf = (ProxyTestFixture *) user_data;
        tf->payload = action;
        g_main_loop_quit (tf->loop);
}

void
on_test_async_cancel_call (GObject *source,
                           GAsyncResult *res,
                           gpointer user_data)
{
        GError *error = NULL;
        g_assert_nonnull (user_data);

        gupnp_service_proxy_call_action_finish (GUPNP_SERVICE_PROXY (source),
                                                res,
                                                &error);

        g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
        g_clear_error (&error);

        ProxyTestFixture *tf = (ProxyTestFixture *) user_data;
        g_main_loop_quit (tf->loop);
}

void
test_async_cancel_call (ProxyTestFixture *tf,
                        G_GNUC_UNUSED gconstpointer user_data)
{
        GUPnPServiceProxyAction *action =
                gupnp_service_proxy_action_new ("Ping", NULL);

        g_signal_connect (tf->service,
                          "action-invoked::Ping",
                          G_CALLBACK (on_test_async_call_ping_delay),
                          tf);

        GCancellable *cancellable = g_cancellable_new ();
        gupnp_service_proxy_call_action_async (tf->proxy,
                                               action,
                                               cancellable,
                                               on_test_async_cancel_call,
                                               tf);

        // This should be called by the action callback
        test_run_loop (tf->loop, g_test_get_path ());
        g_cancellable_cancel (cancellable);

        // This should be finished by the now-cancelled proxy call
        test_run_loop (tf->loop, g_test_get_path ());

        // Free action. There should not be any callback
        gupnp_service_action_return_success (
                (GUPnPServiceAction *) tf->payload);

        GError *error = NULL;
        gupnp_service_proxy_action_get_result (action, &error, NULL);
        g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
        g_clear_error (&error);

        gupnp_service_proxy_action_unref (action);
        g_object_unref (cancellable);

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);
}


void
test_async_call_destroy_with_pending (ProxyTestFixture *tf,
                                      gconstpointer user_data)
{

        GList *actions = NULL;
        // Since the session in the context is using defaults, we can only have
        // two concurrent connections on a remote host
        for (int i = 0; i < 2; i++) {
                GUPnPServiceProxyAction *action =
                        gupnp_service_proxy_action_new ("Ping", NULL);

                gupnp_service_proxy_call_action_async (tf->proxy,
                                                       action,
                                                       NULL,
                                                       NULL,
                                                       NULL);
                gupnp_service_proxy_action_unref (action);

                // This should be called by the action callback
                gulong id = g_signal_connect (
                        tf->service,
                        "action-invoked::Ping",
                        G_CALLBACK (on_test_async_call_ping_delay),
                        tf);
                test_run_loop (tf->loop, g_test_get_path ());
                g_signal_handler_disconnect (tf->service, id);

                actions = g_list_prepend (actions, tf->payload);
        }

        // free the actions
        g_list_free_full (actions, (GDestroyNotify) gupnp_service_action_unref);

        g_clear_object (&tf->proxy);

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);
}

typedef struct {
        GMainLoop *outer_loop;
        GMainContext *outer_context;
        const char *address;
        GCancellable *cancellable;
        GError expected_error;
} ThreadData;

typedef struct {
        GMainLoop *loop;
        GUPnPServiceProxy *p;
} GetProxyData;

void
thead_on_proxy_available (GUPnPControlPoint *cp,
                          GUPnPServiceProxy *p,
                          gpointer user_data)
{
        GetProxyData *d = (GetProxyData *) user_data;
        d->p = g_object_ref (p);
        g_main_loop_quit (d->loop);
}

gboolean
exit_outer_loop (gpointer user_data)
{
        ThreadData *d = (ThreadData *) user_data;
        g_main_loop_quit (d->outer_loop);

        return G_SOURCE_REMOVE;
}

gpointer
thread_func (gpointer data)
{
        ThreadData *d = (ThreadData *) data;
        GMainContext *context = g_main_context_new ();
        GError *error = NULL;
        g_main_context_push_thread_default (context);

        GUPnPContext *ctx = create_context (d->address, 0, &error);
        g_assert_no_error (error);
        g_assert_nonnull (ctx);

        GUPnPControlPoint *cp = gupnp_control_point_new (
                ctx,
                "urn:test-gupnp-org:service:TestService:1");
        GetProxyData gpd;
        gulong id = g_signal_connect (cp,
                                      "service-proxy-available",
                                      G_CALLBACK (thead_on_proxy_available),
                                      &gpd);
        gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

        gpd.loop = g_main_loop_new (context, FALSE);
        test_run_loop (gpd.loop, "Test thread setup");
        g_signal_handler_disconnect (cp, id);

        GUPnPServiceProxyAction *action =
                gupnp_service_proxy_action_new ("Ping", NULL);

        gupnp_service_proxy_call_action (gpd.p, action, d->cancellable, &error);
        gupnp_service_proxy_action_unref (action);

        if (d->expected_error.domain != 0) {
                g_assert_error (error,
                                d->expected_error.domain,
                                d->expected_error.code);
                g_clear_error (&error);
        }

        g_object_unref (gpd.p);
        g_object_unref (cp);
        g_object_unref (ctx);

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, gpd.loop);
        g_main_loop_run (gpd.loop);

        g_main_loop_unref (gpd.loop);
        g_main_context_pop_thread_default (context);
        g_main_context_unref (context);

        g_main_context_invoke (d->outer_context, exit_outer_loop, d);

        return NULL;
}

void
test_sync_call (ProxyTestFixture *tf, gconstpointer user_data)
{
        g_signal_connect (tf->service,
                          "action-invoked::Ping",
                          G_CALLBACK (on_test_async_call_ping_success),
                          tf);
        ThreadData d;
        d.address = (const char *) user_data;
        d.outer_context = g_main_context_get_thread_default ();
        d.outer_loop = tf->loop;
        d.cancellable = NULL;
        d.expected_error.domain = 0;

        GThread *t = g_thread_new ("Sync call test", thread_func, &d);
        test_run_loop (tf->loop, g_test_get_path());
        g_thread_join (t);

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);
}

gboolean
cancel_sync_call (gpointer user_data)
{
        ThreadData *d = (ThreadData *) user_data;

        g_print ("Cancelling...\n");
        g_cancellable_cancel (d->cancellable);

        return G_SOURCE_REMOVE;
}

void
test_cancel_sync_call (ProxyTestFixture *tf, gconstpointer user_data)
{
        g_signal_connect (tf->service,
                          "action-invoked::Ping",
                          G_CALLBACK (on_test_async_call_ping_delay),
                          tf);
        ThreadData d;
        d.address = (const char *) user_data;
        d.outer_context = g_main_context_get_thread_default ();
        d.outer_loop = tf->loop;
        d.cancellable = g_cancellable_new ();
        d.expected_error.domain = G_IO_ERROR;
        d.expected_error.code = G_IO_ERROR_CANCELLED;

        GThread *t = g_thread_new ("Sync call cancel test", thread_func, &d);
        test_run_loop (tf->loop, g_test_get_path());

        g_timeout_add_seconds (1, (GSourceFunc) cancel_sync_call, &d);
        test_run_loop (tf->loop, g_test_get_path());

        g_thread_join (t);

        // Free action. There should not be any callback
        gupnp_service_action_return_success (
                (GUPnPServiceAction *) tf->payload);

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);
        g_object_unref (d.cancellable);
}

void
on_test_async_call_ping_error (G_GNUC_UNUSED GUPnPService *service,
                               GUPnPServiceAction *action,
                               gpointer user_data)
{
        gupnp_service_action_return_error (action,
                                           GUPNP_CONTROL_ERROR_OUT_OF_SYNC,
                                           "Test error");
}

void
on_test_soap_error (GObject *source, GAsyncResult *res, gpointer user_data)
{
        GError *error = NULL;
        g_assert_nonnull (user_data);

        gupnp_service_proxy_call_action_finish (GUPNP_SERVICE_PROXY (source),
                                                res,
                                                &error);
        g_assert_error (error,
                        GUPNP_CONTROL_ERROR,
                        GUPNP_CONTROL_ERROR_OUT_OF_SYNC);

        g_clear_error (&error);

        ProxyTestFixture *tf = (ProxyTestFixture *) user_data;
        g_main_loop_quit (tf->loop);
}

void
test_finish_soap_error (ProxyTestFixture *tf, gconstpointer user_data)
{
        g_signal_connect (tf->service,
                          "action-invoked::Ping",
                          G_CALLBACK (on_test_async_call_ping_error),
                          tf);

        GUPnPServiceProxyAction *action =
                gupnp_service_proxy_action_new ("Ping", NULL);

        gupnp_service_proxy_call_action_async (tf->proxy,
                                               action,
                                               NULL,
                                               on_test_soap_error,
                                               tf);

        gupnp_service_proxy_action_unref (action);
        test_run_loop (tf->loop, g_test_get_path ());

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);
}

void
test_finish_soap_error_sync (ProxyTestFixture *tf, gconstpointer user_data)
{
        g_signal_connect (tf->service,
                          "action-invoked::Ping",
                          G_CALLBACK (on_test_async_call_ping_error),
                          tf);
        ThreadData d;
        d.address = (const char *) user_data;
        d.outer_context = g_main_context_get_thread_default ();
        d.outer_loop = tf->loop;
        d.cancellable = NULL;
        d.expected_error.domain = GUPNP_CONTROL_ERROR;
        d.expected_error.code = GUPNP_CONTROL_ERROR_OUT_OF_SYNC;

        GThread *t = g_thread_new ("Sync call test", thread_func, &d);
        test_run_loop (tf->loop, g_test_get_path ());
        g_thread_join (t);

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);
}

void
auth_message_callback (GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
        ProxyTestFixture *tf = user_data;
        GError *error = NULL;
        GBytes *bytes = soup_session_send_and_read_finish (SOUP_SESSION (source),
                                                      res,
                                                      &error);
        g_assert_no_error (error);
        g_assert_null (g_bytes_get_data (bytes, NULL));

        g_main_loop_quit (tf->loop);
}

static gboolean
on_auth_verification_callback (SoupAuthDomain    *domain,
                               SoupServerMessage *msg,
                               const char        *username,
                               const char        *password,
                               gpointer           user_data)
{
        if (g_strcmp0 (username, "user") == 0 && g_strcmp0 (password, "password") == 0)
                return TRUE;

        return FALSE;
}

void
on_test_async_unauth_call (GObject *source, GAsyncResult *res, gpointer user_data)
{
        GError *error = NULL;
        g_assert_nonnull (user_data);

        gupnp_service_proxy_call_action_finish (GUPNP_SERVICE_PROXY (source),
                                                res,
                                                &error);
        g_assert_nonnull (error);
        g_assert_error (error, GUPNP_SERVER_ERROR, GUPNP_SERVER_ERROR_OTHER);
        g_clear_error (&error);

        ProxyTestFixture *tf = (ProxyTestFixture *) user_data;
        g_main_loop_quit (tf->loop);
}

void
on_test_async_auth_call (GObject *source, GAsyncResult *res, gpointer user_data)
{
        GError *error = NULL;
        g_assert_nonnull (user_data);

        gupnp_service_proxy_call_action_finish (GUPNP_SERVICE_PROXY (source),
                                                res,
                                                &error);
        g_assert_no_error (error);

        ProxyTestFixture *tf = (ProxyTestFixture *) user_data;
        g_main_loop_quit (tf->loop);
}

void
test_finish_soap_authentication_no_credentials (ProxyTestFixture *tf, gconstpointer user_data)
{
        SoupServer *soup_server = gupnp_context_get_server (tf->server_context);
        SoupAuthDomain *auth_domain;
        GUPnPServiceProxyAction *action;

        auth_domain = soup_auth_domain_basic_new ("realm", "Test", NULL);
        soup_auth_domain_add_path (auth_domain, "/TestService/Control");
        soup_auth_domain_basic_set_auth_callback (auth_domain,
                                                  on_auth_verification_callback,
                                                  tf,
                                                  NULL);
        soup_server_add_auth_domain (soup_server, auth_domain);

        g_signal_connect (tf->service,
                          "action-invoked::Ping",
                          G_CALLBACK (on_test_async_call_ping_success),
                          tf);

        action = gupnp_service_proxy_action_new ("Ping", NULL);


        gupnp_service_proxy_call_action_async (tf->proxy,
                                               action,
                                               NULL,
                                               on_test_async_unauth_call,
                                               tf);
        gupnp_service_proxy_action_unref (action);
        test_run_loop(tf->loop, g_test_get_path());

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);

        g_object_unref (auth_domain);
}

void
test_finish_soap_authentication_wrong_credentials (ProxyTestFixture *tf, gconstpointer user_data)
{
        SoupServer *soup_server = gupnp_context_get_server (tf->server_context);
        SoupAuthDomain *auth_domain;
        GUPnPServiceProxyAction *action;

        auth_domain = soup_auth_domain_basic_new ("realm", "Test", NULL);
        soup_auth_domain_add_path (auth_domain, "/TestService/Control");
        soup_auth_domain_basic_set_auth_callback (auth_domain,
                                                  on_auth_verification_callback,
                                                  tf,
                                                  NULL);
        soup_server_add_auth_domain (soup_server, auth_domain);

        g_signal_connect (tf->service,
                          "action-invoked::Ping",
                          G_CALLBACK (on_test_async_call_ping_success),
                          tf);

        gupnp_service_proxy_set_credentials (tf->proxy, "user", "wrong_password");
        action = gupnp_service_proxy_action_new ("Ping", NULL);

        gupnp_service_proxy_call_action_async (tf->proxy,
                                               action,
                                               NULL,
                                               on_test_async_unauth_call,
                                               tf);
        gupnp_service_proxy_action_unref (action);
        test_run_loop(tf->loop, g_test_get_path());

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);

        g_object_unref (auth_domain);
}

void
test_finish_soap_authentication_valid_credentials (ProxyTestFixture *tf, gconstpointer user_data)
{
        SoupServer *soup_server = gupnp_context_get_server (tf->server_context);
        SoupAuthDomain *auth_domain;
        GUPnPServiceProxyAction *action;

        auth_domain = soup_auth_domain_basic_new ("realm", "Test", NULL);
        soup_auth_domain_add_path (auth_domain, "/TestService/Control");
        soup_auth_domain_basic_set_auth_callback (auth_domain,
                                                  on_auth_verification_callback,
                                                  tf,
                                                  NULL);
        soup_server_add_auth_domain (soup_server, auth_domain);

        g_signal_connect (tf->service,
                          "action-invoked::Ping",
                          G_CALLBACK (on_test_async_call_ping_success),
                          tf);

        gupnp_service_proxy_set_credentials (tf->proxy, "user", "password");
        action = gupnp_service_proxy_action_new ("Ping", NULL);

        gupnp_service_proxy_call_action_async (tf->proxy,
                                               action,
                                               NULL,
                                               on_test_async_auth_call,
                                               tf);
        gupnp_service_proxy_action_unref (action);
        test_run_loop(tf->loop, g_test_get_path());

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);
        g_object_unref (auth_domain);
}

void
on_test_action_iter_call_browse (G_GNUC_UNUSED GUPnPService *service,
                                 GUPnPServiceAction *action,
                                 G_GNUC_UNUSED gpointer user_data)
{
        gupnp_service_action_set (action,
                                  "Result",
                                  G_TYPE_STRING,
                                  "FAKE_RESULT",
                                  "NumberReturned",
                                  G_TYPE_UINT,
                                  10,
                                  "TotalMatches",
                                  G_TYPE_UINT,
                                  10,
                                  "UpdateID",
                                  G_TYPE_UINT,
                                  12345,
                                  NULL);
        gupnp_service_action_return_success (action);
}

void
test_action_iter (ProxyTestFixture *tf, gconstpointer user_data)
{
        g_signal_connect (tf->service,
                          "action-invoked::Browse",
                          G_CALLBACK (on_test_action_iter_call_browse),
                          tf);

        GUPnPServiceProxyAction *action;
        action = gupnp_service_proxy_action_new ("Browse",
                                                 "ObjectID",
                                                 G_TYPE_STRING,
                                                 "0",
                                                 "BrowseFlag",
                                                 G_TYPE_STRING,
                                                 "BrowseDirectChildren",
                                                 "Filter",
                                                 G_TYPE_STRING,
                                                 "res,dc:date,res@size",
                                                 "StartingIndex",
                                                 G_TYPE_UINT,
                                                 0,
                                                 "RequestedCount",
                                                 G_TYPE_UINT,
                                                 0,
                                                 "SortCriteria",
                                                 G_TYPE_STRING,
                                                 "",
                                                 NULL);

        gupnp_service_proxy_call_action_async (tf->proxy,
                                               action,
                                               NULL,
                                               on_test_async_auth_call,
                                               tf);
        test_run_loop (tf->loop, g_test_get_path ());

        GError *error = NULL;
        GUPnPServiceProxyActionIter *iter =
                gupnp_service_proxy_action_iterate (action, &error);
        g_assert (G_OBJECT_TYPE (iter) != G_TYPE_NONE);
        g_assert (G_IS_OBJECT (iter));
        g_assert_no_error (error);
        g_assert_nonnull (iter);

        g_assert (gupnp_service_proxy_action_iter_next (iter));

        g_assert_cmpstr (gupnp_service_proxy_action_iter_get_name (iter),
                         ==,
                         "Result");

        GValue value = G_VALUE_INIT;
        g_assert (gupnp_service_proxy_action_iter_get_value (iter, &value));
        g_assert (G_VALUE_HOLDS_STRING (&value));
        g_assert_cmpstr (g_value_get_string (&value), ==, "FAKE_RESULT");
        g_value_unset (&value);

        g_assert (gupnp_service_proxy_action_iter_next (iter));

        g_assert_cmpstr (gupnp_service_proxy_action_iter_get_name (iter),
                         ==,
                         "NumberReturned");

        g_assert (gupnp_service_proxy_action_iter_get_value (iter, &value));
        g_assert (G_VALUE_HOLDS_STRING (&value));
        g_assert_cmpstr (g_value_get_string (&value), ==, "10");
        g_value_unset (&value);

        g_assert (gupnp_service_proxy_action_iter_next (iter));

        g_assert_cmpstr (gupnp_service_proxy_action_iter_get_name (iter),
                         ==,
                         "TotalMatches");

        g_assert (gupnp_service_proxy_action_iter_get_value (iter, &value));
        g_assert (G_VALUE_HOLDS_STRING (&value));
        g_assert_cmpstr (g_value_get_string (&value), ==, "10");
        g_value_unset (&value);

        gupnp_service_proxy_action_unref (action);
        g_assert (gupnp_service_proxy_action_iter_next (iter));

        g_assert_cmpstr (gupnp_service_proxy_action_iter_get_name (iter),
                         ==,
                         "UpdateID");

        g_assert (gupnp_service_proxy_action_iter_get_value (iter, &value));
        g_assert (G_VALUE_HOLDS_STRING (&value));
        g_assert_cmpstr (g_value_get_string (&value), ==, "12345");
        g_value_unset (&value);

        g_assert (!gupnp_service_proxy_action_iter_next (iter));

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);

        g_object_unref (iter);
}

void
on_introspection (GObject *source, GAsyncResult *res, gpointer data)
{
        ProxyTestFixture *tf = data;
        GError *error = NULL;

        GObject *obj = G_OBJECT (gupnp_service_info_introspect_finish (
                GUPNP_SERVICE_INFO (source),
                res,
                &error));
        g_assert_no_error (error);
        g_assert_nonnull (obj);
        g_object_unref (obj);

        g_main_loop_quit (tf->loop);
}

void
test_action_iter_introspected (ProxyTestFixture *tf, gconstpointer user_data)
{
        gupnp_service_info_introspect_async (GUPNP_SERVICE_INFO (tf->proxy),
                                             NULL,
                                             on_introspection,
                                             tf);

        test_run_loop (tf->loop, g_test_get_path ());

        g_signal_connect (tf->service,
                          "action-invoked::Browse",
                          G_CALLBACK (on_test_action_iter_call_browse),
                          tf);

        GUPnPServiceProxyAction *action;
        action = gupnp_service_proxy_action_new ("Browse",
                                                 "ObjectID",
                                                 G_TYPE_STRING,
                                                 "0",
                                                 "BrowseFlag",
                                                 G_TYPE_STRING,
                                                 "BrowseDirectChildren",
                                                 "Filter",
                                                 G_TYPE_STRING,
                                                 "res,dc:date,res@size",
                                                 "StartingIndex",
                                                 G_TYPE_UINT,
                                                 0,
                                                 "RequestedCount",
                                                 G_TYPE_UINT,
                                                 0,
                                                 "SortCriteria",
                                                 G_TYPE_STRING,
                                                 "",
                                                 NULL);

        gupnp_service_proxy_call_action_async (tf->proxy,
                                               action,
                                               NULL,
                                               on_test_async_auth_call,
                                               tf);
        test_run_loop (tf->loop, g_test_get_path ());

        GError *error = NULL;
        GUPnPServiceProxyActionIter *iter =
                gupnp_service_proxy_action_iterate (action, &error);
        g_assert_no_error (error);
        g_assert_nonnull (iter);

        g_assert (gupnp_service_proxy_action_iter_next (iter));

        g_assert_cmpstr (gupnp_service_proxy_action_iter_get_name (iter),
                         ==,
                         "Result");

        GValue value = G_VALUE_INIT;

        g_assert (gupnp_service_proxy_action_iter_get_value (iter, &value));
        g_assert (G_VALUE_HOLDS_STRING (&value));
        g_assert_cmpstr (g_value_get_string (&value), ==, "FAKE_RESULT");
        g_value_unset (&value);

        g_assert (gupnp_service_proxy_action_iter_next (iter));

        g_assert_cmpstr (gupnp_service_proxy_action_iter_get_name (iter),
                         ==,
                         "NumberReturned");

        g_assert (gupnp_service_proxy_action_iter_get_value (iter, &value));
        g_assert (G_VALUE_HOLDS_UINT (&value));
        g_assert_cmpuint (g_value_get_uint (&value), ==, 10);
        g_value_unset (&value);

        g_assert (gupnp_service_proxy_action_iter_next (iter));

        g_assert_cmpstr (gupnp_service_proxy_action_iter_get_name (iter),
                         ==,
                         "TotalMatches");

        g_assert (gupnp_service_proxy_action_iter_get_value (iter, &value));
        g_assert (G_VALUE_HOLDS_UINT (&value));
        g_assert_cmpuint (g_value_get_uint (&value), ==, 10);
        g_value_unset (&value);

        gupnp_service_proxy_action_unref (action);
        g_assert (gupnp_service_proxy_action_iter_next (iter));

        g_assert_cmpstr (gupnp_service_proxy_action_iter_get_name (iter),
                         ==,
                         "UpdateID");

        g_assert (gupnp_service_proxy_action_iter_get_value (iter, &value));
        g_assert (G_VALUE_HOLDS_UINT (&value));
        g_assert_cmpuint (g_value_get_uint (&value), ==, 12345);
        g_value_unset (&value);

        g_assert (!gupnp_service_proxy_action_iter_next (iter));

        // Spin the loop for a bit...
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, tf->loop);
        g_main_loop_run (tf->loop);

        g_object_unref (iter);
}

int
main (int argc, char *argv[])
{
        g_test_init (&argc, &argv, NULL);

        g_test_add ("/service-proxy/async/fire-and-forget",
                    ProxyTestFixture,
                    "127.0.0.1",
                    test_fixture_setup,
                    test_fire_and_forget,
                    test_fixture_teardown);

        g_test_add ("/service-proxy/async/call",
                    ProxyTestFixture,
                    "127.0.0.1",
                    test_fixture_setup,
                    test_async_call,
                    test_fixture_teardown);

        g_test_add ("/service-proxy/async/cancel",
                    ProxyTestFixture,
                    "127.0.0.1",
                    test_fixture_setup,
                    test_async_cancel_call,
                    test_fixture_teardown);

        g_test_add ("/service-proxy/async/destroy-with-pending",
                    ProxyTestFixture,
                    "127.0.0.1",
                    test_fixture_setup,
                    test_async_call_destroy_with_pending,
                    test_fixture_teardown);

        g_test_add ("/service-proxy/async/soap-error-in-finish",
                    ProxyTestFixture,
                    "127.0.0.1",
                    test_fixture_setup,
                    test_finish_soap_error,
                    test_fixture_teardown);

        g_test_add ("/service-proxy/sync/call",
                    ProxyTestFixture,
                    "127.0.0.1",
                    test_fixture_setup,
                    test_sync_call,
                    test_fixture_teardown);

        g_test_add ("/service-proxy/sync/cancel-call",
                    ProxyTestFixture,
                    "127.0.0.1",
                    test_fixture_setup,
                    test_cancel_sync_call,
                    test_fixture_teardown);

        g_test_add ("/service-proxy/sync/soap-error-in-finish",
                    ProxyTestFixture,
                    "127.0.0.1",
                    test_fixture_setup,
                    test_finish_soap_error_sync,
                    test_fixture_teardown);

        g_test_add ("/service-proxy/authentication/no-credentials",
                    ProxyTestFixture,
                    "127.0.0.1",
                    test_fixture_setup,
                    test_finish_soap_authentication_no_credentials,
                    test_fixture_teardown);

        g_test_add ("/service-proxy/authentication/wrong-credentials",
                    ProxyTestFixture,
                    "127.0.0.1",
                    test_fixture_setup,
                    test_finish_soap_authentication_wrong_credentials,
                    test_fixture_teardown);

        g_test_add ("/service-proxy/authentication/valid-credentials",
                    ProxyTestFixture,
                    "127.0.0.1",
                    test_fixture_setup,
                    test_finish_soap_authentication_valid_credentials,
                    test_fixture_teardown);

        g_test_add ("/service-proxy/action/iter",
                    ProxyTestFixture,
                    "127.0.0.1",
                    test_fixture_setup,
                    test_action_iter,
                    test_fixture_teardown);

        g_test_add ("/service-proxy/action/iter_introspected",
                    ProxyTestFixture,
                    "127.0.0.1",
                    test_fixture_setup,
                    test_action_iter_introspected,
                    test_fixture_teardown);

        return g_test_run ();
}
