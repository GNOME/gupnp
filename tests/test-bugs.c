/*
 * Copyright (C) 2013 Intel Corporation.
 *
 * Author: Jens Georg <jensg@openismus.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <libgupnp/gupnp.h>
#include <libgupnp/gupnp-service-private.h>
#include <libgupnp/gupnp-context-private.h>

#include <libgssdp/gssdp-resource-group.h>
#include <libsoup/soup.h>


static GUPnPContext *
create_context (guint16 port, GError **error) {
        return GUPNP_CONTEXT (g_initable_new (GUPNP_TYPE_CONTEXT,
                                              NULL,
                                              error,
                                              "host-ip", "127.0.0.1",
                                              "msearch-port", port,
                                              NULL));
}

typedef struct _TestBgo678701Service {
    GUPnPServiceProxy parent_instance;
}TestBgo678701Service;

typedef struct _TestBgo678701ServiceClass
{
    GUPnPServiceProxyClass parent_class;
}TestBgo678701ServiceClass;

G_DEFINE_TYPE(TestBgo678701Service, test_bgo_678701_service, GUPNP_TYPE_SERVICE_PROXY);
static void test_bgo_678701_service_class_init (TestBgo678701ServiceClass *klass) {}
static void test_bgo_678701_service_init (TestBgo678701Service *self) {}

typedef struct _TestBgo678701Device {
    GUPnPDeviceProxy parent_instance;
}TestBgo678701Device;

typedef struct _TestBgo678701DeviceClass
{
    GUPnPDeviceProxyClass parent_class;
}TestBgo678701DeviceClass;

G_DEFINE_TYPE(TestBgo678701Device, test_bgo_678701_device, GUPNP_TYPE_DEVICE_PROXY);
static void test_bgo_678701_device_class_init (TestBgo678701DeviceClass *klass) {}
static void test_bgo_678701_device_init (TestBgo678701Device *self) {}


typedef struct _TestServiceProxyData {
    GMainLoop *loop;
    GUPnPServiceProxy *proxy;
} TestServiceProxyData;

typedef struct _TestBgo678701Data {
    GMainLoop *loop;
    GUPnPDeviceProxy *proxy;
} TestBgo678701Data;

static void
test_bgo_696762_on_browse_call (G_GNUC_UNUSED GUPnPService *service,
                                G_GNUC_UNUSED GUPnPServiceAction *action,
                                G_GNUC_UNUSED gpointer user_data)
{
    xmlNode *node = action->node->children;
    g_assert (node != NULL);
    g_assert_cmpstr ((const char *) node->name, ==, "ObjectID");
    node = node->next;

    g_assert (node != NULL);
    g_assert_cmpstr ((const char *) node->name, ==, "BrowseFlag");
    node = node->next;

    g_assert (node != NULL);
    g_assert_cmpstr ((const char *) node->name, ==, "Filter");
    node = node->next;

    g_assert (node != NULL);
    g_assert_cmpstr ((const char *) node->name, ==, "StartingIndex");
    node = node->next;

    g_assert (node != NULL);
    g_assert_cmpstr ((const char *) node->name, ==, "RequestedCount");
    node = node->next;

    g_assert (node != NULL);
    g_assert_cmpstr ((const char *) node->name, ==, "SortCriteria");
    gupnp_service_action_return_success (action);
}

static void
test_bgo_696762_on_browse (G_GNUC_UNUSED GObject       *source,
                           G_GNUC_UNUSED GAsyncResult *res,
                           gpointer                               user_data)
{
    TestServiceProxyData *data = (TestServiceProxyData *) user_data;

    g_main_loop_quit (data->loop);
}

static void
test_on_sp_available (G_GNUC_UNUSED GUPnPControlPoint *cp,
                      GUPnPServiceProxy               *proxy,
                      gpointer                         user_data)
{
    TestServiceProxyData *data = (TestServiceProxyData *) user_data;

    data->proxy = g_object_ref (proxy);

    g_main_loop_quit (data->loop);
}

static void
test_bgo_678701_on_dp_available (G_GNUC_UNUSED GUPnPControlPoint *cp,
                                 GUPnPDeviceProxy               *proxy,
                                 gpointer                         user_data)
{
    TestBgo678701Data *data = (TestBgo678701Data *) user_data;

    data->proxy = g_object_ref (proxy);

    g_main_loop_quit (data->loop);
}

void
test_bgo_690400_notify (GUPnPServiceProxy *proxy,
                        const char *variable,
                        GValue *value,
                        gpointer user_data)
{
    TestServiceProxyData *data = (TestServiceProxyData *) user_data;

    gupnp_service_proxy_remove_notify (data->proxy,
                                       "evented_variable",
                                       test_bgo_690400_notify,
                                       user_data);
}

void
test_bgo_690400_notify_too (GUPnPServiceProxy *proxy,
                            const char *variable,
                            GValue *value,
                            gpointer user_data)
{
    TestServiceProxyData *data = (TestServiceProxyData *) user_data;

    g_main_loop_quit (data->loop);
}

static void
test_bgo_690400_query_variable (GUPnPService *service,
                                gchar        *variable,
                                GValue       *value,
                                gpointer      user_data)
{
    g_value_init (value, G_TYPE_STRING);
    g_value_set_string (value, "New Value");
}

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

        timeout_id = g_timeout_add_seconds (timeout, test_on_timeout, NULL);
        g_main_loop_run (loop);
        g_source_remove (timeout_id);
}

static gboolean
delayed_loop_quitter (gpointer user_data)
{
        g_main_loop_quit (user_data);
        return G_SOURCE_REMOVE;
}

/* Test if a call on a service proxy keeps argument order */
static void
test_bgo_696762 (void)
{
    GUPnPContext *context = NULL;
    GUPnPContext *server_context = NULL;
    GError *error = NULL;
    GUPnPControlPoint *cp = NULL;
    GUPnPRootDevice *rd;
    TestServiceProxyData data = { NULL, NULL };
    GUPnPServiceInfo *info = NULL;

    data.loop = g_main_loop_new (NULL, FALSE);

    context = create_context (0, &error);
    g_assert_no_error (error);
    g_assert (context != NULL);

    server_context = create_context (0, &error);
    g_assert_no_error (error);
    g_assert (server_context != NULL);

    cp = gupnp_control_point_new (context,
                                  "urn:test-gupnp-org:service:TestService:1");

    gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

    g_signal_connect (G_OBJECT (cp),
                      "service-proxy-available",
                      G_CALLBACK (test_on_sp_available),
                      &data);


    rd = gupnp_root_device_new (server_context, "TestDevice.xml", DATA_PATH, &error);
    g_assert_no_error (error);
    g_assert (rd != NULL);
    gupnp_root_device_set_available (rd, TRUE);
    info = gupnp_device_info_get_service (GUPNP_DEVICE_INFO (rd),
                                          "urn:test-gupnp-org:service:TestService:1");
    g_signal_connect (G_OBJECT (info),
                      "action-invoked::Browse",
                      G_CALLBACK (test_bgo_696762_on_browse_call),
                      &data);

    test_run_loop (data.loop, g_test_get_path ());
    g_assert (data.proxy != NULL);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS

    GUPnPServiceProxyAction *a =
            gupnp_service_proxy_action_new ("Browse",
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

    gupnp_service_proxy_call_action_async (data.proxy,
                                           a,
                                           NULL,
                                           test_bgo_696762_on_browse,
                                           &data);


    test_run_loop (data.loop, g_test_get_path ());

    gupnp_service_proxy_action_unref (a);

    // tear-down
    g_object_unref (info);
    g_object_unref (data.proxy);

    g_object_unref (cp);
    g_object_unref (rd);
    g_object_unref (server_context);
    g_object_unref (context);

    // Make sure the source teardown handlers get run so we don't confuse valgrind
    g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, data.loop);
    g_main_loop_run (data.loop);

    g_main_loop_unref (data.loop);
}

/* Test that proxies created by ResourceFactory are of the GType
 * set with gupnp_resource_factory_register_resource_proxy_type().
 * https://bugzilla.gnome.org/show_bug.cgi?id=678701 */
static void
test_bgo_678701 (void)
{
    GUPnPContext *context = NULL;
    GError *error = NULL;
    GUPnPControlPoint *cp = NULL;
    TestBgo678701Data data = { NULL, NULL };
    GUPnPRootDevice *rd;
    GUPnPServiceInfo *info = NULL;
    GUPnPDeviceInfo *dev_info = NULL;
    GUPnPResourceFactory *factory;

    data.loop = g_main_loop_new (NULL, FALSE);

    context = create_context (0, &error);
    g_assert_no_error (error);
    g_assert (context != NULL);

    // Do not pollute the default factory with this test
    factory = gupnp_resource_factory_new ();
    gupnp_resource_factory_register_resource_proxy_type (factory,
                                                         "urn:test-gupnp-org:service:TestService:1",
                                                         test_bgo_678701_service_get_type ());
    gupnp_resource_factory_register_resource_proxy_type (factory,
                                                         "urn:test-gupnp-org:device:TestSubDevice:1",
                                                         test_bgo_678701_device_get_type ());

    rd = gupnp_root_device_new (context, "TestDevice.xml", DATA_PATH, &error);
    g_assert_no_error (error);
    g_assert (rd != NULL);
    gupnp_root_device_set_available (rd, TRUE);

    cp = gupnp_control_point_new_full (
            context,
            factory,
            "urn:test-gupnp-org:device:TestDevice:1");
    gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);
    g_signal_connect (G_OBJECT (cp),
                      "device-proxy-available",
                      G_CALLBACK (test_bgo_678701_on_dp_available),
                      &data);

    test_run_loop (data.loop, g_test_get_path ());
    g_assert (data.proxy != NULL);

    info = gupnp_device_info_get_service (GUPNP_DEVICE_INFO (data.proxy),
                                          "urn:test-gupnp-org:service:TestService:1");
    g_assert_cmpstr(G_OBJECT_TYPE_NAME (info), ==, "TestBgo678701Service");

    dev_info = gupnp_device_info_get_device (GUPNP_DEVICE_INFO (data.proxy),
                                          "urn:test-gupnp-org:device:TestSubDevice:1");
    g_assert_cmpstr(G_OBJECT_TYPE_NAME (dev_info), ==, "TestBgo678701Device");

    g_object_unref (data.proxy);
    g_object_unref (cp);
    g_object_unref (info);
    g_object_unref (dev_info);
    g_object_unref (rd);
    g_object_unref (context);
    g_object_unref (factory);

    // Make sure the source teardown handlers get run so we don't confuse valgrind
    g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, data.loop);
    g_main_loop_run (data.loop);

    g_main_loop_unref (data.loop);

}

/* Test that removing a notify-callback from the callback itself works
 * https://bugzilla.gnome.org/show_bug.cgi?id=678701 */
static void
test_bgo_690400 (void)
{
    GUPnPContext *context = NULL;
    GError *error = NULL;
    GUPnPControlPoint *cp = NULL;
    TestServiceProxyData data = { NULL, NULL };
    GUPnPRootDevice *rd;
    GUPnPServiceInfo *service;

    data.loop = g_main_loop_new (NULL, FALSE);

    context = create_context (0, &error);
    g_assert_no_error (error);
    g_assert (context != NULL);

    cp = gupnp_control_point_new (context,
                                  "urn:test-gupnp-org:service:TestService:1");
    gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

    g_signal_connect (G_OBJECT (cp),
                      "service-proxy-available",
                      G_CALLBACK (test_on_sp_available),
                      &data);

    rd = gupnp_root_device_new (context, "TestDevice.xml", DATA_PATH, &error);
    g_assert_no_error (error);
    g_assert (rd != NULL);
    service = gupnp_device_info_get_service (GUPNP_DEVICE_INFO (rd),
                                             "urn:test-gupnp-org:service:TestService:1");
    g_signal_connect (service, "query-variable",
                      G_CALLBACK (test_bgo_690400_query_variable), NULL);
    gupnp_root_device_set_available (rd, TRUE);

    test_run_loop (data.loop, "690400 - waiting for query_variable");
    g_assert (data.proxy != NULL);

    gupnp_service_proxy_add_notify (data.proxy,
                                    "evented_variable",
                                    G_TYPE_STRING,
                                    (GUPnPServiceProxyNotifyCallback)test_bgo_690400_notify,
                                    &data);
    gupnp_service_proxy_add_notify (data.proxy,
                                    "evented_variable",
                                    G_TYPE_STRING,
                                    (GUPnPServiceProxyNotifyCallback)test_bgo_690400_notify_too,
                                    &data);

    gupnp_service_proxy_set_subscribed (data.proxy, TRUE);

    test_run_loop (data.loop, "690400 - waiting for event");

    g_object_unref (data.proxy);
    g_object_unref (cp);
    g_object_unref (service);
    g_object_unref (rd);
    g_object_unref (context);

    // Make sure the source teardown handlers get run so we don't confuse valgrind
    g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, data.loop);
    g_main_loop_run (data.loop);

    g_main_loop_unref (data.loop);
}

/* Test that correct icons are returned for various size requests.
 * https://bugzilla.gnome.org/show_bug.cgi?id=722696 */
static void
test_bgo_722696 (void)
{
    GUPnPContext *context = NULL;
    GError *error = NULL;
    GUPnPRootDevice *rd;
    char *icon;
    int width;

    context = create_context (0, &error);
    g_assert_no_error (error);
    g_assert (context != NULL);

    rd = gupnp_root_device_new (context, "TestDevice.xml", DATA_PATH, &error);
    g_assert_no_error (error);
    g_assert (rd != NULL);

    /* prefer bigger */
    width = -1;
    icon = gupnp_device_info_get_icon_url (GUPNP_DEVICE_INFO (rd),
                                           NULL,
                                           -1, -1, -1,
                                           TRUE,
                                           NULL, NULL, &width, NULL);
    g_assert_cmpint (width, ==, 120);
    g_free (icon);

    /* prefer smaller */
    width = -1;
    icon = gupnp_device_info_get_icon_url (GUPNP_DEVICE_INFO (rd),
                                           NULL,
                                           -1, -1, -1,
                                           FALSE,
                                           NULL, NULL, &width, NULL);
    g_assert_cmpint (width, ==, 24);
    g_free (icon);

    /* prefer width <= 119 */
    width = -1;
    icon = gupnp_device_info_get_icon_url (GUPNP_DEVICE_INFO (rd),
                                           NULL,
                                           -1, 119, -1,
                                           FALSE,
                                           NULL, NULL, &width, NULL);
    g_assert_cmpint (width, ==, 48);
    g_free (icon);

    /* prefer width >= 119 */
    width = -1;
    icon = gupnp_device_info_get_icon_url (GUPNP_DEVICE_INFO (rd),
                                           NULL,
                                           -1, 119, -1,
                                           TRUE,
                                           NULL, NULL, &width, NULL);
    g_assert_cmpint (width, ==, 120);
    g_free (icon);

    g_object_unref (rd);
    g_object_unref (context);
}

#define TEST_BGO_743233_USN "uuid:f28e26f0-fcaa-42aa-b115-3ca12096925c::"

static void
test_bgo_743233 (void)
{
    GUPnPContext *context = NULL;
    GUPnPControlPoint *cp = NULL;
    GError *error = NULL;

    context = create_context (0, &error);
    g_assert_no_error (error);
    g_assert (context != NULL);

    cp = gupnp_control_point_new (context,
                                  "usn:uuid:0dc60534-642c-478f-ae61-1d78dbe1f73d");
    g_assert (cp != NULL);

    g_test_expect_message ("gupnp-control-point",
                           G_LOG_LEVEL_WARNING,
                           "Invalid USN: " TEST_BGO_743233_USN);
    g_signal_emit_by_name (cp, "resource-unavailable", TEST_BGO_743233_USN);
    g_test_assert_expected_messages ();

    g_object_unref (cp);
    g_object_unref (context);
}

static void
test_ggo_24 (void)
{
        // IPv4
        g_assert (
                validate_host_header ("127.0.0.1:4711", "127.0.0.1", 4711));

        g_assert (
                validate_host_header ("127.0.0.1", "127.0.0.1", 80));

        g_assert_false (
                validate_host_header ("example.com", "127.0.0.1", 4711));

        g_assert_false (
                validate_host_header ("example.com:80", "127.0.0.1", 4711));

        g_assert_false (
                validate_host_header ("example.com:4711", "127.0.0.1", 4711));

        g_assert_false (
                validate_host_header ("192.168.1.2:4711", "127.0.0.1", 4711));

        g_assert_false (
                validate_host_header ("[fe80::01]", "127.0.0.1", 4711));

        // Link ids should not be parsed
        g_assert_false (
                validate_host_header ("[fe80::01%1]", "127.0.0.1", 4711));

        g_assert_false (
                validate_host_header ("[fe80::01%eth0]", "127.0.0.1", 4711));

        // IPv6
        g_assert (
                validate_host_header ("[::1]:4711", "::1", 4711));

        g_assert (
                validate_host_header ("[::1]", "::1", 80));

        // Host header needs to be enclosed in [] even without port
        g_assert_false (
                validate_host_header ("::1", "::1", 80));

        g_assert_false (
                validate_host_header ("example.com", "::1", 4711));

        g_assert_false (
                validate_host_header ("example.com:80", "::1", 4711));

        g_assert_false (
                validate_host_header ("example.com:4711", "::1", 4711));

        g_assert_false (
                validate_host_header ("192.168.1.2:4711", "::1", 4711));

        g_assert_false (
                validate_host_header ("[fe80::01]", "::1", 4711));

        // Link ids should not be parsed
        g_assert_false (
                validate_host_header ("[fe80::01%1]", "fe80::acab", 4711));

        g_assert_false (
                validate_host_header ("[fe80::01%eth0]", "fe80::acab", 4711));
}

void
test_ggo_42 ()
{
        GUPnPContext *context = NULL;
        GError *error = NULL;
        GUPnPRootDevice *rd;
        GUPnPServiceInfo *info = NULL;

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
        info = gupnp_device_info_get_service (
                GUPNP_DEVICE_INFO (rd),
                "urn:test-gupnp-org:service:TestService:1");

        GMainLoop *loop = g_main_loop_new (NULL, FALSE);
        g_object_unref (rd);
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, loop);
        g_main_loop_run (loop);

        g_object_unref (info);

        // Make sure the source teardown handlers get run so we don't confuse valgrind
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, loop);
        g_main_loop_run (loop);

        g_object_unref (context);

        // Make sure the source teardown handlers get run so we don't confuse valgrind
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, loop);
        g_main_loop_run (loop);

        g_main_loop_unref (loop);
}

void
on_control (SoupServer *server,
            SoupServerMessage *msg,
            const char *path,
            GHashTable *query,
            gpointer user_data)
{
        SoupMessageBody *b = soup_server_message_get_response_body (msg);

        g_print ("On Control\n");

        soup_message_body_append (b, SOUP_MEMORY_COPY, "<html>\0", 7);
        soup_server_message_set_status (msg, SOUP_STATUS_METHOD_NOT_ALLOWED, NULL);
}

void
on_event (SoupServer *server,
          SoupServerMessage *msg,
          const char *path,
          GHashTable *query,
          gpointer user_data)
{
        soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
}

typedef struct {
        GUPnPServiceProxy *p;
        GMainLoop *loop;
} TestGGO63Data;

void
on_proxy (GUPnPControlPoint *cp, GUPnPServiceProxy *p, gpointer user_data)
{
        g_print ("Got proxy....\n");
        TestGGO63Data *d = (TestGGO63Data *) user_data;
        d->p = g_object_ref (p);

        g_main_loop_quit (d->loop);
}

void
on_ping (GObject *object, GAsyncResult *res, gpointer user_data)
{
        g_print ("On PIng\n");

        TestGGO63Data *d = (TestGGO63Data *) user_data;
        GError *error = NULL;

        GUPnPServiceProxyAction *action =
                gupnp_service_proxy_call_action_finish (
                        GUPNP_SERVICE_PROXY (object),
                        res,
                        &error);

        g_assert_null (action);
        g_assert_error (error, GUPNP_SERVER_ERROR, GUPNP_SERVER_ERROR_OTHER);

        g_error_free (error);

        g_main_loop_quit (d->loop);
}

void
test_ggo_63 ()
{
        GUPnPContext *context = NULL;
        GError *error = NULL;

        context = create_context (0, &error);
        g_assert_no_error (error);
        g_assert (context != NULL);

        SoupServer *server = gupnp_context_get_server (context);
        GSList *uris = soup_server_get_uris (server);
        GSSDPResourceGroup *rg =
                gssdp_resource_group_new (GSSDP_CLIENT (context));

        char *server_uri = g_uri_to_string (uris->data);
        char *resource_path = g_strconcat (server_uri, "TestDevice.xml", NULL);
        g_free (server_uri);
        g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);

        gssdp_resource_group_add_resource_simple (rg,
                                                  "upnp:rootdevice",
                                                  "uuid:1234::upnp:rootdevice",
                                                  resource_path);

        gssdp_resource_group_add_resource_simple (rg,
                                                  "uuid:1234",
                                                  "uuid:1234",
                                                  resource_path);

        gssdp_resource_group_add_resource_simple (
                rg,
                "urn:test-gupnp-org:device:TestDevice:1",
                "uuid:1234::urn:test-gupnp-org:device:TestDevice:1",
                resource_path);

        gssdp_resource_group_add_resource_simple (
                rg,
                "urn:test-gupnp-org:service:TestService:1",
                "uuid:1234::urn:test-gupnp-org:service:TestService:1",
                resource_path);

        g_free (resource_path);

        gupnp_context_host_path (context,
                                 DATA_PATH "/TestDevice.xml",
                                 "/TestDevice.xml");

        soup_server_add_handler (server,
                                 "/TestService/Control",
                                 on_control,
                                 NULL,
                                 NULL);

        soup_server_add_handler (server,
                                 "/TestService/Event",
                                 on_event,
                                 NULL,
                                 NULL);

        gssdp_resource_group_set_available (rg, TRUE);


        GUPnPControlPoint *cp = gupnp_control_point_new (
                context,
                "urn:test-gupnp-org:service:TestService:1");

        TestGGO63Data d = { .loop = g_main_loop_new (NULL, FALSE), .p = NULL };
        g_signal_connect (G_OBJECT (cp),
                          "service-proxy-available",
                          G_CALLBACK (on_proxy),
                          &d);

        gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

        g_main_loop_run (d.loop);

        GUPnPServiceProxyAction *a =
                gupnp_service_proxy_action_new ("Ping", NULL);

        gupnp_service_proxy_call_action_async (d.p, a, NULL, on_ping, &d);

        g_main_loop_run (d.loop);
        gupnp_service_proxy_action_unref (a);

        g_object_unref (d.p);
        g_object_unref (cp);
        g_object_unref (rg);
        g_object_unref (context);


        // Make sure the source teardown handlers get run so we don't confuse valgrind
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, d.loop);
        g_main_loop_run (d.loop);

        g_main_loop_unref (d.loop);
}

void
test_ggo_60_call_ready (GObject *source, GAsyncResult *res, gpointer user_data)
{
        g_main_loop_quit ((GMainLoop *) user_data);
}

void
test_ggo_60_no_crash ()
{
        // Test that there is no crash if not calling call_finish() on call_async()
        GUPnPContext *context = NULL;
        GError *error = NULL;
        GUPnPControlPoint *cp = NULL;
        GUPnPRootDevice *rd;
        TestServiceProxyData data = { .proxy = NULL,
                                      .loop = g_main_loop_new (NULL, FALSE) };

        context = create_context (0, &error);
        g_assert_no_error (error);
        g_assert (context != NULL);

        cp = gupnp_control_point_new (
                context,
                "urn:test-gupnp-org:service:TestService:1");

        gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

        g_signal_connect (G_OBJECT (cp),
                          "service-proxy-available",
                          G_CALLBACK (test_on_sp_available),
                          &data);


        rd = gupnp_root_device_new (context,
                                    "TestDevice.xml",
                                    DATA_PATH,
                                    &error);
        g_assert_no_error (error);
        g_assert (rd != NULL);
        gupnp_root_device_set_available (rd, TRUE);
        test_run_loop (data.loop, g_test_get_path ());
        g_assert (data.proxy != NULL);

        // We just use the default of "Action not implemented" response, since
        // it does not matter for the actual test

        GUPnPServiceProxyAction *action =
                gupnp_service_proxy_action_new ("Ping", NULL);
        gupnp_service_proxy_call_action_async (data.proxy,
                                               action,
                                               NULL,
                                               test_ggo_60_call_ready,
                                               data.loop);

        test_run_loop (data.loop, g_test_get_path());
        gupnp_service_proxy_action_unref (action);
        g_object_unref (data.proxy);
        g_object_unref (rd);
        g_object_unref (cp);
        g_object_unref (context);

        // Make sure the source teardown handlers get run so we don't confuse valgrind
        g_timeout_add (500, (GSourceFunc) delayed_loop_quitter, data.loop);
        g_main_loop_run (data.loop);

        g_main_loop_unref (data.loop);
}

void
test_ggo_81 ()
{
        GInetAddress *address =
                g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);
        GError *error = NULL;

        GUPnPContext *context =
                gupnp_context_new_for_address (address,
                                               4711,
                                               GSSDP_UDA_VERSION_1_0,
                                               &error);
        g_assert_nonnull (context);
        g_assert_no_error (error);
        g_assert_cmpint (4711, ==, gupnp_context_get_port (context));
        g_assert_cmpint (4711,
                         ==,
                         gssdp_client_get_port (GSSDP_CLIENT (context)));

        guint port = 0;
        g_object_get (context, "port", &port, NULL);
        g_assert_cmpint (4711, ==, port);

        g_object_unref (context);

        context = gupnp_context_new_for_address (address,
                                                 0,
                                                 GSSDP_UDA_VERSION_1_0,
                                                 &error);

        g_assert_nonnull (context);
        g_assert_no_error (error);
        port = gupnp_context_get_port (context);
        g_assert_cmpint (0, !=, port);

        guint port2 = 0;
        g_object_get (context, "port", &port2, NULL);
        g_assert_cmpint (port, ==, port2);

        g_assert_cmpint (port2,
                         ==,
                         gssdp_client_get_port (GSSDP_CLIENT (context)));

        g_object_unref (context);
        g_object_unref (address);
}
int
main (int argc, char *argv[]) {
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/bugs/bgo/696762", test_bgo_696762);
    g_test_add_func ("/bugs/bgo/678701", test_bgo_678701);
    g_test_add_func ("/bugs/bgo/690400", test_bgo_690400);
    g_test_add_func ("/bugs/bgo/722696", test_bgo_722696);
    g_test_add_func ("/bugs/bgo/743233", test_bgo_743233);
    g_test_add_func ("/bugs/ggo/24", test_ggo_24);
    g_test_add_func ("/bugs/ggo/42", test_ggo_42);
    g_test_add_func ("/bugs/ggo/63", test_ggo_63);
    g_test_add_func ("/bugs/ggo/60", test_ggo_60_no_crash);
    g_test_add_func ("/bugs/ggo/81", test_ggo_81);

    return g_test_run ();
}
