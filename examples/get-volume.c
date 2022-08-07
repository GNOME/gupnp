// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//         SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.

#include <glib.h>
#include <libgupnp/gupnp.h>

char CONTENT_DIR[] = "urn:schemas-upnp-org:service:RenderingControl:1";

gboolean on_timeout(gpointer user_data)
{
        g_main_loop_quit ((GMainLoop*)(user_data));

        return FALSE;
}

void
gvalue_free (gpointer value)
{
        g_value_unset ((GValue *) value);
        g_free (value);
}

void
on_introspection (GObject *object, GAsyncResult *res, gpointer user_data)
{
        GError *error = NULL;

        GUPnPServiceIntrospection *i = gupnp_service_info_introspect_finish (
                GUPNP_SERVICE_INFO (object),
                res,
                &error);

        if (error != NULL) {
                g_critical ("%s", error->message);
                g_clear_error (&error);
        }

        const GUPnPServiceActionInfo *info =
                gupnp_service_introspection_get_action (i, "GetVolume");
        const char *state_variable_name =
                ((GUPnPServiceActionArgInfo *) info->arguments->next->data)
                        ->related_state_variable;

        const char *channel = gupnp_service_introspection_get_state_variable (
                                      i,
                                      state_variable_name)
                                      ->allowed_values->data;
        g_print ("Calling GetVolume for channel %s...", channel);

        GUPnPServiceProxyAction *a =
                gupnp_service_proxy_action_new ("GetVolume",
                                                "InstanceID", G_TYPE_INT, 0,
                                                "Channel", G_TYPE_STRING, channel,
                                                NULL);
        gupnp_service_proxy_call_action (GUPNP_SERVICE_PROXY (object),
                                         a,
                                         NULL,
                                         &error);

        GList *out_names = NULL;
        out_names = g_list_prepend (out_names, "CurrentVolume");
        GList *out_types = NULL;
        out_types =
                g_list_prepend (out_types, GSIZE_TO_POINTER (G_TYPE_STRING));
        GList *out_values = NULL;


        gupnp_service_proxy_action_get_result_list (a,
                                                    out_names,
                                                    out_types,
                                                    &out_values,
                                                    &error);
        g_list_free (out_types);
        g_list_free (out_names);

        if (error != NULL) {
                g_critical ("%s", error->message);
                g_clear_error (&error);
        } else if (out_values != NULL) {
                g_print ("Current volume: %s\n",
                         g_value_get_string (out_values->data));
        }
        g_list_free_full (out_values, gvalue_free);

        gupnp_service_proxy_action_unref (a);
        g_object_unref (i);
}


void on_proxy_available (GUPnPControlPoint *cp, GUPnPServiceProxy *proxy, gpointer user_data)
{
        g_autofree char *id =
                gupnp_service_info_get_id (GUPNP_SERVICE_INFO (proxy));

        g_print ("Got ServiceProxy %s at %s\n",
                 id,
                 gupnp_service_info_get_location (GUPNP_SERVICE_INFO (proxy)));
        g_print ("Introspecting service ...\n");
        gupnp_service_info_introspect_async (GUPNP_SERVICE_INFO (proxy),
                                             NULL,
                                             on_introspection,
                                             NULL);
}

int main(int argc, char *argv[])
{
        GError *error = NULL;
        GMainLoop *loop = g_main_loop_new (NULL, FALSE);

        GUPnPContext *context = gupnp_context_new_full ("wlp3s0",
                                                        NULL,
                                                        0,
                                                        GSSDP_UDA_VERSION_1_0,
                                                        &error);

        if (error != NULL) {
                g_error ("%s", error->message);
        }

        GUPnPControlPoint *cp = gupnp_control_point_new (context, CONTENT_DIR);
        g_signal_connect (cp, "service-proxy-available", G_CALLBACK (on_proxy_available), NULL);
        gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

        g_timeout_add_seconds (10, on_timeout, loop);

        g_main_loop_run (loop);

        g_object_unref (cp);
        g_object_unref (context);
        g_main_loop_unref (loop);

        return 0;
}
