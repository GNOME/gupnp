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
        g_set_error_literal (error,
                             GUPNP_SERVER_ERROR,
                             code_from_status_code (msg->status_code),
                             msg->reason_phrase);
}

/* Create a #GError with status of @msg */
GError *
_gupnp_error_new_server_error (SoupMessage *msg)
{
        return g_error_new_literal (GUPNP_SERVER_ERROR,
                                    code_from_status_code (msg->status_code),
                                    msg->reason_phrase);
}
