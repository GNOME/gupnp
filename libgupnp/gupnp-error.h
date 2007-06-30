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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GUPNP_ERROR_H__
#define __GUPNP_ERROR_H__

#include <glib.h>

G_BEGIN_DECLS

GQuark
gupnp_server_error_quark (void) G_GNUC_CONST;

#define GUPNP_SERVER_ERROR (gupnp_server_error_quark ())

typedef enum {
        GUPNP_SERVER_ERROR_INTERNAL_SERVER_ERROR,
        GUPNP_SERVER_ERROR_NOT_FOUND,
        GUPNP_SERVER_ERROR_NOT_IMPLEMENTED,
        GUPNP_SERVER_ERROR_INVALID_RESPONSE,
        GUPNP_SERVER_ERROR_OTHER
} GUPnPServerError;

GQuark
gupnp_eventing_error_quark (void) G_GNUC_CONST;

#define GUPNP_EVENTING_ERROR (gupnp_eventing_error_quark ())

typedef enum {
        GUPNP_EVENTING_ERROR_SUBSCRIPTION_FAILED,
        GUPNP_EVENTING_ERROR_SUBSCRIPTION_LOST,
        GUPNP_EVENTING_ERROR_NOTIFY_FAILED,
} GUPnPEventingError;

GQuark
gupnp_control_error_quark (void) G_GNUC_CONST;

#define GUPNP_CONTROL_ERROR (gupnp_control_error_quark ())

typedef enum {
        GUPNP_CONTROL_ERROR_INVALID_ACTION,
        GUPNP_CONTROL_ERROR_INVALID_ARGS,
        GUPNP_CONTROL_ERROR_OUT_OF_SYNC,
        GUPNP_CONTROL_ERROR_ACTION_FAILED,
        GUPNP_CONTROL_ERROR_UPNP_FORUM_DEFINED,
        GUPNP_CONTROL_ERROR_DEVICE_TYPE_DEFINED,
        GUPNP_CONTROL_ERROR_VENDOR_DEFINED,
} GUPnPControlError;

G_END_DECLS

#endif /* __GUPNP_ERROR_H__ */
