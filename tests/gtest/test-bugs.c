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

typedef struct _TestBgo696762Data {
    GMainLoop *loop;
    GUPnPServiceProxy *proxy;
} TestBgo696762Data;

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

static gboolean
test_bgo_696762_on_timeout (G_GNUC_UNUSED gpointer user_data)
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
                                  "urn:test-gupnp-org:service:bgo696762:1");

    gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

    g_signal_connect (G_OBJECT (cp),
                      "service-proxy-available",
                      G_CALLBACK (test_bgo_696762_on_sp_available),
                      &data);


    rd = gupnp_root_device_new (context, "TestBgo696762.xml", DATA_PATH);
    gupnp_root_device_set_available (rd, TRUE);
    info = gupnp_device_info_get_service (GUPNP_DEVICE_INFO (rd),
                                          "urn:test-gupnp-org:service:bgo696762:1");
    g_signal_connect (G_OBJECT (info),
                      "action-invoked::Browse",
                      G_CALLBACK (test_bgo_696762_on_browse_call),
                      &data);

    timeout_id = g_timeout_add_seconds (2, test_bgo_696762_on_timeout, &(data.loop));
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

    timeout_id = g_timeout_add_seconds (2, test_bgo_696762_on_timeout, &(data.loop));
    g_main_loop_run (data.loop);
    g_source_remove (timeout_id);
}

int
main (int argc, char *argv[]) {
#if !GLIB_CHECK_VERSION(2,35,0)
    g_type_init ();
#endif
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/bugs/696762", test_bgo_696762);

    return g_test_run ();
}
