/*
 * Copyright (C) 2007, 2008 OpenedHand Ltd.
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

/**
 * SECTION:gupnp-service
 * @short_description: Class for service implementations.
 *
 * #GUPnPService allows for handling incoming actions and state variable
 * notification. #GUPnPService implements the #GUPnPServiceInfo interface.
 */

#include <gobject/gvaluecollector.h>
#include <gmodule.h>
#include <libsoup/soup-date.h>
#include <string.h>
#include "gupnp-service.h"
#include "gupnp-root-device.h"
#include "gupnp-context-private.h"
#include "gupnp-marshal.h"
#include "gupnp-error.h"
#include "http-headers.h"
#include "gena-protocol.h"
#include "xml-util.h"
#include "gvalue-util.h"

#ifdef G_OS_WIN32
#include <rpc.h>
#else
#include <uuid/uuid.h>
#endif

#define SUBSCRIPTION_TIMEOUT 300 /* DLNA (7.2.22.1) enforced */

G_DEFINE_TYPE (GUPnPService,
               gupnp_service,
               GUPNP_TYPE_SERVICE_INFO);

struct _GUPnPServicePrivate {
        GUPnPRootDevice *root_device;

        SoupSession     *session;

        guint            notify_available_id;

        GHashTable      *subscriptions;

        GList           *state_variables;

        GQueue          *notify_queue;

        gboolean         notify_frozen;
};

enum {
        PROP_0,
        PROP_ROOT_DEVICE
};

enum {
        ACTION_INVOKED,
        QUERY_VARIABLE,
        NOTIFY_FAILED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static char *
create_property_set (GQueue *queue);

static void
notify_subscriber   (gpointer key,
                     gpointer value,
                     gpointer user_data);

typedef struct {
        GUPnPService *service;

        GList        *callbacks;
        char         *sid;

        int           seq;

        GSource      *timeout_src;

        GList        *pending_messages; /* Pending SoupMessages from this
                                           subscription */
        gboolean      initial_state_sent;
        gboolean      to_delete;
} SubscriptionData;

static gboolean
subscription_data_can_delete (SubscriptionData *data) {
    return data->initial_state_sent && data->to_delete;
}

static void
send_initial_state (SubscriptionData *data);

static SoupSession *
gupnp_service_get_session (GUPnPService *service)
{
        if (! service->priv->session) {
                /* Create a dedicated session for this service to
                 * ensure that notifications are sent in the proper
                 * order. The session from GUPnPContext may use
                 * multiple connections.
                 */
                service->priv->session = soup_session_async_new_with_options
                  (SOUP_SESSION_IDLE_TIMEOUT, 60,
                   SOUP_SESSION_ASYNC_CONTEXT,
                   g_main_context_get_thread_default (),
                   SOUP_SESSION_MAX_CONNS_PER_HOST, 1,
                   NULL);

                if (g_getenv ("GUPNP_DEBUG")) {
                        SoupLogger *logger;
                        logger = soup_logger_new (SOUP_LOGGER_LOG_BODY, -1);
                        soup_session_add_feature (
                                        service->priv->session,
                                        SOUP_SESSION_FEATURE (logger));
                }
        }

        return service->priv->session;
}

static void
subscription_data_free (SubscriptionData *data)
{
        SoupSession *session;

        session = gupnp_service_get_session (data->service);

        /* Cancel pending messages */
        while (data->pending_messages) {
                SoupMessage *msg;

                msg = data->pending_messages->data;

                soup_session_cancel_message (session,
                                             msg,
                                             SOUP_STATUS_CANCELLED);

                data->pending_messages =
                        g_list_delete_link (data->pending_messages,
                                            data->pending_messages);
        }
       
        /* Further cleanup */
        while (data->callbacks) {
                g_free (data->callbacks->data);
                data->callbacks = g_list_delete_link (data->callbacks,
                                                      data->callbacks);
        }

        g_free (data->sid);

        if (data->timeout_src)
                g_source_destroy (data->timeout_src);

        g_slice_free (SubscriptionData, data);
}

typedef struct {
        char  *variable;
        GValue value;
} NotifyData;

static void
notify_data_free (NotifyData *data)
{
        g_free (data->variable);
        g_value_unset (&data->value);

        g_slice_free (NotifyData, data);
}

struct _GUPnPServiceAction {
        volatile gint ref_count;

        GUPnPContext *context;

        char         *name;

        SoupMessage  *msg;
        gboolean      accept_gzip;

        GUPnPXMLDoc  *doc;
        xmlNode      *node;

        GString      *response_str;

        guint         argument_count;
};

GUPnPServiceAction *
gupnp_service_action_ref (GUPnPServiceAction *action)
{
        g_return_val_if_fail (action, NULL);
        g_return_val_if_fail (action->ref_count > 0, NULL);

        g_atomic_int_inc (&action->ref_count);

        return action;
}

void
gupnp_service_action_unref (GUPnPServiceAction *action)
{
        g_return_if_fail (action);
        g_return_if_fail (action->ref_count > 0);

        if (g_atomic_int_dec_and_test (&action->ref_count)) {
                g_free (action->name);
                g_object_unref (action->msg);
                g_object_unref (action->context);
                g_object_unref (action->doc);

                g_slice_free (GUPnPServiceAction, action);
        }
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
                our_type = g_boxed_type_register_static
                        ("GUPnPServiceAction",
                         (GBoxedCopyFunc) gupnp_service_action_ref,
                         (GBoxedFreeFunc) gupnp_service_action_unref);

        return our_type;
}

static void
finalize_action (GUPnPServiceAction *action)
{
        SoupServer *server;

        /* Embed action->response_str in a SOAP document */
        g_string_prepend (action->response_str,
                          "<?xml version=\"1.0\"?>"
                          "<s:Envelope xmlns:s="
                                "\"http://schemas.xmlsoap.org/soap/envelope/\" "
                          "s:encodingStyle="
                                "\"http://schemas.xmlsoap.org/soap/encoding/\">"
                          "<s:Body>");

        if (action->msg->status_code != SOUP_STATUS_INTERNAL_SERVER_ERROR) {
                g_string_append (action->response_str, "</u:");
                g_string_append (action->response_str, action->name);
                g_string_append (action->response_str, "Response>");
        }

        g_string_append (action->response_str,
                         "</s:Body>"
                         "</s:Envelope>");

        soup_message_headers_replace (action->msg->response_headers,
                                      "Content-Type",
                                      "text/xml; charset=\"utf-8\"");

        if (action->accept_gzip && action->response_str->len > 1024) {
                http_response_set_body_gzip (action->msg,
                                             action->response_str->str,
                                             action->response_str->len);
                g_string_free (action->response_str, TRUE);
        } else {
                soup_message_body_append (action->msg->response_body,
                                          SOUP_MEMORY_TAKE,
                                          action->response_str->str,
                                          action->response_str->len);
                g_string_free (action->response_str, FALSE);
        }

        soup_message_headers_append (action->msg->response_headers, "Ext", "");

        /* Server header on response */
        soup_message_headers_append
                        (action->msg->response_headers,
                         "Server",
                         gssdp_client_get_server_id
                                (GSSDP_CLIENT (action->context)));

        /* Tell soup server that response is now ready */
        server = gupnp_context_get_server (action->context);
        soup_server_unpause_message (server, action->msg);

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

        return http_request_get_accept_locales (action->msg);
}

/**
 * gupnp_service_action_get:
 * @action: A #GUPnPServiceAction
 * @Varargs: tuples of argument name, argument type, and argument value
 * location, terminated with %NULL.
 *
 * Retrieves the specified action arguments.
 **/
void
gupnp_service_action_get (GUPnPServiceAction *action,
                          ...)
{
        va_list var_args;

        g_return_if_fail (action != NULL);

        va_start (var_args, action);
        gupnp_service_action_get_valist (action, var_args);
        va_end (var_args);
}

/**
 * gupnp_service_action_get_valist:
 * @action: A #GUPnPServiceAction
 * @var_args: va_list of tuples of argument name, argument type, and argument
 * value location.
 *
 * See gupnp_service_action_get(); this version takes a va_list for
 * use by language bindings.
 **/
void
gupnp_service_action_get_valist (GUPnPServiceAction *action,
                                 va_list             var_args)
{
        const char *arg_name;
        GType arg_type;
        GValue value = {0, };
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
 * #GValue. #g_list_free the returned list and #g_value_unset and #g_slice_free
 * each element.
 **/
GList *
gupnp_service_action_get_values (GUPnPServiceAction *action,
                                 GList              *arg_names,
                                 GList              *arg_types)
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
                                const char         *argument,
                                GValue             *value)
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
 * gupnp_service_action_get_gvalue:
 * @action: A #GUPnPServiceAction
 * @argument: The name of the argument to retrieve
 * @type: The type of argument to retrieve
 *
 * Retrieves the value of @argument into a GValue of type @type and returns it.
 * The method exists only and only to satify PyGI, please use
 * #gupnp_service_action_get_value and ignore this if possible.
 *
 * Return value: (transfer full): Value as #GValue associated with @action.
 * #g_value_unset and #g_slice_free it after usage.
 *
 * Rename To: gupnp_service_action_get_value
 **/
GValue *
gupnp_service_action_get_gvalue (GUPnPServiceAction *action,
                                       const char         *argument,
                                       GType               type)
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
 */
guint
gupnp_service_action_get_argument_count (GUPnPServiceAction *action)
{
    return action->argument_count;
}

/**
 * gupnp_service_action_set:
 * @action: A #GUPnPServiceAction
 * @Varargs: tuples of return value name, return value type, and
 * actual return value, terminated with %NULL.
 *
 * Sets the specified action return values.
 **/
void
gupnp_service_action_set (GUPnPServiceAction *action,
                          ...)
{
        va_list var_args;

        g_return_if_fail (action != NULL);

        va_start (var_args, action);
        gupnp_service_action_set_valist (action, var_args);
        va_end (var_args);
}

/**
 * gupnp_service_action_set_valist:
 * @action: A #GUPnPServiceAction
 * @var_args: va_list of tuples of return value name, return value type, and
 * actual return value.
 *
 * See gupnp_service_action_set(); this version takes a va_list for
 * use by language bindings.
 **/
void
gupnp_service_action_set_valist (GUPnPServiceAction *action,
                                 va_list             var_args)
{
        const char *arg_name;
        GType arg_type;
        GValue value = {0, };
        char *collect_error;

        g_return_if_fail (action != NULL);

        collect_error = NULL;

        arg_name = va_arg (var_args, const char *);
        while (arg_name) {
                arg_type = va_arg (var_args, GType);
                g_value_init (&value, arg_type);

                G_VALUE_COLLECT (&value, var_args, G_VALUE_NOCOPY_CONTENTS,
                                 &collect_error);
                if (!collect_error) {
                        gupnp_service_action_set_value (action,
                                                        arg_name, &value);

                        g_value_unset (&value);

                } else {
                        g_warning ("Error collecting value: %s\n",
                                   collect_error);

                        g_free (collect_error);
                }

                arg_name = va_arg (var_args, const char *);
        }
}

/**
 * gupnp_service_action_set_values:
 * @action: A #GUPnPServiceAction
 * @arg_names: (element-type utf8) (transfer none): A #GList of argument names
 * @arg_values: (element-type GValue) (transfer none): The #GList of values (as
 * #GValues) that line up with @arg_names.
 *
 * Sets the specified action return values.
 **/
void
gupnp_service_action_set_values (GUPnPServiceAction *action,
                                 GList              *arg_names,
                                 GList              *arg_values)
{
        g_return_if_fail (action != NULL);
        g_return_if_fail (arg_names != NULL);
        g_return_if_fail (arg_values != NULL);
        g_return_if_fail (g_list_length (arg_names) ==
                          g_list_length (arg_values));

        if (action->msg->status_code == SOUP_STATUS_INTERNAL_SERVER_ERROR) {
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
                                const char         *argument,
                                const GValue       *value)
{
        g_return_if_fail (action != NULL);
        g_return_if_fail (argument != NULL);
        g_return_if_fail (value != NULL);

        if (action->msg->status_code == SOUP_STATUS_INTERNAL_SERVER_ERROR) {
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
 * gupnp_service_action_return:
 * @action: A #GUPnPServiceAction
 *
 * Return succesfully.
 **/
void
gupnp_service_action_return (GUPnPServiceAction *action)
{
        g_return_if_fail (action != NULL);

        soup_message_set_status (action->msg, SOUP_STATUS_OK);

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
                                   guint               error_code,
                                   const char         *error_description)
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

        soup_message_set_status (action->msg,
                                 SOUP_STATUS_INTERNAL_SERVER_ERROR);

        finalize_action (action);
}

/**
 * gupnp_service_action_get_message:
 * @action: A #GUPnPServiceAction
 *
 * Get the #SoupMessage associated with @action. Mainly intended for
 * applications to be able to read HTTP headers received from clients.
 *
 * Return value: (transfer full): #SoupMessage associated with @action. Unref
 * after using it.
 **/
SoupMessage *
gupnp_service_action_get_message (GUPnPServiceAction *action)
{
        return g_object_ref (action->msg);
}

static void
gupnp_service_init (GUPnPService *service)
{
        service->priv = G_TYPE_INSTANCE_GET_PRIVATE (service,
                                                     GUPNP_TYPE_SERVICE,
                                                     GUPnPServicePrivate);

        service->priv->subscriptions =
                g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       NULL,
                                       (GDestroyNotify) subscription_data_free);

        service->priv->notify_queue = g_queue_new ();
}

/* Generate a new action response node for @action_name */
static GString *
new_action_response_str (const char   *action_name,
                         const char   *service_type)
{
        GString *str;

        str = xml_util_new_string ();

        g_string_append (str, "<u:");
        g_string_append (str, action_name);
        g_string_append (str, "Response xmlns:u=");

        if (service_type != NULL) {
                g_string_append (str, service_type);
                g_string_append_c (str, '"');
        } else {
                g_warning ("No serviceType defined. Control may not work "
                           "correctly.");
        }

        g_string_append_c (str, '>');

        return str;
}

/* Handle QueryStateVariable action */
static void
query_state_variable (GUPnPService       *service,
                      GUPnPServiceAction *action)
{
        xmlNode *node;

        /* Iterate requested variables */
        for (node = action->node->children; node; node = node->next) {
                xmlChar *var_name;
                GValue value = {0,};

                if (strcmp ((char *) node->name, "varName") != 0)
                        continue;

                /* varName */
                var_name = xmlNodeGetContent (node);
                if (!var_name) {
                        gupnp_service_action_return_error (action,
                                                           402,
                                                           "Invalid Args");

                        return;
                }

                /* Query variable */
                g_signal_emit (service,
                               signals[QUERY_VARIABLE],
                               g_quark_from_string ((char *) var_name),
                               (char *) var_name,
                               &value);

                if (!G_IS_VALUE (&value)) {
                        gupnp_service_action_return_error (action,
                                                           402,
                                                           "Invalid Args");

                        xmlFree (var_name);

                        return;
                }

                /* Add variable to response */
                gupnp_service_action_set_value (action,
                                                (char *) var_name,
                                                &value);

                /* Cleanup */
                g_value_unset (&value);

                xmlFree (var_name);
        }

        gupnp_service_action_return (action);
}

/* controlURL handler */
static void
control_server_handler (SoupServer                      *server,
                        SoupMessage                     *msg,
                        G_GNUC_UNUSED const char        *server_path,
                        G_GNUC_UNUSED GHashTable        *query,
                        G_GNUC_UNUSED SoupClientContext *soup_client,
                        gpointer                         user_data)
{
        GUPnPService *service;
        GUPnPContext *context;
        xmlDoc *doc;
        xmlNode *action_node, *node;
        const char *soap_action;
        const char *accept_encoding;
        char *action_name;
        char *end;
        GUPnPServiceAction *action;

        service = GUPNP_SERVICE (user_data);

        if (msg->method != SOUP_METHOD_POST) {
                soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);

                return;
        }

        if (msg->request_body->length == 0) {
                soup_message_set_status (msg, SOUP_STATUS_BAD_REQUEST);

                return;
        }

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (service));

        /* Get action name */
        soap_action = soup_message_headers_get_one (msg->request_headers,
                                                    "SOAPAction");
        if (!soap_action) {
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);
                return;
        }

        action_name = strchr (soap_action, '#');
        if (!action_name) {
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        /* This memory is libsoup-owned so we can do this */
        *action_name = '\0';
        action_name += 1;

        /* This memory is libsoup-owned so we can do this */
        end = strrchr (action_name, '"');
        if (end)
                *end = '\0';

        /* Parse action_node */
        doc = xmlRecoverMemory (msg->request_body->data,
                                msg->request_body->length);
        if (doc == NULL) {
                soup_message_set_status (msg, SOUP_STATUS_BAD_REQUEST);

                return;
        }

        action_node = xml_util_get_element ((xmlNode *) doc,
                                            "Envelope",
                                            "Body",
                                            action_name,
                                            NULL);
        if (!action_node) {
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        /* Create action structure */
        action = g_slice_new0 (GUPnPServiceAction);

        action->ref_count      = 1;
        action->name           = g_strdup (action_name);
        action->msg            = g_object_ref (msg);
        action->doc            = gupnp_xml_doc_new(doc);
        action->node           = action_node;
        action->response_str   = new_action_response_str (action_name,
                                                        soap_action);
        action->context        = g_object_ref (context);
        action->argument_count = 0;

        for (node = action->node->children; node; node = node->next)
                if (node->type == XML_ELEMENT_NODE)
                        action->argument_count++;

        /* Get accepted encodings */
        accept_encoding = soup_message_headers_get_list (msg->request_headers,
                                                         "Accept-Encoding");

        if (accept_encoding) {
                GSList *codings;

                codings = soup_header_parse_quality_list (accept_encoding,
                                                          NULL);
                if (codings &&
                    g_slist_find_custom (codings, "gzip",
                                         (GCompareFunc) g_ascii_strcasecmp)) {
                       action->accept_gzip = TRUE;
                }

                soup_header_free_list (codings);
        }

        /* Tell soup server that response is not ready yet */
        soup_server_pause_message (server, msg);

        /* QueryStateVariable? */
        if (strcmp (action_name, "QueryStateVariable") == 0)
                query_state_variable (service, action);
        else {
                GQuark action_name_quark;

                action_name_quark = g_quark_from_string (action_name);
                if (g_signal_has_handler_pending (service,
                                                  signals[ACTION_INVOKED],
                                                  action_name_quark,
                                                  FALSE)) {
                        /* Emit signal. Handler parses request and fills in
                         * response. */
                        g_signal_emit (service,
                                       signals[ACTION_INVOKED],
                                       action_name_quark,
                                       action);
                } else {
                        /* No handlers attached. */
                        gupnp_service_action_return_error (action,
                                                           401,
                                                           "Invalid Action");
                }
        }
}

/* Generates a standard (re)subscription response */
static void
subscription_response (GUPnPService *service,
                       SoupMessage  *msg,
                       const char   *sid,
                       int           timeout)
{
        GUPnPContext *context;
        GSSDPClient *client;
        char *tmp;

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (service));
        client = GSSDP_CLIENT (context);

        /* Server header on response */
        soup_message_headers_append (msg->response_headers,
                                     "Server",
                                     gssdp_client_get_server_id (client));

        /* SID header */
        soup_message_headers_append (msg->response_headers,
                                     "SID",
                                     sid);

        /* Timeout header */
        if (timeout > 0)
                tmp = g_strdup_printf ("Second-%d", timeout);
        else
                tmp = g_strdup ("infinite");

        soup_message_headers_append (msg->response_headers,
                                     "Timeout",
                                     tmp);
        g_free (tmp);

        /* 200 OK */
        soup_message_set_status (msg, SOUP_STATUS_OK);
}

/* Generates a new SID */
static char *
generate_sid (void)
{
#ifdef G_OS_WIN32
        char *ret = NULL;
        UUID uuid;
        RPC_STATUS stat;
        stat = UuidCreate (&uuid);
        if (stat == RPC_S_OK) {
                unsigned char* uuidStr = NULL;
                stat = UuidToString (&uuid, &uuidStr);
                if (stat == RPC_S_OK) {
                        ret = g_strdup_printf ("uuid:%s", uuidStr);
                        RpcStringFree (&uuidStr);
                }
        }

        return ret;
#else
        uuid_t id;
        char out[39];

        uuid_generate (id);
        uuid_unparse (id, out);

        return g_strdup_printf ("uuid:%s", out);
#endif
}

/* Subscription expired */
static gboolean
subscription_timeout (gpointer user_data)
{
        SubscriptionData *data;

        data = user_data;

        g_hash_table_remove (data->service->priv->subscriptions, data->sid);

        return FALSE;
}

static void
send_initial_state (SubscriptionData *data)
{
        GQueue *queue;
        char *mem;
        GList *l;

        /* Send initial event message */
        queue = g_queue_new ();

        for (l = data->service->priv->state_variables; l; l = l->next) {
                NotifyData *ndata;

                ndata = g_slice_new0 (NotifyData);

                g_signal_emit (data->service,
                               signals[QUERY_VARIABLE],
                               g_quark_from_string (l->data),
                               l->data,
                               &ndata->value);

                if (!G_IS_VALUE (&ndata->value)) {
                        g_slice_free (NotifyData, ndata);

                        continue;
                }

                ndata->variable = g_strdup (l->data);

                g_queue_push_tail (queue, ndata);
        }

        mem = create_property_set (queue);
        notify_subscriber (data->sid, data, mem);

        /* Cleanup */
        g_queue_free (queue);

        g_free (mem);
}


/* Subscription request */
static void
subscribe (GUPnPService *service,
           SoupMessage  *msg,
           const char   *callback)
{
        SubscriptionData *data;
        char *start, *end, *uri;

        data = g_slice_new0 (SubscriptionData);

        /* Parse callback list */
        start = (char *) callback;
        while ((start = strchr (start, '<'))) {
                start += 1;
                if (!start || !*start)
                        break;

                end = strchr (start, '>');
                if (!end || !*end)
                        break;

                if (strncmp (start, "http://", strlen ("http://")) == 0) {
                        uri = g_strndup (start, end - start);
                        data->callbacks = g_list_append (data->callbacks, uri);
                }

                start = end;
        }

        if (!data->callbacks) {
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                g_slice_free (SubscriptionData, data);

                return;
        }

        /* Add service and SID */
        data->service = service;
        data->sid     = generate_sid ();

        /* Add timeout */
        data->timeout_src = g_timeout_source_new_seconds (SUBSCRIPTION_TIMEOUT);
        g_source_set_callback (data->timeout_src,
                               subscription_timeout,
                               data,
                               NULL);

        g_source_attach (data->timeout_src,
                         g_main_context_get_thread_default ());

        g_source_unref (data->timeout_src);

        /* Add to hash */
        g_hash_table_insert (service->priv->subscriptions,
                             data->sid,
                             data);

        /* Respond */
        subscription_response (service, msg, data->sid, SUBSCRIPTION_TIMEOUT);

        send_initial_state (data);
}

/* Resubscription request */
static void
resubscribe (GUPnPService *service,
             SoupMessage  *msg,
             const char   *sid)
{
        SubscriptionData *data;

        data = g_hash_table_lookup (service->priv->subscriptions, sid);
        if (!data) {
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        /* Update timeout */
        if (data->timeout_src) {
                g_source_destroy (data->timeout_src);
                data->timeout_src = NULL;
        }

        data->timeout_src = g_timeout_source_new_seconds (SUBSCRIPTION_TIMEOUT);
        g_source_set_callback (data->timeout_src,
                               subscription_timeout,
                               data,
                               NULL);

        g_source_attach (data->timeout_src,
                         g_main_context_get_thread_default ());

        g_source_unref (data->timeout_src);

        /* Respond */
        subscription_response (service, msg, sid, SUBSCRIPTION_TIMEOUT);
}

/* Unsubscription request */
static void
unsubscribe (GUPnPService *service,
             SoupMessage  *msg,
             const char   *sid)
{
        SubscriptionData *data;

        data = g_hash_table_lookup (service->priv->subscriptions, sid);
        if (data) {
                if (data->initial_state_sent)
                        g_hash_table_remove (service->priv->subscriptions,
                                             sid);
                else
                        data->to_delete = TRUE;
                soup_message_set_status (msg, SOUP_STATUS_OK);
        } else
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);
}

/* eventSubscriptionURL handler */
static void
subscription_server_handler (G_GNUC_UNUSED SoupServer        *server,
                             SoupMessage                     *msg,
                             G_GNUC_UNUSED const char        *server_path,
                             G_GNUC_UNUSED GHashTable        *query,
                             G_GNUC_UNUSED SoupClientContext *soup_client,
                             gpointer                         user_data)
{
        GUPnPService *service;
        const char *callback, *nt, *sid;

        service = GUPNP_SERVICE (user_data);

        callback = soup_message_headers_get_one (msg->request_headers,
                                                 "Callback");
        nt       = soup_message_headers_get_one (msg->request_headers, "NT");
        sid      = soup_message_headers_get_one (msg->request_headers, "SID");

        /* Choose appropriate handler */
        if (strcmp (msg->method, GENA_METHOD_SUBSCRIBE) == 0) {
                if (callback) {
                        if (sid) {
                                soup_message_set_status
                                        (msg, SOUP_STATUS_BAD_REQUEST);

                        } else if (!nt || strcmp (nt, "upnp:event") != 0) {
                                soup_message_set_status
                                        (msg, SOUP_STATUS_PRECONDITION_FAILED);

                        } else {
                                subscribe (service, msg, callback);

                        }

                } else if (sid) {
                        if (nt) {
                                soup_message_set_status
                                        (msg, SOUP_STATUS_BAD_REQUEST);

                        } else {
                                resubscribe (service, msg, sid);

                        }

                } else {
                        soup_message_set_status
                                (msg, SOUP_STATUS_PRECONDITION_FAILED);

                }

        } else if (strcmp (msg->method, GENA_METHOD_UNSUBSCRIBE) == 0) {
                if (sid) {
                        if (nt || callback) {
                                soup_message_set_status
                                        (msg, SOUP_STATUS_BAD_REQUEST);

                        } else {
                                unsubscribe (service, msg, sid);

                        }

                } else {
                        soup_message_set_status
                                (msg, SOUP_STATUS_PRECONDITION_FAILED);

                }

        } else {
                soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);

        }
}

static void
got_introspection (GUPnPServiceInfo          *info,
                   GUPnPServiceIntrospection *introspection,
                   const GError              *error,
                   G_GNUC_UNUSED gpointer     user_data)
{
        GUPnPService *service = GUPNP_SERVICE (info);
        const GList *state_variables, *l;
        GHashTableIter iter;
        gpointer data;

        if (introspection) {
                state_variables =
                        gupnp_service_introspection_list_state_variables
                                (introspection);

                for (l = state_variables; l; l = l->next) {
                        GUPnPServiceStateVariableInfo *variable;

                        variable = l->data;

                        if (!variable->send_events)
                                continue;

                        service->priv->state_variables =
                                g_list_prepend (service->priv->state_variables,
                                                g_strdup (variable->name));
                }

                g_object_unref (introspection);
        } else
                g_warning ("Failed to get SCPD: %s\n"
                           "The initial event message will not be sent.",
                           error ? error->message : "No error");

        g_hash_table_iter_init (&iter, service->priv->subscriptions);

        while (g_hash_table_iter_next (&iter, NULL, &data)) {
                send_initial_state ((SubscriptionData *) data);
                if (subscription_data_can_delete ((SubscriptionData *) data))
                        g_hash_table_iter_remove (&iter);
        }
}

static char *
path_from_url (const char *url)
{
        SoupURI *uri;
        gchar   *path;

        uri = soup_uri_new (url);
        path = soup_uri_to_string (uri, TRUE);
        soup_uri_free (uri);

        return path;
}

static GObject *
gupnp_service_constructor (GType                  type,
                           guint                  n_construct_params,
                           GObjectConstructParam *construct_params)
{
        GObjectClass *object_class;
        GObject *object;
        GUPnPServiceInfo *info;
        GUPnPContext *context;
        SoupServer *server;
        char *url;
        char *path;

        object_class = G_OBJECT_CLASS (gupnp_service_parent_class);

        /* Construct */
        object = object_class->constructor (type,
                                            n_construct_params,
                                            construct_params);

        info    = GUPNP_SERVICE_INFO (object);

        /* Get introspection and save state variable names */
        gupnp_service_info_get_introspection_async (info,
                                                    got_introspection,
                                                    NULL);

        /* Get server */
        context = gupnp_service_info_get_context (info);
        server = gupnp_context_get_server (context);

        /* Run listener on controlURL */
        url = gupnp_service_info_get_control_url (info);
        path = path_from_url (url);
        soup_server_add_handler (server, path,
                                 control_server_handler, object, NULL);
        g_free (path);
        g_free (url);

        /* Run listener on eventSubscriptionURL */
        url = gupnp_service_info_get_event_subscription_url (info);
        path = path_from_url (url);
        soup_server_add_handler (server, path,
                                 subscription_server_handler, object, NULL);
        g_free (path);
        g_free (url);

        return object;
}

/* Root device availability changed. */
static void
notify_available_cb (GObject                  *object,
                     G_GNUC_UNUSED GParamSpec *pspec,
                     gpointer                  user_data)
{
        GUPnPService *service;

        service = GUPNP_SERVICE (user_data);

        if (!gupnp_root_device_get_available (GUPNP_ROOT_DEVICE (object))) {
                /* Root device now unavailable: Purge subscriptions */
                g_hash_table_remove_all (service->priv->subscriptions);
        }
}

static void
gupnp_service_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
        GUPnPService *service;

        service = GUPNP_SERVICE (object);

        switch (property_id) {
        case PROP_ROOT_DEVICE: {
                GUPnPRootDevice **dev;

                service->priv->root_device = g_value_get_object (value);
                dev = &(service->priv->root_device);

                g_object_add_weak_pointer
                        (G_OBJECT (service->priv->root_device),
                         (gpointer *) dev);

                service->priv->notify_available_id =
                        g_signal_connect_object (service->priv->root_device,
                                                 "notify::available",
                                                 G_CALLBACK
                                                        (notify_available_cb),
                                                 object,
                                                 0);

                break;
        }
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_service_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
        GUPnPService *service;

        service = GUPNP_SERVICE (object);

        switch (property_id) {
        case PROP_ROOT_DEVICE:
                g_value_set_object (value, service->priv->root_device);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_service_dispose (GObject *object)
{
        GUPnPService *service;
        GObjectClass *object_class;
        GUPnPServiceInfo *info;
        GUPnPContext *context;
        SoupServer *server;
        char *url;
        char *path;

        service = GUPNP_SERVICE (object);

        /* Get server */
        info = GUPNP_SERVICE_INFO (service);
        context = gupnp_service_info_get_context (info);
        server = gupnp_context_get_server (context);

        /* Remove listener on controlURL */
        url = gupnp_service_info_get_control_url (info);
        path = path_from_url (url);
        soup_server_remove_handler (server, path);
        g_free (path);
        g_free (url);

        /* Remove listener on eventSubscriptionURL */
        url = gupnp_service_info_get_event_subscription_url (info);
        path = path_from_url (url);
        soup_server_remove_handler (server, path);
        g_free (path);
        g_free (url);

        if (service->priv->root_device) {
                GUPnPRootDevice **dev = &(service->priv->root_device);

                if (g_signal_handler_is_connected
                        (service->priv->root_device,
                         service->priv->notify_available_id)) {
                        g_signal_handler_disconnect
                                (service->priv->root_device,
                                 service->priv->notify_available_id);
                }

                g_object_remove_weak_pointer
                        (G_OBJECT (service->priv->root_device),
                         (gpointer *) dev);

                service->priv->root_device = NULL;
        }

        /* Cancel pending messages */
        g_hash_table_remove_all (service->priv->subscriptions);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_service_parent_class);
        object_class->dispose (object);
}

static void
gupnp_service_finalize (GObject *object)
{
        GUPnPService *service;
        GObjectClass *object_class;
        NotifyData *data;

        service = GUPNP_SERVICE (object);

        /* Free subscription hash */
        g_hash_table_destroy (service->priv->subscriptions);

        /* Free state variable list */
        while (service->priv->state_variables) {
                g_free (service->priv->state_variables->data);
                service->priv->state_variables =
                        g_list_delete_link (service->priv->state_variables,
                                            service->priv->state_variables);
        }

        /* Free notify queue */
        while ((data = g_queue_pop_head (service->priv->notify_queue)))
                notify_data_free (data);

        g_queue_free (service->priv->notify_queue);

        if (service->priv->session) {
                g_object_unref (service->priv->session);
                service->priv->session = NULL;
        }

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_service_parent_class);
        object_class->finalize (object);
}

static void
gupnp_service_class_init (GUPnPServiceClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_service_set_property;
        object_class->get_property = gupnp_service_get_property;
        object_class->constructor  = gupnp_service_constructor;
        object_class->dispose      = gupnp_service_dispose;
        object_class->finalize     = gupnp_service_finalize;

        g_type_class_add_private (klass, sizeof (GUPnPServicePrivate));

        /**
         * GUPnPService:root-device:
         *
         * The containing #GUPnPRootDevice.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_ROOT_DEVICE,
                 g_param_spec_object ("root-device",
                                      "Root device",
                                      "The GUPnPRootDevice",
                                      GUPNP_TYPE_ROOT_DEVICE,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPService::action-invoked:
         * @service: The #GUPnPService that received the signal
         * @action: The invoked #GUPnPServiceAction
         *
         * Emitted whenever an action is invoked. Handler should process
         * @action and must call either gupnp_service_action_return() or
         * gupnp_service_action_return_error().
         **/
        signals[ACTION_INVOKED] =
                g_signal_new ("action-invoked",
                              GUPNP_TYPE_SERVICE,
                              G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                              G_STRUCT_OFFSET (GUPnPServiceClass,
                                               action_invoked),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__BOXED,
                              G_TYPE_NONE,
                              1,
                              GUPNP_TYPE_SERVICE_ACTION);

        /**
         * GUPnPService::query-variable:
         * @service: The #GUPnPService that received the signal
         * @variable: The variable that is being queried
         * @value: (type GValue)(inout):The location of the #GValue of the variable
         *
         * Emitted whenever @service needs to know the value of @variable.
         * Handler should fill @value with the value of @variable.
         **/
        signals[QUERY_VARIABLE] =
                g_signal_new ("query-variable",
                              GUPNP_TYPE_SERVICE,
                              G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                              G_STRUCT_OFFSET (GUPnPServiceClass,
                                               query_variable),
                              NULL,
                              NULL,
                              gupnp_marshal_VOID__STRING_POINTER,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_STRING,
                              G_TYPE_POINTER /* Not G_TYPE_VALUE as this
                                                is an outward argument! */);

        /**
         * GUPnPService::notify-failed:
         * @service: The #GUPnPService that received the signal
         * @callback_url: (type GList)(element-type SoupURI):A #GList of callback URLs
         * @reason: (type GError): A pointer to a #GError describing why the notify failed
         *
         * Emitted whenever notification of a client fails.
         **/
        signals[NOTIFY_FAILED] =
                g_signal_new ("notify-failed",
                              GUPNP_TYPE_SERVICE,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GUPnPServiceClass,
                                               notify_failed),
                              NULL,
                              NULL,
                              gupnp_marshal_VOID__POINTER_POINTER,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_POINTER,
                              G_TYPE_POINTER);
}

/**
 * gupnp_service_notify:
 * @service: A #GUPnPService
 * @Varargs: Tuples of variable name, variable type, and variable value,
 * terminated with %NULL.
 *
 * Notifies listening clients that the properties listed in @Varargs
 * have changed to the specified values.
 **/
void
gupnp_service_notify (GUPnPService *service,
                      ...)
{
        va_list var_args;

        g_return_if_fail (GUPNP_IS_SERVICE (service));

        va_start (var_args, service);
        gupnp_service_notify_valist (service, var_args);
        va_end (var_args);
}

/**
 * gupnp_service_notify_valist:
 * @service: A #GUPnPService
 * @var_args: A va_list of tuples of variable name, variable type, and variable
 * value, terminated with %NULL.
 *
 * See gupnp_service_notify(); this version takes a va_list for
 * use by language bindings.
 **/
void
gupnp_service_notify_valist (GUPnPService *service,
                             va_list       var_args)
{
        const char *var_name;
        GType var_type;
        GValue value = {0, };
        char *collect_error;

        g_return_if_fail (GUPNP_IS_SERVICE (service));

        collect_error = NULL;

        var_name = va_arg (var_args, const char *);
        while (var_name) {
                var_type = va_arg (var_args, GType);
                g_value_init (&value, var_type);

                G_VALUE_COLLECT (&value, var_args, G_VALUE_NOCOPY_CONTENTS,
                                 &collect_error);
                if (!collect_error) {
                        gupnp_service_notify_value (service, var_name, &value);

                        g_value_unset (&value);

                } else {
                        g_warning ("Error collecting value: %s\n",
                                   collect_error);

                        g_free (collect_error);
                }

                var_name = va_arg (var_args, const char *);
        }
}

/* Received notify response. */
static void
notify_got_response (G_GNUC_UNUSED SoupSession *session,
                     SoupMessage               *msg,
                     gpointer                   user_data)
{
        SubscriptionData *data;

        /* Cancelled? */
        if (msg->status_code == SOUP_STATUS_CANCELLED)
                return;

        data = user_data;

        /* Remove from pending messages list */
        data->pending_messages = g_list_remove (data->pending_messages, msg);

        if (SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
                data->initial_state_sent = TRUE;

                /* Success: reset callbacks pointer */
                data->callbacks = g_list_first (data->callbacks);

        } else if (msg->status_code == SOUP_STATUS_PRECONDITION_FAILED) {
                /* Precondition failed: Cancel subscription */
                g_hash_table_remove (data->service->priv->subscriptions,
                                     data->sid);

        } else {
                /* Other failure: Try next callback or signal failure. */
                if (data->callbacks->next) {
                        SoupURI *uri;
                        SoupSession *service_session;

                        /* Call next callback */
                        data->callbacks = data->callbacks->next;

                        uri = soup_uri_new (data->callbacks->data);
                        soup_message_set_uri (msg, uri);
                        soup_uri_free (uri);

                        /* And re-queue */
                        data->pending_messages = 
                                g_list_prepend (data->pending_messages, msg);

                        service_session = gupnp_service_get_session (data->service);

                        soup_session_requeue_message (service_session, msg);
                } else {
                        /* Emit 'notify-failed' signal */
                        GError *error;

                        error = g_error_new_literal
                                        (GUPNP_EVENTING_ERROR,
                                         GUPNP_EVENTING_ERROR_NOTIFY_FAILED,
                                         msg->reason_phrase);

                        g_signal_emit (data->service,
                                       signals[NOTIFY_FAILED],
                                       0,
                                       data->callbacks,
                                       error);

                        g_error_free (error);

                        /* Reset callbacks pointer */
                        data->callbacks = g_list_first (data->callbacks);
                }
        }
}

/* Send notification @user_data to subscriber @value */
static void
notify_subscriber (G_GNUC_UNUSED gpointer key,
                   gpointer               value,
                   gpointer               user_data)
{
        SubscriptionData *data;
        const char *property_set;
        char *tmp;
        SoupMessage *msg;
        SoupSession *session;

        data = value;
        property_set = user_data;

        /* Subscriber called unsubscribe */
        if (subscription_data_can_delete (data))
                return;

        /* Create message */
        msg = soup_message_new (GENA_METHOD_NOTIFY, data->callbacks->data);
        if (!msg) {
                g_warning ("Invalid callback URL: %s",
                           (char *) data->callbacks->data);

                return;
        }

        soup_message_headers_append (msg->request_headers,
                                     "NT",
                                     "upnp:event");

        soup_message_headers_append (msg->request_headers,
                                     "NTS",
                                     "upnp:propchange");

        soup_message_headers_append (msg->request_headers,
                                     "SID",
                                     data->sid);

        tmp = g_strdup_printf ("%d", data->seq);
        soup_message_headers_append (msg->request_headers,
                                     "SEQ",
                                     tmp);
        g_free (tmp);

        /* Handle overflow */
        if (data->seq < G_MAXINT32)
                data->seq++;
        else
                data->seq = 1;

        /* Add body */
        soup_message_set_request (msg,
                                  "text/xml; charset=\"utf-8\"",
                                  SOUP_MEMORY_TAKE,
                                  g_strdup (property_set),
                                  strlen (property_set));

        /* Queue */
        data->pending_messages = g_list_prepend (data->pending_messages, msg);
        soup_message_headers_append (msg->request_headers,
                                     "Connection", "close");

        session = gupnp_service_get_session (data->service);

        soup_session_queue_message (session,
                                    msg,
                                    notify_got_response,
                                    data);
}

/* Create a property set from @queue */
static char *
create_property_set (GQueue *queue)
{
        NotifyData *data;
        GString *str;

        /* Compose property set */
        str = xml_util_new_string ();

        g_string_append (str,
                         "<?xml version=\"1.0\"?>"
                         "<e:propertyset xmlns:e="
                                "\"urn:schemas-upnp-org:event-1-0\">");

        /* Add variables */
        while ((data = g_queue_pop_head (queue))) {
                xml_util_start_element (str, "e:property");
                xml_util_start_element (str, data->variable);
                gvalue_util_value_append_to_xml_string (&data->value, str);
                xml_util_end_element (str, data->variable);
                xml_util_end_element (str, "e:property");

                /* Cleanup */
                notify_data_free (data);
        }

        g_string_append (str, "</e:propertyset>");

        /* Cleanup & return */
        return g_string_free (str, FALSE);
}

/* Flush all queued notifications */
static void
flush_notifications (GUPnPService *service)
{
        char *mem;

        /* Create property set */
        mem = create_property_set (service->priv->notify_queue);

        /* And send it off */
        g_hash_table_foreach (service->priv->subscriptions,
                              notify_subscriber,
                              mem);

        /* Cleanup */
        g_free (mem);
}

/**
 * gupnp_service_notify_value:
 * @service: A #GUPnPService
 * @variable: The name of the variable to notify
 * @value: The value of the variable
 *
 * Notifies listening clients that @variable has changed to @value.
 **/
void
gupnp_service_notify_value (GUPnPService *service,
                            const char   *variable,
                            const GValue *value)
{
        NotifyData *data;

        g_return_if_fail (GUPNP_IS_SERVICE (service));
        g_return_if_fail (variable != NULL);
        g_return_if_fail (G_IS_VALUE (value));

        /* Queue */
        data = g_slice_new0 (NotifyData);

        data->variable = g_strdup (variable);

        g_value_init (&data->value, G_VALUE_TYPE (value));
        g_value_copy (value, &data->value);

        g_queue_push_tail (service->priv->notify_queue, data);

        /* And flush, if not frozen */
        if (!service->priv->notify_frozen)
                flush_notifications (service);
}

/**
 * gupnp_service_freeze_notify:
 * @service: A #GUPnPService
 *
 * Causes new notifications to be queued up until gupnp_service_thaw_notify()
 * is called.
 **/
void
gupnp_service_freeze_notify (GUPnPService *service)
{
        g_return_if_fail (GUPNP_IS_SERVICE (service));

        service->priv->notify_frozen = TRUE;
}

/**
 * gupnp_service_thaw_notify:
 * @service: A #GUPnPService
 *
 * Sends out any pending notifications, and stops queuing of new ones.
 **/
void
gupnp_service_thaw_notify (GUPnPService *service)
{
        g_return_if_fail (GUPNP_IS_SERVICE (service));

        service->priv->notify_frozen = FALSE;

        if (g_queue_get_length (service->priv->notify_queue) == 0)
                return; /* Empty notify queue */

        flush_notifications (service);
}

/* Convert a CamelCase string to a lowercase string with underscores */
static char *
strip_camel_case (char *camel_str)
{
        char *stripped;
        unsigned int i, j;

        /* Keep enough space for underscores */
        stripped = g_malloc (strlen (camel_str) * 2);

        for (i = 0, j = 0; i <= strlen (camel_str); i++) {
                /* Convert every upper case letter to lower case and unless
                 * it's the first character, the last charachter, in the
                 * middle of an abbreviation or there is already an underscore
                 * before it, add an underscore before it */
                if (g_ascii_isupper (camel_str[i])) {
                        if (i != 0 &&
                            camel_str[i + 1] != '\0' &&
                            camel_str[i - 1] != '_' &&
                            !g_ascii_isupper (camel_str[i - 1])) {
                                stripped[j++] = '_';
                        }
                        stripped[j++] = g_ascii_tolower (camel_str[i]);
                } else
                        stripped[j++] = camel_str[i];
        }

        return stripped;
}

static GCallback
find_callback_by_name (GModule    *module,
                       const char *name)
{
        GCallback callback;
        char *full_name;

        /* First try with 'on_' prefix */
        full_name = g_strjoin ("_",
                               "on",
                               name,
                               NULL);

        if (!g_module_symbol (module,
                              full_name,
                              (gpointer) &callback)) {
                g_free (full_name);

                /* Now try with '_cb' postfix */
                full_name = g_strjoin ("_",
                                       name,
                                       "cb",
                                       NULL);

                if (!g_module_symbol (module,
                                      full_name,
                                      (gpointer) &callback))
                        callback = NULL;
        }

        g_free (full_name);

        return callback;
}

/* Use the strings from @name_list as details to @signal_name, and connect
 * callbacks with names based on these same strings to @signal_name::string. */
static void
connect_names_to_signal_handlers (GUPnPService *service,
                                  GModule      *module,
                                  const GList  *name_list,
                                  const char   *signal_name,
                                  const char   *callback_prefix,
                                  gpointer      user_data)
{
        const GList *name_node;

        for (name_node = name_list;
             name_node;
             name_node = name_node->next) {
                GCallback callback;
                char     *callback_name;
                char     *signal_detail;

                signal_detail = (char *) name_node->data;
                callback_name = strip_camel_case (signal_detail);

                if (callback_prefix) {
                        char *tmp;

                        tmp = g_strjoin ("_",
                                         callback_prefix,
                                         callback_name,
                                         NULL);

                        g_free (callback_name);
                        callback_name = tmp;
                }

                callback = find_callback_by_name (module, callback_name);
                g_free (callback_name);

                if (callback == NULL)
                        continue;

                signal_detail = g_strjoin ("::",
                                           signal_name,
                                           signal_detail,
                                           NULL);

                g_signal_connect (service,
                                  signal_detail,
                                  callback,
                                  user_data);

                g_free (signal_detail);
        }
}

/**
 * gupnp_service_signals_autoconnect:
 * @service: A #GUPnPService
 * @user_data: the data to pass to each of the callbacks
 * @error: return location for a #GError, or %NULL
 *
 * A convenience function that attempts to connect all possible
 * #GUPnPService::action-invoked and #GUPnPService::query-variable signals to
 * appropriate callbacks for the service @service. It uses service introspection
 * and GModule's introspective features. It is very simillar to
 * gtk_builder_connect_signals() except that it attempts to guess the names of
 * the signal handlers on its own.
 *
 * For this function to do its magic, the application must name the callback
 * functions for #GUPnPService::action-invoked signals by striping the CamelCase
 * off the action names and either prepend "on_" or append "_cb" to them. Same
 * goes for #GUPnPService::query-variable signals, except that "query_" should
 * be prepended to the variable name. For example, callback function for
 * "GetSystemUpdateID" action should be either named as
 * "get_system_update_id_cb" or "on_get_system_update_id" and callback function
 * for the query of "SystemUpdateID" state variable should be named
 * "query_system_update_id_cb" or "on_query_system_update_id".
 *
 * Note that this function will not work correctly if GModule is not supported
 * on the platform or introspection is not available for service @service.
 *
 * WARNING: This function can not and therefore does not guarantee that the
 * resulting signal connections will be correct as it depends heavily on a
 * particular naming schemes described above.
 **/
void
gupnp_service_signals_autoconnect (GUPnPService *service,
                                   gpointer      user_data,
                                   GError      **error)
{
        GUPnPServiceIntrospection *introspection;
        const GList               *names;
        GModule                   *module;

        g_return_if_fail (GUPNP_IS_SERVICE (service));

        introspection = gupnp_service_info_get_introspection
                                (GUPNP_SERVICE_INFO (service),
                                 error);
        if (!introspection)
                return;

        /* Get a handle on the main executable -- use this to find symbols */
        module = g_module_open (NULL, 0);
        if (module == NULL) {
                g_error ("Failed to open module: %s", g_module_error ());

                g_object_unref (introspection);

                return;
        }

        names = gupnp_service_introspection_list_action_names (introspection);
        connect_names_to_signal_handlers (service,
                                          module,
                                          names,
                                          "action-invoked",
                                          NULL,
                                          user_data);

        names = gupnp_service_introspection_list_state_variable_names
                        (introspection);
        connect_names_to_signal_handlers (service,
                                          module,
                                          names,
                                          "query-variable",
                                          "query",
                                          user_data);

        g_module_close (module);
        g_object_unref (introspection);
}
