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

#include "gupnp-control-point.h"
#include "gupnp-device-proxy-private.h"

G_DEFINE_TYPE (GUPnPControlPoint,
               gupnp_control_point,
               GSSDP_TYPE_SERVICE_BROWSER);

struct _GUPnPControlPointPrivate {
        GList *devices;
};

enum {
        DEVICE_AVAILABLE,
        DEVICE_UNAVAILABLE, /* XXX */
        SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

static void
gupnp_control_point_init (GUPnPControlPoint *proxy)
{
        proxy->priv = G_TYPE_INSTANCE_GET_PRIVATE (proxy,
                                                   GUPNP_TYPE_CONTROL_POINT,
                                                   GUPnPControlPointPrivate);
}

static void
gupnp_control_point_dispose (GObject *object)
{
        GUPnPControlPoint *proxy;

        proxy = GUPNP_CONTROL_POINT (object);

        while (proxy->priv->devices) {
                g_object_unref (proxy->priv->devices->data);
                proxy->priv->devices =
                        g_list_delete_link (proxy->priv->devices,
                                            proxy->priv->devices);
        }
}

static void
gupnp_control_point_service_available (GSSDPServiceBrowser *browser,
                                       const char          *usn,
                                       const GList         *locations)
{
        GUPnPControlPoint *control_point;

        control_point = GUPNP_CONTROL_POINT (browser);

        g_print ("Discovered %s\n", usn);
}

static void
gupnp_control_point_service_unavailable (GSSDPServiceBrowser *browser,
                                         const char          *usn)
{
        GUPnPControlPoint *control_point;

        control_point = GUPNP_CONTROL_POINT (browser);

        g_print ("Undiscovered %s\n", usn);
}

static void
gupnp_control_point_class_init (GUPnPControlPointClass *klass)
{
        GObjectClass *object_class;
        GSSDPServiceBrowserClass *browser_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = gupnp_control_point_dispose;

        browser_class = GSSDP_SERVICE_BROWSER_CLASS (klass);

        browser_class->service_available =
                gupnp_control_point_service_available;
        browser_class->service_unavailable =
                gupnp_control_point_service_unavailable;
        
        g_type_class_add_private (klass, sizeof (GUPnPControlPointPrivate));
}

/**
 * gupnp_control_point_list_devices
 * @control_point: A #GUPnPControlPoint
 *
 * Return value: A #GList of available #GUPnPDeviceProxy's. Do not free the
 * list nor its values.
 **/
const GList *
gupnp_control_point_list_devices (GUPnPControlPoint *control_point)
{
        g_return_val_if_fail (GUPNP_IS_CONTROL_POINT (control_point), NULL);

        return control_point->priv->devices;
}

/**
 * gupnp_control_point_new
 * @ssdp_client: A #GSSDPClient
 * @target: The search target
 *
 * Return value: A new #GUPnPControlPoint object.
 **/
GUPnPControlPoint *
gupnp_control_point_new (GSSDPClient *ssdp_client,
                         const char  *target)
{
        return g_object_new (GUPNP_TYPE_CONTROL_POINT,
                             "client", ssdp_client,
                             "target", target,
                             NULL);
}
