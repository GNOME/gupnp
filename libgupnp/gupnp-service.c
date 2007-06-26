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
 * #GUPnPService allows for implementing services. #GUPnPService implements
 * the #GUPnPServiceInfo interface.
 */

#include <libsoup/soup-date.h>
#include <string.h>

#include "gupnp-service.h"
#include "gupnp-service-private.h"
#include "gupnp-context-private.h"
#include "gupnp-marshal.h"
#include "accept-language.h"
#include "xml-util.h"

G_DEFINE_TYPE (GUPnPService,
               gupnp_service,
               GUPNP_TYPE_SERVICE_INFO);

struct _GUPnPServicePrivate {
};

enum {
        ACTION_INVOKED,
        QUERY_VARIABLE,
        NOTIFY_FAILED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _GUPnPServiceAction {
        const char  *name;

        SoupMessage *msg;

        xmlNode     *action_node;
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
 * @Varargs: tuples of argument name, argument type, and argument value, 
 * terminated with NULL.
 *
 * Retrieves the specified action arguments.
 **/
void
gupnp_service_action_get (GUPnPServiceAction *action,
                          ...)
{
        g_return_if_fail (action != NULL);
}

/**
 * gupnp_service_action_get_valist
 * @action: A #GUPnPServiceAction
 * @var_args: va_list of tuples of argument name, argument type, and argument
 * value.
 *
 * See gupnp_service_action_get(); this version takes a va_list for
 * use by language bindings.
 **/
void
gupnp_service_action_get_valist (GUPnPServiceAction *action,
                                 va_list             var_args)
{
        g_return_if_fail (action != NULL);
}

/**
 * gupnp_service_action_get_value
 * @action: A #GUPnPServiceAction
 * @argument: The name of the argument to retrieve
 * @value: The #GValue to store the value of the argument
 *
 * Retrieves the value of @argument into @value.
 **/
void
gupnp_service_action_get_value (GUPnPServiceAction *action,
                                const char         *argument,
                                GValue             *value)
{
        g_return_if_fail (action != NULL);
        g_return_if_fail (argument != NULL);
        g_return_if_fail (value != NULL);
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
        g_return_if_fail (action != NULL);
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
        g_return_if_fail (action != NULL);
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
        g_return_if_fail (action != NULL);
        g_return_if_fail (argument != NULL);
        g_return_if_fail (value != NULL);
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
 * @error: The #GError
 *
 * Return @error.
 **/
void
gupnp_service_action_return_error (GUPnPServiceAction *action,
                                   GError             *error)
{
        g_return_if_fail (action != NULL);
        g_return_if_fail (error != NULL);

        soup_message_set_status (action->msg,
                                 SOUP_STATUS_INTERNAL_SERVER_ERROR);
}

static void
gupnp_service_init (GUPnPService *proxy)
{
        proxy->priv = G_TYPE_INSTANCE_GET_PRIVATE (proxy,
                                                   GUPNP_TYPE_SERVICE,
                                                   GUPnPServicePrivate);
}

/* Handle QueryStateVariable action */
static void
query_state_variable (GUPnPService *service,
                      SoupMessage  *msg,
                      xmlNode      *action_node)
{
        xmlDoc *response_doc;
        xmlNode *node, *response_node;
        char *service_type;
        xmlNs *ns;
        xmlChar *mem;
        int size;

        /* Set up response */
        response_doc = xmlNewDoc ((const xmlChar *) "1.0");
        response_node = xmlNewDocNode (response_doc,
                                       NULL,
                                       (const xmlChar *) "Envelope",
                                       NULL);
        ns = xmlNewNs (response_node,
                       (const xmlChar *)
                               "http://schemas.xmlsoap.org/soap/envelope/",
                       (const xmlChar *) "s");
        xmlSetNs (response_node, ns);

        response_node = xmlNewChild (response_node,
                                     ns,
                                     (const xmlChar *) "Body",
                                     NULL);

        response_node = xmlNewChild (response_node,
                                     NULL,
                                     (const xmlChar *)
                                             "QueryStateVariableResponse",
                                     NULL);

        service_type = gupnp_service_info_get_service_type
                                        (GUPNP_SERVICE_INFO (service));
        ns = xmlNewNs (response_node,
                       (xmlChar *) service_type, (const xmlChar *) "u");
        g_free (service_type);

        /* Iterate requested variables */
        for (node = action_node->children; node; node = node->next) {
                xmlChar *var_name;
                GValue value = {0,}, transformed_value = {0, };

                if (strcmp ((char *) node->name, "varName") != 0)
                        continue;

                /* varName */
                var_name = xmlNodeGetContent (node);
                if (!var_name) {
                        g_warning ("No variable name specified.");
                        continue;
                }

                /* Query variable */
                g_signal_emit (service,
                               signals[QUERY_VARIABLE],
                               g_quark_from_string ((char *) var_name),
                               (char *) var_name,
                               &value);

                if (!G_IS_VALUE (&value)) {
                        g_warning ("Failed to query variable \"%s\"\n",
                                   var_name);
                        xmlFree (var_name);
                        continue;
                }

                /* Transform to string */
                g_value_init (&transformed_value, G_TYPE_STRING);
                if (g_value_transform (&value, &transformed_value)) {
                        /* Append to response */
                        xmlNewChild (response_node,
                                     NULL,
                                     var_name,
                                     (xmlChar *)
                                     g_value_get_string (&transformed_value));
                } else
                        g_warning ("Failed to transform value to string");

                /* Cleanup */
                g_value_unset (&value);
                g_value_unset (&transformed_value);

                xmlFree (var_name);
        }

        /* Dump doc into response */
        xmlDocDumpMemory (response_doc, &mem, &size);

        msg->response.body   = (char *) mem;
        msg->response.length = size;

        /* Cleanup */
        xmlFreeDoc (response_doc);
        xmlFree (mem);

        /* OK */
        soup_message_set_status (msg, SOUP_STATUS_OK);
}

static void
control_server_handler (SoupServerContext *server_context,
                        SoupMessage       *msg,
                        gpointer           user_data)
{
        GUPnPService *service;
        GUPnPContext *context;
        GSSDPClient *client;
        xmlDoc *doc;
        xmlNode *action_node;
        const char *soap_action, *action_name;
        char *end, *date;

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

        /* QueryStateVariable? */
        if (strcmp (action_name, "QueryStateVariable") == 0)
                query_state_variable (service, msg, action_node);
        else {
                GUPnPServiceAction *action;

                /* Create action structure */
                action = g_slice_new (GUPnPServiceAction);

                action->name        = action_name;
                action->msg         = msg;
                action->action_node = action_node;

                /* Emit signal. Handler parses request and fills in response. */
                g_signal_emit (service,
                               signals[ACTION_INVOKED],
                               g_quark_from_string (action_name),
                               action);

                /* Cleanup */
                g_slice_free (GUPnPServiceAction, action);
        }

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

static void
subscription_server_handler (SoupServerContext *server_context,
                             SoupMessage       *msg,
                             gpointer           user_data)
{
        GUPnPService *service;

        service = GUPNP_SERVICE (user_data);
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
        SoupUri *uri;
        char *url;

        object_class = G_OBJECT_CLASS (gupnp_service_parent_class);

        /* Construct */
        object = object_class->constructor (type,
                                            n_construct_params,
                                            construct_params);
        info = GUPNP_SERVICE_INFO (object);

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
gupnp_service_dispose (GObject *object)
{
        GUPnPService *proxy;
        GObjectClass *object_class;

        proxy = GUPNP_SERVICE (object);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_service_parent_class);
        object_class->dispose (object);
}

static void
gupnp_service_finalize (GObject *object)
{
        GUPnPService *proxy;
        GObjectClass *object_class;

        proxy = GUPNP_SERVICE (object);

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
        object_class->dispose     = gupnp_service_dispose;
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
         * @notify_url: The notification URL
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
                              gupnp_marshal_VOID__STRING_POINTER,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_STRING,
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
        g_return_if_fail (GUPNP_IS_SERVICE (service));
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
        g_return_if_fail (GUPNP_IS_SERVICE (service));
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
}

/**
 * _gupnp_service_new_from_element
 * @context: A #GUPnPContext
 * @element: The #xmlNode ponting to the right service element
 * @location: The location of the service description file
 * @udn: The UDN of the device the service is contained in
 * @url_base: The URL base for this service, or NULL if none
 *
 * Return value: A #GUPnPService for the service with element @element, as
 * read from the service description file specified by @location.
 **/
GUPnPService *
_gupnp_service_new_from_element (GUPnPContext *context,
                                 xmlNode      *element,
                                 const char   *udn,
                                 const char   *location,
                                 const char   *url_base)
{
        GUPnPService *service;

        g_return_val_if_fail (element, NULL);

        service = g_object_new (GUPNP_TYPE_SERVICE,
                                "context", context,
                                "location", location,
                                "udn", udn,
                                "url-base", url_base,
                                "element", element,
                                NULL);

        return service;
}

/* o Handle event subscription
 * 
 * o Implement notification
 *
 * o Implement action stuff
 */
