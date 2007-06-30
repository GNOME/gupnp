/* 
 * Copyright (C) 2007 Zeeshan Ali <zeenix@gstreamer.net>
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali <zeenix@gstreamer.net>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <libgupnp/gupnp-control-point.h>
#include <libgupnp/gupnp-service-introspection.h>
#include <string.h>

static void
print_action_arguments (GSList *argument_list)
{
        GSList *iter;

        g_print ("\targuments:\n");
        for (iter = argument_list; iter; iter = iter->next) {
                GUPnPServiceActionArgInfo *argument;

                argument = (GUPnPServiceActionArgInfo *) iter->data;

                g_print ("\t\tname: %s\n"
                         "\t\tdirection: %s\n"
                         "\t\trelated state variable: %s\n\n",
                         argument->name,
                         argument->direction,
                         argument->related_state_variable);
        }
}

static void
print_actions (GUPnPServiceIntrospection *introspection)
{
        GSList *action_list;

        action_list = gupnp_service_introspection_list_actions (introspection);
        if (action_list) {
                GSList *iter;

                g_print ("actions:\n");
                for (iter = action_list; iter; iter = iter->next) {
                        GUPnPServiceActionInfo *action_info;
                        
                        action_info = (GUPnPServiceActionInfo *) iter->data;

                        g_print ("\tname: %s\n", action_info->name);
                        print_action_arguments (action_info->arguments);

                        gupnp_service_action_info_free (action_info);
                }
                g_print ("\n");
                g_slist_free (action_list);
        }
}

static void
print_state_variables (GUPnPServiceIntrospection *introspection)
{
        GSList *variables;

        variables = 
                gupnp_service_introspection_list_state_variables (
                                introspection);
        if (variables) {
                GSList *iter;

                g_print ("state variables:\n");
                for (iter = variables; iter; iter = iter->next) {
                        GUPnPServiceStateVariableInfo *variable;
                        GValue default_value;
                        
                        variable = (GUPnPServiceStateVariableInfo *) iter->data;

                        g_print ("\tname: %s\n"
                                 "\ttype: %s\n"
                                 "\tsend events: %s\n",
                                 variable->name,
                                 variable->data_type,
                                 variable->send_events? "yes": "no");

                        memset (&default_value, 0, sizeof (GValue));
                        g_value_init (&default_value, G_TYPE_STRING);
                        g_value_transform (&variable->default_value,
                                           &default_value);
                        g_print ("\tdefault value: %s\n", 
                                 g_value_get_string (&default_value));
                        g_value_unset (&default_value);
                        
                        if (variable->is_numeric) {
                                GValue min, max, step;

                                memset (&min, 0, sizeof (GValue));
                                memset (&max, 0, sizeof (GValue));
                                memset (&step, 0, sizeof (GValue));

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
                                GSList *value_iter;

                                g_print ("\tallowed values: ");
                                for (value_iter = variable->allowed_values;
                                     value_iter;
                                     value_iter = value_iter->next) {
                                        g_print ("\"%s\" ", (gchar *) value_iter->data);
                                }
                        }
                        
                        g_print ("\n");
                        gupnp_service_state_variable_info_free (variable);
                }
                g_print ("\n");
                g_slist_free (variables);
        }
}

static void
service_proxy_available_cb (GUPnPControlPoint *cp,
                            GUPnPServiceProxy *proxy)
{
        GUPnPServiceInfo *info;
        GUPnPServiceIntrospection *introspection;
        char *type;
        const char *location;

        info = GUPNP_SERVICE_INFO (proxy);
        type = gupnp_service_info_get_service_type (info);
        location = gupnp_service_info_get_location (info);

        g_print ("Service available:\n");
        g_print ("type:     %s\n", type);
        g_print ("location: %s\n", location);

        introspection = gupnp_service_info_get_introspection
                                        (GUPNP_SERVICE_INFO (proxy));
        if (introspection) {
                print_actions (introspection);
                print_state_variables (introspection);
                g_object_unref (introspection);
        }

        g_free (type);
}

static void
service_proxy_unavailable_cb (GUPnPControlPoint *cp,
                              GUPnPServiceProxy *proxy)
{
        char *type;
        const char *location;

        type = gupnp_service_info_get_service_type (GUPNP_SERVICE_INFO (proxy));
        location = gupnp_service_info_get_location (GUPNP_SERVICE_INFO (proxy));

        g_print ("Service unavailable:\n");
        g_print ("\ttype:     %s\n", type);
        g_print ("\tlocation: %s\n", location);

        g_free (type);
}

int
main (int argc, char **argv)
{
        GError *error;
        GUPnPContext *context;
        GUPnPControlPoint *cp;
        GMainLoop *main_loop;
        
        g_thread_init (NULL);
        g_type_init ();

        error = NULL;
        context = gupnp_context_new (NULL, NULL, 0, &error);
        if (error) {
                g_error (error->message);
                g_error_free (error);

                return 1;
        }

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
        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_object_unref (cp);
        g_object_unref (context);

        return 0;
}
