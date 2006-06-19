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

#include <string.h>

#include "gupnp-service-proxy.h"
#include "gupnp-service-proxy-private.h"
#include "xml-util.h"

static void
gupnp_service_proxy_info_init (GUPnPServiceInfoIface *iface);

G_DEFINE_TYPE_EXTENDED (GUPnPServiceProxy,
                        gupnp_service_proxy,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUPNP_TYPE_SERVICE_INFO,
                                               gupnp_service_proxy_info_init));

struct _GUPnPServiceProxyPrivate {
        char *location;

        xmlDoc *doc;

        xmlNode *element;
};

static const char *
gupnp_service_proxy_get_location (GUPnPServiceInfo *info)
{
        GUPnPServiceProxy *proxy;
        
        proxy = GUPNP_SERVICE_PROXY (info);
        
        return proxy->priv->location;
}

static xmlNode *
gupnp_service_proxy_get_element (GUPnPServiceInfo *info)
{
        GUPnPServiceProxy *proxy;
        
        proxy = GUPNP_SERVICE_PROXY (info);

        return proxy->priv->element;
}

static void
gupnp_service_proxy_init (GUPnPServiceProxy *proxy)
{
        proxy->priv = G_TYPE_INSTANCE_GET_PRIVATE (proxy,
                                                   GUPNP_TYPE_SERVICE_PROXY,
                                                   GUPnPServiceProxyPrivate);
}

static void
gupnp_service_proxy_finalize (GObject *object)
{
        GUPnPServiceProxy *proxy;

        proxy = GUPNP_SERVICE_PROXY (object);

        g_free (proxy->priv->location);

        if (proxy->priv->doc)
                xmlFreeDoc (proxy->priv->doc);
}

static void
gupnp_service_proxy_class_init (GUPnPServiceProxyClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gupnp_service_proxy_finalize;
        
        g_type_class_add_private (klass, sizeof (GUPnPServiceProxyPrivate));
}

static void
gupnp_service_proxy_info_init (GUPnPServiceInfoIface *iface)
{
        iface->get_location = gupnp_service_proxy_get_location;
        iface->get_element  = gupnp_service_proxy_get_element;
}

/**
 * @element: An #xmlNode pointing to a "device" element
 * @id: The ID of the service element to find
 **/
static xmlNode *
find_service_element_by_id (xmlNode    *element,
                            const char *id)
{
        xmlNode *tmp;

        tmp = xml_util_get_element (element,
                                    "serviceList",
                                    NULL);
        if (tmp) {
                for (tmp = tmp->children; tmp; tmp = tmp->next) {
                        xmlNode *id_element;

                        id_element = xml_util_get_element (tmp,
                                                           "serviceId",
                                                           NULL);
                        if (id_element) {
                                xmlChar *content;
                                gboolean match;

                                content = xmlNodeGetContent (id_element);
                                
                                match = !strcmp (id, (char *) content);

                                xmlFree (content);

                                if (match)
                                        return tmp;
                        }
                }
        }

        tmp = xml_util_get_element (element,
                                    "deviceList",
                                    NULL);
        if (tmp) {
                /* Recurse into children */
                for (tmp = tmp->children; tmp; tmp = tmp->next) {
                        element = find_service_element_by_id (tmp, id);
                        if (element)
                                return element;
                }
        }

        return NULL;
}

/**
 * gupnp_service_proxy_new
 * @location: The location of the service description file
 * @id: The ID of the service to create a proxy for.
 *
 * Return value: A #GUPnPServiceProxy for the service with ID @id, as read
 * from the service description file specified by @location.
 **/
GUPnPServiceProxy *
gupnp_service_proxy_new (const char *location,
                         const char *id)
{
        GUPnPServiceProxy *proxy;

        g_return_val_if_fail (location, NULL);
        g_return_val_if_fail (id, NULL);

        proxy = g_object_new (GUPNP_TYPE_SERVICE_PROXY,
                              NULL);

        proxy->priv->location = g_strdup (location);

        proxy->priv->doc = xmlParseFile (proxy->priv->location);

        proxy->priv->element =
                xml_util_get_element ((xmlNode *) proxy->priv->doc,
                                      "root",
                                      "device",
                                      NULL);
        proxy->priv->element =
                find_service_element_by_id (proxy->priv->element, id);

        if (!proxy->priv->element) {
                g_warning ("Device description does not contain service "
                           "with ID \"%s\".", id);

                g_object_unref (proxy);
                proxy = NULL;
        }

        return proxy;
}

/**
 * _gupnp_service_proxy_new_from_element
 * @location: The location of the service description file
 * @element: The #xmlNode ponting to the right service element
 *
 * Return value: A #GUPnPServiceProxy for the service with element @element, as
 * read from the service description file specified by @location.
 **/
GUPnPServiceProxy *
_gupnp_service_proxy_new_from_element (const char *location,
                                       xmlNode    *element)
{
        GUPnPServiceProxy *proxy;

        g_return_val_if_fail (location, NULL);
        g_return_val_if_fail (element, NULL);

        proxy = g_object_new (GUPNP_TYPE_SERVICE_PROXY,
                              NULL);

        proxy->priv->location = g_strdup (location);

        proxy->priv->element = element;

        return proxy;
}
