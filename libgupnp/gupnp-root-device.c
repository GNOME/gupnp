/*
 * Copyright (C) 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

/**
 * SECTION:gupnp-root-device
 * @short_description: Class for root device implementations.
 *
 * #GUPnPRootDevice allows for implementing root devices.
 */

#include <config.h>
#include <string.h>

#include <libgssdp/gssdp-resource-group.h>

#include "gupnp-root-device.h"
#include "gupnp-context-private.h"
#include "gupnp-error.h"
#include "http-headers.h"
#include "xml-util.h"

static void
gupnp_root_device_initable_iface_init (gpointer g_iface,
                                       gpointer iface_data);

static gboolean
gupnp_root_device_initable_init (GInitable     *initable,
                                 GCancellable  *cancellable,
                                 GError       **error);

struct _GUPnPRootDevicePrivate {
        GUPnPXMLDoc *description_doc;

        GSSDPResourceGroup *group;

        char  *description_path;
        char  *description_dir;
        char  *relative_location;
};
typedef struct _GUPnPRootDevicePrivate GUPnPRootDevicePrivate;

G_DEFINE_TYPE_EXTENDED (GUPnPRootDevice,
                        gupnp_root_device,
                        GUPNP_TYPE_DEVICE,
                        0,
                        G_ADD_PRIVATE(GUPnPRootDevice)
                        G_IMPLEMENT_INTERFACE
                                (G_TYPE_INITABLE,
                                 gupnp_root_device_initable_iface_init))

enum {
        PROP_0,
        PROP_DESCRIPTION_DOC,
        PROP_DESCRIPTION_PATH,
        PROP_DESCRIPTION_DIR,
        PROP_AVAILABLE
};

static void
gupnp_root_device_finalize (GObject *object)
{
        GUPnPRootDevice *device;
        GUPnPRootDevicePrivate *priv;
        GObjectClass *object_class;

        device = GUPNP_ROOT_DEVICE (object);
        priv = gupnp_root_device_get_instance_private (device);

        g_clear_object (&priv->description_doc);
        g_free (priv->description_path);
        g_free (priv->description_dir);
        g_free (priv->relative_location);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_root_device_parent_class);
        object_class->finalize (object);
}

static void
gupnp_root_device_dispose (GObject *object)
{
        GUPnPRootDevice *device;
        GUPnPRootDevicePrivate *priv;
        GObjectClass *object_class;

        device = GUPNP_ROOT_DEVICE (object);
        priv = gupnp_root_device_get_instance_private (device);

        if (priv->group) {
                g_object_unref (priv->group);
                priv->group = NULL;
        }

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_root_device_parent_class);
        object_class->dispose (object);
}

static void
gupnp_root_device_init (GUPnPRootDevice *device)
{
}

static void
gupnp_root_device_initable_iface_init (gpointer g_iface,
                                       gpointer iface_data)
{
        GInitableIface *iface = (GInitableIface *) g_iface;
        iface->init = gupnp_root_device_initable_init;
}

static void
gupnp_root_device_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        GUPnPRootDevice *device;
        GUPnPRootDevicePrivate *priv;

        device = GUPNP_ROOT_DEVICE (object);
        priv = gupnp_root_device_get_instance_private (device);

        switch (property_id) {
        case PROP_DESCRIPTION_DOC:
                priv->description_doc = g_value_dup_object (value);
                break;
        case PROP_DESCRIPTION_PATH:
                priv->description_path = g_value_dup_string (value);
                break;
        case PROP_DESCRIPTION_DIR:
                priv->description_dir = g_value_dup_string (value);
                break;
        case PROP_AVAILABLE:
                gupnp_root_device_set_available
                                        (device, g_value_get_boolean (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_root_device_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        GUPnPRootDevice *device;
        GUPnPRootDevicePrivate *priv;

        device = GUPNP_ROOT_DEVICE (object);
        priv = gupnp_root_device_get_instance_private (device);

        switch (property_id) {
        case PROP_DESCRIPTION_PATH:
                g_value_set_string (value,
                                    priv->description_path);
                break;
        case PROP_DESCRIPTION_DIR:
                g_value_set_string (value,
                                    priv->description_dir);
                break;
        case PROP_AVAILABLE:
                g_value_set_boolean (value,
                                     gupnp_root_device_get_available (device));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
fill_resource_group (xmlNode            *element,
                     const char         *location,
                     GSSDPResourceGroup *group)
{
        xmlNode *child;
        xmlChar *udn, *device_type;
        char *usn;

        /* Add device */
        udn = xml_util_get_child_element_content (element, "UDN");
        if (!udn) {
                g_warning ("No UDN specified.");

                return;
        }

        device_type = xml_util_get_child_element_content (element,
                                                          "deviceType");
        if (!device_type) {
                g_warning ("No deviceType specified.");

                return;
        }

        gssdp_resource_group_add_resource_simple (group,
                                                  (const char *) udn,
                                                  (const char *) udn,
                                                  location);

        usn = g_strdup_printf ("%s::%s", (char *) udn, (char *) device_type);
        gssdp_resource_group_add_resource_simple (group,
                                                  (const char *) device_type,
                                                  (const char *) usn,
                                                  location);
        g_free (usn);

        xmlFree (device_type);

        /* Add embedded services */
        child = xml_util_get_element (element,
                                      "serviceList",
                                      NULL);
        if (child) {
                for (child = child->children; child; child = child->next) {
                        xmlChar *service_type;

                        if (strcmp ("service", (char *) child->name))
                                continue;

                        service_type = xml_util_get_child_element_content
                                                (child, "serviceType");
                        if (!service_type)
                                continue;

                        usn = g_strdup_printf ("%s::%s",
                                               (char *) udn,
                                               (char *) service_type);
                        gssdp_resource_group_add_resource_simple
                                                (group,
                                                 (const char *) service_type,
                                                 (const char *) usn,
                                                 location);
                        g_free (usn);

                        xmlFree (service_type);
                }
        }

        /* Cleanup */
        xmlFree (udn);

        /* Add embedded devices */
        child = xml_util_get_element (element,
                                      "deviceList",
                                      NULL);
        if (child) {
                for (child = child->children; child; child = child->next)
                        if (!strcmp ("device", (char *) child->name))
                                fill_resource_group (child, location, group);
        }
}

/* Load and parse @description_path as a XML document, synchronously. */
static GUPnPXMLDoc *
load_and_parse (const char *description_path)
{
        GUPnPXMLDoc *description_doc;
        GError *error = NULL;

        /* We don't, so load and parse it */
        description_doc = gupnp_xml_doc_new_from_path (description_path,
                                                       &error);
        if (description_doc == NULL) {
                g_critical ("Error loading description: %s\n", error->message);

                g_error_free (error);
        }

        return description_doc;
}

static gboolean
gupnp_root_device_initable_init (GInitable     *initable,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
        GUPnPRootDevice *device;
        GUPnPContext *context;
        const char *udn;
        SoupURI *uri;
        char *desc_path, *location, *usn, *relative_location;
        xmlNode *root_element, *element;
        SoupURI *url_base;
        gboolean result = FALSE;
        GUPnPRootDevicePrivate *priv;

        device = GUPNP_ROOT_DEVICE (initable);
        priv = gupnp_root_device_get_instance_private (device);

        location = NULL;

        context = gupnp_device_info_get_context (GUPNP_DEVICE_INFO (device));
        if (context == NULL) {
                g_set_error_literal (error,
                                     GUPNP_ROOT_DEVICE_ERROR,
                                     GUPNP_ROOT_DEVICE_ERROR_NO_CONTEXT,
                                     "No context specified");

                return FALSE;
        }

        if (priv->description_path == NULL) {
                g_set_error_literal (error,
                                     GUPNP_ROOT_DEVICE_ERROR,
                                     GUPNP_ROOT_DEVICE_ERROR_NO_DESCRIPTION_PATH,
                                     "Path to description document not specified.");

                return FALSE;
        }

        if (priv->description_dir == NULL) {
                g_set_error_literal (error,
                                     GUPNP_ROOT_DEVICE_ERROR,
                                     GUPNP_ROOT_DEVICE_ERROR_NO_DESCRIPTION_FOLDER,
                                     "Path to description directory not specified.");

                return FALSE;
        }

        uri = _gupnp_context_get_server_uri (context);
        if (uri == NULL) {
                g_set_error_literal (error,
                                     GUPNP_ROOT_DEVICE_ERROR,
                                     GUPNP_ROOT_DEVICE_ERROR_NO_NETWORK,
                                     "Network interface is not usable");

                return FALSE;
        }

        if (g_path_is_absolute (priv->description_path))
                desc_path = g_strdup (priv->description_path);
        else
                desc_path = g_build_filename (priv->description_dir,
                                              priv->description_path,
                                              NULL);

        /* Check whether we have a parsed description document */
        if (priv->description_doc == NULL) {
                /* We don't, so load and parse it */
                priv->description_doc = load_and_parse (desc_path);
                if (priv->description_doc == NULL) {
                        g_set_error_literal (error,
                                             GUPNP_XML_ERROR,
                                             GUPNP_XML_ERROR_PARSE,
                                             "Coupld not parse description document");

                        goto DONE;
                }
        }

        /* Find correct element */
        root_element = xml_util_get_element ((xmlNode *)
                                             gupnp_xml_doc_get_doc (priv->description_doc),
                                             "root",
                                             NULL);
        if (!root_element) {
                g_set_error_literal (error,
                                     GUPNP_XML_ERROR,
                                     GUPNP_XML_ERROR_NO_NODE,
                                     "\"/root\" element not found.");

                goto DONE;
        }

        element = xml_util_get_element (root_element,
                                        "device",
                                        NULL);
        if (!element) {
                g_set_error_literal (error,
                                     GUPNP_XML_ERROR,
                                     GUPNP_XML_ERROR_NO_NODE,
                                     "\"/root/device\" element not found.");

                goto DONE;
        }

        g_object_set (G_OBJECT (device),
                      "element", element,
                      NULL);

        /* Generate location relative to HTTP root */
        udn = gupnp_device_info_get_udn (GUPNP_DEVICE_INFO (device));
        if (udn && strstr (udn, "uuid:") == udn)
                priv->relative_location = g_strdup_printf ("%s.xml", udn + 5);
        else
                priv->relative_location = g_strdup_printf ("RootDevice%p.xml", device);

        relative_location = g_strjoin (NULL,
                                       "/",
                                       priv->relative_location,
                                       NULL);

        /* Host the description file and dir */
        gupnp_context_host_path (context, desc_path, relative_location);
        gupnp_context_host_path (context, priv->description_dir, "");

        /* Generate full location */
        soup_uri_set_path (uri, relative_location);
        location = soup_uri_to_string (uri, FALSE);

        g_free (relative_location);

        /* Save the URL base, if any */
        url_base = xml_util_get_child_element_content_uri (root_element,
                                                           "URLBase",
                                                           NULL);
        if (!url_base)
                url_base = soup_uri_new (location);

        /* Set additional properties */
        g_object_set (G_OBJECT (device),
                      "location", location,
                      "url-base", url_base,
                      NULL);

        soup_uri_free (url_base);

        /* Create resource group */
        priv->group = gssdp_resource_group_new (GSSDP_CLIENT (context));

        /* Add services and devices to resource group */
        usn = g_strdup_printf ("%s::upnp:rootdevice", (const char *) udn);
        gssdp_resource_group_add_resource_simple (priv->group,
                                                  "upnp:rootdevice",
                                                  usn,
                                                  location);
        g_free (usn);

        fill_resource_group (element, location, priv->group);

        result = TRUE;

 DONE:
        /* Cleanup */
        if (uri)
                soup_uri_free (uri);

        g_free (desc_path);
        g_free (location);

        return result;
}

static void
gupnp_root_device_class_init (GUPnPRootDeviceClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_root_device_set_property;
        object_class->get_property = gupnp_root_device_get_property;
        object_class->dispose      = gupnp_root_device_dispose;
        object_class->finalize     = gupnp_root_device_finalize;

        /**
         * GUPnPRootDevice:description-doc:
         *
         * Device description document. Constructor property.
         *
         * Since: 0.14.0
         **/
        g_object_class_install_property
                (object_class,
                 PROP_DESCRIPTION_DOC,
                 g_param_spec_object ("description-doc",
                                      "Description document",
                                      "Device description document",
                                      GUPNP_TYPE_XML_DOC,
                                      G_PARAM_WRITABLE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPRootDevice:description-path:
         *
         * The path to device description document. This could either be an
         * absolute path or path relative to GUPnPRootDevice:description-dir.
         *
         * Since: 0.13.0
         **/
        g_object_class_install_property
                (object_class,
                 PROP_DESCRIPTION_PATH,
                 g_param_spec_string ("description-path",
                                      "Description Path",
                                      "The path to device descrition document",
                                      NULL,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPRootDevice:description-dir:
         *
         * The path to directory where description documents are provided.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_DESCRIPTION_DIR,
                 g_param_spec_string ("description-dir",
                                      "Description Directory",
                                      "The path to directory where "
                                      "description documents are provided",
                                      NULL,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPRootDevice:available:
         *
         * TRUE if this device is available.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_AVAILABLE,
                 g_param_spec_boolean ("available",
                                       "Available",
                                       "Whether this device is available",
                                       FALSE,
                                       G_PARAM_READWRITE |
                                       G_PARAM_STATIC_NAME |
                                       G_PARAM_STATIC_NICK |
                                       G_PARAM_STATIC_BLURB));
}

/**
 * gupnp_root_device_new:
 * @context: The #GUPnPContext
 * @description_path: Path to device description document. This could either
 * be an absolute path or path relative to @description_dir.
 * @description_dir: Path to directory where description documents are provided.
 * @error: (inout)(optional)(nullable): The location for a #GError to report issue with
 * creation on or %NULL.
 *
 * Create a new #GUPnPRootDevice object, automatically loading and parsing
 * device description document from @description_path.
 *
 * Return value: A new @GUPnPRootDevice object.
 **/
GUPnPRootDevice *
gupnp_root_device_new (GUPnPContext *context,
                       const char   *description_path,
                       const char   *description_dir,
                       GError      **error)
{
        GUPnPResourceFactory *factory;

        factory = gupnp_resource_factory_get_default ();

        return gupnp_root_device_new_full (context,
                                           factory,
                                           NULL,
                                           description_path,
                                           description_dir,
                                           error);
}

/**
 * gupnp_root_device_new_full:
 * @context: A #GUPnPContext
 * @factory: A #GUPnPResourceFactory
 * @description_doc: Device description document, or %NULL
 * @description_path: Path to device description document. This could either
 * be an absolute path or path relative to @description_dir.
 * @description_dir: Path to directory where description documents are provided.
 * @error: (inout)(optional)(nullable): The location for a #GError to report issue with
 * creation on or %NULL.
 *
 * Create a new #GUPnPRootDevice, automatically loading and parsing
 * device description document from @description_path if @description_doc is
 * %NULL.
 *
 * Return value: A new #GUPnPRootDevice object.
 **/
GUPnPRootDevice *
gupnp_root_device_new_full (GUPnPContext         *context,
                            GUPnPResourceFactory *factory,
                            GUPnPXMLDoc          *description_doc,
                            const char           *description_path,
                            const char           *description_dir,
                            GError              **error)
{
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        g_return_val_if_fail (GUPNP_IS_RESOURCE_FACTORY (factory), NULL);

        return g_initable_new (GUPNP_TYPE_ROOT_DEVICE,
                               NULL,
                               error,
                               "context", context,
                               "resource-factory", factory,
                               "root-device", NULL,
                               "description-doc", description_doc,
                               "description-path", description_path,
                               "description-dir", description_dir,
                               NULL);
}

/**
 * gupnp_root_device_set_available:
 * @root_device: A #GUPnPRootDevice
 * @available: %TRUE if @root_device should be available
 *
 * Controls whether or not @root_device is available (announcing
 * its presence).
 **/
void
gupnp_root_device_set_available (GUPnPRootDevice *root_device,
                                 gboolean         available)
{
        GUPnPRootDevicePrivate *priv;

        g_return_if_fail (GUPNP_IS_ROOT_DEVICE (root_device));

        priv = gupnp_root_device_get_instance_private (root_device);

        gssdp_resource_group_set_available (priv->group, available);

        g_object_notify (G_OBJECT (root_device), "available");
}

/**
 * gupnp_root_device_get_available:
 * @root_device: A #GUPnPRootDevice
 *
 * Get whether or not @root_device is available (announcing its presence).
 *
 * Return value: %TRUE if @root_device is available, %FALSE otherwise.
 **/
gboolean
gupnp_root_device_get_available (GUPnPRootDevice *root_device)
{
        GUPnPRootDevicePrivate *priv;

        g_return_val_if_fail (GUPNP_IS_ROOT_DEVICE (root_device), FALSE);

        priv = gupnp_root_device_get_instance_private (root_device);

        return gssdp_resource_group_get_available (priv->group);
}

/**
 * gupnp_root_device_get_relative_location:
 * @root_device: A #GUPnPRootDevice
 *
 * Get the relative location of @root_device.
 *
 * Return value: The relative location of @root_device.
 **/
const char *
gupnp_root_device_get_relative_location (GUPnPRootDevice *root_device)
{
        GUPnPRootDevicePrivate *priv;

        g_return_val_if_fail (GUPNP_IS_ROOT_DEVICE (root_device), NULL);

        priv = gupnp_root_device_get_instance_private (root_device);

        return priv->relative_location;
}

/**
 * gupnp_root_device_get_description_path:
 * @root_device: A #GUPnPRootDevice
 *
 * Get the path to the device description document of @root_device.
 *
 * Return value: The path to device description document of @root_device.
 **/
const char *
gupnp_root_device_get_description_path (GUPnPRootDevice *root_device)
{
        GUPnPRootDevicePrivate *priv;

        g_return_val_if_fail (GUPNP_IS_ROOT_DEVICE (root_device), NULL);

        priv = gupnp_root_device_get_instance_private (root_device);

        return priv->description_path;
}

/**
 * gupnp_root_device_get_description_dir:
 * @root_device: A #GUPnPRootDevice
 *
 * Get the path to the directory containing description documents related to
 * @root_device.
 *
 * Return value: The path to description document directory of @root_device.
 **/
const char *
gupnp_root_device_get_description_dir (GUPnPRootDevice *root_device)
{
        GUPnPRootDevicePrivate *priv;

        g_return_val_if_fail (GUPNP_IS_ROOT_DEVICE (root_device), NULL);

        priv = gupnp_root_device_get_instance_private (root_device);

        return priv->description_dir;
}

/**
 * gupnp_root_device_get_ssdp_resource_group:
 * @root_device: A #GUPnPRootDevice
 *
 * Get the #GSSDPResourceGroup used by @root_device.
 *
 * Returns: (transfer none): The #GSSDPResourceGroup of @root_device.
 *
 * Since: 0.20.0
 **/
GSSDPResourceGroup *
gupnp_root_device_get_ssdp_resource_group (GUPnPRootDevice *root_device)
{
        GUPnPRootDevicePrivate *priv;

        g_return_val_if_fail (GUPNP_IS_ROOT_DEVICE (root_device), NULL);

        priv = gupnp_root_device_get_instance_private (root_device);

        return priv->group;
}
