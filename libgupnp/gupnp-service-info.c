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

G_DEFINE_ABSTRACT_TYPE (GUPnPServiceInfo,
                        gupnp_service_info,
                        G_TYPE_OBJECT);

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
        info->priv = G_TYPE_INSTANCE_GET_PRIVATE (info,
                                                  GUPNP_TYPE_SERVICE_INFO,
                                                  GUPnPServiceInfoPrivate);
}

static void
gupnp_service_info_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
        GUPnPServiceInfo *info;

        info = GUPNP_SERVICE_INFO (object);

        switch (property_id) {
        case PROP_CONTEXT:
                info->priv->context = g_object_ref (g_value_get_object (value));
                break;
        case PROP_LOCATION:
                info->priv->location = g_value_dup_string (value);
                break;
        case PROP_UDN:
                info->priv->udn = g_value_dup_string (value);
                break;
        case PROP_SERVICE_TYPE:
                info->priv->service_type = g_value_dup_string (value);
                break;
        case PROP_URL_BASE:
                info->priv->url_base = g_value_dup_boxed (value);
                break;
        case PROP_DOCUMENT:
                info->priv->doc = g_value_dup_object (value);
                break;
        case PROP_ELEMENT:
                info->priv->element = g_value_get_pointer (value);
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

        info = GUPNP_SERVICE_INFO (object);

        switch (property_id) {
        case PROP_CONTEXT:
                g_value_set_object (value,
                                    info->priv->context);
                break;
        case PROP_LOCATION:
                g_value_set_string (value,
                                    info->priv->location);
                break;
        case PROP_UDN:
                g_value_set_string (value,
                                    info->priv->udn);
                break;
        case PROP_SERVICE_TYPE:
                g_value_set_string (value,
                                    gupnp_service_info_get_service_type (info));
                break;
        case PROP_URL_BASE:
                g_value_set_boxed (value,
                                   info->priv->url_base);
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

        info = GUPNP_SERVICE_INFO (object);

        /* Cancel any pending SCPD GETs */
        if (info->priv->context) {
                SoupSession *session;

                session = gupnp_context_get_session (info->priv->context);

                while (info->priv->pending_gets) {
                        GetSCPDURLData *data;

                        data = info->priv->pending_gets->data;

                        if (data->cancellable)
                                g_cancellable_disconnect (data->cancellable,
                                                          data->cancelled_id);

                        soup_session_cancel_message (session,
                                                     data->message,
                                                     SOUP_STATUS_CANCELLED);

                        get_scpd_url_data_free (data);

                        info->priv->pending_gets =
                                g_list_delete_link (info->priv->pending_gets,
                                                    info->priv->pending_gets);
                }

                /* Unref context */
                g_object_unref (info->priv->context);
                info->priv->context = NULL;
        }

        if (info->priv->doc) {
                g_object_unref (info->priv->doc);
                info->priv->doc = NULL;
        }

        G_OBJECT_CLASS (gupnp_service_info_parent_class)->dispose (object);
}

static void
gupnp_service_info_finalize (GObject *object)
{
        GUPnPServiceInfo *info;

        info = GUPNP_SERVICE_INFO (object);

        g_free (info->priv->location);
        g_free (info->priv->udn);
        g_free (info->priv->service_type);

        soup_uri_free (info->priv->url_base);

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

        g_type_class_add_private (klass, sizeof (GUPnPServiceInfoPrivate));

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
        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        return info->priv->context;
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
        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        return info->priv->location;
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
        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        return info->priv->url_base;
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
        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        return info->priv->udn;
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
        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        if (!info->priv->service_type) {
                info->priv->service_type =
                        xml_util_get_child_element_content_glib
                                (info->priv->element, "serviceType");
        }

        return info->priv->service_type;
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
        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        return xml_util_get_child_element_content_glib (info->priv->element,
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
        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        return xml_util_get_child_element_content_url (info->priv->element,
                                                       "SCPDURL",
                                                       info->priv->url_base);
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
        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        return xml_util_get_child_element_content_url (info->priv->element,
                                                       "controlURL",
                                                       info->priv->url_base);
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
        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        return xml_util_get_child_element_content_url (info->priv->element,
                                                       "eventSubURL",
                                                       info->priv->url_base);
}

/**
 * gupnp_service_info_get_introspection:
 * @info: A #GUPnPServiceInfo
 * @error: return location for a #GError, or %NULL
 *
 * Note that introspection object is created from the information in service
 * description document (SCPD) provided by the service so it can not be created
 * if the service does not provide an SCPD.
 *
 * Warning: You  should use gupnp_service_info_get_introspection_async()
 * instead, this function re-enter the GMainloop before returning.
 *
 * Return value: (transfer full):  A new #GUPnPServiceIntrospection for this
 * service or %NULL. Unref after use.
 **/
GUPnPServiceIntrospection *
gupnp_service_info_get_introspection (GUPnPServiceInfo *info,
                                      GError          **error)
{
        GUPnPServiceIntrospection *introspection;
        SoupSession *session;
        SoupMessage *msg;
        int status;
        char *scpd_url;
        xmlDoc *scpd;

        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        introspection = NULL;

        scpd_url = gupnp_service_info_get_scpd_url (info);

        msg = NULL;
        if (scpd_url != NULL) {
                msg = soup_message_new (SOUP_METHOD_GET, scpd_url);

                g_free (scpd_url);
        }

        if (msg == NULL) {
                g_set_error (error,
                             GUPNP_SERVER_ERROR,
                             GUPNP_SERVER_ERROR_INVALID_URL,
                             "No valid SCPD URL defined");

                return NULL;
        }

        /* Send off the message */
        session = gupnp_context_get_session (info->priv->context);

        status = soup_session_send_message (session, msg);
        if (!SOUP_STATUS_IS_SUCCESSFUL (status)) {
                _gupnp_error_set_server_error (error, msg);

                g_object_unref (msg);

                return NULL;
        }

        scpd = xmlRecoverMemory (msg->response_body->data,
                                 msg->response_body->length);

        g_object_unref (msg);

        if (scpd) {
                introspection = gupnp_service_introspection_new (scpd);

                xmlFreeDoc (scpd);
        }

        if (!introspection) {
                g_set_error (error,
                             GUPNP_SERVER_ERROR,
                             GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                             "Could not parse SCPD");
        }

        return introspection;
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

        data->info->priv->pending_gets =
                g_list_remove (data->info->priv->pending_gets, data);

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

        data = user_data;
        info = data->info;

        session = gupnp_context_get_session (info->priv->context);
        soup_session_cancel_message (session,
                                     data->message,
                                     SOUP_STATUS_CANCELLED);

        info->priv->pending_gets =
                g_list_remove (info->priv->pending_gets, data);

        error = g_error_new (G_IO_ERROR,
                             G_IO_ERROR_CANCELLED,
                             "The call was canceled");

        data->callback (data->info,
                        NULL,
                        error,
                        data->user_data);

        get_scpd_url_data_free (data);
}

/**
 * gupnp_service_info_get_introspection_async:
 * @info: A #GUPnPServiceInfo
 * @callback: (scope async) : callback to be called when introspection object is ready.
 * @user_data: user_data to be passed to the callback.
 *
 * Note that introspection object is created from the information in service
 * description document (SCPD) provided by the service so it can not be created
 * if the service does not provide an SCPD.
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
 * if the service does not provide an SCPD.
 *
 * If @cancellable is used to cancel the call, @callback will be called with
 * error code %G_IO_ERROR_CANCELLED.
 *
 * Since: 0.20.9.
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

        g_return_if_fail (GUPNP_IS_SERVICE_INFO (info));
        g_return_if_fail (callback != NULL);

        data = g_slice_new (GetSCPDURLData);

        scpd_url = gupnp_service_info_get_scpd_url (info);

        data->message = NULL;
        if (scpd_url != NULL) {
                data->message = soup_message_new (SOUP_METHOD_GET, scpd_url);

                g_free (scpd_url);
        }

        if (data->message == NULL) {
                GError *error;

                error = g_error_new
                                (GUPNP_SERVER_ERROR,
                                 GUPNP_SERVER_ERROR_INVALID_URL,
                                 "No valid SCPD URL defined");

                callback (info, NULL, error, user_data);

                g_error_free (error);

                g_slice_free (GetSCPDURLData, data);

                return;
        }

        data->info      = info;
        data->callback  = callback;
        data->user_data = user_data;

        /* Send off the message */
        info->priv->pending_gets =
                g_list_prepend (info->priv->pending_gets,
                                data);

        session = gupnp_context_get_session (info->priv->context);

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
