/*
 * Copyright (C) 2007 OpenedHand Ltd.
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

#ifndef __GUPNP_SERVICE_H__
#define __GUPNP_SERVICE_H__

#include <stdarg.h>

#include "gupnp-service-info.h"

G_BEGIN_DECLS

GType
gupnp_service_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_SERVICE \
                (gupnp_service_get_type ())
#define GUPNP_SERVICE(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_SERVICE, \
                 GUPnPService))
#define GUPNP_SERVICE_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_SERVICE, \
                 GUPnPServiceClass))
#define GUPNP_IS_SERVICE(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_SERVICE))
#define GUPNP_IS_SERVICE_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_SERVICE))
#define GUPNP_SERVICE_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_SERVICE, \
                 GUPnPServiceClass))

/**
 * GUPnPServiceAction:
 *
 * Opaque structure for holding in-progress action data.
 **/
typedef struct _GUPnPServiceAction GUPnPServiceAction;

GType
gupnp_service_action_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_SERVICE_ACTION (gupnp_service_action_get_type ())

typedef struct _GUPnPServicePrivate GUPnPServicePrivate;
typedef struct _GUPnPService GUPnPService;
typedef struct _GUPnPServiceClass GUPnPServiceClass;

/**
 * GUPnPService:
 *
 * This struct contains private data only, and should be accessed using the
 * functions below.
 */
struct _GUPnPService {
        GUPnPServiceInfo parent;

        GUPnPServicePrivate *priv;
};

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

void
gupnp_service_action_get_valist   (GUPnPServiceAction *action,
                                   va_list             var_args);

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
gupnp_service_action_set_valist   (GUPnPServiceAction *action,
                                   va_list             var_args);

void
gupnp_service_action_set_values   (GUPnPServiceAction *action,
                                   GList              *arg_names,
                                   GList              *arg_values);

void
gupnp_service_action_set_value    (GUPnPServiceAction *action,
                                   const char         *argument,
                                   const GValue       *value);

void
gupnp_service_action_return       (GUPnPServiceAction *action);

void
gupnp_service_action_return_error (GUPnPServiceAction *action,
                                   guint               error_code,
                                   const char         *error_description);

SoupMessage *
gupnp_service_action_get_message  (GUPnPServiceAction *action);

guint
gupnp_service_action_get_argument_count
                                  (GUPnPServiceAction *action);

void
gupnp_service_notify              (GUPnPService *service,
                                   ...) G_GNUC_NULL_TERMINATED;

void
gupnp_service_notify_valist       (GUPnPService *service,
                                   va_list       var_args);

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

G_END_DECLS

#endif /* __GUPNP_SERVICE_H__ */
