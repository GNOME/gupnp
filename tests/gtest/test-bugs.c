/*
 * Copyright (C) 2013 Intel Corporation.
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

#include <libgupnp/gupnp.h>
#include <libgupnp/gupnp-service-private.h>

static GUPnPContext *
create_context (GSSDPUDAVersion uda_version, guint16 port, GError **error) {
        return GUPNP_CONTEXT (g_initable_new (GUPNP_TYPE_CONTEXT,
                                              NULL,
                                              error,
                                              "uda-version",
                                              uda_version,
                                              "host-ip",
                                              "127.0.0.1",
                                              "msearch-port",
                                              port,
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
    node = node->next;
    gupnp_service_action_return (action);
}

static void
test_bgo_696762_on_browse (G_GNUC_UNUSED GUPnPServiceProxy       *proxy,
                           G_GNUC_UNUSED GUPnPServiceProxyAction *action,
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
test_on_timeout (G_GNUC_UNUSED gpointer user_data)
{
    g_assert_not_reached ();

    return FALSE;
}

static void
test_run_loop (GMainLoop *loop)
{
    guint timeout_id = 0;
    const char *test_timeout = g_getenv ("GUPNP_TEST_TIMEOUT");

    timeout_id = g_timeout_add_seconds (
            test_timeout == NULL ? 2 : atoi (test_timeout),
            test_on_timeout,
            NULL);
    g_main_loop_run (loop);
    g_source_remove (timeout_id);
}

/* Test if a call on a service proxy keeps argument order */
static void
test_bgo_696762 (void)
{
    GUPnPContext *context = NULL;
    GError *error = NULL;
    GUPnPControlPoint *cp = NULL;
    GUPnPRootDevice *rd;
    TestServiceProxyData data = { NULL, NULL };
    GUPnPServiceInfo *info = NULL;

    data.loop = g_main_loop_new (NULL, FALSE);

    context = create_context (GSSDP_UDA_VERSION_1_0, 0, &error);
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
    gupnp_root_device_set_available (rd, TRUE);
    info = gupnp_device_info_get_service (GUPNP_DEVICE_INFO (rd),
                                          "urn:test-gupnp-org:service:TestService:1");
    g_signal_connect (G_OBJECT (info),
                      "action-invoked::Browse",
                      G_CALLBACK (test_bgo_696762_on_browse_call),
                      &data);

    test_run_loop (data.loop);
    g_assert (data.proxy != NULL);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gupnp_service_proxy_begin_action (data.proxy,
                                      "Browse",
                                      test_bgo_696762_on_browse,
                                      &data,
                                      "ObjectID", G_TYPE_STRING, "0",
                                      "BrowseFlag", G_TYPE_STRING, "BrowseDirectChildren",
                                      "Filter", G_TYPE_STRING, "res,dc:date,res@size",
                                      "StartingIndex", G_TYPE_UINT, 0,
                                      "RequestedCount", G_TYPE_UINT, 0,
                                      "SortCriteria", G_TYPE_STRING, "",
                                      NULL);
    G_GNUC_END_IGNORE_DEPRECATIONS

    test_run_loop (data.loop);

    g_main_loop_unref (data.loop);
    g_object_unref (data.proxy);
    g_object_unref (cp);
    g_object_unref (rd);
    g_object_unref (context);
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

    context = create_context (GSSDP_UDA_VERSION_1_0, 0, &error);
    g_assert_no_error (error);
    g_assert (context != NULL);

    factory = gupnp_resource_factory_get_default ();
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

    cp = gupnp_control_point_new (context,
                                  "urn:test-gupnp-org:device:TestDevice:1");
    gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);
    g_signal_connect (G_OBJECT (cp),
                      "device-proxy-available",
                      G_CALLBACK (test_bgo_678701_on_dp_available),
                      &data);

    test_run_loop (data.loop);
    g_assert (data.proxy != NULL);

    info = gupnp_device_info_get_service (GUPNP_DEVICE_INFO (data.proxy),
                                          "urn:test-gupnp-org:service:TestService:1");
    g_assert_cmpstr(G_OBJECT_TYPE_NAME (info), ==, "TestBgo678701Service");

    dev_info = gupnp_device_info_get_device (GUPNP_DEVICE_INFO (data.proxy),
                                          "urn:test-gupnp-org:device:TestSubDevice:1");
    g_assert_cmpstr(G_OBJECT_TYPE_NAME (dev_info), ==, "TestBgo678701Device");

    g_main_loop_unref (data.loop);
    g_object_unref (data.proxy);
    g_object_unref (cp);
    g_object_unref (rd);
    g_object_unref (context);
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

    context = create_context (GSSDP_UDA_VERSION_1_0, 0, &error);
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

    test_run_loop (data.loop);
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

    test_run_loop (data.loop);

    g_main_loop_unref (data.loop);
    g_object_unref (data.proxy);
    g_object_unref (cp);
    g_object_unref (service);
    g_object_unref (rd);
    g_object_unref (context);
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

    context = create_context (GSSDP_UDA_VERSION_1_0, 0, &error);
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

    context = create_context (GSSDP_UDA_VERSION_1_0, 0, &error);
    g_assert_no_error (error);
    g_assert (context != NULL);

    cp = gupnp_control_point_new (context,
                                  "usn:uuid:0dc60534-642c-478f-ae61-1d78dbe1f73d");
    g_assert (cp != NULL);

    g_test_expect_message (G_LOG_DOMAIN,
                           G_LOG_LEVEL_WARNING,
                           "Invalid USN: " TEST_BGO_743233_USN);
    g_signal_emit_by_name (cp, "resource-unavailable", TEST_BGO_743233_USN);
    g_test_assert_expected_messages ();

    g_object_unref (cp);
    g_object_unref (context);
}

typedef struct _TestGgo24Data {
        GMainLoop *loop;
        GUPnPServiceInfo *service;
        SoupSession *session;
        SoupKnownStatusCode result;
} TestGgo24Data;

static void
on_message_done (SoupSession *session, SoupMessage *msg, gpointer user_data)
{
        TestGgo24Data *data = (TestGgo24Data *) user_data;

        data->result = msg->status_code;
        g_main_loop_quit (data->loop);
}

#define TICK_CALL                                                              \
        "<?xml version=\"1.0\"?>"                                              \
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "   \
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"       \
        "<s:Body>"                                                             \
        "<u:Tick xmlns:u=\"urn:test-gupnp-org:service:TestService:1\">"        \
        "</u:Tick>"                                                            \
        "</s:Body>"                                                            \
        "</s:Envelope>"

static gboolean
test_ggo_24_1_call_no_content_type (gpointer user_data)
{
        SoupMessage *msg;
        TestGgo24Data *data = (TestGgo24Data *) user_data;
        char *control_url;

        control_url = gupnp_service_info_get_control_url (data->service);
        msg = soup_message_new (SOUP_METHOD_POST, control_url);
        g_free (control_url);
        soup_message_headers_append (
                msg->request_headers,
                "SOAPAction",
                "urn:test-gupnp-org:service:TestService:1#Tick");

        soup_message_body_append_take (msg->request_body,
                                       (guchar *) TICK_CALL,
                                       sizeof (TICK_CALL));

        soup_session_queue_message (data->session,
                                    g_object_ref (msg),
                                    on_message_done,
                                    data);

        return FALSE;
}

static gboolean
test_ggo_24_1_call_invalid_content_type (gpointer user_data)
{
        SoupMessage *msg;
        TestGgo24Data *data = (TestGgo24Data *) user_data;
        char *control_url;

        control_url = gupnp_service_info_get_control_url (data->service);
        msg = soup_message_new (SOUP_METHOD_POST, control_url);
        g_free (control_url);
        soup_message_headers_append (
                msg->request_headers,
                "SOAPAction",
                "urn:test-gupnp-org:service:TestService:1#Tick");


        soup_message_body_append_take (msg->request_body,
                                       (guchar *) TICK_CALL,
                                       sizeof (TICK_CALL));

        soup_message_headers_set_content_type (msg->request_headers,
                                               "text/plain",
                                               NULL);

        soup_session_queue_message (data->session,
                                    g_object_ref (msg),
                                    on_message_done,
                                    data);

        return FALSE;
}

static gboolean
test_ggo_24_1_call_plain_content_type (gpointer user_data)
{
        SoupMessage *msg;
        TestGgo24Data *data = (TestGgo24Data *) user_data;
        char *control_url;

        control_url = gupnp_service_info_get_control_url (data->service);

        msg = soup_message_new (SOUP_METHOD_POST, control_url);
        g_free (control_url);
        soup_message_headers_append (
                msg->request_headers,
                "SOAPAction",
                "urn:test-gupnp-org:service:TestService:1#Tick");


        soup_message_body_append_take (msg->request_body,
                                       (guchar *) TICK_CALL,
                                       sizeof (TICK_CALL));

        soup_message_headers_set_content_type (msg->request_headers,
                                               "text/xml",
                                               NULL);

        soup_session_queue_message (data->session,
                                    g_object_ref (msg),
                                    on_message_done,
                                    data);

        return FALSE;
}

static gboolean
test_ggo_24_1_call_missing_encoding_content_type (gpointer user_data)
{
        SoupMessage *msg;
        TestGgo24Data *data = (TestGgo24Data *) user_data;
        char *control_url;
        GHashTable *params =
                g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

        control_url = gupnp_service_info_get_control_url (data->service);

        msg = soup_message_new (SOUP_METHOD_POST, control_url);
        g_free (control_url);
        soup_message_headers_append (
                msg->request_headers,
                "SOAPAction",
                "urn:test-gupnp-org:service:TestService:1#Tick");


        soup_message_body_append_take (msg->request_body,
                                       (guchar *) TICK_CALL,
                                       sizeof (TICK_CALL));



        g_hash_table_replace (params, "frobnication", "dada");
        soup_message_headers_set_content_type (msg->request_headers,
                                               "text/xml",
                                               params);

        soup_session_queue_message (data->session,
                                    g_object_ref (msg),
                                    on_message_done,
                                    data);
        g_hash_table_unref (params);

        return FALSE;
}

static gboolean
test_ggo_24_1_call_wrong_encoding_content_type (gpointer user_data)
{
        SoupMessage *msg;
        TestGgo24Data *data = (TestGgo24Data *) user_data;
        char *control_url;
        GHashTable *params =
                g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

        control_url = gupnp_service_info_get_control_url (data->service);

        msg = soup_message_new (SOUP_METHOD_POST, control_url);
        g_free (control_url);
        soup_message_headers_append (
                msg->request_headers,
                "SOAPAction",
                "urn:test-gupnp-org:service:TestService:1#Tick");


        soup_message_body_append_take (msg->request_body,
                                       (guchar *) TICK_CALL,
                                       sizeof (TICK_CALL));



        g_hash_table_replace (params, "charset⎄", "iso-8859-1");
        soup_message_headers_set_content_type (msg->request_headers,
                                               "text/xml",
                                               params);

        soup_session_queue_message (data->session,
                                    g_object_ref (msg),
                                    on_message_done,
                                    data);
        g_hash_table_unref (params);

        return FALSE;
}

static gboolean
test_ggo_24_1_call_proper_encoding_content_type (gpointer user_data)
{
        SoupMessage *msg;
        TestGgo24Data *data = (TestGgo24Data *) user_data;
        char *control_url;
        GHashTable *params =
                g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

        control_url = gupnp_service_info_get_control_url (data->service);

        msg = soup_message_new (SOUP_METHOD_POST, control_url);
        g_free (control_url);
        soup_message_headers_append (
                msg->request_headers,
                "SOAPAction",
                "urn:test-gupnp-org:service:TestService:1#Tick");


        soup_message_body_append_take (msg->request_body,
                                       (guchar *) TICK_CALL,
                                       sizeof (TICK_CALL));



        g_hash_table_replace (params, "charset", "utf-8");
        soup_message_headers_set_content_type (msg->request_headers,
                                               "text/xml",
                                               params);
        g_hash_table_unref (params);

        soup_session_queue_message (data->session,
                                    g_object_ref (msg),
                                    on_message_done,
                                    data);

        return FALSE;
}

static void
on_tick_call (GUPnPService *service,
              GUPnPServiceAction *action,
              gpointer user_data)
{
        gupnp_service_action_set (action,
                                  "Result",
                                  G_TYPE_STRING,
                                  "Tock",
                                  NULL);
        gupnp_service_action_return (action);
}

static void
test_ggo_24_1 (void)
{
        // Test the correct handling of the Content-Type header in SOAP calls
        GUPnPContext *context = NULL;
        GError *error = NULL;
        TestGgo24Data data = { .loop = g_main_loop_new (NULL, FALSE),
                               .service = NULL,
                               .session = soup_session_new (),
                               .result = SOUP_KNOWN_STATUS_CODE_NONE };
        GUPnPRootDevice *rd = NULL;

        context = create_context (GSSDP_UDA_VERSION_1_0, 0, &error);
        g_assert_no_error (error);
        g_assert (context != NULL);

        rd = gupnp_root_device_new (context,
                                    "TestDevice.xml",
                                    DATA_PATH,
                                    &error);
        g_assert_no_error (error);
        g_assert (rd != NULL);

        data.service = gupnp_device_info_get_service (
                GUPNP_DEVICE_INFO (rd),
                "urn:test-gupnp-org:service:TestService:1");

        g_signal_connect (data.service,
                          "action-invoked::Tick",
                          G_CALLBACK (on_tick_call),
                          &data);

        gupnp_root_device_set_available (rd, TRUE);

        g_timeout_add_seconds (1, test_ggo_24_1_call_no_content_type, &data);
        test_run_loop (data.loop);
        g_assert_cmpint (data.result,
                         ==,
                         SOUP_KNOWN_STATUS_CODE_PRECONDITION_FAILED);

        g_timeout_add_seconds (1,
                               test_ggo_24_1_call_invalid_content_type,
                               &data);
        test_run_loop (data.loop);
        g_assert_cmpint (data.result,
                         ==,
                         SOUP_KNOWN_STATUS_CODE_PRECONDITION_FAILED);

        g_timeout_add_seconds (1, test_ggo_24_1_call_plain_content_type, &data);
        test_run_loop (data.loop);
        g_assert_cmpint (data.result, ==, SOUP_KNOWN_STATUS_CODE_OK);

        g_timeout_add_seconds (1,
                               test_ggo_24_1_call_missing_encoding_content_type,
                               &data);
        test_run_loop (data.loop);
        g_assert_cmpint (data.result, ==, SOUP_KNOWN_STATUS_CODE_OK);

        g_timeout_add_seconds (1,
                               test_ggo_24_1_call_wrong_encoding_content_type,
                               &data);
        test_run_loop (data.loop);
        g_assert_cmpint (data.result, ==, SOUP_KNOWN_STATUS_CODE_OK);

        g_timeout_add_seconds (1,
                               test_ggo_24_1_call_proper_encoding_content_type,
                               &data);
        test_run_loop (data.loop);
        g_assert_cmpint (data.result, ==, SOUP_KNOWN_STATUS_CODE_OK);

        g_object_unref (rd);
        g_object_unref (context);
        g_main_loop_unref (data.loop);
}

static void
test_ggo_24_2 (void)
{
        // Test the correct handling of the Content-Type header in SOAP calls
        GUPnPContext *context = NULL;
        GError *error = NULL;
        TestGgo24Data data = { .loop = g_main_loop_new (NULL, FALSE),
                               .service = NULL,
                               .session = soup_session_new (),
                               .result = SOUP_KNOWN_STATUS_CODE_NONE };
        GUPnPRootDevice *rd = NULL;

        context = create_context (GSSDP_UDA_VERSION_1_1, 0, &error);
        g_assert_no_error (error);
        g_assert (context != NULL);

        rd = gupnp_root_device_new (context,
                                    "TestDevice.xml",
                                    DATA_PATH,
                                    &error);
        g_assert_no_error (error);
        g_assert (rd != NULL);

        data.service = gupnp_device_info_get_service (
                GUPNP_DEVICE_INFO (rd),
                "urn:test-gupnp-org:service:TestService:1");

        g_signal_connect (data.service,
                          "action-invoked::Tick",
                          G_CALLBACK (on_tick_call),
                          &data);

        gupnp_root_device_set_available (rd, TRUE);

        g_timeout_add_seconds (1, test_ggo_24_1_call_no_content_type, &data);
        test_run_loop (data.loop);
        g_assert_cmpint (data.result, ==, SOUP_KNOWN_STATUS_CODE_PRECONDITION_FAILED);

        g_timeout_add_seconds (1,
                               test_ggo_24_1_call_invalid_content_type,
                               &data);
        test_run_loop (data.loop);
        g_assert_cmpint (data.result, ==, SOUP_KNOWN_STATUS_CODE_PRECONDITION_FAILED);

        g_timeout_add_seconds (1,
                               test_ggo_24_1_call_plain_content_type,
                               &data);
        test_run_loop (data.loop);
        g_assert_cmpint (data.result, ==, SOUP_KNOWN_STATUS_CODE_PRECONDITION_FAILED);

        g_timeout_add_seconds (1,
                               test_ggo_24_1_call_missing_encoding_content_type,
                               &data);
        test_run_loop (data.loop);
        g_assert_cmpint (data.result, ==, SOUP_KNOWN_STATUS_CODE_PRECONDITION_FAILED);

        g_timeout_add_seconds (1,
                               test_ggo_24_1_call_wrong_encoding_content_type,
                               &data);
        test_run_loop (data.loop);
        g_assert_cmpint (data.result, ==, SOUP_KNOWN_STATUS_CODE_PRECONDITION_FAILED);

        // Only the proper content-type with proper encoding should be accepted
        g_timeout_add_seconds (1,
                               test_ggo_24_1_call_proper_encoding_content_type,
                               &data);
        test_run_loop (data.loop);
        g_assert_cmpint (data.result, ==, SOUP_KNOWN_STATUS_CODE_OK);

        g_object_unref (rd);
        g_object_unref (context);
        g_main_loop_unref (data.loop);
}

int
main (int argc, char *argv[]) {
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/bugs/696762", test_bgo_696762);
    g_test_add_func ("/bugs/678701", test_bgo_678701);
    g_test_add_func ("/bugs/690400", test_bgo_690400);
    g_test_add_func ("/bugs/722696", test_bgo_722696);
    g_test_add_func ("/bugs/743233", test_bgo_743233);
    g_test_add_func ("/bugs/24/1", test_ggo_24_1);
    g_test_add_func ("/bugs/24/2", test_ggo_24_2);

    return g_test_run ();
}
