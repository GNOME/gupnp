/*
 * Copyright (C) 2013,2014 Jens Georg <mail@jensge.org>
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

#ifndef __GUPNP_ACL_PRIVATE_H__
#define __GUPNP_ACL_PRIVATE_H__

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
        SoupMessage *message;
        char *path;
        GHashTable *query;
        SoupClientContext *client;
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
                       SoupMessage *message,
                       const char *path,
                       GHashTable *query,
                       SoupClientContext *client,
                       AclServerHandler *handler);

G_GNUC_INTERNAL void
acl_async_handler_free (AclAsyncHandler *handler);


G_END_DECLS

#endif
