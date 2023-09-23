/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */


#define G_LOG_DOMAIN "gupnp-service-info"

#include <config.h>
#include <libsoup/soup.h>
#include <string.h>

#include "gupnp-context-private.h"
#include "gupnp-error-private.h"
#include "gupnp-error.h"
#include "gupnp-service-info-private.h"
#include "gupnp-service-info.h"
#include "gupnp-service-introspection-private.h"
#include "gupnp-xml-doc.h"
#include "xml-util.h"

struct _GUPnPServiceInfoPrivate {
        GUPnPContext *context;

        char *location;
        char *udn;
        char *service_type;

        GUri *url_base;

        GUPnPXMLDoc *doc;

        xmlNode *element;

        GCancellable *pending_downloads_cancellable;
        GUPnPServiceIntrospection *introspection;
};

typedef struct _GUPnPServiceInfoPrivate GUPnPServiceInfoPrivate;


/**
 * GUPnPServiceInfo:
 *
 * Service information shared by local and remote services.
 *
 * A class that contains the common parts between local and remote services.
 */
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

static void
gupnp_service_info_init (GUPnPServiceInfo *info)
{
        GUPnPServiceInfoPrivate *priv;
        priv = gupnp_service_info_get_instance_private (info);

        priv->pending_downloads_cancellable = g_cancellable_new ();
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
        if (!g_cancellable_is_cancelled (priv->pending_downloads_cancellable)) {
                g_cancellable_cancel (priv->pending_downloads_cancellable);
        }

        g_clear_object (&priv->context);
        g_clear_object (&priv->doc);
        g_clear_object (&priv->introspection);

        G_OBJECT_CLASS (gupnp_service_info_parent_class)->dispose (object);
}

static void
gupnp_service_info_finalize (GObject *object)
{
        GUPnPServiceInfo *info;
        GUPnPServiceInfoPrivate *priv;

        info = GUPNP_SERVICE_INFO (object);
        priv = gupnp_service_info_get_instance_private (info);

        g_clear_object (&priv->pending_downloads_cancellable);
        g_free (priv->location);
        g_free (priv->udn);
        g_free (priv->service_type);

        g_uri_unref (priv->url_base);

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
        g_object_class_install_property (
                object_class,
                PROP_URL_BASE,
                g_param_spec_boxed ("url-base",
                                    "URL base",
                                    "The URL base",
                                    G_TYPE_URI,
                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
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
const GUri *
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
 * Example: `urn:schemas-upnp-org:service:RenderingControl:1`
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
 * Get the serviceID of this service, or %NULL if there is no ID.
 *
 * The serviceID should be unique to a device. This makes it possible to provide
 * the same serviceType multiple times on one device
 *
 * Example: `org:serviceId:RenderingControl`
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

static void
get_scpd_document_finished (GObject *source,
                            GAsyncResult *res,
                            gpointer user_data)
{
        GError *error = NULL;
        GTask *task = G_TASK (user_data);
        xmlDoc *scpd = NULL;

        GBytes *bytes =
                soup_session_send_and_read_finish (SOUP_SESSION (source),
                                                   res,
                                                   &error);

        if (error != NULL) {
                g_task_return_error (task, error);

                goto out;
        }

        SoupMessage *message =
                soup_session_get_async_result_message (SOUP_SESSION (source),
                                                       res);
        if (!SOUP_STATUS_IS_SUCCESSFUL (soup_message_get_status (message))) {
                g_task_return_error (task,
                                     _gupnp_error_new_server_error (message));

                goto out;
        }

        gsize length;
        gconstpointer data = g_bytes_get_data (bytes, &length);
        scpd = xmlReadMemory (data,
                              length,
                              NULL,
                              NULL,
                              XML_PARSE_NONET | XML_PARSE_RECOVER);
        if (scpd == NULL) {
                g_task_return_new_error (task,
                                         GUPNP_SERVER_ERROR,
                                         GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                         "Could not parse SCPD");

                goto out;
        }

        GUPnPServiceInfo *info =
                GUPNP_SERVICE_INFO (g_task_get_source_object (task));
        GUPnPServiceInfoPrivate *priv =
                gupnp_service_info_get_instance_private (info);
        priv->introspection = gupnp_service_introspection_new (scpd, &error);

        if (error != NULL) {
                g_task_return_error (task, error);

                goto out;
        }

        g_task_return_pointer (task,
                               g_object_ref (priv->introspection),
                               g_object_unref);

out:
        g_clear_pointer (&scpd, xmlFreeDoc);
        g_clear_pointer (&bytes, g_bytes_unref);
        g_object_unref (task);
}

/**
 * gupnp_service_info_introspect_async:
 * @info: A #GUPnPServiceInfo
 * @cancellable: (nullable) : a #GCancellable that can be used to cancel the call.
 * @callback: (scope async) : callback to be called when introspection object is ready.
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
        GUPnPServiceInfoPrivate *priv =
                gupnp_service_info_get_instance_private (info);
        GTask *task = g_task_new (info, cancellable, callback, user_data);
        g_task_set_name (task, "UPnP service introspection");

        // This service has been previously introspected. Shortcut the introspection
        // from the cached variable
        if (priv->introspection != NULL) {
                g_task_return_pointer (task,
                                       g_object_ref (priv->introspection),
                                       g_object_unref);

                g_object_unref (task);
                return;
        }

        char *scpd_url = gupnp_service_info_get_scpd_url (info);
        if (scpd_url == NULL) {
                g_task_return_new_error (task,
                                         GUPNP_SERVER_ERROR,
                                         GUPNP_SERVER_ERROR_INVALID_URL,
                                         "%s",
                                         "No valid SCPD URL defined");
                g_object_unref (task);

                return;
        }

        GUPnPContext *context = gupnp_service_info_get_context (info);
        GUri *scpd = gupnp_context_rewrite_uri_to_uri (context, scpd_url);
        g_free (scpd_url);

        SoupMessage *message = soup_message_new_from_uri (SOUP_METHOD_GET, scpd);
        g_uri_unref (scpd);

        if (message == NULL) {
                g_task_return_new_error (task,
                                         GUPNP_SERVER_ERROR,
                                         GUPNP_SERVER_ERROR_INVALID_URL,
                                         "%s",
                                         "No valid SCPD URL defined");
                g_object_unref (task);

                return;
        }

        GCancellable *internal_cancellable = g_cancellable_new ();
        if (cancellable != NULL) {
                g_cancellable_connect (cancellable,
                                       G_CALLBACK (g_cancellable_cancel),
                                       internal_cancellable,
                                       NULL);
        }

        /* Send off the message */
        soup_session_send_and_read_async (
                gupnp_context_get_session (priv->context),
                message,
                G_PRIORITY_DEFAULT,
                internal_cancellable,
                get_scpd_document_finished,
                task);
        g_object_unref (message);
        g_object_unref (internal_cancellable);
}

/**
 * gupnp_service_info_introspect_finish:
 * @info: A GUPnPServiceInfo
 * @res: A #GAsyncResult
 * @error: (inout)(optional): Return location for a #GError, or %NULL
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

/**
 * gupnp_service_info_get_introspection:
 * @info: A GUPnPServiceInfo
 * Returns: (nullable)(transfer none): The cached GUPnPServiceIntrospection if
 * available, %NULL otherwise.
 */
GUPnPServiceIntrospection *
gupnp_service_info_get_introspection (GUPnPServiceInfo *info)
{
        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        GUPnPServiceInfoPrivate *priv =
                gupnp_service_info_get_instance_private (info);

        return priv->introspection;
}
