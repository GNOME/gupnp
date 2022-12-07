// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Jens Georg <mail@jensge.org>

#include <glib-unix.h>
#include <libgupnp/gupnp.h>

typedef struct {
        GMainLoop *loop;
        GList *contexts;
} Data;

static void
on_context_available (GUPnPContextManager *context_manager,
                      GUPnPContext *context,
                      gpointer user_data)
{
        Data *data = user_data;

        g_print ("New context: %s %s\n",
                 gssdp_client_get_interface (GSSDP_CLIENT (context)),
                 gssdp_client_get_host_ip (GSSDP_CLIENT (context)));
        data->contexts =
                g_list_prepend (data->contexts, g_object_ref (context));
}

static void
on_context_unavailable (GUPnPContextManager *context_manager,
                        GUPnPContext *context,
                        gpointer user_data)
{
        Data *data = user_data;

        g_print ("Context unavailable: %s %s\n",
                 gssdp_client_get_interface (GSSDP_CLIENT (context)),
                 gssdp_client_get_host_ip (GSSDP_CLIENT (context)));

        GList *item = g_list_find (data->contexts, context);
        if (item != NULL) {
                g_object_unref (G_OBJECT (item->data));
                data->contexts = g_list_delete_link (data->contexts, item);
        }
}

int
main (int argc, char *argv[])
{
        Data data = { NULL, NULL };

        g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
        data.loop = loop;

        g_autoptr (GUPnPContextManager) mgr =
                gupnp_context_manager_create_full (GSSDP_UDA_VERSION_1_0,
                                                   G_SOCKET_FAMILY_INVALID,
                                                   0);

        g_signal_connect (mgr,
                          "context-available",
                          G_CALLBACK (on_context_available),
                          &data);
        g_signal_connect (mgr,
                          "context-unavailable",
                          G_CALLBACK (on_context_unavailable),
                          &data);

        //g_timeout_add_seconds (5, (GSourceFunc) g_main_loop_quit, loop);
        g_unix_signal_add (SIGINT, (GSourceFunc) g_main_loop_quit, loop);

        g_main_loop_run (loop);

        g_signal_handlers_disconnect_by_data (mgr, &data);

        g_list_free_full (data.contexts, g_object_unref);
        return 0;
}
