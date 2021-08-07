/*
 * Copyright (C) 2013,2014 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later 
 */

#ifndef GUPNP_ACL_PRIVATE_H
#define GUPNP_ACL_PRIVATE_H

#include <glib.h>
#include <glib-object.h>
#include <libsoup/soup-session.h>

#include "gupnp-acl.h"
#include "gupnp-context.h"
#include "gupnp-service.h"

G_BEGIN_DECLS

/**
 * AclServerHandler:
 *
 * Closure for the ACL server handler that adds
 * a) Some data from the original server handler such as user_data and callback
 * b) Saves information for later use to pass on to ACL such as the service and context
 */
typedef struct _AclServerHandler
{
        GUPnPService *service;
        GUPnPContext *context;
        SoupServerCallback callback;
        gpointer user_data;
        GDestroyNotify notify;
} AclServerHandler;

/**
 * AclAsyncHandler:
 *
 * Closure when doing an async ACL request. Stores everything passed into the server handler
 */
typedef struct _AclAsyncHandler
{
        SoupServer *server;
        SoupServerMessage *message;
        char *path;
        GHashTable *query;
        AclServerHandler *handler;
} AclAsyncHandler;

G_GNUC_INTERNAL AclServerHandler *
acl_server_handler_new (GUPnPService *service,
                        GUPnPContext *context,
                        SoupServerCallback callback,
                        gpointer user_data,
                        GDestroyNotify notify);

G_GNUC_INTERNAL void
acl_server_handler_free (AclServerHandler *handler);

G_GNUC_INTERNAL AclAsyncHandler *
acl_async_handler_new (SoupServer *server,
                       SoupServerMessage *message,
                       const char *path,
                       GHashTable *query,
                       AclServerHandler *handler);

G_GNUC_INTERNAL void
acl_async_handler_free (AclAsyncHandler *handler);


G_END_DECLS

#endif
