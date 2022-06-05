/*
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_SERVICE_H
#define GUPNP_SERVICE_H

#include "gupnp-service-info.h"

G_BEGIN_DECLS

#define GUPNP_TYPE_SERVICE \
                (gupnp_service_get_type ())

G_DECLARE_DERIVABLE_TYPE (GUPnPService,
                          gupnp_service,
                          GUPNP,
                          SERVICE,
                          GUPnPServiceInfo)

/**
 * GUPnPServiceAction:
 *
 * Opaque structure for holding in-progress action data.
 **/
typedef struct _GUPnPServiceAction GUPnPServiceAction;

GType
gupnp_service_action_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_SERVICE_ACTION (gupnp_service_action_get_type ())

struct _GUPnPServiceClass {
        GUPnPServiceInfoClass parent_class;

        /* <signals> */
        void (* action_invoked) (GUPnPService       *service,
                                 GUPnPServiceAction *action);

        void (* query_variable) (GUPnPService       *service,
                                 const char         *variable,
                                 GValue             *value);

        void (* notify_failed)  (GUPnPService       *service,
                                 const GList        *callback_urls,
                                 const GError       *reason);

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
};


const char *
gupnp_service_action_get_name     (GUPnPServiceAction *action);

GList *
gupnp_service_action_get_locales  (GUPnPServiceAction *action);

void
gupnp_service_action_get          (GUPnPServiceAction *action,
                                   ...) G_GNUC_NULL_TERMINATED;

GList *
gupnp_service_action_get_values (GUPnPServiceAction *action,
                                 GList              *arg_names,
                                 GList              *arg_types);

void
gupnp_service_action_get_value    (GUPnPServiceAction *action,
                                   const char         *argument,
                                   GValue             *value);

GValue *
gupnp_service_action_get_gvalue   (GUPnPServiceAction *action,
                                   const char         *argument,
                                   GType               type);

void
gupnp_service_action_set          (GUPnPServiceAction *action,
                                   ...) G_GNUC_NULL_TERMINATED;

void
gupnp_service_action_set_values   (GUPnPServiceAction *action,
                                   GList              *arg_names,
                                   GList              *arg_values);

void
gupnp_service_action_set_value    (GUPnPServiceAction *action,
                                   const char         *argument,
                                   const GValue       *value);

void
gupnp_service_action_return_success (GUPnPServiceAction *action);

void
gupnp_service_action_return_error (GUPnPServiceAction *action,
                                   guint               error_code,
                                   const char         *error_description);

SoupServerMessage *
gupnp_service_action_get_message (GUPnPServiceAction *action);

guint
gupnp_service_action_get_argument_count
                                  (GUPnPServiceAction *action);

void
gupnp_service_notify              (GUPnPService *service,
                                   ...) G_GNUC_NULL_TERMINATED;

void
gupnp_service_notify_value        (GUPnPService *service,
                                   const char   *variable,
                                   const GValue *value);

void
gupnp_service_freeze_notify       (GUPnPService *service);

void
gupnp_service_thaw_notify         (GUPnPService *service);

void
gupnp_service_signals_autoconnect (GUPnPService *service,
                                   gpointer      user_data,
                                   GError      **error);

void
gupnp_service_action_invoked (GUPnPService *service,
                              GUPnPServiceAction *action);

void
gupnp_service_query_variable (GUPnPService *service,
                              const char *variable,
                              GValue *value);

void
gupnp_service_notify_failed (GUPnPService *service,
                             const GList *callback_urls,
                             const GError *reason);

G_END_DECLS

#endif /* GUPNP_SERVICE_H */
