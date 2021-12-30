/*
 * Copyright (C) 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

/**
 * SECTION:gupnp-service
 * @short_description: Class for service implementations.
 *
 * #GUPnPService allows for handling incoming actions and state variable
 * notification. #GUPnPService implements the #GUPnPServiceInfo interface.
 */

#include <config.h>

#include <gobject/gvaluecollector.h>
#include <gmodule.h>
#include <string.h>

#include "gupnp-service.h"
#include "gupnp-root-device.h"
#include "gupnp-context-private.h"
#include "gupnp-error.h"
#include "gupnp-acl.h"
#include "gupnp-uuid.h"
#include "gupnp-service-private.h"
#include "http-headers.h"
#include "gena-protocol.h"
#include "xml-util.h"
#include "gvalue-util.h"

#include "guul.h"

#define SUBSCRIPTION_TIMEOUT 300 /* DLNA (7.2.22.1) enforced */


struct _GUPnPServicePrivate {
        GUPnPRootDevice           *root_device;

        SoupSession               *session;

        guint                      notify_available_id;

        GHashTable                *subscriptions;

        GList                     *state_variables;

        GQueue                    *notify_queue;

        gboolean                   notify_frozen;

        GUPnPServiceIntrospection *introspection;

        GList                     *pending_autoconnect;
};
typedef struct _GUPnPServicePrivate GUPnPServicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GUPnPService,
                            gupnp_service,
                            GUPNP_TYPE_SERVICE_INFO)

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

static GBytes *
create_property_set (GQueue *queue);

static void
notify_subscriber   (gpointer key,
                     gpointer value,
                     gpointer user_data);

GUPnPServiceAction *
gupnp_service_action_new ();

GUPnPServiceAction *
gupnp_service_action_ref (GUPnPServiceAction *action);

void
gupnp_service_action_unref (GUPnPServiceAction *action);

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
        GCancellable *cancellable;
} SubscriptionData;

typedef struct {
        SubscriptionData *data;
        SoupMessage *msg;
        GBytes *property_set;
} NotifySubscriberData;

static gboolean
subscription_data_can_delete (SubscriptionData *data) {
    return data->initial_state_sent && data->to_delete;
}


static void
send_initial_state (SubscriptionData *data);

static void
gupnp_service_remove_subscription (GUPnPService *service,
                                   const char *sid)
{
        GUPnPServicePrivate *priv;

        priv = gupnp_service_get_instance_private (service);

        g_hash_table_remove (priv->subscriptions, sid);
}

static SoupSession *
gupnp_service_get_session (GUPnPService *service)
{
        GUPnPServicePrivate *priv;

        priv = gupnp_service_get_instance_private (service);
        if (!priv->session) {
                /* Create a dedicated session for this service to
                 * ensure that notifications are sent in the proper
                 * order. The session from GUPnPContext may use
                 * multiple connections.
                 */
                priv->session =
                        soup_session_new_with_options ("max-conns-per-host",
                                                       1,
                                                       NULL);

                if (g_getenv ("GUPNP_DEBUG")) {
                        SoupLogger *logger;
                        logger = soup_logger_new (SOUP_LOGGER_LOG_BODY);
                        soup_session_add_feature (priv->session,
                                                  SOUP_SESSION_FEATURE (logger));
                }
        }

        return priv->session;
}

static void
subscription_data_free (SubscriptionData *data)
{
        g_cancellable_cancel (data->cancellable);
        g_clear_object (&data->cancellable);

        // session = gupnp_service_get_session (data->service);

        /* Cancel pending messages */
        while (data->pending_messages) {
                NotifySubscriberData *d = data->pending_messages->data;
                g_object_unref (d->msg);
                g_bytes_unref (d->property_set);
                g_free (d);

                data->pending_messages =
                        g_list_delete_link (data->pending_messages,
                                            data->pending_messages);
        }
       
        /* Further cleanup */
        g_list_free_full (data->callbacks, (GDestroyNotify) g_uri_unref);

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

static void
gupnp_service_init (GUPnPService *service)
{
        GUPnPServicePrivate *priv;

        priv = gupnp_service_get_instance_private (service);

        priv->subscriptions =
                g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       NULL,
                                       (GDestroyNotify) subscription_data_free);

        priv->notify_queue = g_queue_new ();
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
                g_string_append_c (str, '"');
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
control_server_handler (SoupServer *server,
                        SoupServerMessage *msg,
                        G_GNUC_UNUSED const char *server_path,
                        G_GNUC_UNUSED GHashTable *query,
                        gpointer user_data)
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

        if (soup_server_message_get_method (msg) != SOUP_METHOD_POST) {
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_NOT_IMPLEMENTED,
                                                "Not implemented");

                return;
        }

        SoupMessageBody *request_body =
                soup_server_message_get_request_body (msg);
        SoupMessageHeaders *request_headers =
                soup_server_message_get_request_headers (msg);
        SoupMessageHeaders *response_headers =
                soup_server_message_get_response_headers (msg);

        if (request_body->length == 0) {
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_BAD_REQUEST,
                                                "Bad request");

                return;
        }

        /* DLNA 7.2.5.6: Always use HTTP 1.1 */
        if (soup_server_message_get_http_version (msg) == SOUP_HTTP_1_0) {
                soup_server_message_set_http_version (msg, SOUP_HTTP_1_1);
                soup_message_headers_append (response_headers,
                                             "Connection",
                                             "close");
        }

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (service));

        const char *host_header =
                soup_message_headers_get_one (request_headers, "Host");

        if (!gupnp_context_validate_host_header (context, host_header)) {
                g_warning ("Host header mismatch, expected %s:%d, got %s",
                           gssdp_client_get_host_ip (GSSDP_CLIENT (context)),
                           gupnp_context_get_port (context),
                           host_header);

                soup_server_message_set_status (msg,
                                                SOUP_STATUS_PRECONDITION_FAILED,
                                                "Host header mismatch");

                return;
        }

        /* Get action name */
        soap_action =
                soup_message_headers_get_one (request_headers, "SOAPAction");
        if (!soap_action) {
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_PRECONDITION_FAILED,
                                                "No SOAPAction header");

                return;
        }

        action_name = strchr (soap_action, '#');
        if (!action_name) {
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_PRECONDITION_FAILED,
                                                "No action name");

                return;
        }

        /* This memory is libsoup-owned so we can do this */
        *action_name = '\0';
        action_name += 1;

        if (*soap_action == '"')
                soap_action += 1;

        /* This memory is libsoup-owned so we can do this */
        end = strrchr (action_name, '"');
        if (end)
                *end = '\0';

        /* Parse action_node */
        doc = xmlRecoverMemory (request_body->data, request_body->length);
        if (doc == NULL) {
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_BAD_REQUEST,
                                                "Unable to parse action");

                return;
        }

        action_node = xml_util_get_element ((xmlNode *) doc,
                                            "Envelope",
                                            "Body",
                                            action_name,
                                            NULL);
        if (!action_node) {
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_PRECONDITION_FAILED,
                                                "Missing <action>");

                return;
        }

        /* Create action structure */
        action                 = gupnp_service_action_new ();
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
        accept_encoding = soup_message_headers_get_list (request_headers,
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
                       SoupServerMessage *msg,
                       const char *sid,
                       int timeout)
{
        GUPnPContext *context;
        GSSDPClient *client;
        char *tmp;

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (service));
        client = GSSDP_CLIENT (context);

        SoupMessageHeaders *response_headers =
                soup_server_message_get_response_headers (msg);

        /* Server header on response */
        soup_message_headers_append (response_headers,
                                     "Server",
                                     gssdp_client_get_server_id (client));

        /* SID header */
        soup_message_headers_append (response_headers, "SID", sid);

        /* Timeout header */
        if (timeout > 0)
                tmp = g_strdup_printf ("Second-%d", timeout);
        else
                tmp = g_strdup ("infinite");

        soup_message_headers_append (response_headers, "Timeout", tmp);
        g_free (tmp);

        /* 200 OK */
        soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
}

/**
 * gupnp_get_uuid:
 *
 * Generate and return a new UUID.
 *
 * Returns: (transfer full): A newly generated UUID in string representation.
 */
char *
gupnp_get_uuid (void)
{
        return guul_get_uuid ();
}

/* Generates a new SID */
static char *
generate_sid (void)
{
        char *ret = NULL;
        char *uuid;


        uuid = guul_get_uuid ();
        ret = g_strconcat ("uuid:", uuid, NULL);
        g_free (uuid);

        return ret;
}

/* Subscription expired */
static gboolean
subscription_timeout (gpointer user_data)
{
        SubscriptionData *data;

        data = user_data;

        gupnp_service_remove_subscription (data->service, data->sid);

        return FALSE;
}

static void
send_initial_state (SubscriptionData *data)
{
        GQueue *queue;
        GList *l;
        GUPnPServicePrivate *priv;

        priv = gupnp_service_get_instance_private (data->service);

        /* Send initial event message */
        queue = g_queue_new ();

        for (l = priv->state_variables; l; l = l->next) {
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

        GBytes *property_set = create_property_set (queue);
        notify_subscriber (data->sid, data, property_set);

        /* Cleanup */
        g_queue_free (queue);

        g_bytes_unref (property_set);
}

static GList *
add_subscription_callback (GUPnPContext *context,
                           GList *list,
                           const char *callback)
{
        GUri *local_uri = NULL;

        local_uri = gupnp_context_rewrite_uri_to_uri (context, callback);
        if (local_uri == NULL) {
                return list;
            }

            const char *host = g_uri_get_host (local_uri);
            GSocketAddress *address = g_inet_socket_address_new_from_string (host, 0);

            // CVE-2020-12695: Ignore subscription call-backs that are not "in
            // our network segment"
            if (gssdp_client_can_reach (GSSDP_CLIENT (context), G_INET_SOCKET_ADDRESS (address))) {
                    list = g_list_append (list, local_uri);
            } else {
                    g_warning ("%s is not in our network; ignoring", callback);
            }
            g_object_unref (address);

            return list;
}

/* Subscription request */
static void
subscribe (GUPnPService *service, SoupServerMessage *msg, const char *callback)
{
        SubscriptionData *data;
        char *start, *end;
        GUPnPServicePrivate *priv;
        GUPnPContext *context;
        int callbacks = 0;

        priv = gupnp_service_get_instance_private (service);
        context = gupnp_service_info_get_context
                                        (GUPNP_SERVICE_INFO (service));

        data = g_slice_new0 (SubscriptionData);
        data->cancellable = g_cancellable_new ();

        /* Parse callback list */
        start = (char *) callback;

        // Arbitrarily limit the list of callbacks to 6
        // Part of CVE-2020-12695 mitigation
        while (callbacks < 6 && (start = strchr (start, '<'))) {
                start += 1;
                if (!start || !*start)
                        break;

                end = strchr (start, '>');
                if (!end || !*end)
                        break;

                *end = '\0';
                if (g_str_has_prefix (start, "http://")) {
                        // DLNA 7.3.2.24.4 - URIs shall not exceed 256 bytes
                        // Also one part of CVE-2020-12695 mitigation - limit URI length
                        // UPnP does not impose any restrictions here
                        if (strlen (start) <= 256) {
                                data->callbacks = add_subscription_callback (context,
                                                                             data->callbacks,
                                                                             start);
                        } else {
                                g_warning ("Subscription URI exceeds recommended length "
                                           "of 256 bytes, skipping");
                        }
                }
                *end = '>';

                start = end;
                callbacks++;
        }

        if (!data->callbacks) {
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_PRECONDITION_FAILED,
                                                "No valid callbacks found");

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
        g_hash_table_insert (priv->subscriptions,
                             data->sid,
                             data);

        /* Respond */
        subscription_response (service, msg, data->sid, SUBSCRIPTION_TIMEOUT);

        // FIXME: Should we only send this if we priv->inspection is not NULL?
        // There might not be any useful data in the notification if there is no
        // introspection yet
        send_initial_state (data);
}

/* Resubscription request */
static void
resubscribe (GUPnPService *service, SoupServerMessage *msg, const char *sid)
{
        SubscriptionData *data;
        GUPnPServicePrivate *priv;

        priv = gupnp_service_get_instance_private (service);

        data = g_hash_table_lookup (priv->subscriptions, sid);
        if (!data) {
                soup_server_message_set_status (
                        msg,
                        SOUP_STATUS_PRECONDITION_FAILED,
                        "No previous subscription found");

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
unsubscribe (GUPnPService *service, SoupServerMessage *msg, const char *sid)
{
        SubscriptionData *data;
        GUPnPServicePrivate *priv;

        priv = gupnp_service_get_instance_private (service);

        data = g_hash_table_lookup (priv->subscriptions, sid);
        if (data) {
                if (data->initial_state_sent)
                        g_hash_table_remove (priv->subscriptions,
                                             sid);
                else
                        data->to_delete = TRUE;
                soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
        } else
                soup_server_message_set_status (
                        msg,
                        SOUP_STATUS_PRECONDITION_FAILED,
                        "No previous subscription found");
}

/* eventSubscriptionURL handler */
static void
subscription_server_handler (G_GNUC_UNUSED SoupServer *server,
                             SoupServerMessage *msg,
                             G_GNUC_UNUSED const char *server_path,
                             G_GNUC_UNUSED GHashTable *query,
                             gpointer user_data)
{
        GUPnPService *service;
        const char *callback, *nt, *sid;

        service = GUPNP_SERVICE (user_data);

        SoupMessageHeaders *request_headers =
                soup_server_message_get_request_headers (msg);

        g_print ("Got SUBSCRIBE handler request\n");

        const char *host =
                soup_message_headers_get_one (request_headers, "Host");
        GUPnPContext *context = gupnp_service_info_get_context (user_data);
        if (!gupnp_context_validate_host_header(context, host)) {
                g_warning ("Host header mismatch, expected %s:%d, got %s",
                           gssdp_client_get_host_ip (GSSDP_CLIENT (context)),
                           gupnp_context_get_port (context),
                           host);

                soup_server_message_set_status (msg,
                                                SOUP_STATUS_BAD_REQUEST,
                                                NULL);

                return;
        }

        callback = soup_message_headers_get_one (request_headers, "Callback");
        nt = soup_message_headers_get_one (request_headers, "NT");
        sid = soup_message_headers_get_one (request_headers, "SID");
        const char *method = soup_server_message_get_method (msg);

        /* Choose appropriate handler */
        if (strcmp (method, GENA_METHOD_SUBSCRIBE) == 0) {
                if (callback) {
                        if (sid) {
                                soup_server_message_set_status (
                                        msg,
                                        SOUP_STATUS_BAD_REQUEST,
                                        "SID must not be given on SUBSCRIBE");

                        } else if (!nt || strcmp (nt, "upnp:event") != 0) {
                                soup_server_message_set_status (
                                        msg,
                                        SOUP_STATUS_PRECONDITION_FAILED,
                                        "NT header missing or malformed");

                        } else {
                                subscribe (service, msg, callback);

                        }

                } else if (sid) {
                        if (nt) {
                                soup_server_message_set_status (
                                        msg,
                                        SOUP_STATUS_BAD_REQUEST,
                                        "NT must not be given on RESUBSCRIBE");

                        } else {
                                resubscribe (service, msg, sid);

                        }

                } else {
                        soup_server_message_set_status (
                                msg,
                                SOUP_STATUS_PRECONDITION_FAILED,
                                NULL);
                }

        } else if (strcmp (method, GENA_METHOD_UNSUBSCRIBE) == 0) {
                if (sid) {
                        if (nt || callback) {
                                soup_server_message_set_status (
                                        msg,
                                        SOUP_STATUS_BAD_REQUEST,
                                        NULL);

                        } else {
                                unsubscribe (service, msg, sid);

                        }

                } else {
                        soup_server_message_set_status (
                                msg,
                                SOUP_STATUS_PRECONDITION_FAILED,
                                NULL);
                }

        } else {
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_NOT_IMPLEMENTED,
                                                NULL);
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
        GUPnPServicePrivate *priv;

        priv = gupnp_service_get_instance_private (service);

        if (introspection) {
                /* Handle pending auto-connects */
                priv->introspection  = g_object_ref (introspection);

                /* _autoconnect() just calls prepend() so we reverse the list
                 * here.
                 */
                priv->pending_autoconnect =
                            g_list_reverse (priv->pending_autoconnect);

                /* Re-call _autoconnect(). This will not fill
                 * pending_autoconnect because we set the introspection member
                 * variable before */
                for (l = priv->pending_autoconnect; l; l = l->next)
                        gupnp_service_signals_autoconnect (service,
                                                           l->data,
                                                           NULL);

                g_list_free (priv->pending_autoconnect);
                priv->pending_autoconnect = NULL;

                state_variables =
                        gupnp_service_introspection_list_state_variables
                                (introspection);

                for (l = state_variables; l; l = l->next) {
                        GUPnPServiceStateVariableInfo *variable;

                        variable = l->data;

                        if (!variable->send_events)
                                continue;

                        priv->state_variables =
                                g_list_prepend (priv->state_variables,
                                                g_strdup (variable->name));
                }

                g_object_unref (introspection);
        } else
                g_warning ("Failed to get SCPD: %s\n"
                           "The initial event message will not be sent.",
                           error ? error->message : "No error");

        g_hash_table_iter_init (&iter, priv->subscriptions);

        while (g_hash_table_iter_next (&iter, NULL, &data)) {
                send_initial_state ((SubscriptionData *) data);
                if (subscription_data_can_delete ((SubscriptionData *) data))
                        g_hash_table_iter_remove (&iter);
        }
}

static char *
path_from_url (const char *url)
{
        gchar   *path;
        const char *query = NULL;

        GUri *uri = g_uri_parse (url, G_URI_FLAGS_NONE, NULL);

        query = g_uri_get_query (uri);
        if (query == NULL) {
                path = g_strdup (g_uri_get_path (uri));
        } else {
                path = g_strdup_printf ("%s?%s", g_uri_get_path (uri), query);
        }
        g_uri_unref (uri);

        return path;
}

static void
gupnp_service_constructed (GObject *object)
{
        GObjectClass *object_class;
        GUPnPServiceInfo *info;
        GUPnPContext *context;
        AclServerHandler *handler;
        char *url;
        char *path;

        object_class = G_OBJECT_CLASS (gupnp_service_parent_class);

        /* Construct */
        object_class->constructed (object);

        info = GUPNP_SERVICE_INFO (object);

        /* Get introspection and save state variable names */
        gupnp_service_info_get_introspection_async (info,
                                                    got_introspection,
                                                    NULL);

        /* Get server */
        context = gupnp_service_info_get_context (info);

        /* Run listener on controlURL */
        url = gupnp_service_info_get_control_url (info);
        path = path_from_url (url);
        handler = acl_server_handler_new (GUPNP_SERVICE (object),
                                          context,
                                          control_server_handler,
                                          object,
                                          NULL);
        _gupnp_context_add_server_handler_with_data (context, path, handler);
        g_free (path);
        g_free (url);

        /* Run listener on eventSubscriptionURL */
        url = gupnp_service_info_get_event_subscription_url (info);
        path = path_from_url (url);
        handler = acl_server_handler_new (GUPNP_SERVICE (object),
                                          context,
                                          subscription_server_handler,
                                          object,
                                          NULL);
        _gupnp_context_add_server_handler_with_data (context, path, handler);
        g_free (path);
        g_free (url);
}

/* Root device availability changed. */
static void
notify_available_cb (GObject                  *object,
                     G_GNUC_UNUSED GParamSpec *pspec,
                     gpointer                  user_data)
{
        GUPnPService *service;
        GUPnPServicePrivate *priv;

        service = GUPNP_SERVICE (user_data);
        priv = gupnp_service_get_instance_private (service);

        if (!gupnp_root_device_get_available (GUPNP_ROOT_DEVICE (object))) {
                /* Root device now unavailable: Purge subscriptions */
                g_hash_table_remove_all (priv->subscriptions);
        }
}

static void
gupnp_service_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
        GUPnPService *service;
        GUPnPServicePrivate *priv;

        service = GUPNP_SERVICE (object);
        priv = gupnp_service_get_instance_private (service);

        switch (property_id) {
        case PROP_ROOT_DEVICE: {
                GUPnPRootDevice **dev;

                priv->root_device = g_value_get_object (value);
                dev = &(priv->root_device);

                g_object_add_weak_pointer (G_OBJECT (priv->root_device),
                                           (gpointer *) dev);

                priv->notify_available_id =
                        g_signal_connect_object (priv->root_device,
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
        GUPnPServicePrivate *priv;

        service = GUPNP_SERVICE (object);
        priv = gupnp_service_get_instance_private (service);

        switch (property_id) {
        case PROP_ROOT_DEVICE:
                g_value_set_object (value, priv->root_device);
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
        GUPnPServicePrivate *priv;
        GObjectClass *object_class;
        GUPnPServiceInfo *info;
        GUPnPContext *context;
        char *url;
        char *path;

        service = GUPNP_SERVICE (object);
        priv = gupnp_service_get_instance_private (service);

        /* Get server */
        info = GUPNP_SERVICE_INFO (service);
        context = gupnp_service_info_get_context (info);

        /* Remove listener on controlURL */
        url = gupnp_service_info_get_control_url (info);
        path = path_from_url (url);
        gupnp_context_remove_server_handler (context, path);
        g_free (path);
        g_free (url);

        /* Remove listener on eventSubscriptionURL */
        url = gupnp_service_info_get_event_subscription_url (info);
        path = path_from_url (url);
        gupnp_context_remove_server_handler (context, path);
        g_free (path);
        g_free (url);

        if (priv->root_device) {
                GUPnPRootDevice **dev = &(priv->root_device);

                if (g_signal_handler_is_connected
                        (priv->root_device,
                         priv->notify_available_id)) {
                        g_signal_handler_disconnect
                                (priv->root_device,
                                 priv->notify_available_id);
                }

                g_object_remove_weak_pointer (G_OBJECT (priv->root_device),
                                              (gpointer *) dev);

                priv->root_device = NULL;
        }

        /* Cancel pending messages */
        g_hash_table_remove_all (priv->subscriptions);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_service_parent_class);
        object_class->dispose (object);
}

static void
gupnp_service_finalize (GObject *object)
{
        GUPnPService *service;
        GUPnPServicePrivate *priv;
        GObjectClass *object_class;

        service = GUPNP_SERVICE (object);
        priv = gupnp_service_get_instance_private (service);

        /* Free subscription hash */
        g_hash_table_destroy (priv->subscriptions);

        /* Free state variable list */
        g_list_free_full (priv->state_variables, g_free);

        /* Free notify queue */
        g_queue_free_full (priv->notify_queue,
                           (GDestroyNotify) notify_data_free);

        g_clear_object (&priv->session);
        g_clear_object (&priv->introspection);

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
        object_class->constructed = gupnp_service_constructed;
        object_class->dispose      = gupnp_service_dispose;
        object_class->finalize     = gupnp_service_finalize;

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
                              NULL,
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
                              NULL,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_STRING,
                              G_TYPE_POINTER /* Not G_TYPE_VALUE as this
                                                is an outward argument! */);

        /**
         * GUPnPService::notify-failed:
         * @service: The #GUPnPService that received the signal
         * @callback_url: (type GList)(element-type GUri):A #GList of callback URLs
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
                              NULL,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_POINTER,
                              G_TYPE_POINTER);
}

/**
 * gupnp_service_notify:
 * @service: A #GUPnPService
 * @...: Tuples of variable name, variable type, and variable value,
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
        va_end (var_args);
}


/* Received notify response. */
static void
notify_got_response (GObject *source, GAsyncResult *res, gpointer user_data)
{

        GBytes *body;
        GError *error = NULL;
        NotifySubscriberData *data = user_data;

        body = soup_session_send_and_read_finish (SOUP_SESSION (source),
                                                  res,
                                                  &error);

        /* Cancelled? */
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                g_clear_error (&error);
                // Do nothing else. The data was freed after the message was
                // cancelled
                return;
        }

        // We don't need the body
        g_clear_pointer (&body, g_bytes_unref);

        SoupStatus status = soup_message_get_status (data->msg);

        /* Remove from pending messages list */
        data->data->pending_messages =
                g_list_remove (data->data->pending_messages, data);

        if (SOUP_STATUS_IS_SUCCESSFUL (status)) {
                data->data->initial_state_sent = TRUE;

                /* Success: reset callbacks pointer */
                data->data->callbacks = g_list_first (data->data->callbacks);

        } else if (status == SOUP_STATUS_PRECONDITION_FAILED) {
                /* Precondition failed: Cancel subscription */
                gupnp_service_remove_subscription (data->data->service,
                                                   data->data->sid);

        } else {
                /* Other failure: Try next callback or signal failure. */
                if (data->data->callbacks->next) {
                        /* Call next callback */
                        data->data->callbacks = data->data->callbacks->next;

                        notify_subscriber (NULL,
                                           data->data,
                                           g_bytes_ref (data->property_set));
                } else {
                        /* Emit 'notify-failed' signal */
                        GError *inner_error;

                        inner_error = g_error_new_literal (
                                GUPNP_EVENTING_ERROR,
                                GUPNP_EVENTING_ERROR_NOTIFY_FAILED,
                                soup_message_get_reason_phrase (data->msg));

                        g_signal_emit (data->data->service,
                                       signals[NOTIFY_FAILED],
                                       0,
                                       data->data->callbacks,
                                       inner_error);

                        g_error_free (inner_error);

                        /* Reset callbacks pointer */
                        data->data->callbacks =
                                g_list_first (data->data->callbacks);
                }
        }
        g_clear_error (&error);
        g_bytes_unref (data->property_set);
        g_object_unref (data->msg);
        g_free (data);
}

/* Send notification @user_data to subscriber @value */
static void
notify_subscriber (G_GNUC_UNUSED gpointer key,
                   gpointer value,
                   gpointer user_data)
{
        char *tmp;
        SoupSession *session;

        /* Subscriber called unsubscribe */
        if (subscription_data_can_delete ((SubscriptionData *) value))
                return;

        NotifySubscriberData *data = g_new0 (NotifySubscriberData, 1);

        data->data = value;
        data->property_set = g_bytes_ref ((GBytes *) user_data);

        /* Create message */
        data->msg = soup_message_new_from_uri (GENA_METHOD_NOTIFY,
                                               data->data->callbacks->data);

        SoupMessageHeaders *request_headers =
                soup_message_get_request_headers (data->msg);

        soup_message_headers_append (request_headers, "NT", "upnp:event");
        soup_message_headers_append (request_headers, "NTS", "upnp:propchange");
        soup_message_headers_append (request_headers, "SID", data->data->sid);

        tmp = g_strdup_printf ("%d", data->data->seq);
        soup_message_headers_append (request_headers, "SEQ", tmp);
        g_free (tmp);

        /* Handle overflow */
        if (data->data->seq < G_MAXINT32)
                data->data->seq++;
        else
                data->data->seq = 1;

        /* Add body */
        soup_message_set_request_body_from_bytes (data->msg,
                                                  "text/xml; charset=\"utf-8\"",
                                                  data->property_set);

        /* Queue */
        data->data->pending_messages =
                g_list_prepend (data->data->pending_messages, data);
        soup_message_headers_append (request_headers, "Connection", "close");

        session = gupnp_service_get_session (data->data->service);

        soup_session_send_and_read_async (
                session,
                data->msg,
                G_PRIORITY_DEFAULT,
                data->data->cancellable,
                (GAsyncReadyCallback) notify_got_response,
                data);
}

/* Create a property set from @queue */
static GBytes *
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
        return g_string_free_to_bytes (str);
}

/* Flush all queued notifications */
static void
flush_notifications (GUPnPService *service)
{
        GUPnPServicePrivate *priv;

        priv = gupnp_service_get_instance_private (service);

        /* Create property set */
        GBytes *property_set = create_property_set (priv->notify_queue);

        /* And send it off */
        g_hash_table_foreach (priv->subscriptions,
                              notify_subscriber,
                              property_set);

        /* Cleanup */
        g_bytes_unref (property_set);
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
        GUPnPServicePrivate *priv;

        g_return_if_fail (GUPNP_IS_SERVICE (service));
        g_return_if_fail (variable != NULL);
        g_return_if_fail (G_IS_VALUE (value));

        priv = gupnp_service_get_instance_private (service);

        /* Queue */
        data = g_slice_new0 (NotifyData);

        data->variable = g_strdup (variable);

        g_value_init (&data->value, G_VALUE_TYPE (value));
        g_value_copy (value, &data->value);

        g_queue_push_tail (priv->notify_queue, data);

        /* And flush, if not frozen */
        if (!priv->notify_frozen)
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
        GUPnPServicePrivate *priv;

        g_return_if_fail (GUPNP_IS_SERVICE (service));

        priv = gupnp_service_get_instance_private (service);

        priv->notify_frozen = TRUE;
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
        GUPnPServicePrivate *priv;

        g_return_if_fail (GUPNP_IS_SERVICE (service));

        priv = gupnp_service_get_instance_private (service);

        priv->notify_frozen = FALSE;

        if (g_queue_get_length (priv->notify_queue) == 0)
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
 * @error: (inout)(optional)(nullable): return location for a #GError, or %NULL
 *
 * A convenience function that attempts to connect all possible
 * #GUPnPService::action-invoked and #GUPnPService::query-variable signals to
 * appropriate callbacks for the service @service. It uses service introspection
 * and #GModule<!-- -->'s introspective features. It is very simillar to
 * gtk_builder_connect_signals() except that it attempts to guess the names of
 * the signal handlers on its own.
 *
 * For this function to do its magic, the application must name the callback
 * functions for #GUPnPService::action-invoked signals by striping the CamelCase
 * off the action names and either prepend "on_" or append "_cb" to them. Same
 * goes for #GUPnPService::query-variable signals, except that "query_" should
 * be prepended to the variable name. For example, callback function for
 * <varname>GetSystemUpdateID</varname> action should be either named as
 * "get_system_update_id_cb" or "on_get_system_update_id" and callback function
 * for the query of "SystemUpdateID" state variable should be named
 * <function>query_system_update_id_cb</function> or
 * <function>on_query_system_update_id</function>.
 *
 * <note>This function will not work correctly if #GModule is not supported
 * on the platform or introspection is not available for @service.</note>
 *
 * <warning>This function can not and therefore does not guarantee that the
 * resulting signal connections will be correct as it depends heavily on a
 * particular naming schemes described above.</warning>
 **/
void
gupnp_service_signals_autoconnect (GUPnPService *service,
                                   gpointer      user_data,
                                   GError      **error)
{
        GUPnPServiceIntrospection *introspection;
        const GList               *names;
        GModule                   *module;
        GUPnPServicePrivate       *priv;

        g_return_if_fail (GUPNP_IS_SERVICE (service));

        priv = gupnp_service_get_instance_private (service);

        introspection = priv->introspection;

        if (!introspection) {
                /* Initial introspection is not done yet, delay until we
                 * received that */
                priv->pending_autoconnect =
                    g_list_prepend (priv->pending_autoconnect,
                                    user_data);

                return;
        }

        /* Get a handle on the main executable -- use this to find symbols */
        module = g_module_open (NULL, 0);
        if (module == NULL) {
                g_error ("Failed to open module: %s", g_module_error ());

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
}
