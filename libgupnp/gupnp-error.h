/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_ERROR_H
#define GUPNP_ERROR_H

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
 * Error codes during communication with another server.
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
 * Error codes during eventing of state variables.
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
 * Error codes used during invocation of service actions.
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
 * Errors during occuring during processing of XML data.
 */
typedef enum {
        GUPNP_XML_ERROR_PARSE,
        GUPNP_XML_ERROR_NO_NODE,
        GUPNP_XML_ERROR_EMPTY_NODE,
        GUPNP_XML_ERROR_INVALID_ATTRIBUTE,
        GUPNP_XML_ERROR_OTHER
} GUPnPXMLError;

GQuark
gupnp_rootdevice_error_quark (void) G_GNUC_CONST;

#define GUPNP_ROOT_DEVICE_ERROR (gupnp_rootdevice_error_quark ())

/**
 * GUPnPRootdeviceError:
 * @GUPNP_ROOT_DEVICE_ERROR_NO_CONTEXT: No #GUPnPContext was passed to the root device.
 * @GUPNP_ROOT_DEVICE_ERROR_NO_DESCRIPTION_PATH: Device description path was missing
 * @GUPNP_ROOT_DEVICE_ERROR_NO_DESCRIPTION_FOLDER: Description folder was missing
 * @GUPNP_ROOT_DEVICE_ERROR_NO_NETWORK: Network interface is not usable
 *
 * Errors during [class@GUPnP.RootDevice] creation
 */
typedef enum {
        GUPNP_ROOT_DEVICE_ERROR_NO_CONTEXT,
        GUPNP_ROOT_DEVICE_ERROR_NO_DESCRIPTION_PATH,
        GUPNP_ROOT_DEVICE_ERROR_NO_DESCRIPTION_FOLDER,
        GUPNP_ROOT_DEVICE_ERROR_NO_NETWORK,
        GUPNP_ROOT_DEVICE_ERROR_FAIL
} GUPnPRootdeviceError;

GQuark
gupnp_service_introspection_error_quark (void) G_GNUC_CONST;

#define GUPNP_SERVICE_INTROSPECTION_ERROR                                      \
        (gupnp_service_introspection_error_quark ())

/**
 * GUPnPServiceIntrospectionError:
 * @GUPNP_SERVICE_INTROSPECTION_ERROR_OTHER: Unknown error
 *
 * Errors during service introspection
 */
typedef enum
{
        GUPNP_SERVICE_INTROSPECTION_ERROR_OTHER,
} GUPnPServiceIntrospectionError;


GQuark
gupnp_service_error_quark (void) G_GNUC_CONST;

#define GUPNP_SERVICE_ERROR                                      \
(gupnp_service_introspection_error_quark ())

/**
 * GUPnPServiceError:
 * @GUPNP_SERVICE_ERROR_AUTOCONNECT: [method@GUPnP.Service.signals_autoconnect] failed
 *
 * Errors during service handling
 */
typedef enum
{
        GUPNP_SERVICE_ERROR_AUTOCONNECT,
} GUPnPServiceError;

G_END_DECLS

#endif /* GUPNP_ERROR_H */
