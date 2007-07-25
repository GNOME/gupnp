/* 
 * Copyright (C) 2007 OpenedHand Ltd.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gupnp-service
 * @short_description: Class for service implementations.
 *
 * #GUPnPService allows for handling incoming actions and state variable
 * notification. #GUPnPService implements the #GUPnPServiceInfo interface.
 */

#include <gobject/gvaluecollector.h>
#include <libsoup/soup-date.h>
#include <uuid/uuid.h>
#include <string.h>

#include "gupnp-service.h"
#include "gupnp-service-private.h"
#include "gupnp-context-private.h"
#include "gupnp-marshal.h"
#include "gupnp-error.h"
#include "accept-language.h"
#include "gena-protocol.h"
#include "xml-util.h"

G_DEFINE_TYPE (GUPnPService,
               gupnp_service,
               GUPNP_TYPE_SERVICE_INFO);

struct _GUPnPServicePrivate {
        GHashTable *subscriptions;

        GList      *state_variables;

        GQueue     *notify_queue;

        gboolean    notify_frozen;
};

enum {
        ACTION_INVOKED,
        QUERY_VARIABLE,
        NOTIFY_FAILED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static xmlChar *
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

        guint         timeout_id;
} SubscriptionData;

static void
subscription_data_free (SubscriptionData *data)
{
        while (data->callbacks) {
                g_free (data->callbacks->data);
                data->callbacks = g_list_delete_link (data->callbacks,
                                                      data->callbacks);
        }

        g_free (data->sid);

        if (data->timeout_id)
                g_source_remove (data->timeout_id);

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
        const char  *name;

        SoupMessage *msg;

        xmlNode     *node;

        xmlNode     *response_node;
};

/**
 * gupnp_service_action_get_name
 * @action: A #GUPnPServiceAction
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
 * gupnp_service_action_get_locales
 * @action: A #GUPnPServiceAction
 *
 * Return value: An ordered (preferred first) #GList of locales preferred by 
 * the client. Free list and elements after use.
 **/
GList *
gupnp_service_action_get_locales (GUPnPServiceAction *action)
{
        g_return_val_if_fail (action != NULL, NULL);

        return accept_language_get_locales (action->msg);
}

/**
 * gupnp_service_action_get
 * @action: A #GUPnPServiceAction
 * @Varargs: tuples of argument name, argument type, and argument value
 * location, terminated with NULL.
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
 * gupnp_service_action_get_valist
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

                        return;
                }

                arg_name = va_arg (var_args, const char *);
        }
}

/**
 * gupnp_service_action_get_value
 * @action: A #GUPnPServiceAction
 * @argument: The name of the argument to retrieve
 * @value: The #GValue to store the value of the argument, initialized
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

        g_return_if_fail (action != NULL);
        g_return_if_fail (argument != NULL);
        g_return_if_fail (value != NULL);

        for (node = action->node->children; node; node = node->next) {
                if (strcmp ((char *) node->name, argument) != 0)
                        continue;

                xml_util_node_get_content_value (node, value);

                break;
        }
}

/**
 * gupnp_service_action_set
 * @action: A #GUPnPServiceAction
 * @Varargs: tuples of return value name, return value type, and
 * actual return value, terminated with NULL.
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
 * gupnp_service_action_set_valist
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

                G_VALUE_COLLECT (&value, var_args, 0, &collect_error);
                if (collect_error) {
                        g_warning ("Error collecting value: %s\n",
                                   collect_error);

                        g_free (collect_error);

                        g_value_unset (&value);

                        return;
                }

                gupnp_service_action_set_value (action, arg_name, &value);

                g_value_unset (&value);

                arg_name = va_arg (var_args, const char *);
        }
}

/**
 * gupnp_service_action_set_value
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
        GValue transformed_value = {0, };

        g_return_if_fail (action != NULL);
        g_return_if_fail (argument != NULL);
        g_return_if_fail (value != NULL);

        if (action->msg->status == SOUP_STATUS_INTERNAL_SERVER_ERROR) {
                g_warning ("Calling gupnp_service_action_set_value() after "
                           "having called gupnp_service_action_return_error() "
                           "is not allowed.");

                return;
        }

        /* Transform to string */
        g_value_init (&transformed_value, G_TYPE_STRING);
        if (g_value_transform (value, &transformed_value)) {
                /* Append to response */
                xmlNewTextChild (action->response_node,
                                 NULL,
                                 (const xmlChar *) argument,
                                 (xmlChar *) g_value_get_string
                                                    (&transformed_value));
        } else {
                g_warning ("Failed to transform value of type %s to "
                           "string.", G_VALUE_TYPE_NAME (value));
        }

        /* Cleanup */
        g_value_unset (&transformed_value);
}

/**
 * gupnp_service_action_return
 * @action: A #GUPnPServiceAction
 *
 * Return succesfully.
 **/
void
gupnp_service_action_return (GUPnPServiceAction *action)
{
        g_return_if_fail (action != NULL);

        soup_message_set_status (action->msg, SOUP_STATUS_OK);
}

/**
 * gupnp_service_action_return_error
 * @action: A #GUPnPServiceAction
 * @error_code: The error code
 * @error_description: The error description, or NULL if @error_code is
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
        xmlNode *node;
        xmlNs *ns;
        char *tmp;

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

        /* Replace response_node with a SOAP Fault */
        xmlFreeNode (action->response_node);
        action->response_node = xmlNewNode (NULL,
                                            (const xmlChar *) "Fault");

        xmlNewTextChild (action->response_node,
                         NULL,
                         (const xmlChar *) "faultcode",
                         (const xmlChar *) "s:Client");

        xmlNewTextChild (action->response_node,
                         NULL,
                         (const xmlChar *) "faultstring",
                         (const xmlChar *) "UPnPError");

        node = xmlNewTextChild (action->response_node,
                                NULL,
                                (const xmlChar *) "detail",
                                NULL);

        node = xmlNewTextChild (node,
                                NULL,
                                (const xmlChar *) "UPnPError",
                                NULL);

        ns = xmlNewNs (node,
                       (const xmlChar *) "urn:schemas-upnp-org:control-1-0",
                       (const xmlChar *) "u");
        xmlSetNs (node, ns);

        tmp = g_strdup_printf ("%u", error_code);
        xmlNewTextChild (node,
                         NULL,
                         (const xmlChar *) "errorCode",
                         (const xmlChar *) tmp);
        g_free (tmp);

        xmlNewTextChild (node,
                         NULL,
                         (const xmlChar *) "errorDescription",
                         (const xmlChar *) error_description);

        soup_message_set_status (action->msg,
                                 SOUP_STATUS_INTERNAL_SERVER_ERROR);
}

static void
gupnp_service_init (GUPnPService *proxy)
{
        proxy->priv = G_TYPE_INSTANCE_GET_PRIVATE (proxy,
                                                   GUPNP_TYPE_SERVICE,
                                                   GUPnPServicePrivate);

        proxy->priv->subscriptions =
                g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       NULL,
                                       (GDestroyNotify) subscription_data_free);

        proxy->priv->notify_queue = g_queue_new ();
}

/* Generate a new action response node for @action_name */
static xmlNode *
new_action_response_node (GUPnPService *service,
                          const char   *action_name)
{
        xmlNode *node;
        xmlNs *ns;
        char *tmp;

        tmp = g_strdup_printf ("%sResponse", action_name);
        node = xmlNewNode (NULL, (const xmlChar *) tmp);
        g_free (tmp);

        tmp = gupnp_service_info_get_service_type
                                        (GUPNP_SERVICE_INFO (service));
        ns = xmlNewNs (node, (xmlChar *) tmp, (const xmlChar *) "u");
        g_free (tmp);

        xmlSetNs (node, ns);

        return node;
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
control_server_handler (SoupServerContext *server_context,
                        SoupMessage       *msg,
                        gpointer           user_data)
{
        GUPnPService *service;
        GUPnPContext *context;
        GSSDPClient *client;
        xmlDoc *doc, *response_doc;
        xmlNode *action_node, *response_node;
        xmlNs *ns;
        const char *soap_action, *action_name;
        char *end, *date;
        GUPnPServiceAction *action;
        xmlChar *mem;
        int size;

        service = GUPNP_SERVICE (user_data);

        if (strcmp (msg->method, SOUP_METHOD_POST) != 0) {
                soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);

                return;
        }

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (service));
        client = GSSDP_CLIENT (context);

        /* Get action name */
        soap_action = soup_message_get_header (msg->request_headers,
                                               "SOAPAction");
        action_name = strchr (soap_action, '#');
        if (!action_name) {
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        action_name += 1;

        /* This memory is libsoup-owned so we can do this */
        end = strrchr (action_name, '"');
        *end = '\0';

        /* Parse action_node */
        doc = xmlParseMemory (msg->request.body, msg->request.length);
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
        action = g_slice_new (GUPnPServiceAction);

        action->name          = action_name;
        action->msg           = msg;
        action->node          = action_node;
        action->response_node = new_action_response_node (service, action_name);

        /* QueryStateVariable? */
        if (strcmp (action_name, "QueryStateVariable") == 0)
                query_state_variable (service, action);
        else {
                /* Emit signal. Handler parses request and fills in response. */
                g_signal_emit (service,
                               signals[ACTION_INVOKED],
                               g_quark_from_string (action_name),
                               action);

                /* Was it handled? */
                if (msg->status_code == SOUP_STATUS_NONE) {
                        /* No. */
                        gupnp_service_action_return_error (action,
                                                           401,
                                                           "Invalid Action");
                }
        }

        /* Embed action->response_node in a SOUP document */
        response_doc = xmlNewDoc ((const xmlChar *) "1.0");
        response_node = xmlNewDocNode (response_doc,
                                       NULL,
                                       (const xmlChar *) "Envelope",
                                       NULL);
        xmlDocSetRootElement (response_doc, response_node);

        ns = xmlNewNs (response_node,
                       (const xmlChar *)
                               "http://schemas.xmlsoap.org/soap/envelope/",
                       (const xmlChar *) "s");
        xmlSetNs (response_node, ns);

        response_node = xmlNewChild (response_node,
                                     ns,
                                     (const xmlChar *) "Body",
                                     NULL);

        if (msg->status_code == SOUP_STATUS_INTERNAL_SERVER_ERROR)
                xmlSetNs (action->response_node, ns);

        xmlAddChild (response_node, action->response_node);

        /* Dump document to response */
        xmlDocDumpMemory (response_doc, &mem, &size);

        msg->response.owner  = SOUP_BUFFER_SYSTEM_OWNED;
        msg->response.body   = g_strdup ((char *) mem);
        msg->response.length = size;

        /* Cleanup */
        xmlFreeDoc (response_doc);
        xmlFree (mem);

        g_slice_free (GUPnPServiceAction, action);

        /* Server header on response */
        soup_message_add_header (msg->response_headers,
                                 "Server",
                                 gssdp_client_get_server_id (client));

        /* Date header on response */
        date = soup_date_generate (time (NULL));
        soup_message_add_header (msg->response_headers,
                                 "Date",
                                 date);
        g_free (date);
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
        soup_message_add_header (msg->response_headers,
                                 "Server",
                                 gssdp_client_get_server_id (client));

        /* Date header on response */
        tmp = soup_date_generate (time (NULL));
        soup_message_add_header (msg->response_headers,
                                 "Date",
                                 tmp);
        g_free (tmp);

        /* SID header */
        soup_message_add_header (msg->response_headers,
                                 "SID",
                                 sid);

        /* Timeout header */
        if (timeout > 0)
                tmp = g_strdup_printf ("Second-%d", timeout);
        else
                tmp = g_strdup ("infinite");

        soup_message_add_header (msg->response_headers,
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
        uuid_t id;
        char out[39];

        uuid_generate (id);
        uuid_unparse (id, out);

        return g_strdup_printf ("uuid:%s", out);
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

/* Parse timeout header and return its value in seconds, or -1
 * if infinite */
static int
parse_timeout (const char *timeout)
{
        int timeout_seconds;

        timeout_seconds = 0;

        if (strncmp (timeout, "Second-", strlen ("Second-")) == 0) {
                /* We have a finite timeout */
                timeout_seconds = atoi (timeout + strlen ("Second-"));
                if (timeout_seconds < GENA_MIN_TIMEOUT) {
                        g_warning ("Specified timeout is too short. Assuming "
                                   "default of %d.", GENA_DEFAULT_TIMEOUT);

                        timeout_seconds = GENA_DEFAULT_TIMEOUT;
                }
        }

        return timeout_seconds;
}

/* Subscription request */
static void
subscribe (GUPnPService *service,
           SoupMessage  *msg,
           const char   *callback,
           const char   *timeout)
{
        SubscriptionData *data;
        char *start, *end, *uri;
        int timeout_seconds;
        GQueue *queue;
        xmlChar *mem;
        GList *l;

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
        timeout_seconds = parse_timeout (timeout);
        if (timeout_seconds > 0) {
                data->timeout_id = g_timeout_add (timeout_seconds * 1000,
                                                  subscription_timeout,
                                                  data);
        }

        /* Add to hash */
        g_hash_table_insert (service->priv->subscriptions,
                             data->sid,
                             data);

        /* Respond */
        subscription_response (service, msg, data->sid, timeout_seconds);

        /* Send initial event message */
        queue = g_queue_new ();

        for (l = service->priv->state_variables; l; l = l->next) {      
                NotifyData *ndata;

                ndata = g_slice_new0 (NotifyData);

                g_signal_emit (service,
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

        xmlFree (mem);
}

/* Resubscription request */
static void
resubscribe (GUPnPService *service,
             SoupMessage  *msg,
             const char   *sid,
             const char   *timeout)
{
        SubscriptionData *data;
        int timeout_seconds;

        data = g_hash_table_lookup (service->priv->subscriptions, sid);
        if (!data) {
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        /* Update timeout */
        if (data->timeout_id) {
                g_source_remove (data->timeout_id);
                data->timeout_id = 0;
        }

        timeout_seconds = parse_timeout (timeout);
        if (timeout_seconds > 0) {
                data->timeout_id = g_timeout_add (timeout_seconds * 1000,
                                                  subscription_timeout,
                                                  data);
        }

        /* Respond */
        subscription_response (service, msg, sid, timeout_seconds);
}

/* Unsubscription request */
static void
unsubscribe (GUPnPService *service,
             SoupMessage  *msg,
             const char   *sid)
{
        if (g_hash_table_remove (service->priv->subscriptions, sid))
                soup_message_set_status (msg, SOUP_STATUS_OK);
        else
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);
}

/* eventSubscriptionURL handler */
static void
subscription_server_handler (SoupServerContext *server_context,
                             SoupMessage       *msg,
                             gpointer           user_data)
{
        GUPnPService *service;
        const char *callback, *nt, *timeout, *sid;

        service = GUPNP_SERVICE (user_data);

        callback = soup_message_get_header (msg->request_headers, "Callback");
        nt       = soup_message_get_header (msg->request_headers, "NT");
        timeout  = soup_message_get_header (msg->request_headers, "Timeout");
        sid      = soup_message_get_header (msg->request_headers, "SID");

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
                                subscribe (service, msg, callback, timeout);

                        }

                } else if (sid) {
                        if (nt) {
                                soup_message_set_status
                                        (msg, SOUP_STATUS_BAD_REQUEST);

                        } else {
                                resubscribe (service, msg, sid, timeout);

                        }

                } else {
                        soup_message_set_status
                                (msg, SOUP_STATUS_BAD_REQUEST);

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

static GObject *
gupnp_service_constructor (GType                  type,
                           guint                  n_construct_params,
                           GObjectConstructParam *construct_params)
{
        GObjectClass *object_class;
        GObject *object;
        GUPnPService *service;
        GUPnPServiceInfo *info;
        GError *error;
        GUPnPServiceIntrospection *introspection;
        const GList *state_variables, *l;
        GUPnPContext *context;
        SoupServer *server;
        SoupUri *uri;
        char *url;

        object_class = G_OBJECT_CLASS (gupnp_service_parent_class);

        /* Construct */
        object = object_class->constructor (type,
                                            n_construct_params,
                                            construct_params);

        service = GUPNP_SERVICE (object);
        info    = GUPNP_SERVICE_INFO (object);

        /* Get introspection and save state variable names */
        error = NULL;

        introspection = gupnp_service_info_get_introspection (info, &error);
        if (introspection) {
                state_variables =
                        gupnp_service_introspection_list_state_variable_names
                                                                (introspection);

                for (l = state_variables; l; l = l->next) {
                        service->priv->state_variables =
                                g_list_prepend (service->priv->state_variables,
                                                g_strdup (l->data));
                }

                g_object_unref (introspection);
        } else {
                g_warning ("Failed to get SCPD: %s\n"
                           "The initial event message will not be sent.",
                           error->message);

                g_error_free (error);
        }

        /* Get server */
        context = gupnp_service_info_get_context (info);
        server = _gupnp_context_get_server (context);

        /* Run listener on controlURL */
        url = gupnp_service_info_get_control_url (info);
        uri = soup_uri_new (url);
        g_free (url);

        url = soup_uri_to_string (uri, TRUE);
        soup_uri_free (uri);

        soup_server_add_handler (server, url, NULL,
                                 control_server_handler, NULL, object);
        g_free (url);

        /* Run listener on eventSubscriptionURL */
        url = gupnp_service_info_get_event_subscription_url (info);
        uri = soup_uri_new (url);
        g_free (url);

        url = soup_uri_to_string (uri, TRUE);
        soup_uri_free (uri);

        soup_server_add_handler (server, url, NULL,
                                 subscription_server_handler, NULL, object);
        g_free (url);

        return object;
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

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_service_parent_class);
        object_class->finalize (object);
}

static void
gupnp_service_class_init (GUPnPServiceClass *klass)
{
        GObjectClass *object_class;
        GUPnPServiceInfoClass *info_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = gupnp_service_constructor;
        object_class->finalize    = gupnp_service_finalize;

        info_class = GUPNP_SERVICE_INFO_CLASS (klass);
        
        g_type_class_add_private (klass, sizeof (GUPnPServicePrivate));

        /**
         * GUPnPService::action-invoked
         * @service: The #GUPnPService that received the signal
         * @action: The invoked #GUPnPAction
         *
         * Emitted whenever an action is invoked. Handler should process
         * @action.
         **/
        signals[ACTION_INVOKED] =
                g_signal_new ("action-invoked",
                              GUPNP_TYPE_SERVICE,
                              G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                              G_STRUCT_OFFSET (GUPnPServiceClass,
                                               action_invoked),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_POINTER);

        /**
         * GUPnPService::query-variable
         * @service: The #GUPnPService that received the signal
         * @variable: The variable that is being queried
         * @value: The location of the #GValue of the variable
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
                              G_TYPE_POINTER);

        /**
         * GUPnPService::notify-failed
         * @service: The #GUPnPService that received the signal
         * @callback_url: The callback URL
         * @reason: A pointer to a #GError describing why the notify failed
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
 * gupnp_service_notify
 * @service: A #GUPnPService
 * @Varargs: Tuples of variable name, variable type, and variable value,
 * terminated with NULL.
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
 * gupnp_service_notify_valist
 * @service: A #GUPnPService
 * @var_args: A va_list of tuples of variable name, variable type, and variable
 * value, terminated with NULL.
 *
 * See gupnp_service_notify_valist(); this version takes a va_list for
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

                G_VALUE_COLLECT (&value, var_args, 0, &collect_error);
                if (collect_error) {
                        g_warning ("Error collecting value: %s\n",
                                   collect_error);

                        g_free (collect_error);

                        g_value_unset (&value);

                        return;
                }

                gupnp_service_notify_value (service, var_name, &value);

                g_value_unset (&value);

                var_name = va_arg (var_args, const char *);
        }
}

/* Receiveid notify response. */
static void
notify_got_response (SoupMessage *msg,
                     gpointer     user_data)
{
        SubscriptionData *data;

        data = user_data;

        if (SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
                /* Success: reset callbacks pointer */
                data->callbacks = g_list_first (data->callbacks);
        } else {
                /* Notify failed */
                if (data->callbacks->next) {
                        SoupUri *uri;
                        GUPnPContext *context;
                        SoupSession *session;

                        /* Call next callback */
                        data->callbacks = data->callbacks->next;

                        uri = soup_uri_new (data->callbacks->data);
                        soup_message_set_uri (msg, uri);
                        soup_uri_free (uri);

                        /* And re-queue */
                        context = gupnp_service_info_get_context
                                        (GUPNP_SERVICE_INFO (data->service));
                        session = _gupnp_context_get_session (context);

                        soup_session_requeue_message (session, msg);
                } else {
                        /* Emit 'notify-failed' signal */
                        GError *error;

                        error = g_error_new (GUPNP_EVENTING_ERROR,
                                             GUPNP_EVENTING_ERROR_NOTIFY_FAILED,
                                             "Notify failed");

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
notify_subscriber (gpointer key,
                   gpointer value,
                   gpointer user_data)
{
        SubscriptionData *data;
        const char *property_set;
        char *tmp;
        SoupMessage *msg;
        GUPnPContext *context;
        SoupSession *session;

        data = value;
        property_set = user_data;

        /* Create message */
        msg = soup_message_new (GENA_METHOD_NOTIFY, data->callbacks->data);
        if (!msg) {
                g_warning ("Invalid URL: %s", (char *) data->callbacks->data);

                return;
        }

        /* Add headers */
        soup_message_add_header (msg->request_headers,
                                 "Content-Type",
                                 "text/xml");

        soup_message_add_header (msg->request_headers,
                                 "NT",
                                 "upnp:event");

        soup_message_add_header (msg->request_headers,
                                 "NTS",
                                 "upnp:propchange");

        soup_message_add_header (msg->request_headers,
                                 "SID",
                                 data->sid);

        tmp = g_strdup_printf ("%d", data->seq);
        soup_message_add_header (msg->request_headers,
                                 "SEQ",
                                 tmp);
        g_free (tmp);

        /* Handle overflow */
        if (data->seq < G_MAXINT32)
                data->seq++;
        else
                data->seq = 1;

        /* Add body */
        msg->request.owner  = SOUP_BUFFER_SYSTEM_OWNED;
        msg->request.body   = g_strdup (property_set);
        msg->request.length = strlen (property_set);

        /* Queue */
        context = gupnp_service_info_get_context
                        (GUPNP_SERVICE_INFO (data->service));
        session = _gupnp_context_get_session (context);
        soup_session_queue_message (session,
                                    msg,
                                    notify_got_response,
                                    data);
}

/* Create a property set from @queue */
static xmlChar *
create_property_set (GQueue *queue)
{
        GValue transformed_value = {0, };
        NotifyData *data;
        xmlDoc *doc;
        xmlNode *node;
        xmlNs *ns;
        xmlChar *mem;
        int size;

        /* Compose property set */
        doc = xmlNewDoc ((const xmlChar *) "1.0");
        node = xmlNewDocNode (doc,
                              NULL,
                              (const xmlChar *) "propertyset",
                              NULL);
        xmlDocSetRootElement (doc, node);

        ns = xmlNewNs (node,
                       (const xmlChar *) "urn:schemas-upnp-org:event-1-0",
                       (const xmlChar *) "e");
        xmlSetNs (node, ns);

        node = xmlNewTextChild (node,
                                ns,
                                (const xmlChar *) "property",
                                NULL);

        /* Add variables */
        while ((data = g_queue_pop_head (queue))) {
                g_value_init (&transformed_value, G_TYPE_STRING);
                if (g_value_transform (&data->value, &transformed_value)) {
                        /* Add to property set */
                        xmlNewTextChild
                                (node,
                                 NULL,
                                 (const xmlChar *) data->variable,
                                 (xmlChar *) g_value_get_string
                                                    (&transformed_value));
                } else {
                        g_warning ("Failed to transform value of type %s to "
                                   "string.", G_VALUE_TYPE_NAME (&data->value));
                }

                /* Cleanup */
                g_value_unset (&transformed_value);

                notify_data_free (data);
        }

        /* Dump document */
        xmlDocDumpMemory (doc, &mem, &size);

        /* Cleanup */
        xmlFreeDoc (doc);

        /* Return */
        return mem;
}

/* Flush all queued notifications */
static void
flush_notifications (GUPnPService *service)
{
        xmlChar *mem;

        /* Create property set */
        mem = create_property_set (service->priv->notify_queue);

        /* And send it off */
        g_hash_table_foreach (service->priv->subscriptions,
                              notify_subscriber,
                              mem);

        /* Cleanup */
        xmlFree (mem);
}

/**
 * gupnp_service_notify_value
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
        g_return_if_fail (GUPNP_IS_SERVICE (service));
        g_return_if_fail (variable != NULL);
        g_return_if_fail (G_IS_VALUE (value));

        /* Queue */
        NotifyData *data;

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
 * gupnp_service_freeze_notify
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
 * gupnp_service_thaw_notify
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

/**
 * _gupnp_service_new
 * @context: A #GUPnPContext
 * @element: The #xmlNode ponting to the right service element
 * @location: The location of the service description file
 * @udn: The UDN of the device the service is contained in
 * @url_base: The URL base for this service
 *
 * Return value: A #GUPnPService for the service with element @element, as
 * read from the service description file specified by @location.
 **/
GUPnPService *
_gupnp_service_new (GUPnPContext *context,
                    xmlNode      *element,
                    const char   *udn,
                    const char   *location,
                    SoupUri      *url_base)
{
        GUPnPService *service;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (element != NULL, NULL);
        g_return_val_if_fail (location != NULL, NULL);
        g_return_val_if_fail (url_base != NULL, NULL);

        service = g_object_new (GUPNP_TYPE_SERVICE,
                                "context", context,
                                "location", location,
                                "udn", udn,
                                "url-base", url_base,
                                "element", element,
                                NULL);

        return service;
}
