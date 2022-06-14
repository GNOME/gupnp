#include <config.h>

#include <glib.h>
#include <glib-object.h>

#include "libgupnp/gupnp-context-manager.h"

#define TEST_CONTEXT_MANAGER (test_context_manager_get_type ())

struct _TestContextManager {
        GUPnPContextManager parent;
};
typedef struct _TestContextManager TestContextManager;

struct _TestContextManagerClass {
        GUPnPContextManagerClass parent_class;
};
typedef struct _TestContextManagerClass TestContextManagerClass;

G_DEFINE_TYPE (TestContextManager,
               test_context_manager,
               GUPNP_TYPE_CONTEXT_MANAGER)
static void
test_context_manager_class_init (TestContextManagerClass *klass)
{
}
static void
test_context_manager_init (TestContextManager *self)
{
}

void
test_context_manager_manage ()
{
        GError *error = NULL;

        GUPnPContext *ctx = gupnp_context_new ("lo", 0, &error);
        g_assert_null (error);

        GUPnPControlPoint *cp =
                gupnp_control_point_new (ctx, "upnp::rootdevice");
        gpointer weak = cp;

        g_object_add_weak_pointer (G_OBJECT (cp), &weak);

        TestContextManager *cm =
                g_object_new (test_context_manager_get_type (), NULL);

        gupnp_context_manager_manage_control_point (GUPNP_CONTEXT_MANAGER (cm),
                                                    cp);
        g_object_unref (cp);

        // Check that the context manager has kept a reference on cp
        g_assert_nonnull (weak);

        g_signal_emit_by_name (cm, "context-unavailable", ctx, NULL);

        // Check that the context manager dropped the reference if the
        // context is gone
        g_assert_null (weak);

        GUPnPRootDevice *rd = gupnp_root_device_new (ctx,
                                                     "TestDevice.xml",
                                                     DATA_PATH,
                                                     &error);

        weak = rd;
        g_object_add_weak_pointer (G_OBJECT (rd), &weak);
        gupnp_context_manager_manage_root_device (GUPNP_CONTEXT_MANAGER (cm),
                                                  rd);
        g_object_unref (rd);

        // Check that the context manager has kept a reference on cp
        g_assert_nonnull (weak);

        g_signal_emit_by_name (cm, "context-unavailable", ctx, NULL);

        // Check that the context manager dropped the reference if the
        // context is gone
        g_assert_null (weak);

        g_object_unref (cm);
        g_object_unref (ctx);
}


typedef struct _EnableDisableTestData {
        GUPnPContext *expected_context;
        gboolean triggered;
} EnableDisableTestData;

void
on_context_available (GUPnPContextManager *cm,
                      GUPnPContext *ctx,
                      gpointer user_data)
{
        EnableDisableTestData *d = user_data;

        g_assert (d->expected_context == ctx);
        d->triggered = TRUE;
}

void
test_context_manager_filter_enable_disable ()
{
        GError *error = NULL;

        GUPnPContext *ctx = g_initable_new (GUPNP_TYPE_CONTEXT,
                                            NULL,
                                            &error,
                                            "host-ip",
                                            "127.0.0.1",

                                            "network",
                                            "Free WiFi!",

                                            "active",
                                            FALSE,
                                            NULL);
        EnableDisableTestData context_available_triggered = {
                .expected_context = ctx,
                .triggered = FALSE
        };


        const char *iface = gssdp_client_get_interface (GSSDP_CLIENT (ctx));

        g_assert_no_error (error);
        g_assert_nonnull (ctx);

        TestContextManager *cm =
                g_object_new (test_context_manager_get_type (), NULL);

        // Check that the context filter is off and empty by default
        GUPnPContextFilter *cf = gupnp_context_manager_get_context_filter (
                GUPNP_CONTEXT_MANAGER (cm));
        g_assert_false (gupnp_context_filter_get_enabled (cf));
        g_assert (gupnp_context_filter_is_empty (cf));

        g_signal_connect (cm,
                          "context-available",
                          G_CALLBACK (on_context_available),
                          &context_available_triggered);

        g_signal_emit_by_name (cm, "context-available", ctx, NULL);

        g_assert (context_available_triggered.triggered);

        // Enable context filter. Since it is empty, we should still see the event
        context_available_triggered.triggered = FALSE;
        gupnp_context_filter_set_enabled (cf, TRUE);
        g_assert (gupnp_context_filter_get_enabled (cf));

        g_signal_emit_by_name (cm, "context-available", ctx, NULL);

        g_assert (context_available_triggered.triggered);

        // Enable context filter. Since it is empty, we should still see the event
        context_available_triggered.triggered = FALSE;
        gupnp_context_filter_set_enabled (cf, TRUE);
        g_assert (gupnp_context_filter_get_enabled (cf));

        g_signal_emit_by_name (cm, "context-available", ctx, NULL);

        g_assert (context_available_triggered.triggered);

        // Filter is enabled and not empty, we should get it since it matches
        context_available_triggered.triggered = FALSE;
        g_assert (gupnp_context_filter_get_enabled (cf));
        gupnp_context_filter_add_entry (cf, iface);
        g_assert_false (gupnp_context_filter_is_empty (cf));

        g_signal_emit_by_name (cm, "context-available", ctx, NULL);
        g_assert (context_available_triggered.triggered);

        g_clear_object (&ctx);
        g_clear_error (&error);
}

int
main (int argc, char *argv[])
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/context-manager/manage",
                         test_context_manager_manage);
        g_test_add_func ("/context_manager/filter/enable_disable",
                         test_context_manager_filter_enable_disable);

        return g_test_run ();
}
