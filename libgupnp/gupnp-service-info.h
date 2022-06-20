/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_SERVICE_INFO_H
#define GUPNP_SERVICE_INFO_H

#include <glib-object.h>

#include "gupnp-context.h"
#include "gupnp-service-introspection.h"

G_BEGIN_DECLS

#define GUPNP_TYPE_SERVICE_INFO (gupnp_service_info_get_type ())

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

GUPnPContext *
gupnp_service_info_get_context                (GUPnPServiceInfo *info);

const char *
gupnp_service_info_get_location               (GUPnPServiceInfo *info);

const GUri *
gupnp_service_info_get_url_base (GUPnPServiceInfo *info);

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
