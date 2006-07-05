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

        GList *pending_gets;
};

typedef enum {
        QUARK_SERVICE_TYPE,
        QUARK_ID,
        QUARK_SCPD_URL,
        QUARK_CONTROL_URL,
        QUARK_EVENT_SUB_URL,
        QUARK_LAST
} Quark;

static GQuark quarks[QUARK_LAST];

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
        
        quarks[QUARK_SERVICE_TYPE] =
                g_quark_from_static_string
                        ("gupnp-service-info-service-type");
        quarks[QUARK_ID] =
                g_quark_from_static_string
                        ("gupnp-service-info-id");
        quarks[QUARK_SCPD_URL] =
                g_quark_from_static_string
                        ("gupnp-service-info-scpd-url");
        quarks[QUARK_CONTROL_URL] =
                g_quark_from_static_string
                        ("gupnp-service-info-control-url");
        quarks[QUARK_EVENT_SUB_URL] =
                g_quark_from_static_string
                        ("gupnp-service-info-event-sub-url");
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
 * @info: An object implementing the #GUPnPServiceInfo interface
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
 * @info: An object implementing the #GUPnPServiceInfo interface
 *
 * Return value: The UDN of the containing device.
 **/
const char *
gupnp_service_info_get_udn (GUPnPServiceInfo *info)
{
        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        return info->priv->udn;
}

static const char *
get_property (GUPnPServiceInfo *info,
              const char       *element_name,
              Quark             quark)
{
        xmlChar *value;

        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        value = g_object_get_qdata (G_OBJECT (info), quarks[quark]);
        if (!value) {
                GUPnPServiceInfoClass *class;
                xmlNode *element;
                
                class = GUPNP_SERVICE_INFO_GET_CLASS (info);

                g_return_val_if_fail (class->get_element, NULL);
                element = class->get_element (info);
        
                element = xml_util_get_element (element,
                                                element_name,
                                                NULL);

                if (element)
                        value = xmlNodeGetContent (element);
                else {
                        /* So that g_object_get_qdata() does not return NULL */
                        value = xmlStrdup ((xmlChar *) "");
                }

                g_object_set_qdata_full (G_OBJECT (info),
                                         quarks[quark],
                                         value,
                                         xmlFree);
        }

        return (const char *) value;
}

/**
 * gupnp_service_info_get_service_type
 * @info: An object implementing the #GUPnPServiceInfo interface
 *
 * Return value: The UPnP service type.
 **/
const char *
gupnp_service_info_get_service_type (GUPnPServiceInfo *info)
{
        return get_property (info, "serviceType", QUARK_SERVICE_TYPE);
}

/**
 * gupnp_service_info_get_type
 * @info: An object implementing the #GUPnPServiceInfo interface
 *
 * Return value: The ID.
 **/
const char *
gupnp_service_info_get_id (GUPnPServiceInfo *info)
{
        return get_property (info, "serviceId", QUARK_ID);
}

/**
 * gupnp_service_info_get_type
 * @info: An object implementing the #GUPnPServiceInfo interface
 *
 * Return value: The SCPD URL.
 **/
const char *
gupnp_service_info_get_scpd_url (GUPnPServiceInfo *info)
{
        return get_property (info, "SCPDURL", QUARK_SCPD_URL);
}

/**
 * gupnp_service_info_get_type
 * @info: An object implementing the #GUPnPServiceInfo interface
 *
 * Return value: The control URL.
 **/
const char *
gupnp_service_info_get_control_url (GUPnPServiceInfo *info)
{
        return get_property (info, "controlURL", QUARK_CONTROL_URL);
}

/**
 * gupnp_service_info_get_type
 * @info: An object implementing the #GUPnPServiceInfo interface
 *
 * Return value: The event subscription URL.
 **/
const char *
gupnp_service_info_get_event_subscription_url (GUPnPServiceInfo *info)
{
        return get_property (info, "eventSubURL", QUARK_EVENT_SUB_URL);
}

/**
 * SCPD URL downloaded.
 **/
static void
got_scpd_url (SoupMessage                        *msg,
              GUPnPServiceInfoListActionsCallback cb)
{
        GList *actions;

        actions = NULL;

        if (SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
                xmlDoc *xml_doc;

                xml_doc = xmlParseMemory (msg->response.body,
                                          msg->response.length);
                if (xml_doc) {
                        xmlFreeDoc (xml_doc);
                } //else
                    //    g_warning ("Failed to parse %s", data->description_url);
        } //else
            //            g_warning ("Failed to GET %s", data->description_url);

//        cb (info, actions, user_data);
}

void
gupnp_service_info_list_actions (GUPnPServiceInfo                   *info,
                                 GUPnPServiceInfoListActionsCallback cb,
                                 gpointer                            user_data)
{
        SoupSession *session;
        SoupMessage *message;

        g_return_if_fail (GUPNP_IS_SERVICE_INFO (info));
        g_return_if_fail (cb);

        session = _gupnp_context_get_session (info->priv->context);

        message = soup_message_new (SOUP_METHOD_GET,
                                    gupnp_service_info_get_scpd_url (info));

        soup_session_queue_message (session,
                                    message,
                                    (SoupMessageCallbackFn) got_scpd_url,
                                    cb);

        /* XXX */
        info->priv->pending_gets =
                g_list_prepend (info->priv->pending_gets,
                                message);
}

void
gupnp_service_info_free_action_list (GList *list)
{
        while (list) {
                list = g_list_delete_link (list, list);
        }
}
