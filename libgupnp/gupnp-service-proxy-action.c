/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include <config.h>

#include <libxml/parser.h>

#include "gupnp-error.h"
#include "gupnp-service-proxy.h"
#include "gvalue-util.h"
#include "xml-util.h"

#include "gupnp-error-private.h"
#include "gupnp-service-info-private.h"
#include "gupnp-service-proxy-action-private.h"

struct _GUPnPServiceProxyActionIter {
        GObject parent;

        GUPnPServiceProxyAction *action;
        xmlNode *current;
        GUPnPServiceIntrospection *introspection;
        gboolean iterating;
};

G_DEFINE_TYPE (GUPnPServiceProxyActionIter,
               gupnp_service_proxy_action_iter,
               G_TYPE_OBJECT)

static void
gupnp_service_proxy_action_iter_init (GUPnPServiceProxyActionIter *self)
{
}

static void
gupnp_service_proxy_action_iter_dispose (GObject *object)
{
        GUPnPServiceProxyActionIter *self =
                GUPNP_SERVICE_PROXY_ACTION_ITER (object);
        g_clear_pointer (&self->action, gupnp_service_proxy_action_unref);
        g_clear_object (&self->introspection);

        G_OBJECT_CLASS (gupnp_service_proxy_action_iter_parent_class)
                ->dispose (object);
}

static void
gupnp_service_proxy_action_iter_class_init (
        GUPnPServiceProxyActionIterClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->dispose = gupnp_service_proxy_action_iter_dispose;
}

/**
 * gupnp_service_proxy_action_iterate:
 * @action: A finished GUPnPServiceProxyAction
 * @error: (out)(nullable): Return location for an optional error
 *
 * Iterate over the out arguments of a finished action
 *
 * Returns: (transfer full)(nullable): A newly created GUPnPServiceProxyActionIterator, or %NULL on error
 * Since: 1.6.6
 */
GUPnPServiceProxyActionIter *
gupnp_service_proxy_action_iterate (GUPnPServiceProxyAction *action,
                                    GError **error)
{
        g_return_val_if_fail (action != NULL, NULL);
        g_return_val_if_fail (!action->pending, NULL);

        /* Check for saved error from begin_action() */
        if (action->error) {
                g_propagate_error (error, g_error_copy (action->error));

                return NULL;
        }

        /* Check response for errors and do initial parsing */
        gupnp_service_proxy_action_check_response (action);
        if (action->error != NULL) {
                g_propagate_error (error, g_error_copy (action->error));

                return NULL;
        }

        g_type_ensure (gupnp_service_proxy_action_iter_get_type ());

        GUPnPServiceProxyActionIter *iter =
                g_object_new (gupnp_service_proxy_action_iter_get_type (),
                              NULL);
        iter->action = gupnp_service_proxy_action_ref (action);

        if (action->proxy != NULL) {
                iter->introspection = gupnp_service_info_get_introspection (
                        GUPNP_SERVICE_INFO (action->proxy));
                if (iter->introspection != NULL) {
                        g_object_ref (iter->introspection);
                }
        }

        return iter;
}

/**
 * gupnp_service_proxy_action_iter_next:
 * @self: A GUPnP.ServiceProxyActionIter
 *
 * Move @self to the next out value of the iterated action
 *
 * Returns: %TRUE if the next value was available
 * Since: 1.6.6
 */
gboolean
gupnp_service_proxy_action_iter_next (GUPnPServiceProxyActionIter *self)
{
        if (self->iterating) {
                self->current = self->current->next;
        } else {
                self->current = self->action->params->children;
                self->iterating = TRUE;
        }

        return self->current != NULL;
}

/**
 * gupnp_service_proxy_action_iter_get_name:
 * @self: A GUPnP.ServiceProxyActionIter
 *
 * Get the name of the current out argument
 *
 * Returns: Name of the current argument
 * Since: 1.6.6
 */
const char *
gupnp_service_proxy_action_iter_get_name (GUPnPServiceProxyActionIter *self)
{
        g_return_val_if_fail (self->current != NULL, NULL);

        return (const char *) self->current->name;
}

static int
find_argument (gconstpointer a, gconstpointer b)
{
        const GUPnPServiceActionArgInfo *arg = a;
        const char *needle = b;

        return g_strcmp0 (arg->name, needle);
}

/**
 * gupnp_service_proxy_action_iter_get_value:
 * @self: A GUPnP.ServiceProxyActionIter
 * @value: (out): The value
 *
 * Get the value of the current parameter.
 *
 * If the service proxy had a successful introspection, the type according
 * to the introspection data will be used, otherwise it will be string.
 *
 * Returns: %TRUE if the value could be read successfully
 * Since: 1.6.6
 */

gboolean
gupnp_service_proxy_action_iter_get_value (GUPnPServiceProxyActionIter *self,
                                           GValue *value)
{
        if (self->introspection != NULL) {
                const GUPnPServiceActionInfo *action_info =
                        gupnp_service_introspection_get_action (
                                self->introspection,
                                self->action->name);
                GList *result = g_list_find_custom (action_info->arguments,
                                                    self->current->name,
                                                    find_argument);
                if (result == NULL) {
                        g_debug ("No argument %s\n", self->current->name);
                        return FALSE;
                }

                const GUPnPServiceActionArgInfo *arg = result->data;
                const char *var = arg->related_state_variable;
                const GUPnPServiceStateVariableInfo *info =
                        gupnp_service_introspection_get_state_variable (
                                self->introspection,
                                var);

                if (info == NULL) {
                        g_debug ("No state variable for %s\n",
                                 self->current->name);
                        return FALSE;
                }

                g_value_init (value, info->type);
        } else {
                // We know nothing about the type, so just give out the string and let
                // the user transform the value
                g_value_init (value, G_TYPE_STRING);
        }

        gvalue_util_set_value_from_xml_node (value, self->current);
        return TRUE;
}

/**
 * gupnp_service_proxy_action_iter_get_value_as:
 * @self: A GUPnP.ServiceProxyActionIter
 * @type: The type to convert the value to
 * @value: (out): The value
 *
 * Get the value of the current parameter.
 *
 * Converts the value to the given type, similar to the other
 * finish_action functions.
 *
 * Returns: %TRUE if the value could be read successfully
 * Since: 1.6.8
 */

gboolean
gupnp_service_proxy_action_iter_get_value_as (GUPnPServiceProxyActionIter *self,
                                              GType type,
                                              GValue *value)
{
        g_value_init (value, type);

        gvalue_util_set_value_from_xml_node (value, self->current);
        return TRUE;
}

struct _ActionArgument {
        char *name;
        GValue value;
};
typedef struct _ActionArgument ActionArgument;

void
action_argument_free (ActionArgument* arg)
{
        g_free (arg->name);
        g_value_unset (&arg->value);
        g_free (arg);
}

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

/**
 * gupnp_service_proxy_action_new_plain:
 * @action: The name of a remote action to call
 *
 * Prepares action @action with to be sent off to
 * a remote service later with gupnp_service_proxy_call_action() or
 * gupnp_service_proxy_call_action_async() if no arguments required or by adding more
 * parameters with gupnp_service_proxy_action_add()
 *
 * After the action call has finished, the results of the call may be
 * retrived from the #GUPnPServiceProxyAction by using
 * gupnp_service_proxy_action_get_result(),
 * gupnp_service_proxy_action_get_result_list() or
 * gupnp_service_proxy_action_get_result_hash()
 *
 * ```c
 * GUPnPServiceProxyAction *action =
 *         gupnp_service_proxy_action_new_plain ("GetVolume");
 * gupnp_service_proxy_action_add (action, "InstanceID", value_instance);
 * gupnp_service_proxy_action_add (action, "Channel", value_channel);
 * ````
 *
 * Returns: A newly created #GUPnPServiceProxyAction
 * Since: 1.6.6
 */
GUPnPServiceProxyAction *
gupnp_service_proxy_action_new_plain (const char *action)
{
        GUPnPServiceProxyAction *ret;

        g_return_val_if_fail (action != NULL, NULL);

        ret = g_atomic_rc_box_new0 (GUPnPServiceProxyAction);
        ret->name = g_strdup (action);
        ret->args = g_ptr_array_new_with_free_func((GDestroyNotify)action_argument_free);
        ret->arg_map = g_hash_table_new (g_str_hash, g_str_equal);

        return ret;
}

/**
 * gupnp_service_proxy_action_ref:
 * @action: an action
 *
 * Increases reference count of `action`
 *
 * Returns: (nullable): @action with an increased reference count
 * Since: 1.2.0
 */
GUPnPServiceProxyAction *
gupnp_service_proxy_action_ref (GUPnPServiceProxyAction *action)
{
        g_return_val_if_fail (action, NULL);

        return g_atomic_rc_box_acquire (action);
}

void
gupnp_service_proxy_action_reset (GUPnPServiceProxyAction *action)
{
        g_clear_weak_pointer (&action->proxy);
        g_clear_error (&action->error);
        g_clear_object (&action->msg);

        if (action->msg_str != NULL) {
                g_string_free (action->msg_str, TRUE);
                action->msg_str = NULL;
        }
        g_clear_pointer (&action->response, g_bytes_unref);
        action->params = NULL;
        g_clear_pointer (&action->doc, xmlFreeDoc);
}

static void
action_dispose (GUPnPServiceProxyAction *action)
{
        gupnp_service_proxy_action_reset (action);
        g_hash_table_destroy (action->arg_map);
        g_ptr_array_unref (action->args);

        g_free (action->name);
}

/**
 * gupnp_service_proxy_action_unref:
 * @action: an action
 *
 * Decreases reference count of `action`. If reference count drops to 0,
 * the action and its contents will be freed.
 *
 * Since: 1.2.0
 */
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
write_in_parameter (ActionArgument *arg,
                    GString    *msg_str)
{
        /* Write parameter pair */
        xml_util_start_element (msg_str, arg->name);
        gvalue_util_value_append_to_xml_string (&arg->value, msg_str);
        xml_util_end_element (msg_str, arg->name);
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

/**
 * gupnp_service_proxy_action_new:
 * @action: The name of a remote action to call
 * @...: tuples of in parameter name, in parameter type, and in parameter
 * value, terminated with %NULL
 *
 * Prepares action @action with parameters @Varargs to be sent off to
 * a remote service later with gupnp_service_proxy_call_action() or
 * gupnp_service_proxy_call_action_async().
 *
 * After the action call has finished, the results of the call may be
 * retrived from the #GUPnPServiceProxyAction by using
 * gupnp_service_proxy_action_get_result(),
 * gupnp_service_proxy_action_get_result_list() or
 * gupnp_service_proxy_action_get_result_hash()
 *
 * ```c
 * GUPnPServiceProxyAction *action =
 *         gupnp_service_proxy_action_new ("GetVolume",
 *                                         // Parameters
 *                                         "InstanceID", G_TYPE_INT, 0,
 *                                         "Channel", G_TYPE_STRING, "Master",
 *                                         NULL);
 *
 * GError *error = NULL;
 * gupnp_service_proxy_call_action (proxy, action, NULL, &error);
 * if (error != NULL) {
 *         g_warning ("Failed to call GetVolume: %s", error->message);
 *         g_clear_error (&error);
 *
 *         return;
 * }
 *
 * guint16 volume = 0;
 * if (!gupnp_service_proxy_action_get_result (action,
 *                                             &error,
 *                                             "CurrentVolume", G_TYPE_UINT, &volume,
 *                                             NULL)) {
 *         g_message ("Current Volume: %u", volume);
 * }
 *
 * gupnp_service_proxy_action_unref (action);
 * ```
 *
 * Returns: A newly created #GUPnPServiceProxyAction
 */
GUPnPServiceProxyAction *
gupnp_service_proxy_action_new (const char *action,
                                ...)
{
        GUPnPServiceProxyAction *result = NULL;
        va_list var_args;

        g_return_val_if_fail (action != NULL, NULL);

        va_start (var_args, action);
        const char *arg_name = va_arg (var_args, const char *);

        result = gupnp_service_proxy_action_new_plain (action);

        while (arg_name != NULL) {
                GValue value = G_VALUE_INIT;

                GType type = va_arg (var_args, GType);
                char *error = NULL;

                G_VALUE_COLLECT_INIT (&value, type, var_args, 0, &error);
                if (error == NULL) {
                        gupnp_service_proxy_action_add_argument (result,
                                                                 arg_name,
                                                                 &value);
                        g_value_unset (&value);
                } else {
                        g_warning (
                                "Failed to collect value of type %s for %s: %s",
                                g_type_name (type),
                                arg_name,
                                error);
                        g_free (error);
                }
                arg_name = va_arg (var_args, const char *);
        }
        va_end (var_args);

        return result;
}

/**
 * gupnp_service_proxy_action_new_from_list:
 * @action: An action
 * @in_names: (element-type utf8) (transfer none): #GList of 'in' parameter
 * names (as strings)
 * @in_values: (element-type GValue) (transfer none): #GList of values (as
 * #GValue) that line up with @in_names
 *
 * Prepares action @action with parameters @in_names and @in_values to be
 * sent off to a remote service later with gupnp_service_proxy_call_action() or
 * gupnp_service_proxy_call_action_async(). This is mainly useful for language
 * bindings.
 *
 * After the action call has finished, the results of the call may be
 * retrived from the #GUPnPServiceProxyAction by using
 * gupnp_service_proxy_action_get_result(),
 * gupnp_service_proxy_action_get_result_list() or
 * gupnp_service_proxy_action_get_result_hash()
 * ```c
 * GList *in_args = NULL;
 * in_args = g_list_append (in_args, "InstanceID");
 * in_args = g_list_append (in_args, "Unit");
 * in_args = g_list_append (in_args, "Target");
 *
 * GValue instance = G_VALUE_INIT;
 * g_value_set_int (&instance, 0);
 * GValue unit = G_VALUE_INIT;
 * g_value_set_static_string (&unit, "ABS_TIME");
 * GValue target = G_VALUE_INIT;
 * g_value_set_static_string (&target, "00:00:00.000");
 *
 * GList *in_values = NULL;
 * in_values = g_list_append (in_values, &instance);
 * in_values = g_list_append (in_values, &unit);
 * in_values = g_list_append (in_values, &target);
 *
 * GUPnPServiceProxyAction *action =
 *         gunp_service_proxy_action_new_from_list ("Seek", in_args, in_values);
 *
 * GError *error = NULL;
 * gupnp_service_proxy_call_action_async (proxy, action, NULL, on_action_finished, NULL);
 * gupnp_service_proxy_action_unref (action);
 * ```
 *
 * Returns: A newly created #GUPnPServiceProxyAction
 */
GUPnPServiceProxyAction *
gupnp_service_proxy_action_new_from_list (const char *action_name,
                                          GList      *in_names,
                                          GList      *in_values)
{
        GUPnPServiceProxyAction *action;
        GList *names = NULL;
        GList *values = NULL;
        guint position;

        action = gupnp_service_proxy_action_new_plain (action_name);

        /* Arguments */
        for (names = in_names, values = in_values, position = 0;
             names && values;
             names = names->next, values = values->next, position++) {
                GValue *val = values->data;
                gupnp_service_proxy_action_add_argument (
                        action,
                        (const char *) names->data,
                        val);
        }

        return action;
}

/**
 * gupnp_service_proxy_action_add_argument:
 * @action: The action to append to
 * @name: The name of the argument
 * @value: The value of the argument
 *
 * Append @name to the list of arguments used by @action
 *
 * Returns: (transfer none): @action for convenience.
 * Since: 1.6.6
 */
GUPnPServiceProxyAction *
gupnp_service_proxy_action_add_argument (GUPnPServiceProxyAction *action,
                                         const char *name,
                                         const GValue *value)
{
        g_return_val_if_fail (g_hash_table_lookup_extended (action->arg_map,
                                                            name,
                                                            NULL,
                                                            NULL) == FALSE,
                              NULL);

        ActionArgument *arg = g_new0 (ActionArgument, 1);
        arg->name = g_strdup (name);
        g_value_init (&arg->value, G_VALUE_TYPE (value));
        g_value_copy (value, &arg->value);
        g_hash_table_insert (action->arg_map,
                             arg->name,
                             GUINT_TO_POINTER (action->args->len));
        g_ptr_array_add (action->args, arg);

        return action;
}

void
gupnp_service_proxy_action_serialize (GUPnPServiceProxyAction *action,
                                      const char *service_type)
{
        if (action->msg_str != NULL) {
                g_string_free (action->msg_str, TRUE);
        }
        action->msg_str = xml_util_new_string ();

        g_string_append (action->msg_str,
                         "<?xml version=\"1.0\"?>"
                         "<s:Envelope xmlns:s="
                         "\"http://schemas.xmlsoap.org/soap/envelope/\" "
                         "s:encodingStyle="
                         "\"http://schemas.xmlsoap.org/soap/encoding/\">"
                         "<s:Body>");
        action->header_pos = action->msg_str->len;

        g_ptr_array_foreach (action->args,
                             (GFunc) write_in_parameter,
                             action->msg_str);

        write_footer (action);

        g_string_insert (action->msg_str, action->header_pos, "<u:");
        action->header_pos += strlen ("<u:");
        g_string_insert (action->msg_str, action->header_pos, action->name);
        action->header_pos += strlen (action->name);
        g_string_insert (action->msg_str, action->header_pos, " xmlns:u=\"");
        action->header_pos += strlen (" xmlns:u=\"");
        g_string_insert (action->msg_str, action->header_pos, service_type);
        action->header_pos += strlen (service_type);
        g_string_insert (action->msg_str, action->header_pos, "\">");
}

/* Checks an action response for errors and returns the parsed
 * xmlDoc object. */
void
gupnp_service_proxy_action_check_response (GUPnPServiceProxyAction *action)
{
        xmlDoc *response;
        int code;

        if (action->doc != NULL) {
                return;
        }

        if (action->error != NULL) {
                return;
        }

        if (action->msg == NULL || action->response == NULL) {
                g_set_error_literal (&action->error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                     "No message, the action was not sent?");

                return;
        }

        SoupStatus status = soup_message_get_status (action->msg);

        if (status != SOUP_STATUS_OK &&
            status != SOUP_STATUS_INTERNAL_SERVER_ERROR) {
                _gupnp_error_set_server_error (&action->error, action->msg);
                return;
        }

        /* Parse response */
        gconstpointer data;
        gsize length;
        data = g_bytes_get_data (action->response, &length);
        response = xmlReadMemory (data,
                                  length,
                                  NULL,
                                  NULL,
                                  XML_PARSE_NONET | XML_PARSE_RECOVER);

        g_clear_pointer (&action->response, g_bytes_unref);

        if (!response) {
                if (status == SOUP_STATUS_OK) {
                        g_set_error_literal (
                                &action->error,
                                GUPNP_SERVER_ERROR,
                                GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                "Could not parse SOAP response");
                } else {
                        g_set_error_literal (
                                &action->error,
                                GUPNP_SERVER_ERROR,
                                GUPNP_SERVER_ERROR_INTERNAL_SERVER_ERROR,
                                soup_message_get_reason_phrase (action->msg));
                }

                return;
        }

        xmlNodePtr params = NULL;
        /* Get parameter list */
        params = xml_util_get_element ((xmlNode *) response, "Envelope", NULL);
        if (params != NULL)
                params = xml_util_real_node (params->children);

        if (params != NULL) {
                if (strcmp ((const char *) params->name, "Header") == 0)
                        params = xml_util_real_node (params->next);

                if (params != NULL)
                        if (strcmp ((const char *) params->name, "Body") != 0)
                                params = NULL;
        }

        if (params != NULL)
                params = xml_util_real_node (params->children);

        if (params == NULL) {
                g_set_error (&action->error,
                             GUPNP_SERVER_ERROR,
                             GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                             "Invalid Envelope");

                xmlFreeDoc (response);

                return;
        }

        /* Check whether we have a Fault */
        if (status == SOUP_STATUS_INTERNAL_SERVER_ERROR) {
                xmlNode *param;
                char *desc;

                param = xml_util_get_element (params,
                                              "detail",
                                              "UPnPError",
                                              NULL);

                if (!param) {
                        g_set_error (&action->error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                     "Invalid Fault");

                        xmlFreeDoc (response);

                        return;
                }

                /* Code */
                code = xml_util_get_child_element_content_int (param,
                                                               "errorCode");
                if (code == -1) {
                        g_set_error (&action->error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                     "Invalid Fault");

                        xmlFreeDoc (response);

                        return;
                }

                /* Description */
                desc = xml_util_get_child_element_content_glib (
                        param,
                        "errorDescription");
                if (desc == NULL)
                        desc = g_strdup (
                                soup_message_get_reason_phrase (action->msg));

                g_set_error_literal (&action->error,
                                     GUPNP_CONTROL_ERROR,
                                     code,
                                     desc);

                g_free (desc);

                xmlFreeDoc (response);

                return;
        }

        action->params = params;
        action->doc = response;
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
 * @error:(inout)(optional)(nullable): The location where to store any error, or %NULL
 *
 * A variant of gupnp_service_proxy_action_get_result() that takes lists of
 * out-parameter names, types and place-holders for values.
 *
 * The returned list in @out_values must be freed using `g_list_free` and each element
 * in it using `g_value_unset` and `g_free`.
 * ```c
 * void on_action_finished(GObject *object, GAsyncResult *res, gpointer user_data)
 * {
 *     GUPnPServiceProxyAction *action;
 *     GError *error;
 *
 *     action = gupnp_service_proxy_call_action_finish (GUPNP_SERVICE_PROXY (object),
 *                                                      res,
 *                                                      &error);
 *
 *     if (error != NULL) {
 *              g_print ("Call failed: %s", error->message);
 *              g_clear_error (&error);
 *              return;
 *     }
 *
 *     GList *out_args = NULL;
 *     out_args = g_list_append (out_args, "PlayMode");
 *     out_args = g_list_append (out_args, "RecQualityMode");
 *     GList *out_types = NULL;
 *     out_types = g_list_append (out_types, GSIZE_TO_POINTER (G_TYPE_STRING));
 *     out_types = g_list_append (out_types, GSIZE_TO_POINTER (G_TYPE_STRING));
 *     GList *out_values = NULL;
 *
 *     if (!gupnp_service_proxy_action_get_result_list (action,
 *                                                      out_args,
 *                                                      out_types,
 *                                                      &out_values,
 *                                                      &error)) {
 *              g_print ("Getting results failed: %s", error->message);
 *              g_clear_error (&error);
 *              return;
 *     }
 *
 *     GList *iter = out_values;
 *     while (iter != NULL) {
 *         GValue *value = iter->data;
 *         g_print ("Result: %s\n", g_value_get_string (value));
 *         g_value_unset (value);
 *         g_free (value);
 *         iter = g_list_remove_link (iter, iter);
 *     }
 *     g_list_free (out_values);
 * }
 *```
 *
 * Return value : %TRUE on success.
 *
 * Since: 1.2.0
 *
 **/
gboolean
gupnp_service_proxy_action_get_result_list (GUPnPServiceProxyAction *action,
                                            GList                   *out_names,
                                            GList                   *out_types,
                                            GList                  **out_values,
                                            GError                 **error)
{
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
        gupnp_service_proxy_action_check_response (action);
        if (action->error != NULL) {
                g_propagate_error (error, g_error_copy (action->error));

                return FALSE;
        }

        /* Read arguments */
        types = out_types;
        for (names = out_names; names; names=names->next) {
                GValue *val;

                val = g_new0 (GValue, 1);
                g_value_init (val, (GType) types->data);

                read_out_parameter (names->data, val, action->params);

                out_values_list = g_list_append (out_values_list, val);

                types = types->next;
        }

        *out_values = out_values_list;

        return TRUE;

}

/**
 * gupnp_service_proxy_action_get_result_hash:
 * @action: A #GUPnPServiceProxyAction handle
 * @out_hash: (element-type utf8 GValue) (inout) (transfer none): A #GHashTable of
 * out parameter name and initialised #GValue pairs
 * @error:(inout)(optional)(nullable): The location where to store any error, or %NULL
 *
 * See gupnp_service_proxy_action_get_result(); this version takes a #GHashTable for
 * runtime generated parameter lists.
 *
 * The @out_hash needs to be pre-initialized with key value pairs denoting the argument
 * to retrieve and an empty #GValue initialized to hold the wanted type with g_value_init().
 *
 *```c
 * void on_action_finished(GObject *object, GAsyncResult *res, gpointer user_data)
 * {
 *     GUPnPServiceProxyAction *action;
 *     GError *error;
 *
 *     action = gupnp_service_proxy_call_action_finish (GUPNP_SERVICE_PROXY (object),
 *                                                      res,
 *                                                      &error);
 *
 *     if (error != NULL) {
 *              g_print ("Call failed: %s", error->message);
 *              g_clear_error (&error);
 *              return;
 *     }
 *
 *     GValue play_mode = G_VALUE_INIT;
 *     g_value_init(&play_mode, G_TYPE_STRING);
 *     GValue rec_quality_mode = G_VALUE_INIT;
 *     g_value_init(&rec_quality_mode, G_TYPE_STRING);
 *
 *     GHashTable *out_args = g_hash_table_new (g_str_hash, g_str_equal);
 *     g_hash_table_insert(out_args, "PlayMode", &play_mode);
 *     g_hash_table_insert(out_args, "RecQualityMode", &rec_quality_mode);
 *
 *     if (!gupnp_service_proxy_action_get_result_hash (action,
 *                                                      out_args,
 *                                                      &error)) {
 *              g_print ("Getting results failed: %s", error->message);
 *              g_clear_error (&error);
 *              return;
 *     }
 *
 *     g_value_unset (&play_mode);
 *     g_value_unset (&rec_quality_mode);
 *
 *     g_hash_table_unref (out_args);
 * }
 * ```
 *
 * Return value: %TRUE on success.
 *
 * Since: 1.2.0
 **/
gboolean
gupnp_service_proxy_action_get_result_hash (GUPnPServiceProxyAction *action,
                                            GHashTable              *hash,
                                            GError                 **error)
{
        g_return_val_if_fail (action, FALSE);

        /* Check for saved error from begin_action() */
        if (action->error) {
                g_propagate_error (error, g_error_copy (action->error));

                return FALSE;
        }

        /* Check response for errors and do initial parsing */
        gupnp_service_proxy_action_check_response (action);
        if (action->error != NULL) {
                g_propagate_error (error, g_error_copy (action->error));

                return FALSE;
        }

        /* Read arguments */
        g_hash_table_foreach (hash,
                              (GHFunc) read_out_parameter,
                              action->params);

        return TRUE;

}

/**
 * gupnp_service_proxy_action_get_result:
 * @action: A #GUPnPServiceProxyAction handle
 * @error: (inout)(optional)(nullable): The location where to store any error, or %NULL
 * @...: tuples of out parameter name, out parameter type, and out parameter
 * value location, terminated with %NULL. The out parameter values should be
 * freed after use
 *
 * Retrieves the result of @action. The out parameters in @Varargs will be
 * filled in, and if an error occurred, @error will be set. In case of
 * an UPnP error the error code will be the same in @error.
 *
 * Return value: %TRUE on success.
 **/
gboolean
gupnp_service_proxy_action_get_result (GUPnPServiceProxyAction *action,
                                       GError                 **error,
                                       ...)
{
        g_return_val_if_fail (action, FALSE);

        va_list var_args;

        va_start (var_args, error);
        GHashTable *out_hash;
        va_list var_args_copy;
        gboolean result;
        GError *local_error;


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
        va_end (var_args);

        return result;
}

/**
 * gupnp_service_proxy_action_set:
 * @action: the action to modify
 * @key: the name of the value to modify
 * @value: the new value of @key
 * @error: (nullable): a return location for an #GError
 *
 * Update the value of @key to @value.
 *
 * @key needs to already exist in @action.
 *
 * Returns: true if successfully modified, false otherwise
 * Since: 1.4.0
 */
gboolean
gupnp_service_proxy_action_set (GUPnPServiceProxyAction *action,
                                const char *key,
                                const GValue *value,
                                GError **error)
{
        g_return_val_if_fail (key != NULL, FALSE);
        g_return_val_if_fail (value != NULL, FALSE);
        g_return_val_if_fail (error != NULL && *error == NULL, FALSE);
        gpointer position;

        if (!g_hash_table_lookup_extended (action->arg_map,
                                           key,
                                           NULL,
                                           &position)) {
                g_propagate_error (error,
                                   g_error_new (GUPNP_SERVER_ERROR,
                                                GUPNP_SERVER_ERROR_OTHER,
                                                "Unknown argument: %s",
                                                key));

                return FALSE;
        }

        ActionArgument *arg =
                g_ptr_array_index (action->args, GPOINTER_TO_UINT (position));

        if (G_VALUE_TYPE (value) != G_VALUE_TYPE (&arg->value)) {
                g_propagate_error (
                        error,
                        g_error_new (
                                GUPNP_SERVER_ERROR,
                                GUPNP_SERVER_ERROR_OTHER,
                                "Type mismatch for %s. Expected %s, got %s",
                                key,
                                G_VALUE_TYPE_NAME (&arg->value),
                                G_VALUE_TYPE_NAME (value)));

                return FALSE;
        }

        g_value_copy (value, &arg->value);

        return TRUE;
}
