/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
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

#ifndef GUPNP_SERVICE_INFO_H
#define GUPNP_SERVICE_INFO_H

#include <glib-object.h>
#include <libsoup/soup-uri.h>

#include "gupnp-context.h"
#include "gupnp-service-introspection.h"

G_BEGIN_DECLS

#define GUPNP_TYPE_SERVICE_INFO \
                (gupnp_service_info_get_type ())

G_DECLARE_DERIVABLE_TYPE (GUPnPServiceInfo,
                          gupnp_service_info,
                          GUPNP,
                          SERVICE_INFO,
                          GObject)

struct _GUPnPServiceInfoClass {
        GObjectClass parent_class;

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
};

/**
 * GUPnPServiceIntrospectionCallback:
 * @info: The #GUPnPServiceInfo introspection was requested for
 * @introspection: (nullable): The new #GUPnPServiceIntrospection object, or NULL
 * @error: (nullable): The #GError that occurred, or NULL
 * @user_data: User data
 *
 * Callback notifying that @introspection for @info has been obtained.
 **/
typedef void (* GUPnPServiceIntrospectionCallback) (
                                 GUPnPServiceInfo           *info,
                                 GUPnPServiceIntrospection  *introspection,
                                 const GError               *error,
                                 gpointer                    user_data);

GUPnPContext *
gupnp_service_info_get_context                (GUPnPServiceInfo *info);

const char *
gupnp_service_info_get_location               (GUPnPServiceInfo *info);

const SoupURI *
gupnp_service_info_get_url_base               (GUPnPServiceInfo *info);

const char *
gupnp_service_info_get_udn                    (GUPnPServiceInfo *info);

const char *
gupnp_service_info_get_service_type           (GUPnPServiceInfo *info);

char *
gupnp_service_info_get_id                     (GUPnPServiceInfo *info);

char *
gupnp_service_info_get_scpd_url               (GUPnPServiceInfo *info);

char *
gupnp_service_info_get_control_url            (GUPnPServiceInfo *info);

char *
gupnp_service_info_get_event_subscription_url (GUPnPServiceInfo *info);

void
gupnp_service_info_get_introspection_async
                              (GUPnPServiceInfo                 *info,
                               GUPnPServiceIntrospectionCallback callback,
                               gpointer                          user_data);

void
gupnp_service_info_get_introspection_async_full
                              (GUPnPServiceInfo                 *info,
                               GUPnPServiceIntrospectionCallback callback,
                               GCancellable                     *cancellable,
                               gpointer                          user_data);

void
gupnp_service_info_introspect_async           (GUPnPServiceInfo    *info,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data);

GUPnPServiceIntrospection *
gupnp_service_info_introspect_finish          (GUPnPServiceInfo   *info,
                                               GAsyncResult       *res,
                                               GError            **error);

G_END_DECLS

#endif /* GUPNP_SERVICE_INFO_H */
