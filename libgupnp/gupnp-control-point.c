/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#define G_LOG_DOMAIN "gupnp-control-point"

/**
 * GUPnPControlPoint:
 *
 * Network resource discovery.
 *
 * #GUPnPControlPoint handles device and service discovery. After creating
 * a control point and activating it using [method@GSSDP.ResourceBrowser.set_active],
 * the [signal@GUPnP.ControlPoint::device-proxy-available],
 * [signal@GUPnP.ControlPoint::service-proxy-available],
 * [signal@GUPnP.ControlPoint::device-proxy-unavailable] and
 * [signal@GUPnP.ControlPoint::service-proxy-unavailable] signals will
 * be emitted whenever the availability of a device or service matching
 * the specified discovery target changes.
 */

#include <config.h>
#include <string.h>

#include <libxml/parser.h>

#include "gupnp-control-point.h"
#include "gupnp-context-private.h"
#include "gupnp-resource-factory-private.h"
#include "http-headers.h"
#include "xml-util.h"

#define GUPNP_MAX_DESCRIPTION_DOWNLOAD_RETRIES 4
#define GUPNP_INITIAL_DESCRIPTION_RETRY_TIMEOUT 5

struct _GUPnPControlPointPrivate {
        GUPnPResourceFactory *factory;

        GList *devices;
        GList *services;

        GHashTable *doc_cache;

        GList *pending_gets;
};
typedef struct _GUPnPControlPointPrivate GUPnPControlPointPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GUPnPControlPoint,
                            gupnp_control_point,
                            GSSDP_TYPE_RESOURCE_BROWSER)

enum {
        PROP_0,
        PROP_RESOURCE_FACTORY,
};

enum {
        DEVICE_PROXY_AVAILABLE,
        DEVICE_PROXY_UNAVAILABLE,
        SERVICE_PROXY_AVAILABLE,
        SERVICE_PROXY_UNAVAILABLE,
        SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

typedef struct {
        GUPnPControlPoint *control_point;

        char *udn;
        char *service_type;
        char *description_url;

        SoupMessage *message;
        GSource *timeout_source;
        GCancellable *cancellable;
        int tries;
        int timeout;
} GetDescriptionURLData;

static void
gupnp_control_point_remove_pending_get (GUPnPControlPoint     *control_point,
                                        GetDescriptionURLData *data);

static void
get_description_url_data_free (GetDescriptionURLData *data)
{
        gupnp_control_point_remove_pending_get (data->control_point, data);

        if (data->timeout_source) {
                g_source_destroy (data->timeout_source);
                g_source_unref (data->timeout_source);
        }

        /* Cancel any pending description file GETs */
        if (!g_cancellable_is_cancelled (data->cancellable)) {
                g_cancellable_cancel (data->cancellable);
        }

        g_free (data->udn);
        g_free (data->service_type);
        g_free (data->description_url);
        g_object_unref (data->control_point);
        g_object_unref (data->cancellable);

        g_slice_free (GetDescriptionURLData, data);
}

static GetDescriptionURLData*
find_get_description_url_data (GUPnPControlPoint *control_point,
                               const char        *udn,
                               const char        *service_type)
{
        GList *l;
        GUPnPControlPointPrivate *priv;

        priv = gupnp_control_point_get_instance_private (control_point);
        l = priv->pending_gets;

        while (l) {
                GetDescriptionURLData *data = l->data;

                if ((g_strcmp0 (udn, data->udn) == 0) &&
                    (service_type == data->service_type ||
                     g_strcmp0 (service_type, data->service_type) == 0))
                        break;
                l = g_list_next (l);
        }

        return l ? l->data : NULL;
}

static void
gupnp_control_point_init (GUPnPControlPoint *control_point)
{
        GUPnPControlPointPrivate *priv;

        priv = gupnp_control_point_get_instance_private (control_point);
        priv->doc_cache = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 NULL);
}

/* Return TRUE if value == user_data */
static gboolean
find_doc (G_GNUC_UNUSED gpointer key,
          gpointer               value,
          gpointer               user_data)
{
        return (value == user_data);
}

/* xmlDoc wrapper finalized */
static void
doc_finalized (gpointer user_data,
               GObject *where_the_object_was)
{
        GUPnPControlPoint *control_point;
        GUPnPControlPointPrivate *priv;

        control_point = GUPNP_CONTROL_POINT (user_data);
        priv = gupnp_control_point_get_instance_private (control_point);

        g_hash_table_foreach_remove (priv->doc_cache,
                                     find_doc,
                                     where_the_object_was);
}

/* Release weak reference on xmlDoc wrapper */
static void
weak_unref_doc (G_GNUC_UNUSED gpointer key,
                gpointer               value,
                gpointer               user_data)
{
        g_object_weak_unref (G_OBJECT (value), doc_finalized, user_data);
}

static void
gupnp_control_point_dispose (GObject *object)
{
        GUPnPControlPoint *control_point;
        GSSDPResourceBrowser *browser;
        GObjectClass *object_class;
        GUPnPControlPointPrivate *priv;

        control_point = GUPNP_CONTROL_POINT (object);
        priv = gupnp_control_point_get_instance_private (control_point);
        browser = GSSDP_RESOURCE_BROWSER (control_point);

        gssdp_resource_browser_set_active (browser, FALSE);

        g_clear_object (&priv->factory);

        /* Cancel any pending description file GETs */
        while (priv->pending_gets) {
                GetDescriptionURLData *data;

                data = priv->pending_gets->data;
                get_description_url_data_free (data);
        }

        /* Release weak references on remaining cached documents */
        g_hash_table_foreach (priv->doc_cache,
                              weak_unref_doc,
                              control_point);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_control_point_parent_class);
        object_class->dispose (object);
}

static void
gupnp_control_point_finalize (GObject *object)
{
        GUPnPControlPoint *control_point;
        GObjectClass *object_class;
        GUPnPControlPointPrivate *priv;

        control_point = GUPNP_CONTROL_POINT (object);
        priv = gupnp_control_point_get_instance_private (control_point);

        g_hash_table_destroy (priv->doc_cache);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_control_point_parent_class);
        object_class->finalize (object);
}

static GList *
find_service_node (GUPnPControlPoint *control_point,
                   const char        *udn,
                   const char        *service_type)
{
        GList *l;
        GUPnPControlPointPrivate *priv;

        priv = gupnp_control_point_get_instance_private (control_point);
        l = priv->services;

        while (l) {
                GUPnPServiceInfo *info;

                info = GUPNP_SERVICE_INFO (l->data);

                if ((strcmp (gupnp_service_info_get_udn (info), udn) == 0) &&
                    (strcmp (gupnp_service_info_get_service_type (info),
                             service_type) == 0))
                        break;

                l = l->next;
        }

        return l;
}

static GList *
find_device_node (GUPnPControlPoint *control_point,
                  const char        *udn)
{
        GList *l;
        GUPnPControlPointPrivate *priv;

        priv = gupnp_control_point_get_instance_private (control_point);
        l = priv->devices;

        while (l) {
                GUPnPDeviceInfo *info;

                info = GUPNP_DEVICE_INFO (l->data);

                if (strcmp (udn, gupnp_device_info_get_udn (info)) == 0)
                        break;

                l = l->next;
        }

        return l;
}

static void
create_and_report_service_proxy (GUPnPControlPoint *control_point,
                                 GUPnPXMLDoc *doc,
                                 xmlNode *element,
                                 const char *udn,
                                 const char *service_type,
                                 const char *description_url,
                                 GUri *url_base)
{
        GUPnPServiceProxy *proxy;
        GUPnPResourceFactory *factory;
        GUPnPContext *context;
        GUPnPControlPointPrivate *priv;

        if (find_service_node (control_point, udn, service_type) != NULL)
                /* We already have a proxy for this service */
                return;

        priv = gupnp_control_point_get_instance_private (control_point);
        factory = gupnp_control_point_get_resource_factory (control_point);
        context = gupnp_control_point_get_context (control_point);

        /* Create proxy */
        proxy = gupnp_resource_factory_create_service_proxy (factory,
                                                             context,
                                                             doc,
                                                             element,
                                                             udn,
                                                             service_type,
                                                             description_url,
                                                             url_base);

        priv->services = g_list_prepend (priv->services, proxy);

        g_signal_emit (control_point,
                       signals[SERVICE_PROXY_AVAILABLE],
                       0,
                       proxy);
}

static void
create_and_report_device_proxy (GUPnPControlPoint *control_point,
                                GUPnPXMLDoc *doc,
                                xmlNode *element,
                                const char *udn,
                                const char *description_url,
                                GUri *url_base)
{
        GUPnPDeviceProxy *proxy;
        GUPnPResourceFactory *factory;
        GUPnPContext *context;
        GUPnPControlPointPrivate *priv;

        if (find_device_node (control_point, udn) != NULL)
                /* We already have a proxy for this device */
                return;

        priv = gupnp_control_point_get_instance_private (control_point);
        factory = gupnp_control_point_get_resource_factory (control_point);
        context = gupnp_control_point_get_context (control_point);

        proxy = gupnp_resource_factory_create_device_proxy (factory,
                                                            context,
                                                            doc,
                                                            element,
                                                            udn,
                                                            description_url,
                                                            url_base);

        priv->devices = g_list_prepend (priv->devices, proxy);

        g_signal_emit (control_point,
                       signals[DEVICE_PROXY_AVAILABLE],
                       0,
                       proxy);
}

static gboolean
compare_service_types_versioned (const char *searched_service,
                                 const char *current_service)
{
        const char *searched_version_ptr, *current_version_ptr;
        guint searched_version, current_version, searched_length;
        guint current_length;

        searched_version_ptr = strrchr (searched_service, ':');
        if (searched_version_ptr == NULL)
                return FALSE;

        current_version_ptr = strrchr (current_service, ':');
        if (current_version_ptr == NULL)
                return FALSE;

        searched_length = (searched_version_ptr - searched_service);
        current_length = (current_version_ptr - current_service);

        if (searched_length != current_length)
                return FALSE;

        searched_version = (guint) atol (searched_version_ptr + 1);
        if (searched_version == 0)
                return FALSE;

        current_version = (guint) atol (current_version_ptr + 1);
        if (current_version == 0)
                return FALSE;

        if (current_version < searched_version)
                return FALSE;

        return strncmp (searched_service,
                        current_service,
                        searched_length) == 0;
}

/* Search @element for matching services */
static void
process_service_list (xmlNode *element,
                      GUPnPControlPoint *control_point,
                      GUPnPXMLDoc *doc,
                      const char *udn,
                      const char *service_type,
                      const char *description_url,
                      GUri *url_base)
{
        g_object_ref (control_point);

        for (element = element->children; element; element = element->next) {
                xmlChar *prop;
                gboolean match;

                if (strcmp ((char *) element->name, "service") != 0)
                        continue;

                /* See if this is a matching service */
                prop = xml_util_get_child_element_content (element,
                                                           "serviceType");
                if (!prop)
                        continue;

                match = compare_service_types_versioned (service_type,
                                                         (char *) prop);
                xmlFree (prop);

                if (!match)
                        continue;

                /* Match */

                create_and_report_service_proxy (control_point,
                                                 doc,
                                                 element,
                                                 udn,
                                                 service_type,
                                                 description_url,
                                                 url_base);
        }

        g_object_unref (control_point);
}

/* Recursively search @element for matching devices */
static void
process_device_list (xmlNode *element,
                     GUPnPControlPoint *control_point,
                     GUPnPXMLDoc *doc,
                     const char *udn,
                     const char *service_type,
                     const char *description_url,
                     GUri *url_base)
{
        g_object_ref (control_point);

        for (element = element->children; element; element = element->next) {
                xmlNode *children;
                xmlChar *prop;
                gboolean match;

                if (strcmp ((char *) element->name, "device") != 0)
                        continue;

                /* Recurse into children */
                children = xml_util_get_element (element,
                                                 "deviceList",
                                                 NULL);

                if (children) {
                        process_device_list (children,
                                             control_point,
                                             doc,
                                             udn,
                                             service_type,
                                             description_url,
                                             url_base);
                }

                /* See if this is a matching device */
                prop = xml_util_get_child_element_content (element, "UDN");
                if (!prop)
                        continue;

                match = (strcmp ((char *) prop, udn) == 0);

                xmlFree (prop);

                if (!match)
                        continue;

                /* Match */

                if (service_type) {
                        /* Dive into serviceList */
                        children = xml_util_get_element (element,
                                                         "serviceList",
                                                         NULL);

                        if (children) {
                                process_service_list (children,
                                                      control_point,
                                                      doc,
                                                      udn,
                                                      service_type,
                                                      description_url,
                                                      url_base);
                        }
                } else
                        create_and_report_device_proxy (control_point,
                                                        doc,
                                                        element,
                                                        udn,
                                                        description_url,
                                                        url_base);
        }

        g_object_unref (control_point);
}

/*
 * Called when the description document is loaded.
 */
static void
description_loaded (GUPnPControlPoint *control_point,
                    GUPnPXMLDoc       *doc,
                    const char        *udn,
                    const char        *service_type,
                    const char        *description_url)
{
        xmlNode *element;
        GUri *url_base;

        /* Save the URL base, if any */
        element = xml_util_get_element ((xmlNode *)
                                        gupnp_xml_doc_get_doc (doc),
                                        "root",
                                        NULL);
        if (element == NULL) {
                g_warning ("No 'root' element found in description document"
                           " '%s'. Ignoring device '%s'\n",
                           description_url,
                           udn);

                return;
        }

        url_base = xml_util_get_child_element_content_uri (element,
                                                           "URLBase",
                                                           NULL);
        if (!url_base)
                url_base =
                        g_uri_parse (description_url, G_URI_FLAGS_NONE, NULL);

        /* Iterate matching devices */
        process_device_list (element,
                             control_point,
                             doc,
                             udn,
                             service_type,
                             description_url,
                             url_base);

        /* Cleanup */
        g_uri_unref (url_base);
}


static gboolean
description_url_retry_timeout (gpointer user_data);

/*
 * Description URL downloaded.
 */
static void
got_description_url (GObject *source,
                     GAsyncResult *res,
                     GetDescriptionURLData *data)
{
        GUPnPXMLDoc *doc;
        GUPnPControlPointPrivate *priv;
        GError *error = NULL;
        gboolean retry = FALSE;
        SoupMessage *message =
                soup_session_get_async_result_message (SOUP_SESSION (source),
                                                       res);

        GBytes *body = soup_session_send_and_read_finish (SOUP_SESSION (source),
                                                          res,
                                                          &error);

        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                goto out;

        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT)) {
                g_clear_error (&error);
                retry = TRUE;
        }

        // FIXME: This needs better handling
        if (error != NULL) {
                g_warning ("Retrieving the description document failed: %s",
                           error->message);
                goto out;
        }

        priv = gupnp_control_point_get_instance_private (data->control_point);

        /* Now, make sure again this document is not already cached. If it is,
         * we re-use the cached one. */
        doc = g_hash_table_lookup (priv->doc_cache, data->description_url);
        if (doc) {
                /* Doc was cached */
                description_loaded (data->control_point,
                                    doc,
                                    data->udn,
                                    data->service_type,
                                    data->description_url);

                goto out;
        }

        /* Not cached */
        if (!retry &&
            SOUP_STATUS_IS_SUCCESSFUL (soup_message_get_status (message))) {
                xmlDoc *xml_doc;
                gsize length;
                gconstpointer body_data;

                body_data = g_bytes_get_data (body, &length);

                /* Parse response */
                xml_doc = xmlReadMemory (body_data,
                                         length,
                                         NULL,
                                         NULL,
                                         XML_PARSE_NONET | XML_PARSE_RECOVER);
                if (xml_doc) {
                        doc = gupnp_xml_doc_new (xml_doc);

                        description_loaded (data->control_point,
                                            doc,
                                            data->udn,
                                            data->service_type,
                                            data->description_url);

                        /* Insert into document cache */
                        g_hash_table_insert (priv->doc_cache,
                                             g_strdup (data->description_url),
                                             doc);

                        /* Make sure the document is removed from the cache
                         * once finalized. */
                        g_object_weak_ref (G_OBJECT (doc),
                                           doc_finalized,
                                           data->control_point);

                        /* If no proxy was created, make sure doc is freed. */
                        g_object_unref (doc);
                } else
                        g_warning ("Failed to parse %s", data->description_url);

        } else {
                GMainContext *async_context =
                        g_main_context_get_thread_default ();

                /* Retry GET after a timeout */
                data->tries--;

                if (data->tries > 0) {
                        g_warning (
                                "Failed to GET %s: %s, retrying in %d seconds",
                                data->description_url,
                                !retry ? soup_message_get_reason_phrase (
                                                 message)
                                       : "Timed out",
                                data->timeout);

                        data->timeout_source = g_timeout_source_new_seconds
                                        (data->timeout);
                        g_source_set_callback (data->timeout_source,
                                               description_url_retry_timeout,
                                               data,
                                               NULL);
                        g_source_attach (data->timeout_source, async_context);
                        data->timeout <<= 1;
                        return;
                } else {
                        g_warning ("Maximum number of retries failed, not trying again");
                }
        }

out:
        g_clear_error (&error);
        get_description_url_data_free (data);
        g_bytes_unref (body);
        g_object_unref (message);
}

/*
 * Downloads and parses (or takes from cache) @description_url,
 * creating:
 *  - A #GUPnPDeviceProxy for the device specified by @udn if @service_type
 *    is %NULL.
 *  - A #GUPnPServiceProxy for the service of type @service_type from the device
 *    specified by @udn if @service_type is not %NULL.
 */
static void
load_description (GUPnPControlPoint *control_point,
                  const char        *description_url,
                  const char        *udn,
                  const char        *service_type,
                  guint              max_tries,
                  guint              timeout)
{
        GUPnPXMLDoc *doc;
        GUPnPControlPointPrivate *priv;

        g_debug ("Loading description document %s", description_url);

        priv = gupnp_control_point_get_instance_private (control_point);
        doc = g_hash_table_lookup (priv->doc_cache,
                                   description_url);
        if (doc) {
                /* Doc was cached */
                description_loaded (control_point,
                                    doc,
                                    udn,
                                    service_type,
                                    description_url);
        } else {
                /* Asynchronously download doc */
                GUPnPContext *context;
                SoupSession *session;
                GetDescriptionURLData *data;
                char *local_description = NULL;

                context = gupnp_control_point_get_context (control_point);

                session = gupnp_context_get_session (context);

                data = g_slice_new (GetDescriptionURLData);

                data->tries = max_tries;
                data->timeout = timeout;
                local_description = gupnp_context_rewrite_uri (context,
                                                               description_url);
                if (local_description == NULL) {
                        g_warning ("Invalid description URL: %s",
                                   description_url);

                        g_slice_free (GetDescriptionURLData, data);

                        return;
                }

                data->message = soup_message_new (SOUP_METHOD_GET,
                                                  local_description);
                g_free (local_description);

                if (data->message == NULL) {
                        g_warning ("Invalid description URL: %s",
                                   description_url);

                        g_slice_free (GetDescriptionURLData, data);

                        return;
                }

                http_request_set_accept_language (data->message);

                data->control_point = g_object_ref (control_point);
                data->cancellable = g_cancellable_new ();
                data->udn             = g_strdup (udn);
                data->service_type    = g_strdup (service_type);
                data->description_url = g_strdup (description_url);
                data->timeout_source  = NULL;
                priv->pending_gets = g_list_prepend (priv->pending_gets,
                                                     data);

                soup_session_send_and_read_async (
                        session,
                        data->message,
                        G_PRIORITY_DEFAULT,
                        data->cancellable,
                        (GAsyncReadyCallback) got_description_url,
                        data);
        }
}

/*
 * Retry the description download
 */
static gboolean
description_url_retry_timeout (gpointer user_data)
{
        GetDescriptionURLData *data = (GetDescriptionURLData *) user_data;

        load_description (data->control_point,
                          data->description_url,
                          data->udn,
                          data->service_type,
                          data->tries,
                          data->timeout);

        get_description_url_data_free (data);

        return FALSE;
}

static gboolean
parse_usn (const char *usn,
           char      **udn,
           char      **service_type)
{
        gboolean ret;
        char **bits;
        guint count, i;

        ret = FALSE;

        *udn = *service_type = NULL;

        /* Verify we have a valid USN */
        if (strncmp (usn, "uuid:", strlen ("uuid:"))) {
                g_warning ("Invalid USN: %s", usn);

                return FALSE;
        }

        /* Parse USN */
        bits = g_strsplit (usn, "::", -1);

        /* Count elements */
        count = g_strv_length (bits);

        if (count == 1) {
                /* uuid:device-UUID */

                *udn = bits[0];

                ret = TRUE;

        } else if (count == 2) {
                char **second_bits;
                guint n_second_bits;

                second_bits = g_strsplit (bits[1], ":", -1);
                n_second_bits = g_strv_length (second_bits);

                if (n_second_bits >= 2 &&
                    !strcmp (second_bits[0], "upnp") &&
                    !strcmp (second_bits[1], "rootdevice")) {
                        /* uuid:device-UUID::upnp:rootdevice */

                        *udn = bits[0];

                        ret = TRUE;
                } else if (n_second_bits >= 3 &&
                           !strcmp (second_bits[0], "urn")) {
                        /* uuid:device-UIID::urn:domain-name:service/device:
                         * type:v */

                        if (!strcmp (second_bits[2], "device")) {
                                *udn = bits[0];

                                ret = TRUE;
                        } else if (!strcmp (second_bits[2], "service")) {
                                *udn = bits[0];
                                *service_type = bits[1];

                                ret = TRUE;
                        }
                }

                g_strfreev (second_bits);
        }

        if (*udn == NULL)
                g_warning ("Invalid USN: %s", usn);

        for (i = 0; i < count; i++) {
                if ((bits[i] != *udn) &&
                    (bits[i] != *service_type))
                        g_free (bits[i]);
        }

        g_free (bits);

        return ret;
}

static void
gupnp_control_point_resource_available (GSSDPResourceBrowser *resource_browser,
                                        const char           *usn,
                                        const GList          *locations)
{
        GUPnPControlPoint *control_point;
        char *udn, *service_type;

        control_point = GUPNP_CONTROL_POINT (resource_browser);

        /* Verify we have a location */
        if (!locations) {
                g_warning ("No Location header for device with USN %s", usn);
                return;
        }

        /* Parse USN */
        if (!parse_usn (usn, &udn, &service_type))
                return;

        load_description (control_point,
                          locations->data,
                          udn,
                          service_type,
                          GUPNP_MAX_DESCRIPTION_DOWNLOAD_RETRIES,
                          GUPNP_INITIAL_DESCRIPTION_RETRY_TIMEOUT);

        g_free (udn);
        g_free (service_type);
}

static void
gupnp_control_point_resource_unavailable
                                (GSSDPResourceBrowser *resource_browser,
                                 const char           *usn)
{
        GUPnPControlPoint *control_point;
        char *udn, *service_type;
        GetDescriptionURLData *get_data;
        GUPnPControlPointPrivate *priv;

        control_point = GUPNP_CONTROL_POINT (resource_browser);
        priv = gupnp_control_point_get_instance_private (control_point);

        /* Parse USN */
        if (!parse_usn (usn, &udn, &service_type))
                return;

        /* Find proxy */
        if (service_type) {
                GList *l = find_service_node (control_point, udn, service_type);

                if (l) {
                        GUPnPServiceProxy *proxy;

                        /* Remove proxy */
                        proxy = GUPNP_SERVICE_PROXY (l->data);

                        priv->services = g_list_delete_link (priv->services,
                                                             l);

                        g_signal_emit (control_point,
                                       signals[SERVICE_PROXY_UNAVAILABLE],
                                       0,
                                       proxy);

                        g_object_unref (proxy);
                }
        } else {
                GList *l = find_device_node (control_point, udn);

                if (l) {
                        GUPnPDeviceProxy *proxy;

                        /* Remove proxy */
                        proxy = GUPNP_DEVICE_PROXY (l->data);

                        priv->devices = g_list_delete_link (priv->devices,
                                                            l);

                        g_signal_emit (control_point,
                                       signals[DEVICE_PROXY_UNAVAILABLE],
                                       0,
                                       proxy);

                        g_object_unref (proxy);
                }
        }

        /* Find the description get request if it has not finished yet and stop
         * and remove it */
        get_data = find_get_description_url_data (control_point,
                                                  udn,
                                                  service_type);

        if (get_data) {
                if (!g_cancellable_is_cancelled (get_data->cancellable))
                        g_cancellable_cancel (get_data->cancellable);
        }

        g_free (udn);
        g_free (service_type);
}

static void
gupnp_control_point_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
        GUPnPControlPoint *control_point;
        GUPnPControlPointPrivate *priv;

        control_point = GUPNP_CONTROL_POINT (object);
        priv = gupnp_control_point_get_instance_private (control_point);

        switch (property_id) {
        case PROP_RESOURCE_FACTORY:
                priv->factory =
                        GUPNP_RESOURCE_FACTORY (g_value_dup_object (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_control_point_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
        GUPnPControlPoint *control_point;

        control_point = GUPNP_CONTROL_POINT (object);

        switch (property_id) {
        case PROP_RESOURCE_FACTORY:
                g_value_set_object (value,
                                    gupnp_control_point_get_resource_factory (control_point));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_control_point_remove_pending_get (GUPnPControlPoint     *control_point,
                                        GetDescriptionURLData *data)
{
        GUPnPControlPointPrivate *priv;

        priv = gupnp_control_point_get_instance_private (control_point);
        priv->pending_gets = g_list_remove (priv->pending_gets, data);
}


static void
gupnp_control_point_class_init (GUPnPControlPointClass *klass)
{
        GObjectClass *object_class;
        GSSDPResourceBrowserClass *browser_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_control_point_set_property;
        object_class->get_property = gupnp_control_point_get_property;
        object_class->dispose      = gupnp_control_point_dispose;
        object_class->finalize     = gupnp_control_point_finalize;

        browser_class = GSSDP_RESOURCE_BROWSER_CLASS (klass);

        browser_class->resource_available =
                gupnp_control_point_resource_available;
        browser_class->resource_unavailable =
                gupnp_control_point_resource_unavailable;

        /**
         * GUPnPControlPoint:resource-factory:(attributes org.gtk.Property.get=gupnp_control_point_get_resource_factory)
         *
         * The resource factory to use. Set to NULL for default factory.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_RESOURCE_FACTORY,
                 g_param_spec_object ("resource-factory",
                                      "Resource Factory",
                                      "The resource factory to use",
                                      GUPNP_TYPE_RESOURCE_FACTORY,
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_READWRITE |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPControlPoint::device-proxy-available:
         * @control_point: The #GUPnPControlPoint that received the signal
         * @proxy: The now available #GUPnPDeviceProxy
         *
         * The ::device-proxy-available signal is emitted whenever a new
         * device has become available.
         **/
        signals[DEVICE_PROXY_AVAILABLE] =
                g_signal_new ("device-proxy-available",
                              GUPNP_TYPE_CONTROL_POINT,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GUPnPControlPointClass,
                                               device_proxy_available),
                              NULL,
                              NULL,
                              NULL,
                              G_TYPE_NONE,
                              1,
                              GUPNP_TYPE_DEVICE_PROXY);

        /**
         * GUPnPControlPoint::device-proxy-unavailable:
         * @control_point: The #GUPnPControlPoint that received the signal
         * @proxy: The now unavailable #GUPnPDeviceProxy
         *
         * The ::device-proxy-unavailable signal is emitted whenever a
         * device is not available any more.
         **/
        signals[DEVICE_PROXY_UNAVAILABLE] =
                g_signal_new ("device-proxy-unavailable",
                              GUPNP_TYPE_CONTROL_POINT,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GUPnPControlPointClass,
                                               device_proxy_unavailable),
                              NULL,
                              NULL,
                              NULL,
                              G_TYPE_NONE,
                              1,
                              GUPNP_TYPE_DEVICE_PROXY);

        /**
         * GUPnPControlPoint::service-proxy-available:
         * @control_point: The #GUPnPControlPoint that received the signal
         * @proxy: The now available #GUPnPServiceProxy
         *
         * The ::service-proxy-available signal is emitted whenever a new
         * service has become available.
         **/
        signals[SERVICE_PROXY_AVAILABLE] =
                g_signal_new ("service-proxy-available",
                              GUPNP_TYPE_CONTROL_POINT,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GUPnPControlPointClass,
                                               service_proxy_available),
                              NULL,
                              NULL,
                              NULL,
                              G_TYPE_NONE,
                              1,
                              GUPNP_TYPE_SERVICE_PROXY);

        /**
         * GUPnPControlPoint::service-proxy-unavailable:
         * @control_point: The #GUPnPControlPoint that received the signal
         * @proxy: The now unavailable #GUPnPServiceProxy
         *
         * The ::service-proxy-unavailable signal is emitted whenever a
         * service is not available any more.
         **/
        signals[SERVICE_PROXY_UNAVAILABLE] =
                g_signal_new ("service-proxy-unavailable",
                              GUPNP_TYPE_CONTROL_POINT,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GUPnPControlPointClass,
                                               service_proxy_unavailable),
                              NULL,
                              NULL,
                              NULL,
                              G_TYPE_NONE,
                              1,
                              GUPNP_TYPE_SERVICE_PROXY);
}

/**
 * gupnp_control_point_new:
 * @context: A #GUPnPContext
 * @target: The search target
 *
 * Create a new #GUPnPControlPoint with the specified @context and @target.
 *
 * @target should be a service or device name, such as
 * `urn:schemas-upnp-org:service:WANIPConnection:1` or
 * `urn:schemas-upnp-org:device:MediaRenderer:1`.
 *
 * Return value: A new #GUPnPControlPoint object.
 **/
GUPnPControlPoint *
gupnp_control_point_new (GUPnPContext *context,
                         const char   *target)
{
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (target, NULL);

        return g_object_new (GUPNP_TYPE_CONTROL_POINT,
                             "client", context,
                             "target", target,
                             NULL);
}

/**
 * gupnp_control_point_new_full:
 * @context: A #GUPnPContext
 * @factory: A #GUPnPResourceFactory
 * @target: The search target
 *
 * Create a new #GUPnPControlPoint with the specified @context, @factory and
 * @target.
 *
 * @target should be a service or device name, such as
 * `urn:schemas-upnp-org:service:WANIPConnection:1` or
 * `urn:schemas-upnp-org:device:MediaRenderer:1`.
 *
 * By passing a custom `GUPnPResourceFactory`, the proxies handed out in ::device-available and
 * ::service-available can be overridden to hand out custom classes instead of the generic
 * [class@GUPnP.ServiceProxy] and [class@GUPnP.DeviceProxy].
 *
 * Return value: A new #GUPnPControlPoint object.
 **/
GUPnPControlPoint *
gupnp_control_point_new_full (GUPnPContext         *context,
                              GUPnPResourceFactory *factory,
                              const char           *target)
{
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (factory == NULL ||
                              GUPNP_IS_RESOURCE_FACTORY (factory), NULL);
        g_return_val_if_fail (target, NULL);

        return g_object_new (GUPNP_TYPE_CONTROL_POINT,
                             "client", context,
                             "target", target,
                             "resource-factory", factory,
                             NULL);
}

/**
 * gupnp_control_point_get_context:
 * @control_point: A #GUPnPControlPoint
 *
 * Get the #GUPnPControlPoint associated with @control_point.
 *
 * Returns: (transfer none): The #GUPnPContext.
 * Deprecated: 1.4.0: Use [method@GSSDP.ResourceBrowser.get_client] instead.
 **/
GUPnPContext *
gupnp_control_point_get_context (GUPnPControlPoint *control_point)
{
        GSSDPClient *client;

        g_return_val_if_fail (GUPNP_IS_CONTROL_POINT (control_point), NULL);

        client = gssdp_resource_browser_get_client
                                (GSSDP_RESOURCE_BROWSER (control_point));

        return GUPNP_CONTEXT (client);
}

/**
 * gupnp_control_point_list_device_proxies:
 * @control_point: A #GUPnPControlPoint
 *
 * Get the list of #GUPnPDeviceProxy objects the control point currently assumes to
 * be active.
 *
 * Since a device might have gone offline without signalizing it, but
 * the automatic resource timeout has not happened yet, it is possible that some of
 * the devices listed are not available anymore on the network.
 *
 * Do not free the list nor its elements.
 *
 * Return value: (element-type GUPnP.DeviceProxy) (transfer none): Device proxies
 * currently assumed to be active.
 **/
const GList *
gupnp_control_point_list_device_proxies (GUPnPControlPoint *control_point)
{
        GUPnPControlPointPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTROL_POINT (control_point), NULL);

        priv = gupnp_control_point_get_instance_private (control_point);

        return (const GList *) priv->devices;
}

/**
 * gupnp_control_point_list_service_proxies:
 * @control_point: A #GUPnPControlPoint
 *
 * Get the list of discovered #GUPnPServiceProxy objects the control point currently assumes to
 * be active.
 *
 * Since a device might have gone offline without signalizing it, but
 * the automatic resource timeout has not happened yet, it is possible that some of
 * the services listed are not available anymore on the network.
 *
 * Do not free the list nor its elements.
 *
 * Return value: (element-type GUPnP.ServiceProxy) (transfer none): Service proxies
 * currently assumed to be active.
 **/
const GList *
gupnp_control_point_list_service_proxies (GUPnPControlPoint *control_point)
{
        GUPnPControlPointPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTROL_POINT (control_point), NULL);

        priv = gupnp_control_point_get_instance_private (control_point);

        return (const GList *) priv->services;
}

/**
 * gupnp_control_point_get_resource_factory:(attributes org.gtk.Method.get_property=resource-factory)
 * @control_point: A #GUPnPControlPoint
 *
 * Get the #GUPnPResourceFactory used by the @control_point. If none was set during construction
 * by calling [ctor@GUPnP.ControlPoint.new_full], equivalent to calling
 * [func@GUPnP.ResourceFactory.get_default]
 *
 * Returns: (transfer none): The #GUPnPResourceFactory used by this control point
 **/
GUPnPResourceFactory *
gupnp_control_point_get_resource_factory (GUPnPControlPoint *control_point)
{
        GUPnPControlPointPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTROL_POINT (control_point), NULL);

        priv = gupnp_control_point_get_instance_private (control_point);

        if (priv->factory)
                  return priv->factory;

        return gupnp_resource_factory_get_default ();
}

