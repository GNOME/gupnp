/* 
 * Copyright (C) 2006 OpenedHand Ltd.
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

#include <libsoup/soup.h>
#include <string.h>

#include "gupnp-service-info.h"
#include "gupnp-context-private.h"
#include "xml-util.h"

G_DEFINE_ABSTRACT_TYPE (GUPnPServiceInfo,
                        gupnp_service_info,
                        G_TYPE_OBJECT);

struct _GUPnPServiceInfoPrivate {
        GUPnPContext *context;

        char *location;
        char *udn;
};

enum {
        PROP_0,
        PROP_CONTEXT,
        PROP_LOCATION,
        PROP_UDN
};

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
                info->priv->context =
                        g_object_ref (g_value_get_object (value));
                break;
        case PROP_LOCATION:
                g_free (info->priv->location);
                info->priv->location =
                        g_value_dup_string (value);
                break;
        case PROP_UDN:
                g_free (info->priv->udn);
                info->priv->udn =
                        g_value_dup_string (value);
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

        if (info->priv->context) {
                g_object_unref (info->priv->context);
                info->priv->context = NULL;
        }
}

static void
gupnp_service_info_finalize (GObject *object)
{
        GUPnPServiceInfo *info;

        info = GUPNP_SERVICE_INFO (object);

        g_free (info->priv->location);
        g_free (info->priv->udn);
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

        g_object_class_install_property
                (object_class,
                 PROP_CONTEXT,
                 g_param_spec_object ("context",
                                      "Context",
                                      "GUPnPContext",
                                      GUPNP_TYPE_CONTEXT,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property
                (object_class,
                 PROP_LOCATION,
                 g_param_spec_string ("location",
                                      "Location",
                                      "The location of the device description "
                                      "file",
                                      NULL,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property
                (object_class,
                 PROP_UDN,
                 g_param_spec_string ("udn",
                                      "UDN",
                                      "The UDN of the containing device",
                                      NULL,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY));
}

/**
 * gupnp_service_info_get_context
 * @info: A #GUPnPServiceInfo
 *
 * Return value: The #GUPnPContext associated with @info.
 **/
GUPnPContext *
gupnp_service_info_get_context (GUPnPServiceInfo *info)
{
        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        return info->priv->context;
}

/**
 * gupnp_service_info_get_location
 * @info: A #GUPnPServiceInfo
 *
 * Return value: The location of the device description file.
 **/
const char *
gupnp_service_info_get_location (GUPnPServiceInfo *info)
{
        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        return info->priv->location;
}

/**
 * gupnp_service_info_get_udn
 * @info: A #GUPnPServiceInfo
 *
 * Return value: The UDN of the containing device.
 **/
const char *
gupnp_service_info_get_udn (GUPnPServiceInfo *info)
{
        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        return info->priv->udn;
}

static char *
get_property (GUPnPServiceInfo *info,
              const char       *element_name)
{
        GUPnPServiceInfoClass *class;
        xmlNode *element;

        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        class = GUPNP_SERVICE_INFO_GET_CLASS (info);

        g_return_val_if_fail (class->get_element, NULL);
        element = class->get_element (info);

        element = xml_util_get_element (element,
                                        element_name,
                                        NULL);

        if (element) {
                xmlChar *value;
                char *ret;
                
                /* Make glib memmanaged */
                value = xmlNodeGetContent (element);
                ret = g_strdup ((char *) value);
                xmlFree (value);

                return ret;
        } else
                return NULL;
}

/**
 * gupnp_service_info_get_service_type
 * @info: A #GUPnPServiceInfo
 *
 * Return value: The UPnP service type, or NULL. g_free() after use.
 **/
char *
gupnp_service_info_get_service_type (GUPnPServiceInfo *info)
{
        return get_property (info, "serviceType");
}

/**
 * gupnp_service_info_get_type
 * @info: A #GUPnPServiceInfo
 *
 * Return value: The ID, or NULL. g_free() after use.
 **/
char *
gupnp_service_info_get_id (GUPnPServiceInfo *info)
{
        return get_property (info, "serviceId");
}

/**
 * gupnp_service_info_get_type
 * @info: A #GUPnPServiceInfo
 *
 * Return value: The SCPD URL, or NULL. g_free() after use.
 **/
char *
gupnp_service_info_get_scpd_url (GUPnPServiceInfo *info)
{
        return get_property (info, "SCPDURL");
}

/**
 * gupnp_service_info_get_type
 * @info: A #GUPnPServiceInfo
 *
 * Return value: The control URL, or NULL. g_free() after use.
 **/
char *
gupnp_service_info_get_control_url (GUPnPServiceInfo *info)
{
        return get_property (info, "controlURL");
}

/**
 * gupnp_service_info_get_type
 * @info: A #GUPnPServiceInfo
 *
 * Return value: The event subscription URL, or NULL. g_free() after use.
 **/
char *
gupnp_service_info_get_event_subscription_url (GUPnPServiceInfo *info)
{
        return get_property (info, "eventSubURL");
}
