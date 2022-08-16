// SPDX-License-Identifier: LGPL-2.1-or-later

#define G_LOG_DOMAIN "gupnp-service-proxy"

#include <config.h>

#include "gupnp-error.h"
#include "gupnp-service-private.h"
#include "gupnp-service.h"
#include "gupnp-xml-doc.h"
#include "gvalue-util.h"
#include "http-headers.h"
#include "xml-util.h"

#include <gobject/gvaluecollector.h>

GUPnPServiceAction *
gupnp_service_action_new ()
{
        return g_atomic_rc_box_new0 (GUPnPServiceAction);
}

GUPnPServiceAction *
gupnp_service_action_ref (GUPnPServiceAction *action)
{
        g_return_val_if_fail (action, NULL);

        return g_atomic_rc_box_acquire (action);
}

static void
action_dispose (GUPnPServiceAction *action)
{
        g_free (action->name);
        g_object_unref (action->msg);
        g_object_unref (action->context);
        g_object_unref (action->doc);
        if (action->response_str)
                g_string_free (action->response_str, TRUE);
}

void
gupnp_service_action_unref (struct _GUPnPServiceAction *action)
{
        g_return_if_fail (action);

        g_atomic_rc_box_release_full (action, (GDestroyNotify) action_dispose);
}

/**
 * gupnp_service_action_get_type:
 *
 * Get the gtype for GUPnPServiceActon
 *
 * Return value: The gtype of GUPnPServiceAction
 **/
GType
gupnp_service_action_get_type (void)
{
        static GType our_type = 0;

        if (our_type == 0)
                our_type = g_boxed_type_register_static (
                        "GUPnPServiceAction",
                        (GBoxedCopyFunc) gupnp_service_action_ref,
                        (GBoxedFreeFunc) gupnp_service_action_unref);

        return our_type;
}

static void
finalize_action (GUPnPServiceAction *action)
{
        /* Embed action->response_str in a SOAP document */
        g_string_prepend (action->response_str,
                          "<?xml version=\"1.0\"?>"
                          "<s:Envelope xmlns:s="
                          "\"http://schemas.xmlsoap.org/soap/envelope/\" "
                          "s:encodingStyle="
                          "\"http://schemas.xmlsoap.org/soap/encoding/\">"
                          "<s:Body>");

        if (soup_server_message_get_status (action->msg) !=
            SOUP_STATUS_INTERNAL_SERVER_ERROR) {
                g_string_append (action->response_str, "</u:");
                g_string_append (action->response_str, action->name);
                g_string_append (action->response_str, "Response>");
        }

        g_string_append (action->response_str,
                         "</s:Body>"
                         "</s:Envelope>");

        SoupMessageHeaders *headers =
                soup_server_message_get_response_headers (action->msg);

        soup_message_headers_replace (headers,
                                      "Content-Type",
                                      "text/xml; charset=\"utf-8\"");

        if (action->accept_gzip && action->response_str->len > 1024) {
                // Fixme: Probably easier to use an output stream converter
                // instead
                http_response_set_body_gzip (action->msg,
                                             action->response_str->str,
                                             action->response_str->len);
                g_string_free (action->response_str, TRUE);
        } else {
                SoupMessageBody *msg_body =
                        soup_server_message_get_response_body (action->msg);
                GBytes *bytes = g_string_free_to_bytes (action->response_str);
                soup_message_body_append_bytes (msg_body, bytes);
                g_bytes_unref (bytes);
        }
        action->response_str = NULL;

        soup_message_headers_append (headers, "Ext", "");

        /* Server header on response */
        soup_message_headers_append (
                headers,
                "Server",
                gssdp_client_get_server_id (GSSDP_CLIENT (action->context)));

        /* Tell soup server that response is now ready */
#if SOUP_CHECK_VERSION(3,1,2)
        soup_server_message_unpause (action->msg);
#else
        SoupServer *server = gupnp_context_get_server (action->context);
        soup_server_unpause_message (server, action->msg);
#endif

        /* Cleanup */
        gupnp_service_action_unref (action);
}

/**
 * gupnp_service_action_get_name:
 * @action: A #GUPnPServiceAction
 *
 * Get the name of @action.
 *
 * Return value: The name of @action
 **/
const char *
gupnp_service_action_get_name (GUPnPServiceAction *action)
{
        g_return_val_if_fail (action != NULL, NULL);

        return action->name;
}

/**
 * gupnp_service_action_get_locales:
 * @action: A #GUPnPServiceAction
 *
 * Get an ordered (preferred first) #GList of locales preferred by
 * the client. Free list and elements after use.
 *
 * Return value: (element-type utf8) (transfer full): A #GList of #char*
 * locale names.
 **/
GList *
gupnp_service_action_get_locales (GUPnPServiceAction *action)
{
        g_return_val_if_fail (action != NULL, NULL);

        return http_request_get_accept_locales (
                soup_server_message_get_request_headers (action->msg));
}

/**
 * gupnp_service_action_get:
 * @action: A #GUPnPServiceAction
 * @...: tuples of argument name, argument type, and argument value
 * location, terminated with %NULL.
 *
 * Retrieves the specified action arguments.
 **/
void
gupnp_service_action_get (GUPnPServiceAction *action, ...)
{
        va_list var_args;

        g_return_if_fail (action != NULL);

        va_start (var_args, action);
        const char *arg_name;
        GType arg_type;
        GValue value = G_VALUE_INIT;
        char *copy_error;

        g_return_if_fail (action != NULL);

        copy_error = NULL;

        arg_name = va_arg (var_args, const char *);
        while (arg_name) {
                arg_type = va_arg (var_args, GType);
                g_value_init (&value, arg_type);

                gupnp_service_action_get_value (action, arg_name, &value);

                G_VALUE_LCOPY (&value, var_args, 0, &copy_error);

                g_value_unset (&value);

                if (copy_error) {
                        g_warning ("Error lcopying value: %s\n", copy_error);

                        g_free (copy_error);
                }

                arg_name = va_arg (var_args, const char *);
        }
        va_end (var_args);
}


/**
 * gupnp_service_action_get_values:
 * @action: A #GUPnPServiceAction
 * @arg_names: (element-type utf8) : A #GList of argument names as string
 * @arg_types: (element-type GType): A #GList of argument types as #GType
 *
 * A variant of #gupnp_service_action_get that uses #GList instead of varargs.
 *
 * Return value: (element-type GValue) (transfer full): The values as #GList of
 * #GValue. g_list_free() the returned list and g_value_unset() and g_slice_free()
 * each element.
 *
 * Since: 0.14.0
 **/
GList *
gupnp_service_action_get_values (GUPnPServiceAction *action,
                                 GList *arg_names,
                                 GList *arg_types)
{
        GList *arg_values;
        guint i;

        g_return_val_if_fail (action != NULL, NULL);

        arg_values = NULL;

        for (i = 0; i < g_list_length (arg_names); i++) {
                const char *arg_name;
                GType arg_type;
                GValue *arg_value;

                arg_name = (const char *) g_list_nth_data (arg_names, i);
                arg_type = (GType) g_list_nth_data (arg_types, i);

                arg_value = g_slice_new0 (GValue);
                g_value_init (arg_value, arg_type);

                gupnp_service_action_get_value (action, arg_name, arg_value);

                arg_values = g_list_append (arg_values, arg_value);
        }

        return arg_values;
}

/**
 * gupnp_service_action_get_value: (skip)
 * @action: A #GUPnPServiceAction
 * @argument: The name of the argument to retrieve
 * @value: (inout):The #GValue to store the value of the argument, initialized
 * to the correct type.
 *
 * Retrieves the value of @argument into @value.
 **/
void
gupnp_service_action_get_value (GUPnPServiceAction *action,
                                const char *argument,
                                GValue *value)
{
        xmlNode *node;
        gboolean found;

        g_return_if_fail (action != NULL);
        g_return_if_fail (argument != NULL);
        g_return_if_fail (value != NULL);

        found = FALSE;
        for (node = action->node->children; node; node = node->next) {
                if (strcmp ((char *) node->name, argument) != 0)
                        continue;

                found = gvalue_util_set_value_from_xml_node (value, node);

                break;
        }

        if (!found)
                g_warning ("Failed to retrieve '%s' argument of '%s' action",
                           argument,
                           action->name);
}

/**
 * gupnp_service_action_get_gvalue: (rename-to gupnp_service_action_get_value)
 * @action: A #GUPnPServiceAction
 * @argument: The name of the argument to retrieve
 * @type: The type of argument to retrieve
 *
 * Retrieves the value of @argument into a GValue of type @type and returns it.
 * The method exists only and only to satify PyGI, please use
 * gupnp_service_action_get_value() and ignore this if possible.
 *
 * Return value: (transfer full): Value as #GValue associated with @action.
 * g_value_unset() and g_slice_free() it after usage.
 *
 * Since: 0.14.0
 **/
GValue *
gupnp_service_action_get_gvalue (GUPnPServiceAction *action,
                                 const char *argument,
                                 GType type)
{
        GValue *val;

        val = g_slice_new0 (GValue);
        g_value_init (val, type);

        gupnp_service_action_get_value (action, argument, val);

        return val;
}

/**
 * gupnp_service_action_get_argument_count:
 * @action: A #GUPnPServiceAction
 *
 * Get the number of IN arguments from the @action and return it.
 *
 * Return value: The number of IN arguments from the @action.
 *
 * Since: 0.18.0
 */
guint
gupnp_service_action_get_argument_count (GUPnPServiceAction *action)
{
        return action->argument_count;
}

/**
 * gupnp_service_action_set:
 * @action: A #GUPnPServiceAction
 * @...: tuples of return value name, return value type, and
 * actual return value, terminated with %NULL.
 *
 * Sets the specified action return values.
 **/
void
gupnp_service_action_set (GUPnPServiceAction *action, ...)
{
        va_list var_args;

        g_return_if_fail (action != NULL);

        va_start (var_args, action);
        const char *arg_name;
        GType arg_type;
        GValue value = G_VALUE_INIT;
        char *collect_error;

        g_return_if_fail (action != NULL);

        collect_error = NULL;

        arg_name = va_arg (var_args, const char *);
        while (arg_name) {
                arg_type = va_arg (var_args, GType);
                g_value_init (&value, arg_type);

                G_VALUE_COLLECT (&value,
                                 var_args,
                                 G_VALUE_NOCOPY_CONTENTS,
                                 &collect_error);
                if (!collect_error) {
                        gupnp_service_action_set_value (action,
                                                        arg_name,
                                                        &value);

                        g_value_unset (&value);

                } else {
                        g_warning ("Error collecting value: %s\n",
                                   collect_error);

                        g_free (collect_error);
                }

                arg_name = va_arg (var_args, const char *);
        }
        va_end (var_args);
}

/**
 * gupnp_service_action_set_values:
 * @action: A #GUPnPServiceAction
 * @arg_names: (element-type utf8) (transfer none): A #GList of argument names
 * @arg_values: (element-type GValue) (transfer none): The #GList of values (as
 * #GValue<!-- -->s) that line up with @arg_names.
 *
 * Sets the specified action return values.
 *
 * Since: 0.14.0
 **/
void
gupnp_service_action_set_values (GUPnPServiceAction *action,
                                 GList *arg_names,
                                 GList *arg_values)
{
        g_return_if_fail (action != NULL);
        g_return_if_fail (arg_names != NULL);
        g_return_if_fail (arg_values != NULL);
        g_return_if_fail (g_list_length (arg_names) ==
                          g_list_length (arg_values));

        if (soup_server_message_get_status (action->msg) ==
            SOUP_STATUS_INTERNAL_SERVER_ERROR) {
                g_warning ("Calling gupnp_service_action_set_value() after "
                           "having called gupnp_service_action_return_error() "
                           "is not allowed.");

                return;
        }

        /* Append to response */
        for (; arg_names; arg_names = arg_names->next) {
                const char *arg_name;
                GValue *value;

                arg_name = arg_names->data;
                value = arg_values->data;

                xml_util_start_element (action->response_str, arg_name);
                gvalue_util_value_append_to_xml_string (value,
                                                        action->response_str);
                xml_util_end_element (action->response_str, arg_name);

                arg_values = arg_values->next;
        }
}

/**
 * gupnp_service_action_set_value:
 * @action: A #GUPnPServiceAction
 * @argument: The name of the return value to retrieve
 * @value: The #GValue to store the return value
 *
 * Sets the value of @argument to @value.
 **/
void
gupnp_service_action_set_value (GUPnPServiceAction *action,
                                const char *argument,
                                const GValue *value)
{
        g_return_if_fail (action != NULL);
        g_return_if_fail (argument != NULL);
        g_return_if_fail (value != NULL);

        if (soup_server_message_get_status (action->msg) ==
            SOUP_STATUS_INTERNAL_SERVER_ERROR) {
                g_warning ("Calling gupnp_service_action_set_value() after "
                           "having called gupnp_service_action_return_error() "
                           "is not allowed.");

                return;
        }

        /* Append to response */
        xml_util_start_element (action->response_str, argument);
        gvalue_util_value_append_to_xml_string (value, action->response_str);
        xml_util_end_element (action->response_str, argument);
}

/**
 * gupnp_service_action_return_success:
 * @action: A #GUPnPServiceAction
 *
 * Return successfully.
 *
 * Since: 1.4.2
 **/
void
gupnp_service_action_return_success (GUPnPServiceAction *action)
{
        g_return_if_fail (action != NULL);

        soup_server_message_set_status (action->msg, SOUP_STATUS_OK, NULL);

        finalize_action (action);
}

/**
 * gupnp_service_action_return_error:
 * @action: A #GUPnPServiceAction
 * @error_code: The error code
 * @error_description: The error description, or %NULL if @error_code is
 * one of #GUPNP_CONTROL_ERROR_INVALID_ACTION,
 * #GUPNP_CONTROL_ERROR_INVALID_ARGS, #GUPNP_CONTROL_ERROR_OUT_OF_SYNC or
 * #GUPNP_CONTROL_ERROR_ACTION_FAILED, in which case a description is
 * provided automatically.
 *
 * Return @error_code.
 **/
void
gupnp_service_action_return_error (GUPnPServiceAction *action,
                                   guint error_code,
                                   const char *error_description)
{
        g_return_if_fail (action != NULL);

        switch (error_code) {
        case GUPNP_CONTROL_ERROR_INVALID_ACTION:
                if (error_description == NULL)
                        error_description = "Invalid Action";

                break;
        case GUPNP_CONTROL_ERROR_INVALID_ARGS:
                if (error_description == NULL)
                        error_description = "Invalid Args";

                break;
        case GUPNP_CONTROL_ERROR_OUT_OF_SYNC:
                if (error_description == NULL)
                        error_description = "Out of Sync";

                break;
        case GUPNP_CONTROL_ERROR_ACTION_FAILED:
                if (error_description == NULL)
                        error_description = "Action Failed";

                break;
        default:
                g_return_if_fail (error_description != NULL);
                break;
        }

        /* Replace response_str with a SOAP Fault */
        g_string_erase (action->response_str, 0, -1);

        xml_util_start_element (action->response_str, "s:Fault");

        xml_util_start_element (action->response_str, "faultcode");
        g_string_append (action->response_str, "s:Client");
        xml_util_end_element (action->response_str, "faultcode");

        xml_util_start_element (action->response_str, "faultstring");
        g_string_append (action->response_str, "UPnPError");
        xml_util_end_element (action->response_str, "faultstring");

        xml_util_start_element (action->response_str, "detail");

        xml_util_start_element (action->response_str,
                                "UPnPError "
                                "xmlns=\"urn:schemas-upnp-org:control-1-0\"");

        xml_util_start_element (action->response_str, "errorCode");
        g_string_append_printf (action->response_str, "%u", error_code);
        xml_util_end_element (action->response_str, "errorCode");

        xml_util_start_element (action->response_str, "errorDescription");
        xml_util_add_content (action->response_str, error_description);
        xml_util_end_element (action->response_str, "errorDescription");

        xml_util_end_element (action->response_str, "UPnPError");
        xml_util_end_element (action->response_str, "detail");

        xml_util_end_element (action->response_str, "s:Fault");

        soup_server_message_set_status (action->msg,
                                        SOUP_STATUS_INTERNAL_SERVER_ERROR,
                                        "Internal server error");

        finalize_action (action);
}

/**
 * gupnp_service_action_get_message:
 * @action: A #GUPnPServiceAction
 *
 * Get the #SoupMessage associated with @action. Mainly intended for
 * applications to be able to read HTTP headers received from clients.
 *
 * Return value: (transfer full): #SoupServerMessage associated with @action.
 *Unref after using it.
 *
 * Since: 0.14.0
 **/
SoupServerMessage *
gupnp_service_action_get_message (GUPnPServiceAction *action)
{
        return g_object_ref (action->msg);
}
