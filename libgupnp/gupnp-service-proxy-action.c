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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "gupnp-service-proxy.h"
#include "gupnp-service-proxy-private.h"
#include "gupnp-service-proxy-action-private.h"

GUPnPServiceProxyAction *
gupnp_service_proxy_action_new (const char *action) {
        GUPnPServiceProxyAction *ret;

        ret = g_slice_new (GUPnPServiceProxyAction);
        ret->ref_count = 1;

        return ret;
}

GUPnPServiceProxyAction *
gupnp_service_proxy_action_ref (GUPnPServiceProxyAction *action)
{
        g_return_val_if_fail (action, NULL);
        g_return_val_if_fail (action->ref_count > 0, NULL);

        g_atomic_int_inc (&action->ref_count);

        return action;
}

void
gupnp_service_proxy_action_unref (GUPnPServiceProxyAction *action)
{

        g_return_if_fail (action);
        g_return_if_fail (action->ref_count > 0);


        if (g_atomic_int_dec_and_test (&action->ref_count)) {
                if (action->proxy != NULL) {
                        g_object_remove_weak_pointer (G_OBJECT (action->proxy),
                                                      (gpointer *)&(action->proxy));
                        gupnp_service_proxy_remove_action (action->proxy, action);
                }

                if (action->msg != NULL)
                        g_object_unref (action->msg);

                g_slice_free (GUPnPServiceProxyAction, action);
        }
}

G_DEFINE_BOXED_TYPE (GUPnPServiceProxyAction,
                     gupnp_service_proxy_action,
                     gupnp_service_proxy_action_ref,
                     gupnp_service_proxy_action_unref)
