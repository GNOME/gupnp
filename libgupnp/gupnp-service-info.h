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

#ifndef __GUPNP_SERVICE_INFO_H__
#define __GUPNP_SERVICE_INFO_H__

#include <glib-object.h>
#include <libxml/tree.h>
#include <libsoup/soup-uri.h>

#include "gupnp-context.h"
#include "gupnp-service-introspection.h"

G_BEGIN_DECLS

GType
gupnp_service_info_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_SERVICE_INFO \
                (gupnp_service_info_get_type ())
#define GUPNP_SERVICE_INFO(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_SERVICE_INFO, \
                 GUPnPServiceInfo))
#define GUPNP_SERVICE_INFO_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_SERVICE_INFO, \
                 GUPnPServiceInfoClass))
#define GUPNP_IS_SERVICE_INFO(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_SERVICE_INFO))
#define GUPNP_IS_SERVICE_INFO_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_SERVICE_INFO))
#define GUPNP_SERVICE_INFO_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_SERVICE_INFO, \
                 GUPnPServiceInfoClass))

typedef struct _GUPnPServiceInfoPrivate GUPnPServiceInfoPrivate;
typedef struct _GUPnPServiceInfo GUPnPServiceInfo;
typedef struct _GUPnPServiceInfoClass GUPnPServiceInfoClass;

/**
 * GUPnPServiceInfo:
 *
 * This struct contains private data only, and should be accessed using the
 * functions below.
 */
struct _GUPnPServiceInfo {
        GObject parent;

        GUPnPServiceInfoPrivate *priv;
};

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
 * @introspection: The new #GUPnPServiceIntrospection object, or NULL
 * @error: The #GError that occurred, or NULL
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

GUPnPServiceIntrospection *
gupnp_service_info_get_introspection          (GUPnPServiceInfo *info,
                                               GError          **error);

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

G_END_DECLS

#endif /* __GUPNP_SERVICE_INFO_H__ */
