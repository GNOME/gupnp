/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

/**
 * SECTION:gupnp-device-info
 * @short_description: Base abstract class for querying device information.
 *
 * The #GUPnPDeviceInfo base abstract class provides methods for querying
 * device information.
 */

#include <config.h>
#include <string.h>

#include "gupnp-device-info.h"
#include "gupnp-device-info-private.h"
#include "gupnp-resource-factory-private.h"
#include "xml-util.h"

struct _GUPnPDeviceInfoPrivate {
        GUPnPResourceFactory *factory;
        GUPnPContext         *context;

        char *location;
        char *udn;
        char *device_type;

        GUri *url_base;

        GUPnPXMLDoc *doc;

        xmlNode *element;
};
typedef struct _GUPnPDeviceInfoPrivate GUPnPDeviceInfoPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GUPnPDeviceInfo,
                                     gupnp_device_info,
                                     G_TYPE_OBJECT)


enum {
        PROP_0,
        PROP_RESOURCE_FACTORY,
        PROP_CONTEXT,
        PROP_LOCATION,
        PROP_UDN,
        PROP_DEVICE_TYPE,
        PROP_URL_BASE,
        PROP_DOCUMENT,
        PROP_ELEMENT
};

static void
gupnp_device_info_init (GUPnPDeviceInfo *info)
{
}

static void
gupnp_device_info_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        GUPnPDeviceInfo *info;
        GUPnPDeviceInfoPrivate *priv;

        info = GUPNP_DEVICE_INFO (object);
        priv = gupnp_device_info_get_instance_private (info);

        switch (property_id) {
        case PROP_RESOURCE_FACTORY:
                priv->factory =
                        GUPNP_RESOURCE_FACTORY (g_value_dup_object (value));
                break;
        case PROP_CONTEXT:
                priv->context = g_value_dup_object (value);
                break;
        case PROP_LOCATION:
                priv->location = g_value_dup_string (value);
                break;
        case PROP_UDN:
                priv->udn = g_value_dup_string (value);
                break;
        case PROP_DEVICE_TYPE:
                priv->device_type = g_value_dup_string (value);
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
gupnp_device_info_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        GUPnPDeviceInfo *info;
        GUPnPDeviceInfoPrivate *priv;

        info = GUPNP_DEVICE_INFO (object);
        priv = gupnp_device_info_get_instance_private (info);

        switch (property_id) {
        case PROP_RESOURCE_FACTORY:
                g_value_set_object (value,
                                    priv->factory);
                break;
        case PROP_CONTEXT:
                g_value_set_object (value,
                                    priv->context);
                break;
        case PROP_LOCATION:
                g_value_set_string (value,
                                    priv->location);
                break;
        case PROP_UDN:
                g_value_set_string (value,
                                    gupnp_device_info_get_udn (info));
                break;
        case PROP_DEVICE_TYPE:
                g_value_set_string (value,
                                    gupnp_device_info_get_device_type (info));
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
gupnp_device_info_dispose (GObject *object)
{
        GUPnPDeviceInfo *info;
        GUPnPDeviceInfoPrivate *priv;

        info = GUPNP_DEVICE_INFO (object);
        priv = gupnp_device_info_get_instance_private (info);

        g_clear_object (&priv->factory);
        g_clear_object (&priv->factory);
        g_clear_object (&priv->context);
        g_clear_object (&priv->doc);

        G_OBJECT_CLASS (gupnp_device_info_parent_class)->dispose (object);
}

static void
gupnp_device_info_finalize (GObject *object)
{
        GUPnPDeviceInfo *info;
        GUPnPDeviceInfoPrivate *priv;

        info = GUPNP_DEVICE_INFO (object);
        priv = gupnp_device_info_get_instance_private (info);

        g_free (priv->location);
        g_free (priv->udn);
        g_free (priv->device_type);

        g_clear_pointer (&priv->url_base, g_uri_unref);

        G_OBJECT_CLASS (gupnp_device_info_parent_class)->finalize (object);
}

static void
gupnp_device_info_class_init (GUPnPDeviceInfoClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_device_info_set_property;
        object_class->get_property = gupnp_device_info_get_property;
        object_class->dispose      = gupnp_device_info_dispose;
        object_class->finalize     = gupnp_device_info_finalize;

        /**
         * GUPnPDeviceInfo:resource-factory:
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
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPDeviceInfo:context:
         *
         * The #GUPnPContext to use.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_CONTEXT,
                 g_param_spec_object ("context",
                                      "Context",
                                      "The GUPnPContext",
                                      GUPNP_TYPE_CONTEXT,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPDeviceInfo:location:
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
                                      G_PARAM_CONSTRUCT |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPDeviceInfo:udn:
         *
         * The UDN of this device.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_UDN,
                 g_param_spec_string ("udn",
                                      "UDN",
                                      "The UDN",
                                      NULL,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPDeviceInfo:device-type:
         *
         * The device type.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_DEVICE_TYPE,
                 g_param_spec_string ("device-type",
                                      "Device type",
                                      "The device type",
                                      NULL,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPDeviceInfo:url-base:
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
                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                            G_PARAM_STATIC_NAME |
                                            G_PARAM_STATIC_NICK |
                                            G_PARAM_STATIC_BLURB));

        /**
         * GUPnPDeviceInfo:document:
         *
         * Private property.
         *
         * Stability: Private
         **/
        g_object_class_install_property (
                object_class,
                PROP_DOCUMENT,
                g_param_spec_object ("document",
                                     "Document",
                                     "The XML document related to this "
                                     "device",
                                     GUPNP_TYPE_XML_DOC,
                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                             G_PARAM_STATIC_NAME |
                                             G_PARAM_STATIC_NICK |
                                             G_PARAM_STATIC_BLURB));

        /**
         * GUPnPDeviceInfo:element:
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
                                       G_PARAM_CONSTRUCT |
                                       G_PARAM_STATIC_NAME |
                                       G_PARAM_STATIC_NICK |
                                       G_PARAM_STATIC_BLURB));
}

/**
 * gupnp_device_info_get_resource_factory:
 * @device_info: A #GUPnPDeviceInfo
 *
 * Get the #GUPnPResourceFactory used by the @device_info.
 *
 * Returns: (transfer none): A #GUPnPResourceFactory.
 **/
GUPnPResourceFactory *
gupnp_device_info_get_resource_factory (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return priv->factory;
}

/**
 * gupnp_device_info_get_context:
 * @info: A #GUPnPDeviceInfo
 *
 * Get the associated #GUPnPContext.
 *
 * Returns: (transfer none): A #GUPnPContext.
 **/
GUPnPContext *
gupnp_device_info_get_context (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return priv->context;
}

/**
 * gupnp_device_info_get_location:
 * @info: A #GUPnPDeviceInfo
 *
 * Get the location of the device description file.
 *
 * Returns: A constant string.
 **/
const char *
gupnp_device_info_get_location (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return priv->location;
}

/**
 * gupnp_device_info_get_url_base:
 * @info: A #GUPnPDeviceInfo
 *
 * Get the URL base of this device.
 *
 * Returns: A #SoupURI.
 **/
const GUri *
gupnp_device_info_get_url_base (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return priv->url_base;
}

/**
 * gupnp_device_info_get_udn:
 * @info: A #GUPnPDeviceInfo
 *
 * Get the Unique Device Name of the device.
 *
 * Returns: A constant string.
 **/
const char *
gupnp_device_info_get_udn (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);
        if (!priv->udn) {
                priv->udn =
                        xml_util_get_child_element_content_glib
                                (priv->element, "UDN");
        }

        return priv->udn;
}

/**
 * gupnp_device_info_get_device_type:
 * @info: A #GUPnPDeviceInfo
 *
 * Get the UPnP device type.
 *
 * Returns: A constant string, or %NULL.
 **/
const char *
gupnp_device_info_get_device_type (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);
        if (!priv->device_type) {
                priv->device_type =
                        xml_util_get_child_element_content_glib
                                (priv->element, "deviceType");
        }

        return priv->device_type;
}

/**
 * gupnp_device_info_get_friendly_name:
 * @info: A #GUPnPDeviceInfo
 *
 * Get the friendly name of the device.
 *
 * Return value: A string, or %NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_friendly_name (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return xml_util_get_child_element_content_glib (priv->element,
                                                        "friendlyName");
}

/**
 * gupnp_device_info_get_manufacturer:
 * @info: A #GUPnPDeviceInfo
 *
 * Get the manufacturer of the device.
 *
 * Return value:(nullable)(transfer full): A string, or %NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_manufacturer (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return xml_util_get_child_element_content_glib (priv->element,
                                                        "manufacturer");
}

/**
 * gupnp_device_info_get_manufacturer_url:
 * @info: A #GUPnPDeviceInfo
 *
 * Get an URL pointing to the manufacturer's website.
 *
 * Return value:(nullable)(transfer full): A string, or %NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_manufacturer_url (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return xml_util_get_child_element_content_url (priv->element,
                                                       "manufacturerURL",
                                                       priv->url_base);
}

/**
 * gupnp_device_info_get_model_description:
 * @info: A #GUPnPDeviceInfo
 *
 * Get the description of the device model.
 *
 * Return value:(nullable)(transfer full): A string, or %NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_model_description (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return xml_util_get_child_element_content_glib (priv->element,
                                                        "modelDescription");
}

/**
 * gupnp_device_info_get_model_name:
 * @info: A #GUPnPDeviceInfo
 *
 * Get the model name of the device.
 *
 * Return value:(nullable)(transfer full): A string, or %NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_model_name (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return xml_util_get_child_element_content_glib (priv->element,
                                                        "modelName");
}

/**
 * gupnp_device_info_get_model_number:
 * @info: A #GUPnPDeviceInfo
 *
 * Get the model number of the device.
 *
 * Return value:(nullable)(transfer full): A string, or %NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_model_number (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return xml_util_get_child_element_content_glib (priv->element,
                                                        "modelNumber");
}

/**
 * gupnp_device_info_get_model_url:
 * @info: A #GUPnPDeviceInfo
 *
 * Get an URL pointing to the device model's website.
 *
 * Return value:(nullable)(transfer full): A string, or %NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_model_url (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return xml_util_get_child_element_content_url (priv->element,
                                                       "modelURL",
                                                       priv->url_base);
}

/**
 * gupnp_device_info_get_serial_number:
 * @info: A #GUPnPDeviceInfo
 *
 * Get the serial number of the device.
 *
 * Return value:(nullable)(transfer full): A string, or %NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_serial_number (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return xml_util_get_child_element_content_glib (priv->element,
                                                        "serialNumber");
}

/**
 * gupnp_device_info_get_upc:
 * @info: A #GUPnPDeviceInfo
 *
 * Get the Universal Product Code of the device.
 *
 * Return value:(nullable)(transfer full): A string, or %NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_upc (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return xml_util_get_child_element_content_glib (priv->element,
                                                        "UPC");
}

/**
 * gupnp_device_info_get_presentation_url:
 * @info: A #GUPnPDeviceInfo
 *
 * Get an URL pointing to the device's presentation page, for web-based
 * administration.
 *
 * Return value:(nullable)(transfer full): A string, or %NULL. g_free() after use.
 **/
char *
gupnp_device_info_get_presentation_url (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return xml_util_get_child_element_content_url (priv->element,
                                                       "presentationURL",
                                                       priv->url_base);
}

typedef struct {
        xmlChar *mime_type;
        int      width;
        int      height;
        int      depth;
        xmlChar *url;

        int      weight;
} Icon;

static Icon *
icon_parse (xmlNode *element)
{
        Icon *icon;

        icon = g_slice_new0 (Icon);

        icon->mime_type = xml_util_get_child_element_content     (element,
                                                                  "mimetype");
        icon->width     = xml_util_get_child_element_content_int (element,
                                                                  "width");
        icon->height    = xml_util_get_child_element_content_int (element,
                                                                  "height");
        icon->depth     = xml_util_get_child_element_content_int (element,
                                                                  "depth");
        icon->url       = xml_util_get_child_element_content     (element,
                                                                  "url");

        return icon;
}

static void
icon_free (Icon *icon)
{
        if (icon->mime_type)
                xmlFree (icon->mime_type);

        if (icon->url)
                xmlFree (icon->url);

        g_slice_free (Icon, icon);
}

/**
 * gupnp_device_info_get_icon_url:
 * @info: A #GUPnPDeviceInfo
 * @requested_mime_type: (nullable) (transfer none): The requested file
 * format, or %NULL for any
 * @requested_depth: The requested color depth, or -1 for any
 * @requested_width: The requested width, or -1 for any
 * @requested_height: The requested height, or -1 for any
 * @prefer_bigger: %TRUE if a bigger, rather than a smaller icon should be
 * returned if no exact match could be found
 * @mime_type: (out) (optional): The location where to store the the format
 * of the returned icon, or %NULL. The returned string should be freed after
 * use
 * @depth: (out) (optional):  The location where to store the depth of the
 * returned icon, or %NULL
 * @width: (out) (optional): The location where to store the width of the
 * returned icon, or %NULL
 * @height: (out) (optional): The location where to store the height of the
 * returned icon, or %NULL
 *
 * Get an URL pointing to the icon most closely matching the
 * given criteria, or %NULL. If @requested_mime_type is set, only icons with
 * this mime type will be returned. If @requested_depth is set, only icons with
 * this or lower depth will be returned. If @requested_width and/or
 * @requested_height are set, only icons that are this size or smaller are
 * returned, unless @prefer_bigger is set, in which case the next biggest icon
 * will be returned. The returned strings should be freed.
 *
 * Return value:(nullable)(transfer full): a string, or %NULL.  g_free() after use.
 **/
char *
gupnp_device_info_get_icon_url (GUPnPDeviceInfo *info,
                                const char      *requested_mime_type,
                                int              requested_depth,
                                int              requested_width,
                                int              requested_height,
                                gboolean         prefer_bigger,
                                char           **mime_type,
                                int             *depth,
                                int             *width,
                                int             *height)
{
        GList *icons, *l;
        xmlNode *element;
        Icon *icon, *closest;
        char *ret;
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        /* List available icons */
        icons = NULL;

        element = xml_util_get_element (priv->element,
                                        "iconList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("icon", (char *) element->name)) {
                        gboolean mime_type_ok;

                        icon = icon_parse (element);

                        if (requested_mime_type) {
                                if (icon->mime_type)
                                        mime_type_ok = !strcmp (
                                                requested_mime_type,
                                                (char *) icon->mime_type);
                                else
                                        mime_type_ok = FALSE;
                        } else
                                mime_type_ok = TRUE;

                        if (requested_depth >= 0)
                                icon->weight = requested_depth - icon->depth;

                        /* Filter out icons with incorrect mime type or
                         * incorrect depth. */
                        /* Note: Meaning of 'weight' changes when no
                         * size request is included. */
                        if (mime_type_ok && icon->weight >= 0) {
                                if (requested_width < 0 && requested_height < 0) {
                                        icon->weight = icon->width * icon->height;
                                } else {
                                        if (requested_width >= 0) {
                                                if (prefer_bigger) {
                                                        icon->weight +=
                                                                icon->width -
                                                                requested_width;
                                                } else {
                                                        icon->weight +=
                                                                requested_width -
                                                                icon->width;
                                                }
                                        }

                                        if (requested_height >= 0) {
                                                if (prefer_bigger) {
                                                        icon->weight +=
                                                                icon->height -
                                                                requested_height;
                                                } else {
                                                        icon->weight +=
                                                                requested_height -
                                                                icon->height;
                                                }
                                        }
                                }
                                icons = g_list_prepend (icons, icon);
                        } else
                                icon_free (icon);
                }
        }

        if (icons == NULL)
                return NULL;

        /* If no size was requested, find the largest or smallest */
        closest = NULL;
        if (requested_height < 0 && requested_width < 0) {
                for (l = icons; l; l = l->next) {
                        icon = l->data;

                        if (!closest ||
                            (prefer_bigger && icon->weight > closest->weight) ||
                            (!prefer_bigger && icon->weight < closest->weight))
                                closest = icon;
                }
        }

        /* Find the match closest to requested size */
        if (!closest) {
                for (l = icons; l; l = l->next) {
                        icon = l->data;

                        /* Look between icons with positive weight first */
                        if (icon->weight >= 0) {
                                if (!closest || icon->weight < closest->weight)
                                        closest = icon;
                        }
                }
        }

        if (!closest) {
                for (l = icons; l; l = l->next) {
                        icon = l->data;

                        /* No icons with positive weight, look at ones with
                         * negative weight */
                        if (!closest || icon->weight > closest->weight)
                                closest = icon;
                }
        }

        /* Fill in return values */
        if (closest) {
                icon = closest;

                if (mime_type) {
                        if (icon->mime_type) {
                                *mime_type = g_strdup
                                                ((char *) icon->mime_type);
                        } else
                                *mime_type = NULL;
                }

                if (depth)
                        *depth = icon->depth;
                if (width)
                        *width = icon->width;
                if (height)
                        *height = icon->height;

                if (icon->url) {
                        GUri *uri;

                        uri = g_uri_parse_relative (priv->url_base,
                                                    (const char *) icon->url,
                                                    G_URI_FLAGS_NONE,
                                                    NULL);
                        ret = g_uri_to_string_partial (uri,
                                                       G_URI_HIDE_PASSWORD);
                        g_uri_unref (uri);
                } else
                        ret = NULL;
        } else {
                if (mime_type)
                        *mime_type = NULL;
                if (depth)
                        *depth = -1;
                if (width)
                        *width = -1;
                if (height)
                        *height = -1;

                ret = NULL;
        }

        /* Cleanup */
        g_list_free_full (icons, (GDestroyNotify) icon_free);

        return ret;
}

/* Returns TRUE if @query matches against @base.
 * - If @query does not specify a version, it matches any version specified
 *   in @base.
 * - If @query specifies a version, it matches any version specified in @base
 *   that is greater or equal.
 * */
static gboolean
resource_type_match (const char *query,
                     const char *base)
{
        gboolean match;
        guint type_len;
        char *colon;
        guint query_ver, base_ver;

        /* Inspect last colon (if any!) on @base */
        colon = strrchr (base, ':');
        if (G_UNLIKELY (colon == NULL))
                return !strcmp (query, base); /* No colon */

        /* Length of type until last colon */
        type_len = strlen (base) - strlen (colon);

        /* Match initial portions */
        match = (strncmp (query, base, type_len) == 0);
        if (match == FALSE)
                return FALSE;

        /* Initial portions matched. Try to position pointers after
         * last colons of both @query and @base. */
        colon += 1;
        if (G_UNLIKELY (*colon == 0))
                return TRUE;

        query += type_len;
        switch (*query) {
        case 0:
                return TRUE; /* @query does not specify a version */
        case ':':
                query += 1;
                if (G_UNLIKELY (*query == 0))
                        return TRUE;

                break;
        default:
                return FALSE; /* Hmm, not EOS, nor colon.. bad */
        }

        /* Parse versions */
        query_ver = atoi (query);
        base_ver  = atoi (colon);

        /* Compare versions */
        return (query_ver <= base_ver);
}

/**
 * gupnp_device_info_list_dlna_device_class_identifier:
 * @info: A #GUPnPDeviceInfo
 *
 * Get a #GList of strings that represent the device class and version as
 * announced in the device description file using the &lt;dlna:X_DLNADOC&gt;
 * element.
 * Returns:(nullable)(transfer full) (element-type utf8): a #GList of newly allocated strings or
 * %NULL if the device description doesn't contain the &lt;dlna:X_DLNADOC&gt;
 * element.
 *
 * Since: 0.20.4
 **/
GList *
gupnp_device_info_list_dlna_device_class_identifier (GUPnPDeviceInfo *info)
{
        xmlNode *element;
        GList *list  = NULL;
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        element = priv->element;

        for (element = element->children; element; element = element->next) {
                /* No early exit since the node explicitly may appear multiple
                 * times: 7.2.10.3 */
                if (!strcmp ("X_DLNADOC", (char *) element->name)) {
                        xmlChar *content = NULL;

                        content = xmlNodeGetContent (element);

                        if (content == NULL)
                                continue;

                        list = g_list_prepend (list,
                                               g_strdup ((char *) content));
                        xmlFree (content);
                }
        }

        /* Return in order of appearance in document */
        return g_list_reverse (list);
}

/**
 * gupnp_device_info_list_dlna_capabilities:
 * @info: A #GUPnPDeviceInfo
 *
 * Get a #GList of strings that represent the device capabilities as announced
 * in the device description file using the &lt;dlna:X_DLNACAP&gt; element.
 *
 * Returns:(nullable)(transfer full) (element-type utf8): a #GList of newly allocated strings or
 * %NULL if the device description doesn't contain the &lt;dlna:X_DLNACAP&gt;
 * element.
 *
 * Since: 0.14.0
 **/
GList *
gupnp_device_info_list_dlna_capabilities (GUPnPDeviceInfo *info)
{
        xmlChar *caps;
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        priv = gupnp_device_info_get_instance_private (info);

        caps = xml_util_get_child_element_content (priv->element,
                                                   "X_DLNACAP");

        if (caps) {
                GList         *list  = NULL;
                const xmlChar *start = caps;

                while (*start) {
                        const xmlChar *end = start;

                        while (*end && *end != ',')
                                end++;

                        if (end > start) {
                                gchar *value;

                                value = g_strndup ((const gchar *) start,
                                                   end - start);

                                list = g_list_prepend (list, value);
                        }

                        if (*end)
                                start = end + 1;
                        else
                                break;
                }

                xmlFree (caps);

                return g_list_reverse (list);
        }

        return NULL;
}

/**
 * gupnp_device_info_get_description_value:
 * @info:    A #GUPnPDeviceInfo
 * @element: Name of the description element to retrieve
 *
 * This function provides generic access to the contents of arbitrary elements
 * in the device description file.
 *
 * Return value:(nullable)(transfer full): a newly allocated string or %NULL if the device
 *               description doesn't contain the given @element
 *
 * Since: 0.14.0
 **/
char *
gupnp_device_info_get_description_value (GUPnPDeviceInfo *info,
                                         const char      *element)
{
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);
        g_return_val_if_fail (element != NULL, NULL);

        priv = gupnp_device_info_get_instance_private (info);

        return xml_util_get_child_element_content_glib (priv->element,
                                                        element);
}

/**
 * gupnp_device_info_list_devices:
 * @info: A #GUPnPDeviceInfo
 *
 * Get a #GList of new objects implementing #GUPnPDeviceInfo
 * representing the devices directly contained in @info. The returned list
 * should be g_list_free()'d and the elements should be g_object_unref()'d.
 *
 * Note that devices are not cached internally, so that every time you
 * call this function new objects are created. The application
 * must cache any used devices if it wishes to keep them around and re-use
 * them.
 *
 * Return value:(nullable)(element-type GUPnP.DeviceInfo) (transfer full): a #GList of
 * new #GUPnPDeviceInfo objects.
 **/
GList *
gupnp_device_info_list_devices (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoClass *class;
        GList *devices;
        xmlNode *element;
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        class = GUPNP_DEVICE_INFO_GET_CLASS (info);

        g_return_val_if_fail (class->get_device, NULL);

        devices = NULL;

        priv = gupnp_device_info_get_instance_private (info);

        element = xml_util_get_element (priv->element,
                                        "deviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("device", (char *) element->name)) {
                        GUPnPDeviceInfo *child;

                        child = class->get_device (info, element);
                        devices = g_list_prepend (devices, child);
                }
        }

        return devices;
}

/**
 * gupnp_device_info_list_device_types:
 * @info: A #GUPnPDeviceInfo
 *
 * Get a #GList of strings representing the types of the devices
 * directly contained in @info.
 *
 * Return value:(nullable)(element-type utf8) (transfer full): A #GList of strings. The
 * elements should be g_free()'d and the list should be g_list_free()'d.
 **/
GList *
gupnp_device_info_list_device_types (GUPnPDeviceInfo *info)
{
        GList *device_types;
        xmlNode *element;
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        device_types = NULL;

        priv = gupnp_device_info_get_instance_private (info);

        element = xml_util_get_element (priv->element,
                                        "deviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("device", (char *) element->name)) {
                        char *type;

                        type = xml_util_get_child_element_content_glib
                                                   (element, "deviceType");
                        if (!type)
                                continue;

                        device_types = g_list_prepend (device_types, type);
                }
        }

        return device_types;
}

/**
 * gupnp_device_info_get_device:
 * @info: A #GUPnPDeviceInfo
 * @type: The type of the device to be retrieved.
 *
 * Get the service with type @type directly contained in @info as
 * a new object implementing #GUPnPDeviceInfo, or %NULL if no such device
 * was found. The returned object should be unreffed when done.
 *
 * Note that devices are not cached internally, so that every time you call
 * this function a new object is created. The application must cache any used
 * devices if it wishes to keep them around and re-use them.
 *
 * Returns: (transfer full)(nullable): A new #GUPnPDeviceInfo.
 **/
GUPnPDeviceInfo *
gupnp_device_info_get_device (GUPnPDeviceInfo *info,
                              const char      *type)
{
        GUPnPDeviceInfoClass *class;
        GUPnPDeviceInfo *device;
        xmlNode *element;
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);
        g_return_val_if_fail (type != NULL, NULL);

        class = GUPNP_DEVICE_INFO_GET_CLASS (info);

        g_return_val_if_fail (class->get_device, NULL);

        device = NULL;
        priv = gupnp_device_info_get_instance_private (info);

        element = xml_util_get_element (priv->element,
                                        "deviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("device", (char *) element->name)) {
                        xmlNode *type_element;
                        xmlChar *type_str;

                        type_element = xml_util_get_element (element,
                                                             "deviceType",
                                                             NULL);
                        if (!type_element)
                                continue;

                        type_str = xmlNodeGetContent (type_element);
                        if (!type_str)
                                continue;

                        if (resource_type_match (type, (char *) type_str))
                                device = class->get_device (info, element);

                        xmlFree (type_str);

                        if (device)
                                break;
                }
        }

        return device;
}
/**
 * gupnp_device_info_list_services:
 * @info: A #GUPnPDeviceInfo
 *
 * Get a #GList of new objects implementing #GUPnPServiceInfo representing the
 * services directly contained in @info. The returned list should be
 * g_list_free()'d and the elements should be g_object_unref()'d.
 *
 * Note that services are not cached internally, so that every time you call
 * function new objects are created. The application must cache any used
 * services if it wishes to keep them around and re-use them.
 *
 * Return value: (nullable)(element-type GUPnP.ServiceInfo) (transfer full) : A #GList of
 * new #GUPnPServiceInfo objects.
 */
GList *
gupnp_device_info_list_services (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoClass *class;
        GList *services;
        xmlNode *element;
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        class = GUPNP_DEVICE_INFO_GET_CLASS (info);

        g_return_val_if_fail (class->get_service, NULL);

        services = NULL;

        priv = gupnp_device_info_get_instance_private (info);

        element = xml_util_get_element (priv->element,
                                        "serviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("service", (char *) element->name)) {
                        GUPnPServiceInfo *service;

                        service = class->get_service (info, element);
                        services = g_list_prepend (services, service);
                }
        }

        return services;
}

/**
 * gupnp_device_info_list_service_types:
 * @info: A #GUPnPDeviceInfo
 *
 * Get a #GList of strings representing the types of the services
 * directly contained in @info.
 *
 * Return value: (nullable)(element-type utf8) (transfer full): A #GList of strings. The
 * elements should be g_free()'d and the list should be g_list_free()'d.
 **/
GList *
gupnp_device_info_list_service_types (GUPnPDeviceInfo *info)
{
        GList *service_types;
        xmlNode *element;
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);

        service_types = NULL;

        priv = gupnp_device_info_get_instance_private (info);

        element = xml_util_get_element (priv->element,
                                        "serviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("service", (char *) element->name)) {
                        char *type;

                        type = xml_util_get_child_element_content_glib
                                                   (element, "serviceType");
                        if (!type)
                                continue;

                        service_types = g_list_prepend (service_types, type);
                }
        }

        return service_types;
}

/**
 * gupnp_device_info_get_service:
 * @info: A #GUPnPDeviceInfo
 * @type: The type of the service to be retrieved.
 *
 * Get the service with type @type directly contained in @info as a new object
 * implementing #GUPnPServiceInfo, or %NULL if no such device was found. The
 * returned object should be unreffed when done.
 *
 * Note that services are not cached internally, so that every time you call
 * this function a new object is created. The application must cache any used
 * services if it wishes to keep them around and re-use them.
 *
 * Returns: (nullable)(transfer full): A #GUPnPServiceInfo.
 **/
GUPnPServiceInfo *
gupnp_device_info_get_service (GUPnPDeviceInfo *info,
                               const char      *type)
{
        GUPnPDeviceInfoClass *class;
        GUPnPServiceInfo *service;
        xmlNode *element;
        GUPnPDeviceInfoPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_DEVICE_INFO (info), NULL);
        g_return_val_if_fail (type != NULL, NULL);

        class = GUPNP_DEVICE_INFO_GET_CLASS (info);

        g_return_val_if_fail (class->get_service, NULL);

        service = NULL;

        priv = gupnp_device_info_get_instance_private (info);

        element = xml_util_get_element (priv->element,
                                        "serviceList",
                                        NULL);
        if (!element)
                return NULL;

        for (element = element->children; element; element = element->next) {
                if (!strcmp ("service", (char *) element->name)) {
                        xmlNode *type_element;
                        xmlChar *type_str;

                        type_element = xml_util_get_element (element,
                                                             "serviceType",
                                                             NULL);
                        if (!type_element)
                                continue;

                        type_str = xmlNodeGetContent (type_element);
                        if (!type_str)
                                continue;

                        if (resource_type_match (type, (char *) type_str))
                                service = class->get_service (info, element);

                        xmlFree (type_str);

                        if (service)
                                break;
                }
        }

        return service;
}

/* Return associated xmlDoc */
GUPnPXMLDoc *
_gupnp_device_info_get_document (GUPnPDeviceInfo *info)
{
        GUPnPDeviceInfoPrivate *priv;

        priv = gupnp_device_info_get_instance_private (info);

        return priv->doc;
}
