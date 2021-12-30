/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include <config.h>

#include "gupnp-error.h"
#include "gupnp-error-private.h"

/**
 * SECTION:gupnp-error
 * @short_description: Error domains and codes.
 */

/**
 * GUPNP_SERVER_ERROR:
 *
 * The #GQuark uniquely used by GUPnP's server errors.
 *
 * Returns: a #GQuark uniquely used by GUPnP's server errors.
 **/
GQuark
gupnp_server_error_quark (void)
{
        static GQuark quark = 0;

        if (!quark)
                quark = g_quark_from_static_string ("gupnp-server-error");

        return quark;
}

/**
 * GUPNP_EVENTING_ERROR:
 *
 * The #GQuark uniquely used by GUPnP's eventing errors.
 *
 * Returns: a #GQuark uniquely used by GUPnP's eventing errors.
 **/
GQuark
gupnp_eventing_error_quark (void)
{
        static GQuark quark = 0;

        if (!quark)
                quark = g_quark_from_static_string ("gupnp-eventing-error");

        return quark;
}

/**
 * GUPNP_CONTROL_ERROR:
 *
 * The #GQuark uniquely used by GUPnP's control errors.
 *
 * Returns: a #GQuark uniquely used by GUPnP's control errors.
 **/
GQuark
gupnp_control_error_quark (void)
{
        static GQuark quark = 0;

        if (!quark)
                quark = g_quark_from_static_string ("gupnp-control-error");

        return quark;
}

/**
 * GUPNP_XML_ERROR:
 *
 * The #GQuark uniquely used by GUPnP XML processing errors.
 *
 * Returns: a #GQuark uniquely used by GUPnP XML processing errors.
 **/
GQuark
gupnp_xml_error_quark (void)
{
        static GQuark quark = 0;

        if (!quark)
                quark = g_quark_from_static_string ("gupnp-xml-error");

        return quark;
}

/**
 * GUPNP_ROOT_DEVICE_ERROR:
 *
 * The #GQuark uniquely used by GUPnP RootDevice creation errors.
 *
 * Returns: a #GQuark uniquely used by GUPnP RootDevice creation errors.
 */
GQuark
gupnp_rootdevice_error_quark (void)
{
        static GQuark quark = 0;

        if (!quark)
                quark = g_quark_from_static_string ("gupnp-root-device-error");

        return quark;
}

/**
 * GUPNP_SERVICE_INTROSPECTION_ERROR:
 *
 * The #GQuark uniquely used by GUPnP ServiceIntrospection errors.
 *
 * Returns: a #GQuark uniquely used by GUPnP ServiceIntrospection creation errors.
 */
GQuark
gupnp_service_introspection_error_quark (void)
{
        static GQuark quark = 0;

        if (!quark)
                quark = g_quark_from_static_string ("gupnp-service-introspection-error");

        return quark;
}

/**
 * GUPNP_SERVICE_ERROR:
 *
 * The #GQuark uniquely used by GUPnP Service errors.
 *
 * Returns: a #GQuark uniquely used by GUPnP Service creation errors.
 */
GQuark
gupnp_service_error_quark (void)
{
        static GQuark quark = 0;

        if (!quark)
                quark = g_quark_from_static_string ("gupnp-service-error");

        return quark;
}

/* Soup status code => GUPnPServerError */
static int
code_from_status_code (int status_code)
{
        switch (status_code) {
        case SOUP_STATUS_INTERNAL_SERVER_ERROR:
                return GUPNP_SERVER_ERROR_INTERNAL_SERVER_ERROR;
        case SOUP_STATUS_NOT_IMPLEMENTED:
                return GUPNP_SERVER_ERROR_NOT_IMPLEMENTED;
        case SOUP_STATUS_NOT_FOUND:
                return GUPNP_SERVER_ERROR_NOT_FOUND;
        default:
                return GUPNP_SERVER_ERROR_OTHER;
        }
}

/* Set status of @msg to @error */
void
_gupnp_error_set_server_error (GError     **error,
                               SoupMessage *msg)
{
        g_set_error_literal (
                error,
                GUPNP_SERVER_ERROR,
                code_from_status_code (soup_message_get_status (msg)),
                soup_message_get_reason_phrase (msg));
}

/* Create a #GError with status of @msg */
GError *
_gupnp_error_new_server_error (SoupMessage *msg)
{
        return g_error_new_literal (
                GUPNP_SERVER_ERROR,
                code_from_status_code (soup_message_get_status (msg)),
                soup_message_get_reason_phrase (msg));
}
