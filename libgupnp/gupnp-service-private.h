/*
 * Copyright (C) 2019 Jens Georg.
 *
 * Author: Jens Georg <mail@jensge.org>
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

#ifndef GUPNP_SERVICE_PRIVATE_H
#define GUPNP_SERVICE_PRIVATE_H

struct _GUPnPServiceAction {
        GUPnPContext *context;

        char         *name;

        SoupMessage  *msg;
        gboolean      accept_gzip;

        GUPnPXMLDoc  *doc;
        xmlNode      *node;

        GString      *response_str;

        guint         argument_count;
};

#endif
