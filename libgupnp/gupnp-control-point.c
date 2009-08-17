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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gupnp-control-point
 * @short_description: Class for resource discovery.
 *
 * #GUPnPControlPoint handles device and service discovery. After creating
 * a control point and activating it using gssdp_resource_browser_set_active(),
 * the ::device-proxy-available, ::service-proxy-available,
 * ::device-proxy-unavailable and ::service-proxy-unavailable signals will
 * be emitted whenever the availability of a device or service matching
 * the specified discovery target changes.
 */

#include <string.h>

#include "gupnp-control-point.h"
#include "gupnp-context-private.h"
#include "gupnp-resource-factory-private.h"
#include "http-headers.h"
#include "xml-util.h"

G_DEFINE_TYPE (GUPnPControlPoint,
               gupnp_control_point,
               GSSDP_TYPE_RESOURCE_BROWSER);

struct _GUPnPControlPointPrivate {
        GUPnPResourceFactory *factory;

        GList *devices;
        GList *services;

        GHashTable *doc_cache;

        GList *pending_gets;
};

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
} GetDescriptionURLData;

static void
get_description_url_data_free (GetDescriptionURLData *data)
{
        data->control_point->priv->pending_gets =
                g_list_remove (data->control_point->priv->pending_gets, data);

        g_free (data->udn);
        g_free (data->service_type);
        g_free (data->description_url);

        g_slice_free (GetDescriptionURLData, data);
}

static void
gupnp_control_point_init (GUPnPControlPoint *control_point)
{
        control_point->priv =
                G_TYPE_INSTANCE_GET_PRIVATE (control_point,
                                             GUPNP_TYPE_CONTROL_POINT,
                                             GUPnPControlPointPrivate);

        control_point->priv->doc_cache =
                g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       NULL);
}

/* Return TRUE if value == user_data */
static gboolean
find_doc (gpointer key,
          gpointer value,
          gpointer user_data)
{
        return (value == user_data);
}

/* xmlDoc wrapper finalized */
static void
doc_finalized (gpointer user_data,
               GObject *where_the_object_was)
{
        GUPnPControlPoint *control_point;

        control_point = GUPNP_CONTROL_POINT (user_data);

        g_hash_table_foreach_remove (control_point->priv->doc_cache,
                                     find_doc,
                                     where_the_object_was);
}

/* Release weak reference on xmlDoc wrapper */
static void
weak_unref_doc (gpointer key,
                gpointer value,
                gpointer user_data)
{
        g_object_weak_unref (G_OBJECT (value), doc_finalized, user_data);
}

static void
gupnp_control_point_dispose (GObject *object)
{
        GUPnPControlPoint *control_point;
        GSSDPResourceBrowser *browser;
        GObjectClass *object_class;

        control_point = GUPNP_CONTROL_POINT (object);
        browser = GSSDP_RESOURCE_BROWSER (control_point);

        gssdp_resource_browser_set_active (browser, FALSE);

        if (control_point->priv->factory) {
                g_object_unref (control_point->priv->factory);
                control_point->priv->factory = NULL;
        }

        /* Cancel any pending description file GETs */
        while (control_point->priv->pending_gets) {
                GetDescriptionURLData *data;
                GUPnPContext *context;
                SoupSession *session;

                data = control_point->priv->pending_gets->data;

                context = gupnp_control_point_get_context (control_point);
                session = gupnp_context_get_session (context);

                soup_session_cancel_message (session,
                                             data->message,
                                             SOUP_STATUS_CANCELLED);

                get_description_url_data_free (data);
        }

        /* Release weak references on remaining cached documents */
        g_hash_table_foreach (control_point->priv->doc_cache,
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

        control_point = GUPNP_CONTROL_POINT (object);

        g_hash_table_destroy (control_point->priv->doc_cache);

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

        l = control_point->priv->services;

        while (l) {
                GUPnPServiceInfo *info;

                info = GUPNP_SERVICE_INFO (l->data);

                if ((strcmp (gupnp_service_info_get_udn (info), udn) == 0) ||
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

        l = control_point->priv->devices;

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
create_and_report_service_proxy (GUPnPControlPoint  *control_point,
                                 GUPnPXMLDoc        *doc,
                                 xmlNode            *element,
                                 const char         *udn,
                                 const char         *service_type,
                                 const char         *description_url,
                                 SoupURI            *url_base)
{
        GUPnPServiceProxy *proxy;
        GUPnPResourceFactory *factory;
        GUPnPContext *context;

        if (find_service_node (control_point, udn, service_type) != NULL)
                /* We already have a proxy for this service */
                return;

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

        control_point->priv->services =
                                g_list_prepend (control_point->priv->services,
                                                proxy);

        g_signal_emit (control_point,
                       signals[SERVICE_PROXY_AVAILABLE],
                       0,
                       proxy);
}

static void
create_and_report_device_proxy (GUPnPControlPoint  *control_point,
                                GUPnPXMLDoc        *doc,
                                xmlNode            *element,
                                const char         *udn,
                                const char         *description_url,
                                SoupURI            *url_base)
{
        GUPnPDeviceProxy *proxy;
        GUPnPResourceFactory *factory;
        GUPnPContext *context;

        if (find_device_node (control_point, udn) != NULL)
                /* We already have a proxy for this device */
                return;

        factory = gupnp_control_point_get_resource_factory (control_point);
        context = gupnp_control_point_get_context (control_point);

        proxy = gupnp_resource_factory_create_device_proxy (factory,
                                                            context,
                                                            doc,
                                                            element,
                                                            udn,
                                                            description_url,
                                                            url_base);

        control_point->priv->devices = g_list_prepend
                                                (control_point->priv->devices,
                                                 proxy);

        g_signal_emit (control_point,
                       signals[DEVICE_PROXY_AVAILABLE],
                       0,
                       proxy);
}

/* Search @element for matching services */
static void
process_service_list (xmlNode           *element,
                      GUPnPControlPoint *control_point,
                      GUPnPXMLDoc       *doc,
                      const char        *udn,
                      const char        *service_type,
                      const char        *description_url,
                      SoupURI           *url_base)
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

                match = (strcmp ((char *) prop, service_type) == 0);

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
process_device_list (xmlNode           *element,
                     GUPnPControlPoint *control_point,
                     GUPnPXMLDoc       *doc,
                     const char        *udn,
                     const char        *service_type,
                     const char        *description_url,
                     SoupURI           *url_base)
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
        SoupURI *url_base;

        /* Save the URL base, if any */
        element = xml_util_get_element ((xmlNode *) doc->doc,
                                        "root",
                                        NULL);
        if (element == NULL) {
                g_warning ("No 'root' element found in description document"
                           " '%s'. Ignoring device '%s'\n",
                           description_url,
                           udn);

                return;
        }

        if (element == NULL)
                return;

        url_base = xml_util_get_child_element_content_uri (element,
                                                           "URLBase",
                                                           NULL);
        if (!url_base)
                url_base = soup_uri_new (description_url);

        /* Iterate matching devices */
        process_device_list (element,
                             control_point,
                             doc,
                             udn,
                             service_type,
                             description_url,
                             url_base);

        /* Cleanup */
        soup_uri_free (url_base);
}

/*
 * Description URL downloaded.
 */
static void
got_description_url (SoupSession           *session,
                     SoupMessage           *msg,
                     GetDescriptionURLData *data)
{
        GUPnPXMLDoc *doc;

        if (msg->status_code == SOUP_STATUS_CANCELLED)
                return;

        /* Now, make sure again this document is not already cached. If it is,
         * we re-use the cached one. */
        doc = g_hash_table_lookup (data->control_point->priv->doc_cache,
                                   data->description_url);
        if (doc) {
                /* Doc was cached */
                description_loaded (data->control_point,
                                    doc,
                                    data->udn,
                                    data->service_type,
                                    data->description_url);

                get_description_url_data_free (data);

                return;
        }

        /* Not cached */
        if (SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
                xmlDoc *xml_doc;

                /* Parse response */
                xml_doc = xmlRecoverMemory (msg->response_body->data,
                                            msg->response_body->length);
                if (xml_doc) {
                        doc = gupnp_xml_doc_new (xml_doc);

                        description_loaded (data->control_point,
                                            doc,
                                            data->udn,
                                            data->service_type,
                                            data->description_url);

                        /* Insert into document cache */
                        g_hash_table_insert
                                          (data->control_point->priv->doc_cache,
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
        } else
                g_warning ("Failed to GET %s: %s",
                           data->description_url,
                           msg->reason_phrase);

        get_description_url_data_free (data);
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
                  const char        *service_type)
{
        GUPnPXMLDoc *doc;

        doc = g_hash_table_lookup (control_point->priv->doc_cache,
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

                context = gupnp_control_point_get_context (control_point);

                session = gupnp_context_get_session (context);

                data = g_slice_new (GetDescriptionURLData);

                data->message = soup_message_new (SOUP_METHOD_GET,
                                                  description_url);
                if (data->message == NULL) {
                        g_warning ("Invalid description URL: %s",
                                   description_url);

                        g_slice_free (GetDescriptionURLData, data);

                        return;
                }

                http_request_set_user_agent (data->message);
                http_request_set_accept_language (data->message);

                data->control_point   = control_point;

                data->udn             = g_strdup (udn);
                data->service_type    = g_strdup (service_type);
                data->description_url = g_strdup (description_url);

                control_point->priv->pending_gets =
                        g_list_prepend (control_point->priv->pending_gets,
                                        data);

	        soup_session_queue_message (session,
                                            data->message,
                                            (SoupSessionCallback)
                                                   got_description_url,
                                            data);
        }
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
                          service_type);

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
        GList *l, *cur_l;

        control_point = GUPNP_CONTROL_POINT (resource_browser);

        /* Parse USN */
        if (!parse_usn (usn, &udn, &service_type))
                return;

        /* Find proxy */
        if (service_type) {
                l = find_service_node (control_point, udn, service_type);

                if (l) {
                        GUPnPServiceProxy *proxy;

                        /* Remove proxy */
                        proxy = GUPNP_SERVICE_PROXY (l->data);

                        cur_l = l;
                        l = l->next;

                        control_point->priv->services =
                                g_list_delete_link
                                        (control_point->priv->services, cur_l);

                        g_signal_emit (control_point,
                                       signals[SERVICE_PROXY_UNAVAILABLE],
                                       0,
                                       proxy);

                        g_object_unref (proxy);
                }
        } else {
                l = find_device_node (control_point, udn);

                if (l) {
                        GUPnPDeviceProxy *proxy;

                        /* Remove proxy */
                        proxy = GUPNP_DEVICE_PROXY (l->data);

                        cur_l = l;
                        l = l->next;

                        control_point->priv->devices =
                                 g_list_delete_link
                                        (control_point->priv->devices, cur_l);

                        g_signal_emit (control_point,
                                       signals[DEVICE_PROXY_UNAVAILABLE],
                                       0,
                                       proxy);

                        g_object_unref (proxy);
                }
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

        control_point = GUPNP_CONTROL_POINT (object);

        switch (property_id) {
        case PROP_RESOURCE_FACTORY:
                control_point->priv->factory = 
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

        g_type_class_add_private (klass, sizeof (GUPnPControlPointPrivate));

        /**
         * GUPnPControlPoint:resource-factory
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
         * GUPnPControlPoint::device-proxy-available
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
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE,
                              1,
                              GUPNP_TYPE_DEVICE_PROXY);

        /**
         * GUPnPControlPoint::device-proxy-unavailable
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
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE,
                              1,
                              GUPNP_TYPE_DEVICE_PROXY);

        /**
         * GUPnPControlPoint::service-proxy-available
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
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE,
                              1,
                              GUPNP_TYPE_SERVICE_PROXY);

        /**
         * GUPnPControlPoint::service-proxy-unavailable
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
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE,
                              1,
                              GUPNP_TYPE_SERVICE_PROXY);
}

/**
 * gupnp_control_point_new
 * @context: A #GUPnPContext
 * @target: The search target
 *
 * Create a new #GUPnPControlPoint with the specified @context and @target.
 *
 * @target should be a service or device name, such as
 * <literal>urn:schemas-upnp-org:service:WANIPConnection:1</literal> or
 * <literal>urn:schemas-upnp-org:device:MediaRenderer:1</literal>.
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
 * gupnp_control_point_new_full
 * @context: A #GUPnPContext
 * @factory: A #GUPnPResourceFactory
 * @target: The search target
 *
 * Create a new #GUPnPControlPoint with the specified @context, @factory and
 * @target.
 *
 * @target should be a service or device name, such as
 * <literal>urn:schemas-upnp-org:service:WANIPConnection:1</literal> or
 * <literal>urn:schemas-upnp-org:device:MediaRenderer:1</literal>.
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
 * gupnp_control_point_get_context
 * @control_point: A #GUPnPControlPoint
 *
 * Get the #GUPnPControlPoint associated with @control_point.
 *
 * Return value: The #GUPnPContext.
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
 * gupnp_control_point_list_device_proxies
 * @control_point: A #GUPnPControlPoint
 *
 * Get the #GList of discovered #GUPnPDeviceProxy objects. Do not free the list
 * nor its elements.
 *
 * Return value: a #GList of #GUPnPDeviceProxy objects.
 **/
const GList *
gupnp_control_point_list_device_proxies (GUPnPControlPoint *control_point)
{
        g_return_val_if_fail (GUPNP_IS_CONTROL_POINT (control_point), NULL);

        return (const GList *) control_point->priv->devices;
}

/**
 * gupnp_control_point_list_service_proxies
 * @control_point: A #GUPnPControlPoint
 *
 * Get the #GList of discovered #GUPnPServiceProxy objects. Do not free the list
 * nor its elements.
 *
 * Return value: a #GList of #GUPnPServiceProxy objects.
 **/
const GList *
gupnp_control_point_list_service_proxies (GUPnPControlPoint *control_point)
{
        g_return_val_if_fail (GUPNP_IS_CONTROL_POINT (control_point), NULL);

        return (const GList *) control_point->priv->services;
}

/**
 * gupnp_control_point_get_resource_factory
 * @control_point: A #GUPnPControlPoint
 *
 * Get the #GUPnPResourceFactory used by the @control_point.
 *
 * Return value: A #GUPnPResourceFactory.
 **/
GUPnPResourceFactory *
gupnp_control_point_get_resource_factory (GUPnPControlPoint *control_point)
{
        g_return_val_if_fail (GUPNP_IS_CONTROL_POINT (control_point), NULL);

        if (control_point->priv->factory)
                  return control_point->priv->factory;

        return gupnp_resource_factory_get_default ();
}
