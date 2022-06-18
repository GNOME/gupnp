/*
 * SPDX-FileCopyrightText: 2022 Jens Georg
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <config.h>

#include "libgupnp/gupnp-context-filter.h"

#include <glib-object.h>
#include <glib.h>

void
test_context_filter_construction ()
{
        // Default-created filter. No entries, disabled.
        GUPnPContextFilter *filter =
                g_object_new (GUPNP_TYPE_CONTEXT_FILTER, NULL);

        g_assert_null (gupnp_context_filter_get_entries (filter));
        g_assert (gupnp_context_filter_is_empty (filter));
        g_assert_false (gupnp_context_filter_get_enabled (filter));

        g_object_unref (filter);

        // Filter that is enabled since construction
        filter =
                g_object_new (GUPNP_TYPE_CONTEXT_FILTER, "enabled", TRUE, NULL);

        g_assert_null (gupnp_context_filter_get_entries (filter));
        g_assert (gupnp_context_filter_is_empty (filter));
        g_assert (gupnp_context_filter_get_enabled (filter));

        g_object_unref (filter);

        GList *entries = NULL;
        entries = g_list_prepend (entries, g_strdup ("eth0"));
        entries = g_list_prepend (entries, g_strdup ("::1"));
        entries = g_list_prepend (entries, g_strdup ("127.0.0.1"));
        entries = g_list_prepend (entries, g_strdup ("Free WiFi!"));

        filter = g_object_new (GUPNP_TYPE_CONTEXT_FILTER,
                               "entries",
                               entries,
                               NULL);

        g_list_free_full (entries, g_free);

        entries = gupnp_context_filter_get_entries (filter);
        g_assert_cmpint (g_list_length (entries), ==, 4);
        GList *it = entries;
        while (it != NULL) {
                g_assert (g_str_equal ("eth0", it->data) ||
                          g_str_equal ("::1", it->data) ||
                          g_str_equal ("127.0.0.1", it->data) ||
                          g_str_equal ("Free WiFi!", it->data));
                it = g_list_next (it);
        }

        g_list_free (entries);

        g_object_unref (filter);
}

void
on_entry (GObject *object, GParamSpec *psec, gpointer user_data)
{
        int *count = user_data;
        *count = *count + 1;
}

void
test_context_filter_entry_management ()
{
        int entry_change_count = 0;

        // Default-created filter. No entries, disabled.
        GUPnPContextFilter *filter =
                g_object_new (GUPNP_TYPE_CONTEXT_FILTER, NULL);

        g_assert_null (gupnp_context_filter_get_entries (filter));
        g_assert (gupnp_context_filter_is_empty (filter));
        g_assert_false (gupnp_context_filter_get_enabled (filter));

        g_signal_connect (filter,
                          "notify::entries",
                          G_CALLBACK (on_entry),
                          &entry_change_count);
        g_assert_cmpint (entry_change_count, ==, 0);

        entry_change_count = 0;
        gupnp_context_filter_add_entry (filter, "eth0");

        // Just adding an entry should not trigger the filter
        g_assert_false (gupnp_context_filter_get_enabled (filter));
        g_assert_false (gupnp_context_filter_is_empty (filter));
        g_assert_cmpint (entry_change_count, ==, 1);

        GList *entries = gupnp_context_filter_get_entries (filter);
        g_assert_cmpstr (entries->data, ==, "eth0");

        g_list_free (entries);

        entry_change_count = 0;
        gupnp_context_filter_add_entry (filter, "Free WiFi!");

        // Just adding an entry should not trigger the filter
        g_assert_false (gupnp_context_filter_get_enabled (filter));
        g_assert_false (gupnp_context_filter_is_empty (filter));
        g_assert_cmpint (entry_change_count, ==, 1);

        entries = gupnp_context_filter_get_entries (filter);

        g_assert_cmpstr (entries->data, ==, "eth0");

        GList *it = entries;
        while (it != NULL) {
                g_assert (g_str_equal ("eth0", it->data) ||
                          g_str_equal ("Free WiFi!", it->data));
                it = g_list_next (it);
        }

        g_list_free (entries);

        entry_change_count = 0;
        gupnp_context_filter_remove_entry (filter, "eth0");

        // Just adding an entry should not trigger the filter
        g_assert_false (gupnp_context_filter_get_enabled (filter));
        g_assert_false (gupnp_context_filter_is_empty (filter));
        g_assert_cmpint (entry_change_count, ==, 1);
        entries = gupnp_context_filter_get_entries (filter);

        g_assert_cmpstr (entries->data, ==, "Free WiFi!");
        g_list_free (entries);

        entry_change_count = 0;
        gupnp_context_filter_remove_entry (filter, "Free WiFi!");

        g_assert_false (gupnp_context_filter_get_enabled (filter));
        g_assert (gupnp_context_filter_is_empty (filter));
        g_assert_cmpint (entry_change_count, ==, 1);
        g_assert_null (gupnp_context_filter_get_entries (filter));

        {
                char *entryv[] = { "eth0", "eth1", "eth2", "eth3", NULL };

                entry_change_count = 0;
                gupnp_context_filter_add_entryv (filter, entryv);
                g_assert_false (gupnp_context_filter_get_enabled (filter));
                g_assert_false (gupnp_context_filter_is_empty (filter));
                g_assert_cmpint (entry_change_count, ==, 1);
        }

        // Re-adding the same entries should not cause any notification
        {
                char *entryv[] = { "eth0", "eth3", NULL };

                entry_change_count = 0;
                gupnp_context_filter_add_entryv (filter, entryv);
                g_assert_false (gupnp_context_filter_get_enabled (filter));
                g_assert_false (gupnp_context_filter_is_empty (filter));
                g_assert_cmpint (entry_change_count, ==, 0);
        }

        entry_change_count = 0;
        gupnp_context_filter_clear (filter);
        g_assert_false (gupnp_context_filter_get_enabled (filter));
        g_assert_true (gupnp_context_filter_is_empty (filter));
        g_assert_cmpint (entry_change_count, ==, 1);

        g_object_unref (filter);
}

void
on_enabled (GObject *object, GParamSpec *psec, gpointer user_data)
{
        int *count = user_data;
        *count = *count + 1;
}

void
test_context_filter_enable_disable ()
{
        int enabled_count = 0;

        // Default-created filter. No entries, disabled.
        GUPnPContextFilter *filter =
                g_object_new (GUPNP_TYPE_CONTEXT_FILTER, NULL);

        g_signal_connect (filter,
                          "notify::enabled",
                          G_CALLBACK (on_enabled),
                          &enabled_count);

        g_assert_false (gupnp_context_filter_get_enabled (filter));

        enabled_count = 0;
        gupnp_context_filter_set_enabled (filter, FALSE);
        g_assert_false (gupnp_context_filter_get_enabled (filter));
        g_assert_cmpint (enabled_count, ==, 0);

        enabled_count = 0;
        gupnp_context_filter_set_enabled (filter, TRUE);
        g_assert (gupnp_context_filter_get_enabled (filter));
        g_assert_cmpint (enabled_count, ==, 1);

        enabled_count = 0;
        gupnp_context_filter_set_enabled (filter, TRUE);
        g_assert (gupnp_context_filter_get_enabled (filter));

        g_assert_cmpint (enabled_count, ==, 0);
        enabled_count = 0;
        gupnp_context_filter_set_enabled (filter, FALSE);
        g_assert_false (gupnp_context_filter_get_enabled (filter));
        g_assert_cmpint (enabled_count, ==, 1);

        enabled_count = 0;
        gupnp_context_filter_set_enabled (filter, FALSE);
        g_assert_false (gupnp_context_filter_get_enabled (filter));
        g_assert_cmpint (enabled_count, ==, 0);

        enabled_count = 0;
        g_object_set (G_OBJECT (filter), "enabled", FALSE, NULL);
        g_assert_false (gupnp_context_filter_get_enabled (filter));
        g_assert_cmpint (enabled_count, ==, 0);

        enabled_count = 0;
        g_object_set (G_OBJECT (filter), "enabled", TRUE, NULL);
        g_assert (gupnp_context_filter_get_enabled (filter));
        g_assert_cmpint (enabled_count, ==, 1);

        enabled_count = 0;
        g_object_set (G_OBJECT (filter), "enabled", TRUE, NULL);
        g_assert (gupnp_context_filter_get_enabled (filter));
        g_assert_cmpint (enabled_count, ==, 0);

        enabled_count = 0;
        g_object_set (G_OBJECT (filter), "enabled", FALSE, NULL);
        g_assert_false (gupnp_context_filter_get_enabled (filter));
        g_assert_cmpint (enabled_count, ==, 1);

        g_object_unref (filter);
}

void
test_context_filter_match ()
{
        // Default-created filter. No entries, disabled.
        GUPnPContextFilter *filter =
                g_object_new (GUPNP_TYPE_CONTEXT_FILTER, NULL);

        GUPnPContext *context = g_object_new (GUPNP_TYPE_CONTEXT,
                                              "host-ip",
                                              "127.0.0.1",
                                              "interface",
                                              "lo",
                                              "network",
                                              "FreeWiFi",
                                              NULL);

        g_assert_false (gupnp_context_filter_check_context (filter, context));
        gupnp_context_filter_set_enabled (filter, TRUE);
        g_assert_false (gupnp_context_filter_check_context (filter, context));
        gupnp_context_filter_set_enabled (filter, FALSE);

        g_assert (gupnp_context_filter_add_entry (filter, "FreeWiFi"));
        g_assert (gupnp_context_filter_check_context (filter, context));
        gupnp_context_filter_set_enabled (filter, TRUE);
        g_assert (gupnp_context_filter_check_context (filter, context));
        gupnp_context_filter_set_enabled (filter, FALSE);
        gupnp_context_filter_remove_entry (filter, "FreeWiFi");

        g_assert (gupnp_context_filter_add_entry (filter, "lo"));
        g_assert (gupnp_context_filter_check_context (filter, context));
        gupnp_context_filter_set_enabled (filter, TRUE);
        g_assert (gupnp_context_filter_check_context (filter, context));
        gupnp_context_filter_set_enabled (filter, FALSE);
        gupnp_context_filter_remove_entry (filter, "lo");

        g_assert (gupnp_context_filter_add_entry (filter, "127.0.0.1"));
        g_assert (gupnp_context_filter_check_context (filter, context));
        gupnp_context_filter_set_enabled (filter, TRUE);
        g_assert (gupnp_context_filter_check_context (filter, context));
        gupnp_context_filter_set_enabled (filter, FALSE);
        gupnp_context_filter_remove_entry (filter, "127.0.0.1");

        g_object_unref (context);

        g_object_unref (filter);
}

int
main (int argc, char *argv[])
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/context-filter/construction",
                         test_context_filter_construction);

        g_test_add_func ("/context-filter/entry-management",
                         test_context_filter_entry_management);

        g_test_add_func ("/context-filter/enable-disable",
                         test_context_filter_enable_disable);

        g_test_add_func ("/context-filter/match", test_context_filter_match);

        return g_test_run ();
}
