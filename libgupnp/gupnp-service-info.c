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

/**
 * SECTION:gupnp-service-info
 * @short_description: Base abstract class for querying service information.
 *
 * The #GUPnPDeviceInfo base abstract class provides methods for querying
 * service information.
 */

#define G_LOG_DOMAIN "GUPnPServiceInfo"

#include <config.h>
#include <libsoup/soup.h>
#include <string.h>

#include "gupnp-service-info.h"
#include "gupnp-service-introspection-private.h"
#include "gupnp-context-private.h"
#include "gupnp-error.h"
#include "gupnp-error-private.h"
#include "gupnp-xml-doc.h"
#include "http-headers.h"
#include "xml-util.h"

struct _GUPnPServiceInfoPrivate {
        GUPnPContext *context;

        char *location;
        char *udn;
        char *service_type;

        SoupURI *url_base;

        GUPnPXMLDoc *doc;

        xmlNode *element;

        /* For async downloads */
        GList *pending_gets;
};

typedef struct _GUPnPServiceInfoPrivate GUPnPServiceInfoPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GUPnPServiceInfo,
                                     gupnp_service_info,
                                     G_TYPE_OBJECT)

enum {
        PROP_0,
        PROP_CONTEXT,
        PROP_LOCATION,
        PROP_UDN,
        PROP_SERVICE_TYPE,
        PROP_URL_BASE,
        PROP_DOCUMENT,
        PROP_ELEMENT
};

typedef struct {
        GUPnPServiceInfo                 *info;

        GUPnPServiceIntrospectionCallback callback;
        gpointer                          user_data;

        GCancellable                     *cancellable;
        gulong                            cancelled_id;

        SoupMessage                      *message;
        GError                           *error;
} GetSCPDURLData;

static void
get_scpd_url_data_free (GetSCPDURLData *data)
{
        if (data->cancellable)
                g_object_unref (data->cancellable);

        g_slice_free (GetSCPDURLData, data);
}

static void
gupnp_service_info_init (GUPnPServiceInfo *info)
{
}

static void
gupnp_service_info_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
        GUPnPServiceInfo *info;
        GUPnPServiceInfoPrivate *priv;

        info = GUPNP_SERVICE_INFO (object);
        priv = gupnp_service_info_get_instance_private (info);

        switch (property_id) {
        case PROP_CONTEXT:
                priv->context = g_object_ref (g_value_get_object (value));
                break;
        case PROP_LOCATION:
                priv->location = g_value_dup_string (value);
                break;
        case PROP_UDN:
                priv->udn = g_value_dup_string (value);
                break;
        case PROP_SERVICE_TYPE:
                priv->service_type = g_value_dup_string (value);
                break;
        case PROP_URL_BASE:
                priv->url_base = g_value_dup_boxed (value);
                break;
        case PROP_DOCUMENT:
                priv->doc = g_value_dup_object (value);
                break;
        case PROP_ELEMENT:
                priv->element = g_value_get_pointer (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_service_info_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
        GUPnPServiceInfo *info;
        GUPnPServiceInfoPrivate *priv;

        info = GUPNP_SERVICE_INFO (object);
        priv = gupnp_service_info_get_instance_private (info);

        switch (property_id) {
        case PROP_CONTEXT:
                g_value_set_object (value, priv->context);
                break;
        case PROP_LOCATION:
                g_value_set_string (value, priv->location);
                break;
        case PROP_UDN:
                g_value_set_string (value, priv->udn);
                break;
        case PROP_SERVICE_TYPE:
                g_value_set_string (value,
                                    gupnp_service_info_get_service_type (info));
                break;
        case PROP_URL_BASE:
                g_value_set_boxed (value, priv->url_base);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_service_info_dispose (GObject *object)
{
        GUPnPServiceInfo *info;
        GUPnPServiceInfoPrivate *priv;

        info = GUPNP_SERVICE_INFO (object);
        priv = gupnp_service_info_get_instance_private (info);

        /* Cancel any pending SCPD GETs */
        if (priv->context) {
                SoupSession *session;

                session = gupnp_context_get_session (priv->context);

                while (priv->pending_gets) {
                        GetSCPDURLData *data;

                        data = priv->pending_gets->data;

                        if (data->cancellable)
                                g_cancellable_disconnect (data->cancellable,
                                                          data->cancelled_id);

                        soup_session_cancel_message (session,
                                                     data->message,
                                                     SOUP_STATUS_CANCELLED);

                        get_scpd_url_data_free (data);

                        priv->pending_gets =
                                g_list_delete_link (priv->pending_gets,
                                                    priv->pending_gets);
                }

                /* Unref context */
                g_object_unref (priv->context);
                priv->context = NULL;
        }

        if (priv->doc) {
                g_object_unref (priv->doc);
                priv->doc = NULL;
        }

        G_OBJECT_CLASS (gupnp_service_info_parent_class)->dispose (object);
}

static void
gupnp_service_info_finalize (GObject *object)
{
        GUPnPServiceInfo *info;
        GUPnPServiceInfoPrivate *priv;

        info = GUPNP_SERVICE_INFO (object);
        priv = gupnp_service_info_get_instance_private (info);

        g_free (priv->location);
        g_free (priv->udn);
        g_free (priv->service_type);

        soup_uri_free (priv->url_base);

        G_OBJECT_CLASS (gupnp_service_info_parent_class)->finalize (object);
}

static void
gupnp_service_info_class_init (GUPnPServiceInfoClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_service_info_set_property;
        object_class->get_property = gupnp_service_info_get_property;
        object_class->dispose      = gupnp_service_info_dispose;
        object_class->finalize     = gupnp_service_info_finalize;

        /**
         * GUPnPServiceInfo:context:
         *
         * The #GUPnPContext to use.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_CONTEXT,
                 g_param_spec_object ("context",
                                      "Context",
                                      "The GUPnPContext.",
                                      GUPNP_TYPE_CONTEXT,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPServiceInfo:location:
         *
         * The location of the device description file.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_LOCATION,
                 g_param_spec_string ("location",
                                      "Location",
                                      "The location of the device description "
                                      "file",
                                      NULL,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPServiceInfo:udn:
         *
         * The UDN of the containing device.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_UDN,
                 g_param_spec_string ("udn",
                                      "UDN",
                                      "The UDN of the containing device",
                                      NULL,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPServiceInfo:service-type:
         *
         * The service type.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_SERVICE_TYPE,
                 g_param_spec_string ("service-type",
                                      "Service type",
                                      "The service type",
                                      NULL,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPServiceInfo:url-base:
         *
         * The URL base (#SoupURI).
         **/
        g_object_class_install_property
                (object_class,
                 PROP_URL_BASE,
                 g_param_spec_boxed ("url-base",
                                     "URL base",
                                     "The URL base",
                                     SOUP_TYPE_URI,
                                     G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_NAME |
                                     G_PARAM_STATIC_NICK |
                                     G_PARAM_STATIC_BLURB));

        /**
         * GUPnPServiceInfo:document:
         *
         * Private property.
         *
         * Stability: Private
         **/
        g_object_class_install_property
                (object_class,
                 PROP_DOCUMENT,
                 g_param_spec_object ("document",
                                      "Document",
                                      "The XML document related to this "
                                      "service",
                                      GUPNP_TYPE_XML_DOC,
                                      G_PARAM_WRITABLE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPServiceInfo:element:
         *
         * Private property.
         *
         * Stability: Private
         **/
        g_object_class_install_property
                (object_class,
                 PROP_ELEMENT,
                 g_param_spec_pointer ("element",
                                       "Element",
                                       "The XML element related to this "
                                       "device",
                                       G_PARAM_WRITABLE |
                                       G_PARAM_CONSTRUCT_ONLY |
                                       G_PARAM_STATIC_NAME |
                                       G_PARAM_STATIC_NICK |
                                       G_PARAM_STATIC_BLURB));
}

/**
 * gupnp_service_info_get_context:
 * @info: A #GUPnPServiceInfo
 *
 * Get the #GUPnPContext associated with @info.
 *
 * Returns: (transfer none): A #GUPnPContext.
 **/
GUPnPContext *
gupnp_service_info_get_context (GUPnPServiceInfo *info)
{
        GUPnPServiceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        priv = gupnp_service_info_get_instance_private (info);

        return priv->context;
}

/**
 * gupnp_service_info_get_location:
 * @info: A #GUPnPServiceInfo
 *
 * Get the location of the device description file.
 *
 * Returns: A constant string.
 **/
const char *
gupnp_service_info_get_location (GUPnPServiceInfo *info)
{
        GUPnPServiceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        priv = gupnp_service_info_get_instance_private (info);

        return priv->location;
}

/**
 * gupnp_service_info_get_url_base:
 * @info: A #GUPnPServiceInfo
 *
 * Get the URL base of this service.
 *
 * Returns: A constant #SoupURI.
 **/
const SoupURI *
gupnp_service_info_get_url_base (GUPnPServiceInfo *info)
{
        GUPnPServiceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        priv = gupnp_service_info_get_instance_private (info);

        return priv->url_base;
}

/**
 * gupnp_service_info_get_udn:
 * @info: A #GUPnPServiceInfo
 *
 * Get the Unique Device Name of the containing device.
 *
 * Returns: A constant string.
 **/
const char *
gupnp_service_info_get_udn (GUPnPServiceInfo *info)
{
        GUPnPServiceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        priv = gupnp_service_info_get_instance_private (info);

        return priv->udn;
}

/**
 * gupnp_service_info_get_service_type:
 * @info: A #GUPnPServiceInfo
 *
 * Get the UPnP service type, or %NULL.
 *
 * Returns: A constant string.
 **/
const char *
gupnp_service_info_get_service_type (GUPnPServiceInfo *info)
{
        GUPnPServiceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        priv = gupnp_service_info_get_instance_private (info);

        if (!priv->service_type) {
                priv->service_type =
                        xml_util_get_child_element_content_glib
                                (priv->element, "serviceType");
        }

        return priv->service_type;
}

/**
 * gupnp_service_info_get_id:
 * @info: A #GUPnPServiceInfo
 *
 * Get the ID of this service, or %NULL if there is no ID.
 *
 * Return value: A string. This string should be freed with g_free() after use.
 **/
char *
gupnp_service_info_get_id (GUPnPServiceInfo *info)
{
        GUPnPServiceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        priv = gupnp_service_info_get_instance_private (info);

        return xml_util_get_child_element_content_glib (priv->element,
                                                        "serviceId");
}

/**
 * gupnp_service_info_get_scpd_url:
 * @info: A #GUPnPServiceInfo
 *
 * Get the SCPD URL for this service, or %NULL if there is no SCPD.
 *
 * Return value: A string. This string should be freed with g_free() after use.
 **/
char *
gupnp_service_info_get_scpd_url (GUPnPServiceInfo *info)
{
        GUPnPServiceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        priv = gupnp_service_info_get_instance_private (info);

        return xml_util_get_child_element_content_url (priv->element,
                                                       "SCPDURL",
                                                       priv->url_base);
}

/**
 * gupnp_service_info_get_control_url:
 * @info: A #GUPnPServiceInfo
 *
 * Get the control URL for this service, or %NULL..
 *
 * Return value: A string. This string should be freed with g_free() after use.
 **/
char *
gupnp_service_info_get_control_url (GUPnPServiceInfo *info)
{
        GUPnPServiceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        priv = gupnp_service_info_get_instance_private (info);

        return xml_util_get_child_element_content_url (priv->element,
                                                       "controlURL",
                                                       priv->url_base);
}

/**
 * gupnp_service_info_get_event_subscription_url:
 * @info: A #GUPnPServiceInfo
 *
 * Get the event subscription URL for this service, or %NULL.
 *
 * Return value: A string. This string should be freed with g_free() after use.
 **/
char *
gupnp_service_info_get_event_subscription_url (GUPnPServiceInfo *info)
{
        GUPnPServiceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        priv = gupnp_service_info_get_instance_private (info);

        return xml_util_get_child_element_content_url (priv->element,
                                                       "eventSubURL",
                                                       priv->url_base);
}

/*
 * SCPD URL downloaded.
 */
static void
got_scpd_url (G_GNUC_UNUSED SoupSession *session,
              SoupMessage               *msg,
              GetSCPDURLData            *data)
{
        GUPnPServiceIntrospection *introspection;
        GError *error;
        GUPnPServiceInfoPrivate *priv;

        introspection = NULL;
        error = NULL;

        if (msg->status_code == SOUP_STATUS_CANCELLED)
                return;

        if (SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
                xmlDoc *scpd;

                scpd = xmlRecoverMemory (msg->response_body->data,
                                         msg->response_body->length);
                if (scpd) {
                        introspection = gupnp_service_introspection_new (scpd);

                        xmlFreeDoc (scpd);
                }

                if (!introspection) {
                        error = g_error_new
                                        (GUPNP_SERVER_ERROR,
                                         GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                         "Could not parse SCPD");
                }
        } else
                error = _gupnp_error_new_server_error (msg);

        /* prevent the callback from canceling the cancellable
         * (and so freeing data just before we do) */
        if (data->cancellable)
                g_cancellable_disconnect (data->cancellable,
                                          data->cancelled_id);

        priv = gupnp_service_info_get_instance_private (data->info);
        priv->pending_gets = g_list_remove (priv->pending_gets, data);

        data->callback (data->info,
                        introspection,
                        error,
                        data->user_data);

        if (error)
                g_error_free (error);

        get_scpd_url_data_free (data);
}

static void
cancellable_cancelled_cb (GCancellable *cancellable,
                          gpointer user_data)
{
        GUPnPServiceInfo *info;
        GetSCPDURLData *data;
        SoupSession *session;
        GError *error;
        GUPnPServiceInfoPrivate *priv;

        data = user_data;
        info = data->info;

        priv = gupnp_service_info_get_instance_private (info);

        session = gupnp_context_get_session (priv->context);
        soup_session_cancel_message (session,
                                     data->message,
                                     SOUP_STATUS_CANCELLED);

        priv->pending_gets = g_list_remove (priv->pending_gets, data);

        error = g_error_new (G_IO_ERROR,
                             G_IO_ERROR_CANCELLED,
                             "The call was canceled");

        data->callback (data->info,
                        NULL,
                        error,
                        data->user_data);

        get_scpd_url_data_free (data);
}

static gboolean
introspection_error_cb (gpointer user_data)
{
        GetSCPDURLData *data = (GetSCPDURLData *)user_data;

        data->callback (data->info, NULL, data->error, data->user_data);
        g_error_free (data->error);
        g_slice_free (GetSCPDURLData, data);

        return FALSE;
}

/**
 * gupnp_service_info_get_introspection_async:
 * @info: A #GUPnPServiceInfo
 * @callback: (scope async) : callback to be called when introspection object is ready.
 * @user_data: user_data to be passed to the callback.
 *
 * Note that introspection object is created from the information in service
 * description document (SCPD) provided by the service so it can not be created
 * if the service does not provide a SCPD.
 **/
void
gupnp_service_info_get_introspection_async
                                (GUPnPServiceInfo                 *info,
                                 GUPnPServiceIntrospectionCallback callback,
                                 gpointer                          user_data)
{
        gupnp_service_info_get_introspection_async_full (info,
                                                         callback,
                                                         NULL,
                                                         user_data);
}

/**
 * gupnp_service_info_get_introspection_async_full:
 * @info: A #GUPnPServiceInfo
 * @callback: (scope async) : callback to be called when introspection object is ready.
 * @cancellable: GCancellable that can be used to cancel the call, or %NULL.
 * @user_data: user_data to be passed to the callback.
 *
 * Note that introspection object is created from the information in service
 * description document (SCPD) provided by the service so it can not be created
 * if the service does not provide a SCPD.
 *
 * If @cancellable is used to cancel the call, @callback will be called with
 * error code %G_IO_ERROR_CANCELLED.
 *
 * Since: 0.20.9
 **/
void
gupnp_service_info_get_introspection_async_full
                                (GUPnPServiceInfo                 *info,
                                 GUPnPServiceIntrospectionCallback callback,
                                 GCancellable                     *cancellable,
                                 gpointer                          user_data)
{
        GetSCPDURLData *data;
        char *scpd_url;
        SoupSession *session;
        GUPnPServiceInfoPrivate *priv;

        g_return_if_fail (GUPNP_IS_SERVICE_INFO (info));
        g_return_if_fail (callback != NULL);

        data = g_slice_new (GetSCPDURLData);

        scpd_url = gupnp_service_info_get_scpd_url (info);

        data->message = NULL;
        if (scpd_url != NULL) {
                GUPnPContext *context = NULL;
                char *local_scpd_url = NULL;

                context = gupnp_service_info_get_context (info);

                local_scpd_url = gupnp_context_rewrite_uri (context, scpd_url);
                g_free (scpd_url);

                data->message = soup_message_new (SOUP_METHOD_GET,
                                                  local_scpd_url);
                g_free (local_scpd_url);
        }

        data->info      = info;
        data->callback  = callback;
        data->user_data = user_data;

        if (data->message == NULL) {
                GError *error;
                GSource *idle_source;

                error = g_error_new
                                (GUPNP_SERVER_ERROR,
                                 GUPNP_SERVER_ERROR_INVALID_URL,
                                 "No valid SCPD URL defined");
                data->error = error;

                idle_source = g_idle_source_new ();
                g_source_set_callback (idle_source,
                                       introspection_error_cb,
                                       data, NULL);
                g_source_attach (idle_source,
                                 g_main_context_get_thread_default ());

                return;

                return;
        }


        /* Send off the message */
        priv = gupnp_service_info_get_instance_private (info);
        priv->pending_gets = g_list_prepend (priv->pending_gets, data);

        session = gupnp_context_get_session (priv->context);

        soup_session_queue_message (session,
                                    data->message,
                                    (SoupSessionCallback) got_scpd_url,
                                    data);

        data->cancellable = cancellable;
        if (data->cancellable) {
                g_object_ref (cancellable);
                data->cancelled_id = g_cancellable_connect
                                (data->cancellable,
                                 G_CALLBACK (cancellable_cancelled_cb),
                                 data,
                                 NULL);
        }
}

static void
prv_introspection_cb (GUPnPServiceInfo *info,
                      GUPnPServiceIntrospection *introspection,
                      const GError *error,
                      gpointer user_data)
{
        if (error != NULL) {
                g_task_return_error (G_TASK (user_data),
                                     g_error_copy (error));
        } else {
                g_task_return_pointer (G_TASK (user_data),
                                       introspection,
                                       g_object_unref);
        }

        g_object_unref (G_OBJECT (user_data));
}

/**
 * gupnp_service_info_introspect_async:
 * @info: A #GUPnPServiceInfo
 * @cancellable: (nullable) : #GCancellable that can be used to cancel the call, or %NULL.
 * @callback: (scope async) : callback to be called when introspeciton object is ready.
 * @user_data: user_data to be passed to the callback.
 *
 * Note that introspection object is created from the information in service
 * description document (SCPD) provided by the service so it can not be created
 * if the service does not provide a SCPD.
 *
 * If @cancellable is used to cancel the call, @callback will be called with
 * error code %G_IO_ERROR_CANCELLED.
 *
 * Since: 1.2.2
 **/
void
gupnp_service_info_introspect_async           (GUPnPServiceInfo    *info,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
        GTask *task = g_task_new (info, cancellable, callback, user_data);
        g_task_set_name (task, "UPnP service introspection");

        gupnp_service_info_get_introspection_async_full (info,
                                                         prv_introspection_cb,
                                                         cancellable,
                                                         task);
}

/**
 * gupnp_service_info_introspect_finish:
 * @info: A GUPnPServiceInfo
 * @res: A #GAsyncResult
 * @error: (inout)(optional)(nullable): Return location for a #GError, or %NULL
 *
 * Finish an asynchronous call initiated with
 * gupnp_service_info_introspect_async().
 *
 * Returns: (nullable)(transfer full): %NULL, if the call had an error, a
 * #GUPnPServiceIntrospection object otherwise.
 *
 * Since: 1.2.2
 */
GUPnPServiceIntrospection *
gupnp_service_info_introspect_finish          (GUPnPServiceInfo   *info,
                                               GAsyncResult       *res,
                                               GError            **error)
{
        g_return_val_if_fail (g_task_is_valid (res, info), NULL);

        return g_task_propagate_pointer (G_TASK (res), error);
}
