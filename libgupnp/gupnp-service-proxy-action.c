/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
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

#include "gupnp-service-proxy.h"
#include "gupnp-service-proxy-private.h"
#include "gupnp-service-proxy-action-private.h"
#include "gvalue-util.h"
#include "xml-util.h"

GUPnPServiceProxyAction *
gupnp_service_proxy_action_new_internal (const char *action) {
        GUPnPServiceProxyAction *ret;

        g_return_val_if_fail (action != NULL, NULL);

        ret = g_atomic_rc_box_new0 (GUPnPServiceProxyAction);
        ret->name = g_strdup (action);

        return ret;
}

GUPnPServiceProxyAction *
gupnp_service_proxy_action_ref (GUPnPServiceProxyAction *action)
{
        g_return_val_if_fail (action, NULL);

        return g_atomic_rc_box_acquire (action);
}

static void
action_dispose (GUPnPServiceProxyAction *action)
{
        if (action->proxy != NULL) {
                g_object_remove_weak_pointer (G_OBJECT (action->proxy),
                                (gpointer *)&(action->proxy));
                gupnp_service_proxy_remove_action (action->proxy, action);
        }

        g_clear_object (&action->msg);
        g_free (action->name);
}

void
gupnp_service_proxy_action_unref (GUPnPServiceProxyAction *action)
{
        g_return_if_fail (action);

        g_atomic_rc_box_release_full (action, (GDestroyNotify) action_dispose);
}

G_DEFINE_BOXED_TYPE (GUPnPServiceProxyAction,
                     gupnp_service_proxy_action,
                     gupnp_service_proxy_action_ref,
                     gupnp_service_proxy_action_unref)

/* Writes a parameter name and GValue pair to @msg */
static void
write_in_parameter (const char *arg_name,
                    GValue     *value,
                    GString    *msg_str)
{
        /* Write parameter pair */
        xml_util_start_element (msg_str, arg_name);
        gvalue_util_value_append_to_xml_string (value, msg_str);
        xml_util_end_element (msg_str, arg_name);
}

static void
write_footer (GUPnPServiceProxyAction *action)
{
        /* Finish message */
        g_string_append (action->msg_str, "</u:");
        g_string_append (action->msg_str, action->name);
        g_string_append_c (action->msg_str, '>');

        g_string_append (action->msg_str,
                         "</s:Body>"
                         "</s:Envelope>");
}

GUPnPServiceProxyAction *
gupnp_service_proxy_action_new (const char *action,
                                ...)
{
        va_list var_args;
        GList *in_names = NULL;
        GList *in_values = NULL;

        g_return_val_if_fail (action != NULL, NULL);

        va_start (var_args, action);
        VAR_ARGS_TO_IN_LIST (var_args, in_names, in_values);
        va_end (var_args);

        return gupnp_service_proxy_action_new_from_list (action,
                                                         in_names,
                                                         in_values);;
}


/**
 * gupnp_service_proxy_action_new_from_list:
 * @action: An action
 * @in_names: (element-type utf8) (transfer none): #GList of 'in' parameter
 * names (as strings)
 * @in_values: (element-type GValue) (transfer none): #GList of values (as
 * #GValue) that line up with @in_names
 */
GUPnPServiceProxyAction *
gupnp_service_proxy_action_new_from_list (const char *action_name,
                                          GList      *in_names,
                                          GList      *in_values)
{
        GUPnPServiceProxyAction *action;
        GList *names, *values;

        action = gupnp_service_proxy_action_new_internal (action_name);
        action->msg_str = xml_util_new_string ();

        g_string_append (action->msg_str,
                         "<?xml version=\"1.0\"?>"
                         "<s:Envelope xmlns:s="
                                "\"http://schemas.xmlsoap.org/soap/envelope/\" "
                          "s:encodingStyle="
                                "\"http://schemas.xmlsoap.org/soap/encoding/\">"
                         "<s:Body>");
        action->header_pos = action->msg_str->len;

        /* Arguments */
        for (names = in_names, values = in_values;
             names && values;
             names=names->next, values = values->next) {
                GValue* val = values->data;

                write_in_parameter (names->data,
                                    val,
                                    action->msg_str);
        }

        /* Finish and send off */
        write_footer (action);

        return action;
}
