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

#include "gupnp-error-private.h"
#include "gupnp-service-proxy.h"
#include "gupnp-service-proxy-private.h"
#include "gupnp-service-proxy-action-private.h"
#include "gvalue-util.h"
#include "xml-util.h"

/* Reads a value into the parameter name and initialised GValue pair
 * from @response */
static void
read_out_parameter (const char *arg_name,
                    GValue     *value,
                    xmlNode    *params)
{
        xmlNode *param;

        /* Try to find a matching parameter in the response*/
        param = xml_util_get_element (params,
                                      arg_name,
                                      NULL);
        if (!param) {
                g_warning ("Could not find variable \"%s\" in response",
                           arg_name);

                return;
        }

        gvalue_util_set_value_from_xml_node (value, param);
}


/* Checks an action response for errors and returns the parsed
 * xmlDoc object. */
static xmlDoc *
check_action_response (G_GNUC_UNUSED GUPnPServiceProxy *proxy,
                       GUPnPServiceProxyAction         *action,
                       xmlNode                        **params,
                       GError                         **error)
{
        xmlDoc *response;
        int code;

        /* Check for errors */
        switch (action->msg->status_code) {
        case SOUP_STATUS_OK:
        case SOUP_STATUS_INTERNAL_SERVER_ERROR:
                break;
        default:
                _gupnp_error_set_server_error (error, action->msg);

                return NULL;
        }

        /* Parse response */
        response = xmlRecoverMemory (action->msg->response_body->data,
                                     action->msg->response_body->length);

        if (!response) {
                if (action->msg->status_code == SOUP_STATUS_OK) {
                        g_set_error (error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                     "Could not parse SOAP response");
                } else {
                        g_set_error_literal
                                    (error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INTERNAL_SERVER_ERROR,
                                     action->msg->reason_phrase);
                }

                return NULL;
        }

        /* Get parameter list */
        *params = xml_util_get_element ((xmlNode *) response,
                                        "Envelope",
                                        NULL);
        if (*params != NULL)
                *params = xml_util_real_node ((*params)->children);

        if (*params != NULL) {
                if (strcmp ((const char *) (*params)->name, "Header") == 0)
                        *params = xml_util_real_node ((*params)->next);

                if (*params != NULL)
                        if (strcmp ((const char *) (*params)->name, "Body") != 0)
                                *params = NULL;
        }

        if (*params != NULL)
                *params = xml_util_real_node ((*params)->children);

        if (*params == NULL) {
                g_set_error (error,
                             GUPNP_SERVER_ERROR,
                             GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                             "Invalid Envelope");

                xmlFreeDoc (response);

                return NULL;
        }

        /* Check whether we have a Fault */
        if (action->msg->status_code == SOUP_STATUS_INTERNAL_SERVER_ERROR) {
                xmlNode *param;
                char *desc;

                param = xml_util_get_element (*params,
                                              "detail",
                                              "UPnPError",
                                              NULL);

                if (!param) {
                        g_set_error (error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                     "Invalid Fault");

                        xmlFreeDoc (response);

                        return NULL;
                }

                /* Code */
                code = xml_util_get_child_element_content_int
                                        (param, "errorCode");
                if (code == -1) {
                        g_set_error (error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                     "Invalid Fault");

                        xmlFreeDoc (response);

                        return NULL;
                }

                /* Description */
                desc = xml_util_get_child_element_content_glib
                                        (param, "errorDescription");
                if (desc == NULL)
                        desc = g_strdup (action->msg->reason_phrase);

                g_set_error_literal (error,
                                     GUPNP_CONTROL_ERROR,
                                     code,
                                     desc);

                g_free (desc);

                xmlFreeDoc (response);

                return NULL;
        }

        return response;
}


/* GDestroyNotify for GHashTable holding GValues.
 */
G_GNUC_INTERNAL void
_value_free (gpointer data);

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

        if (action->cancellable != NULL && action->cancellable_connection_id != 0) {
                g_cancellable_disconnect (action->cancellable,
                                          action->cancellable_connection_id);
        }
        g_clear_object (&action->cancellable);
        g_clear_error (&action->error);
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
        GList *in_names = NULL;
        GList *in_values = NULL;
        GUPnPServiceProxyAction *result = NULL;
        va_list var_args;

        g_return_val_if_fail (action != NULL, NULL);

        va_start (var_args, action);
        VAR_ARGS_TO_IN_LIST (var_args, in_names, in_values);
        va_end (var_args);

        result = gupnp_service_proxy_action_new_from_list (action,
                                                           in_names,
                                                           in_values);;
        g_list_free_full (in_names, g_free);
        g_list_free_full (in_values, gvalue_free);

        return result;
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

/**
 * gupnp_service_proxy_action_get_result_list:
 * @action: A #GUPnPServiceProxyAction handle
 * @out_names: (element-type utf8) (transfer none): #GList of 'out' parameter
 * names (as strings)
 * @out_types: (element-type GType) (transfer none): #GList of types (as #GType)
 * that line up with @out_names
 * @out_values: (element-type GValue) (transfer full) (out): #GList of values
 * (as #GValue) that line up with @out_names and @out_types.
 * @error: The location where to store any error, or %NULL
 *
 * A variant of gupnp_service_proxy_action_get_result() that takes lists of
 * out-parameter names, types and place-holders for values. The returned list
 * in @out_values must be freed using #g_list_free and each element in it using
 * #g_value_unset and #g_free.
 *
 * Return value : %TRUE on success.
 *
 **/
gboolean
gupnp_service_proxy_action_get_result_list (GUPnPServiceProxyAction *action,
                                            GList                   *out_names,
                                            GList                   *out_types,
                                            GList                  **out_values,
                                            GError                 **error)
{
        xmlDoc *response;
        xmlNode *params;
        GList *names;
        GList *types;
        GList *out_values_list;

        out_values_list = NULL;

        g_return_val_if_fail (action, FALSE);

        /* Check for saved error from begin_action() */
        if (action->error) {
                g_propagate_error (error, g_error_copy (action->error));

                return FALSE;
        }

        /* Check response for errors and do initial parsing */
        response = check_action_response (NULL, action, &params, &action->error);
        if (response == NULL) {
                g_propagate_error (error, g_error_copy (action->error));

                return FALSE;
        }

        /* Read arguments */
        types = out_types;
        for (names = out_names; names; names=names->next) {
                GValue *val;

                val = g_new0 (GValue, 1);
                g_value_init (val, (GType) types->data);

                read_out_parameter (names->data, val, params);

                out_values_list = g_list_append (out_values_list, val);

                types = types->next;
        }

        *out_values = out_values_list;

        /* Cleanup */
        xmlFreeDoc (response);

        return TRUE;

}

/**
 * gupnp_service_proxy_action_get_result_hash:
 * @action: A #GUPnPServiceProxyAction handle
 * @out_hash: (element-type utf8 GValue) (inout) (transfer none): A #GHashTable of
 * out parameter name and initialised #GValue pairs
 * @error: The location where to store any error, or %NULL
 *
 * See gupnp_service_proxy_action_get_result(); this version takes a #GHashTable for
 * runtime generated parameter lists.
 *
 * Return value: %TRUE on success.
 *
 **/
gboolean
gupnp_service_proxy_action_get_result_hash (GUPnPServiceProxyAction *action,
                                            GHashTable              *hash,
                                            GError                 **error)
{
        xmlDoc *response;
        xmlNode *params;

        g_return_val_if_fail (action, FALSE);

        /* Check for saved error from begin_action() */
        if (action->error) {
                g_propagate_error (error, g_error_copy (action->error));

                return FALSE;
        }

        /* Check response for errors and do initial parsing */
        response = check_action_response (NULL, action, &params, &action->error);
        if (response == NULL) {
                g_propagate_error (error, g_error_copy (action->error));

                return FALSE;
        }

        /* Read arguments */
        g_hash_table_foreach (hash, (GHFunc) read_out_parameter, params);

        /* Cleanup */
        xmlFreeDoc (response);

        return TRUE;

}

gboolean
gupnp_service_proxy_action_get_result (GUPnPServiceProxyAction *action,
                                       GError                 **error,
                                       ...)
{
        va_list var_args;
        gboolean ret;

        va_start (var_args, error);
        ret = gupnp_service_proxy_action_get_result_valist (action,
                                                            error,
                                                            var_args);
        va_end (var_args);

        return ret;
}

gboolean
gupnp_service_proxy_action_get_result_valist (GUPnPServiceProxyAction *action,
                                              GError                 **error,
                                              va_list                  var_args)
{
        GHashTable *out_hash;
        va_list var_args_copy;
        gboolean result;
        GError *local_error;

        g_return_val_if_fail (action, FALSE);

        out_hash = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          gvalue_free);
        G_VA_COPY (var_args_copy, var_args);
        VAR_ARGS_TO_OUT_HASH_TABLE (var_args, out_hash);
        local_error = NULL;
        result = gupnp_service_proxy_action_get_result_hash (action,
                                                             out_hash,
                                                             &local_error);

        if (local_error == NULL) {
                OUT_HASH_TABLE_TO_VAR_ARGS (out_hash, var_args_copy);
        } else {
                g_propagate_error (error, local_error);
        }
        va_end (var_args_copy);
        g_hash_table_unref (out_hash);

        return result;
}
