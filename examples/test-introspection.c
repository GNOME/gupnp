/*
 * Copyright (C) 2007 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
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

#include <libgupnp/gupnp-control-point.h>
#include <libgupnp/gupnp-service-introspection.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <glib.h>

#ifndef G_OS_WIN32
#include <glib-unix.h>
#endif

GMainLoop *main_loop;

static GCancellable *cancellable;

static gboolean
unix_signal_handler (gpointer user_data)
{
        if (g_cancellable_is_cancelled (cancellable)) {
                g_main_loop_quit (main_loop);

                return G_SOURCE_REMOVE;
        } else {
                g_print ("Canceling all introspection calls. "
                         "Press ^C again to force quit.\n");
                g_cancellable_cancel (cancellable);
                return G_SOURCE_CONTINUE;
        }
}

#ifdef G_OS_WIN32
/*
 *  Since in Windows this is basically called from
 *  the ConsoleCtrlHandler which does not share the restrictions of Unix signals
 */
static void
interrupt_signal_handler (G_GNUC_UNUSED int signum)
{
        unix_signal_handler (NULL);
}

#endif

static void
print_action_arguments (GList *argument_list)
{
        GList *iter;

        g_print ("\targuments:\n");
        for (iter = argument_list; iter; iter = iter->next) {
                GUPnPServiceActionArgInfo *argument;

                argument = (GUPnPServiceActionArgInfo *) iter->data;

                g_print ("\t\tname: %s\n"
                         "\t\tdirection: %s\n"
                         "\t\trelated state variable: %s\n\n",
                         argument->name,
                         (argument->direction ==
                          GUPNP_SERVICE_ACTION_ARG_DIRECTION_IN)? "in": "out",
                         argument->related_state_variable);
        }
}

static void
print_actions (GUPnPServiceIntrospection *introspection)
{
        const GList *action_list;

        action_list = gupnp_service_introspection_list_actions (introspection);
        if (action_list) {
                const GList *iter;

                g_print ("actions:\n");
                for (iter = action_list; iter; iter = iter->next) {
                        GUPnPServiceActionInfo *action_info;

                        action_info = (GUPnPServiceActionInfo *) iter->data;

                        g_print ("\tname: %s\n", action_info->name);
                        print_action_arguments (action_info->arguments);
                }
                g_print ("\n");
        }
}

static void
print_state_variables (GUPnPServiceIntrospection *introspection)
{
        const GList *variables;

        variables =
                gupnp_service_introspection_list_state_variables (
                                introspection);
        if (variables) {
                const GList *iter;

                g_print ("state variables:\n");
                for (iter = variables; iter; iter = iter->next) {
                        GUPnPServiceStateVariableInfo *variable;
                        GValue default_value = G_VALUE_INIT;
                        const char * default_value_str;

                        variable = (GUPnPServiceStateVariableInfo *) iter->data;

                        g_print ("\tname: %s\n"
                                 "\ttype: %s\n"
                                 "\tsend events: %s\n",
                                 variable->name,
                                 g_type_name (variable->type),
                                 variable->send_events? "yes": "no");

                        g_value_init (&default_value, G_TYPE_STRING);
                        g_value_transform (&variable->default_value,
                                           &default_value);
                        default_value_str = g_value_get_string (&default_value);
                        if (default_value_str) {
                                g_print ("\tdefault value: %s\n",
                                         default_value_str);
                        }
                        g_value_unset (&default_value);

                        if (variable->is_numeric) {
                                GValue min = G_VALUE_INIT, max = G_VALUE_INIT, step = G_VALUE_INIT;

                                g_value_init (&min, G_TYPE_STRING);
                                g_value_init (&max, G_TYPE_STRING);
                                g_value_init (&step, G_TYPE_STRING);

                                g_value_transform (&variable->minimum, &min);
                                g_value_transform (&variable->maximum, &max);
                                g_value_transform (&variable->step, &step);

                                g_print ("\tminimum: %s\n"
                                         "\tmaximum: %s\n"
                                         "\tstep: %s\n",
                                         g_value_get_string (&min),
                                         g_value_get_string (&max),
                                         g_value_get_string (&step));

                                g_value_unset (&min);
                                g_value_unset (&max);
                                g_value_unset (&step);
                        }

                        if (variable->allowed_values) {
                                GList *value_iter;

                                g_print ("\tallowed values: ");
                                for (value_iter = variable->allowed_values;
                                     value_iter;
                                     value_iter = value_iter->next) {
                                        g_print ("\"%s\" ",
                                                 (gchar *) value_iter->data);
                                }
                        }

                        g_print ("\n");
                }
                g_print ("\n");
        }
}

static void
got_introspection (GObject           *source,
                   GAsyncResult *res,
                   G_GNUC_UNUSED gpointer     user_data)
{
        GError *error = NULL;
        GUPnPServiceIntrospection *introspection =
                gupnp_service_info_introspect_finish (GUPNP_SERVICE_INFO (GUPNP_SERVICE_INFO (source)),
                                                      res,
                                                      &error);
        if (error) {
                g_warning ("Failed to create introspection for '%s': %s",
                           gupnp_service_info_get_udn (GUPNP_SERVICE_INFO (source)),
                           error->message);

                return;
        }

        g_print ("service:  %s\nlocation: %s\n",
                 gupnp_service_info_get_udn (GUPNP_SERVICE_INFO (source)),
                gupnp_service_info_get_location (GUPNP_SERVICE_INFO (source)));
        print_actions (introspection);
        print_state_variables (introspection);
        g_object_unref (introspection);
}

static void
service_proxy_available_cb (G_GNUC_UNUSED GUPnPControlPoint *cp,
                            GUPnPServiceProxy               *proxy)
{
        GUPnPServiceInfo *info;

        info = GUPNP_SERVICE_INFO (proxy);

        gupnp_service_info_introspect_async (info,
                                             cancellable,
                                             got_introspection,
                                             NULL);
}

static void
service_proxy_unavailable_cb (G_GNUC_UNUSED GUPnPControlPoint *cp,
                              GUPnPServiceProxy               *proxy)
{
        const char *type, *location;

        type = gupnp_service_info_get_service_type (GUPNP_SERVICE_INFO (proxy));
        location = gupnp_service_info_get_location (GUPNP_SERVICE_INFO (proxy));

        g_print ("Service unavailable:\n");
        g_print ("\ttype:     %s\n", type);
        g_print ("\tlocation: %s\n", location);
}

int
main (int argc, char **argv)
{
        GError *error = NULL;
        GUPnPContext *context;
        GUPnPControlPoint *cp;

        error = NULL;
        context = gupnp_context_new_for_address (
                g_inet_address_new_from_string ("192.168.178.78"),
                0,
                GSSDP_UDA_VERSION_1_0,
                &error);
        if (error) {
                g_printerr ("Error creating the GUPnP context: %s\n",
                            error->message);
                g_error_free (error);

                return EXIT_FAILURE;
        }

        cancellable = g_cancellable_new ();

        /* We're interested in everything */
        cp = gupnp_control_point_new (context, "ssdp:all");

        g_signal_connect (cp,
                          "service-proxy-available",
                          G_CALLBACK (service_proxy_available_cb),
                          NULL);
        g_signal_connect (cp,
                          "service-proxy-unavailable",
                          G_CALLBACK (service_proxy_unavailable_cb),
                          NULL);

        gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

        main_loop = g_main_loop_new (NULL, FALSE);

        /* Hook the handler for SIGINT */
#ifndef G_OS_WIN32
        g_unix_signal_add (SIGINT, unix_signal_handler, NULL);
#else
        signal(SIGINT,interrupt_signal_handler);
#endif /* G_OS_WIN32 */

        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_object_unref (cp);
        g_object_unref (context);

        return EXIT_SUCCESS;
}
