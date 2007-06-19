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
 * SECTION:gupnp-root-device
 * @short_description: Class for root device implementations.
 *
 * #GUPnPRootDevice allows for implementing root devices. 
 */

#include <string.h>

#include <libgssdp/gssdp-resource-group.h>

#include "gupnp-root-device.h"
#include "gupnp-context-private.h"
#include "xml-util.h"

G_DEFINE_TYPE (GUPnPRootDevice,
               gupnp_root_device,
               GUPNP_TYPE_DEVICE);

struct _GUPnPRootDevicePrivate {
        xmlDoc *description_doc;

        GSSDPResourceGroup *group;

        char *relative_url_base;
        char *relative_location;
};

enum {
        PROP_0,
        PROP_DESCRIPTION_DOC,
        PROP_RELATIVE_LOCATION,
        PROP_RELATIVE_URL_BASE,
        PROP_AVAILABLE
};

static void
gupnp_root_device_finalize (GObject *object)
{
        GUPnPRootDevice *device;
        
        device = GUPNP_ROOT_DEVICE (object);

        g_free (device->priv->relative_location);
        g_free (device->priv->relative_url_base);
}

static void
gupnp_root_device_dispose (GObject *object)
{
        GUPnPRootDevice *device;
        
        device = GUPNP_ROOT_DEVICE (object);

        if (device->priv->group) {
                g_object_unref (device->priv->group);
                device->priv->group = NULL;
        }
}

static void
gupnp_root_device_init (GUPnPRootDevice *device)
{
        device->priv = G_TYPE_INSTANCE_GET_PRIVATE (device,
                                                    GUPNP_TYPE_ROOT_DEVICE,
                                                    GUPnPRootDevicePrivate);
}

static void
gupnp_root_device_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        GUPnPRootDevice *device;

        device = GUPNP_ROOT_DEVICE (object);

        switch (property_id) {
        case PROP_DESCRIPTION_DOC:
                device->priv->description_doc = g_value_get_pointer (value);
                break;
        case PROP_RELATIVE_LOCATION:
                device->priv->relative_location = g_value_dup_string (value);
                break;
        case PROP_RELATIVE_URL_BASE:
                device->priv->relative_url_base = g_value_dup_string (value);
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

        device = GUPNP_ROOT_DEVICE (object);

        switch (property_id) {
        case PROP_RELATIVE_LOCATION:
                g_value_set_string (value,
                                    device->priv->relative_location);
                break;
        case PROP_RELATIVE_URL_BASE:
                g_value_set_string (value,
                                    device->priv->relative_url_base);
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

/* Serve our modified description document */
static void
server_handler (SoupServerContext *server_context,
                SoupMessage       *msg,
                gpointer           user_data)
{
        GUPnPRootDevice *device;
        xmlChar *mem;
        int size;

        device = GUPNP_ROOT_DEVICE (user_data);

        if (strcmp (msg->method, SOUP_METHOD_GET) != 0) {
                /* We don't implement this method */
                soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);

                return;
        }

        xmlDocDumpFormatMemory (device->priv->description_doc,
                                &mem, &size, TRUE);

        msg->response.owner = SOUP_BUFFER_SYSTEM_OWNED;
        msg->response.length = size;
        msg->response.body = g_strdup ((char *) mem);

        soup_message_set_status (msg, SOUP_STATUS_OK);

        xmlFree (mem);
}

static void
set_child_element_content (xmlNode    *element,
                           const char *child_name,
                           const char *value)
{
        xmlNode *child_element;

        child_element = xml_util_get_element (element,
                                              child_name,
                                              NULL);

        if (!child_element) {
                child_element = xmlNewNode (NULL, (xmlChar *) child_name);

                xmlAddChild (element, child_element);
        }

        xmlNodeSetContent (child_element, (xmlChar *) value);
}

static xmlChar *
get_child_element_content (xmlNode    *element,
                           const char *child_name)
{
        xmlNode *child_element;

        child_element = xml_util_get_element (element,
                                              child_name,
                                              NULL);
        if (!child_element)
                return NULL;

        return xmlNodeGetContent (child_element);
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
        udn = get_child_element_content (element, "UDN");
        if (!udn) {
                g_warning ("No UDN specified.");

                return;
        }

        device_type = get_child_element_content (element, "deviceType");
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

                        service_type = get_child_element_content
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

static GObject *
gupnp_root_device_constructor (GType                  type,
                               guint                  n_construct_params,
                               GObjectConstructParam *construct_params)
{
        GObjectClass *object_class;
        GObject *object;
        GUPnPRootDevice *device;
        GUPnPContext *context;
        SoupServer *server;
        const char *server_url;
        char *url_base, *location, *usn;
        int i;
        xmlDoc *description_doc;
        xmlNode *root_element, *element;
        xmlChar *udn;

        /* Get 'description-doc' property value */
        description_doc = NULL;

        for (i = 0; i < n_construct_params; i++) {
                const char *par_name;
                
                par_name = construct_params[i].pspec->name;

                if (strcmp (par_name, "description-doc") == 0) {
                        description_doc =
                                g_value_get_pointer (construct_params[i].value);

                        break;
                }
        }

        if (!description_doc) {
                g_warning ("No description document specified.");

                return NULL;
        }

        /* Find correct element */
        root_element = xml_util_get_element ((xmlNode *) description_doc,
                                             "root",
                                             NULL);
        if (!root_element) {
                g_warning ("\"/root\" element not found.");

                return NULL;
        }

        element = xml_util_get_element (root_element, 
                                        "device",
                                        NULL);
        if (!element) {
                g_warning ("\"/root/device\" element not found.");

                return NULL;
        }

        /* Set element */
        for (i = 0; i < n_construct_params; i++) {
                const char *par_name;
                
                par_name = construct_params[i].pspec->name;

                if (strcmp (par_name, "element") == 0) {
                        g_value_set_pointer (construct_params[i].value,
                                             element);

                        break;
                }
        }

        /* Get UDN */
        udn = get_child_element_content (element, "UDN");
        if (!udn) {
                g_warning ("No UDN specified.");

                return NULL;
        }

        /* Create object */
        object_class = G_OBJECT_CLASS (gupnp_root_device_parent_class);

        object = object_class->constructor (type,
                                            n_construct_params,
                                            construct_params);
        device = GUPNP_ROOT_DEVICE (object);

        /* Get context */
        context = gupnp_device_info_get_context (GUPNP_DEVICE_INFO (device));

        /* Get server URL */
        server_url = _gupnp_context_get_server_url (context);

        /* Generate full URL base */
        if (device->priv->relative_url_base) {
                url_base = g_build_path ("/",
                                         server_url,
                                         device->priv->relative_url_base,
                                         NULL);

                /* Set URL base in description XML */
                set_child_element_content (root_element, "URLBase", url_base);
        } else
                url_base = NULL;

        /* Generate full location */
        location = g_build_path ("/",
                                 server_url,
                                 device->priv->relative_location,
                                 NULL);

        /* Host description file */
        server = _gupnp_context_get_server (context);
        soup_server_add_handler (server,
                                 device->priv->relative_location,
                                 NULL,
                                 server_handler,
                                 NULL,
                                 device);

        /* Set .. */
        g_object_set (object,
                      "url-base", url_base,
                      "location", location,
                      NULL);

        /* Create resource group */
        device->priv->group = gssdp_resource_group_new (GSSDP_CLIENT (context));

        /* Add services and devices to resource group */
        usn = g_strdup_printf ("%s::upnp:rootdevice", (const char *) udn);
        gssdp_resource_group_add_resource_simple (device->priv->group,
                                                  "upnp:rootdevice",
                                                  usn,
                                                  location);
        g_free (usn);

        fill_resource_group (element, location, device->priv->group);

        /* Cleanup */
        g_free (url_base);
        g_free (location);

        xmlFree (udn);

        return object;
}

static void
gupnp_root_device_class_init (GUPnPRootDeviceClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_root_device_set_property;
        object_class->get_property = gupnp_root_device_get_property;
        object_class->constructor  = gupnp_root_device_constructor;
        object_class->dispose      = gupnp_root_device_dispose;
        object_class->finalize     = gupnp_root_device_finalize;

        g_type_class_add_private (klass, sizeof (GUPnPRootDevicePrivate));

        /**
         * GUPnPRootDevice:description-doc
         *
         * Pointer to description document. Constructor property.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_DESCRIPTION_DOC,
                 g_param_spec_pointer ("description-doc",
                                       "Description document",
                                       "Pointer to description document",
                                       G_PARAM_WRITABLE |
                                       G_PARAM_CONSTRUCT_ONLY |
                                       G_PARAM_STATIC_NAME |
                                       G_PARAM_STATIC_NICK |
                                       G_PARAM_STATIC_BLURB));

        /**
         * GUPnPRootDevice:relative-location
         *
         * Relative location.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_RELATIVE_LOCATION,
                 g_param_spec_string ("relative-location",
                                      "Relative location",
                                      "Relative location",
                                      NULL,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPRootDevice:relative-url-base
         *
         * Relative URL base.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_RELATIVE_URL_BASE,
                 g_param_spec_string ("relative-url-base",
                                      "Relative URL base",
                                      "Relative URL base",
                                      NULL,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPRootDevice:available
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
 * gupnp_root_device_new
 * @context: The #GUPnPContext
 * @description_doc: Pointer to the device description document
 * @relative_url_base: The URL base to use for this device, relative to the
 * URL root path
 * @relative_url_location: The location to use for this device, relative to the
 * URL root path
 *
 * Return value: A new @GUPnPRootDevice object.
 **/
GUPnPRootDevice *
gupnp_root_device_new (GUPnPContext *context,
                       xmlDoc       *description_doc,
                       const char   *relative_location,
                       const char   *relative_url_base)
{
        return g_object_new (GUPNP_TYPE_ROOT_DEVICE,
                             "context", context,
                             "description-doc", description_doc,
                             "relative-url-base", relative_url_base,
                             "relative-location", relative_location,
                             NULL);
}

/**
 * gupnp_root_device_set_available
 * @root_device: A #GUPnPRootDevice
 * @available: TRUE if @root_device should be available
 *
 * Controls whether or not @root_device is available (announcing
 * its presence).
 **/
void
gupnp_root_device_set_available (GUPnPRootDevice *root_device,
                                 gboolean         available)
{
        g_return_if_fail (GUPNP_IS_ROOT_DEVICE (root_device));

        gssdp_resource_group_set_available (root_device->priv->group,
                                            available);

        g_object_notify (G_OBJECT (root_device), "available");
}

/**
 * gupnp_root_device_get_available
 * @root_device: A #GUPnPRootDevice
 *
 * Return value: TRUE if @root_device is available.
 **/
gboolean
gupnp_root_device_get_available (GUPnPRootDevice *root_device)
{
        g_return_val_if_fail (GUPNP_IS_ROOT_DEVICE (root_device), FALSE);

        return gssdp_resource_group_get_available (root_device->priv->group);
}

/**
 * gupnp_root_device_get_relative_url_base
 * @root_device: A #GUPnPRootDevice
 *
 * Return value: The relative URL base of @root_device.
 **/
const char *
gupnp_root_device_get_relative_url_base (GUPnPRootDevice *root_device)
{
        g_return_val_if_fail (GUPNP_IS_ROOT_DEVICE (root_device), NULL);

        return root_device->priv->relative_url_base;
}

/**
 * gupnp_root_device_get_relative_location
 * @root_device: A #GUPnPRootDevice
 *
 * Return value: The relative location of @root_device.
 **/
const char *
gupnp_root_device_get_relative_location (GUPnPRootDevice *root_device)
{
        g_return_val_if_fail (GUPNP_IS_ROOT_DEVICE (root_device), NULL);

        return root_device->priv->relative_location;
}