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


struct _GUPnPServiceAction {
        volatile gint ref_count;

        GUPnPContext *context;

        char         *name;

        SoupMessage  *msg;
        gboolean      accept_gzip;

        GUPnPXMLDoc  *doc;
        xmlNode      *node;

        GString      *response_str;

        guint         argument_count;
};


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


typedef struct _TestBgo696762Data {
    GMainLoop *loop;
    GUPnPServiceProxy *proxy;
} TestBgo696762Data;

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
    TestBgo696762Data *data = (TestBgo696762Data *) user_data;

    g_main_loop_quit (data->loop);
}

static void
test_bgo_696762_on_sp_available (G_GNUC_UNUSED GUPnPControlPoint *cp,
                                 GUPnPServiceProxy               *proxy,
                                 gpointer                         user_data)
{
    TestBgo696762Data *data = (TestBgo696762Data *) user_data;

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

static gboolean
test_on_timeout (G_GNUC_UNUSED gpointer user_data)
{
    g_assert_not_reached ();

    return FALSE;
}

/* Test if a call on a service proxy keeps argument order */
static void
test_bgo_696762 (void)
{
    GUPnPContext *context = NULL;
    GError *error = NULL;
    GUPnPControlPoint *cp = NULL;
    guint timeout_id = 0;
    GUPnPRootDevice *rd;
    TestBgo696762Data data = { NULL, NULL };
    GUPnPServiceInfo *info = NULL;

    data.loop = g_main_loop_new (NULL, FALSE);

    context = gupnp_context_new (NULL, "lo", 0, &error);
    g_assert (context != NULL);
    g_assert (error == NULL);

    cp = gupnp_control_point_new (context,
                                  "urn:test-gupnp-org:service:TestService:1");

    gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

    g_signal_connect (G_OBJECT (cp),
                      "service-proxy-available",
                      G_CALLBACK (test_bgo_696762_on_sp_available),
                      &data);


    rd = gupnp_root_device_new (context, "TestDevice.xml", DATA_PATH);
    gupnp_root_device_set_available (rd, TRUE);
    info = gupnp_device_info_get_service (GUPNP_DEVICE_INFO (rd),
                                          "urn:test-gupnp-org:service:TestService:1");
    g_signal_connect (G_OBJECT (info),
                      "action-invoked::Browse",
                      G_CALLBACK (test_bgo_696762_on_browse_call),
                      &data);

    timeout_id = g_timeout_add_seconds (2, test_on_timeout, &(data.loop));
    g_main_loop_run (data.loop);
    g_source_remove (timeout_id);
    g_assert (data.proxy != NULL);

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

    timeout_id = g_timeout_add_seconds (2, test_on_timeout, &(data.loop));
    g_main_loop_run (data.loop);
    g_source_remove (timeout_id);

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
    guint timeout_id = 0;
    TestBgo678701Data data = { NULL, NULL };
    GUPnPRootDevice *rd;
    GUPnPServiceInfo *info = NULL;
    GUPnPDeviceInfo *dev_info = NULL;
    GUPnPResourceFactory *factory;

    data.loop = g_main_loop_new (NULL, FALSE);

    context = gupnp_context_new (NULL, "lo", 0, &error);
    g_assert (context != NULL);
    g_assert (error == NULL);

    factory = gupnp_resource_factory_get_default ();
    gupnp_resource_factory_register_resource_proxy_type (factory,
                                                         "urn:test-gupnp-org:service:TestService:1",
                                                         test_bgo_678701_service_get_type ());
    gupnp_resource_factory_register_resource_proxy_type (factory,
                                                         "urn:test-gupnp-org:device:TestSubDevice:1",
                                                         test_bgo_678701_device_get_type ());

    rd = gupnp_root_device_new (context, "TestDevice.xml", DATA_PATH);
    gupnp_root_device_set_available (rd, TRUE);

    cp = gupnp_control_point_new (context,
                                  "urn:test-gupnp-org:device:TestDevice:1");
    gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);
    g_signal_connect (G_OBJECT (cp),
                      "device-proxy-available",
                      G_CALLBACK (test_bgo_678701_on_dp_available),
                      &data);

    timeout_id = g_timeout_add_seconds (2, test_on_timeout, &(data.loop));
    g_main_loop_run (data.loop);
    g_source_remove (timeout_id);
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

int
main (int argc, char *argv[]) {
#if !GLIB_CHECK_VERSION(2,35,0)
    g_type_init ();
#endif
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/bugs/696762", test_bgo_696762);
    g_test_add_func ("/bugs/678701", test_bgo_678701);

    return g_test_run ();
}
