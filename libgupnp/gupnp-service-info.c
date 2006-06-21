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

#include "gupnp-service-info.h"
#include "xml-util.h"

typedef enum {
        QUARK_SERVICE_TYPE,
        QUARK_ID,
        QUARK_SCPD_URL,
        QUARK_CONTROL_URL,
        QUARK_EVENT_SUB_URL,
        QUARK_LAST
} Quark;

static GQuark quarks[QUARK_LAST];

static void
gupnp_service_info_base_init (gpointer class)
{
        static gboolean initialized = FALSE;

        if (!initialized) {
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
                
                initialized = TRUE;
        }
}

GType
gupnp_service_info_get_type (void)
{
        static GType type = 0;

        if (!type) {
                static const GTypeInfo info = {
                        sizeof (GUPnPServiceInfoIface),
                	gupnp_service_info_base_init,
                	NULL,
                	NULL,
                	NULL,
                	NULL,
                	0,
                	0,
                	NULL
                };

                type = g_type_register_static (G_TYPE_INTERFACE,
                                               "GUPnPServiceInfo",
				               &info,
                                               0);
        }

        return type;
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
        GUPnPServiceInfoIface *iface;

        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        iface = GUPNP_SERVICE_INFO_GET_IFACE (info);

        g_return_val_if_fail (iface->get_location, NULL);
        return iface->get_location (info);
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
        GUPnPServiceInfoIface *iface;

        g_return_val_if_fail (GUPNP_IS_SERVICE_INFO (info), NULL);

        iface = GUPNP_SERVICE_INFO_GET_IFACE (info);

        g_return_val_if_fail (iface->get_udn, NULL);
        return iface->get_udn (info);
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
                GUPnPServiceInfoIface *iface;
                xmlNode *element;
                
                iface = GUPNP_SERVICE_INFO_GET_IFACE (info);

                g_return_val_if_fail (iface->get_element, NULL);
                element = iface->get_element (info);
        
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
