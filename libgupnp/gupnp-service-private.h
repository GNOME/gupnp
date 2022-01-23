/*
 * Copyright (C) 2019 Jens Georg.
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_SERVICE_PRIVATE_H
#define GUPNP_SERVICE_PRIVATE_H

#include "gupnp-context.h"
#include "gupnp-xml-doc.h"

#include <libsoup/soup.h>

struct _GUPnPServiceAction {
        GUPnPContext *context;

        char         *name;

        SoupServerMessage *msg;
        gboolean      accept_gzip;

        GUPnPXMLDoc  *doc;
        xmlNode      *node;

        GString      *response_str;

        guint         argument_count;
};

void
gupnp_service_action_unref (struct _GUPnPServiceAction *action);

#endif
