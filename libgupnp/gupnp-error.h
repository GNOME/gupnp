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

#ifndef __GUPNP_ERROR_H__
#define __GUPNP_ERROR_H__

#include <glib.h>

G_BEGIN_DECLS

GQuark
gupnp_server_error_quark (void) G_GNUC_CONST;

#define GUPNP_SERVER_ERROR (gupnp_server_error_quark ())

/**
 * GUPnPServerError:
 * @GUPNP_SERVER_ERROR_INTERNAL_SERVER_ERROR: Internal server error.
 * @GUPNP_SERVER_ERROR_NOT_FOUND: The resource was not found.
 * @GUPNP_SERVER_ERROR_NOT_IMPLEMENTED: This method is not implemented.
 * @GUPNP_SERVER_ERROR_INVALID_RESPONSE: Invalid response.
 * @GUPNP_SERVER_ERROR_INVALID_URL: Invalid URL.
 * @GUPNP_SERVER_ERROR_OTHER: Unknown/unhandled error.
 *
 * #GError codes used for errors in the #GUPNP_SERVER_ERROR domain, when there
 * is communication with another server.
 */
typedef enum {
        GUPNP_SERVER_ERROR_INTERNAL_SERVER_ERROR,
        GUPNP_SERVER_ERROR_NOT_FOUND,
        GUPNP_SERVER_ERROR_NOT_IMPLEMENTED,
        GUPNP_SERVER_ERROR_INVALID_RESPONSE,
        GUPNP_SERVER_ERROR_INVALID_URL,
        GUPNP_SERVER_ERROR_OTHER
} GUPnPServerError;

GQuark
gupnp_eventing_error_quark (void) G_GNUC_CONST;

#define GUPNP_EVENTING_ERROR (gupnp_eventing_error_quark ())

/**
 * GUPnPEventingError:
 * @GUPNP_EVENTING_ERROR_SUBSCRIPTION_FAILED: The subscription attempt failed.
 * @GUPNP_EVENTING_ERROR_SUBSCRIPTION_LOST: The subscription was lost.
 * @GUPNP_EVENTING_ERROR_NOTIFY_FAILED: The notification failed.
 *
 * #GError codes used for errors in the #GUPNP_EVENTING_ERROR domain, during
 * eventing of state variables.
 */
typedef enum {
        GUPNP_EVENTING_ERROR_SUBSCRIPTION_FAILED,
        GUPNP_EVENTING_ERROR_SUBSCRIPTION_LOST,
        GUPNP_EVENTING_ERROR_NOTIFY_FAILED
} GUPnPEventingError;

GQuark
gupnp_control_error_quark (void) G_GNUC_CONST;

#define GUPNP_CONTROL_ERROR (gupnp_control_error_quark ())

/**
 * GUPnPControlError:
 * @GUPNP_CONTROL_ERROR_INVALID_ACTION: The action name was invalid.
 * @GUPNP_CONTROL_ERROR_INVALID_ARGS: The action arguments were invalid.
 * @GUPNP_CONTROL_ERROR_OUT_OF_SYNC: Out of sync (deprecated).
 * @GUPNP_CONTROL_ERROR_ACTION_FAILED: The action failed.
 *
 * #GError codes used for errors in the #GUPNP_CONTROL_ERROR domain, during
 * invocation of service actions.
 */
typedef enum {
        GUPNP_CONTROL_ERROR_INVALID_ACTION = 401,
        GUPNP_CONTROL_ERROR_INVALID_ARGS   = 402,
        GUPNP_CONTROL_ERROR_OUT_OF_SYNC    = 403,
        GUPNP_CONTROL_ERROR_ACTION_FAILED  = 501
} GUPnPControlError;

GQuark
gupnp_xml_error_quark (void) G_GNUC_CONST;

#define GUPNP_XML_ERROR (gupnp_xml_error_quark ())

/**
 * GUPnPXMLError:
 * @GUPNP_XML_ERROR_PARSE: Generic XML parsing error.
 * @GUPNP_XML_ERROR_NO_NODE: A required XML node was not found.
 * @GUPNP_XML_ERROR_EMPTY_NODE: An XML node is unexpectedly empty.
 * @GUPNP_XML_ERROR_INVALID_ATTRIBUTE: An XML node has an unknown attribute.
 * @GUPNP_XML_ERROR_OTHER: Unknown/unhandled XML related errors.
 *
 * #GError codes used for errors in the #GUPNP_XML_ERROR domain, during
 * processing of XML data.
 */
typedef enum {
        GUPNP_XML_ERROR_PARSE,
        GUPNP_XML_ERROR_NO_NODE,
        GUPNP_XML_ERROR_EMPTY_NODE,
        GUPNP_XML_ERROR_INVALID_ATTRIBUTE,
        GUPNP_XML_ERROR_OTHER
} GUPnPXMLError;

G_END_DECLS

#endif /* __GUPNP_ERROR_H__ */
