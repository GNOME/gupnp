/*
 * Copyright (C) 2013,2014 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "gupnp-acl"

#include <config.h>

#include "gupnp-acl.h"
#include "gupnp-acl-private.h"
#include "gupnp-device.h"

/**
 * GUPnPAcl:
 *
 * Access control provider for [class@GUPnP.Context]
 *
 * GUPnPAcl provides either synchronous or asynchronous functions to check
 * whether a peer should be able to access a resource that is hosted by GUPnP or not.
 *
 * Since: 0.20.11
 */

G_DEFINE_INTERFACE(GUPnPAcl, gupnp_acl, G_TYPE_OBJECT)

static void
gupnp_acl_default_init (GUPnPAclInterface *klass)
{
}

/**
 * gupnp_acl_is_allowed:
 * @self: an instance of #GUPnPAcl
 * @device: (nullable): The [class@GUPnP.Device] associated with @path or %NULL if
 * unknown.
 * @service: (nullable): The [class@GUPnP.Service] associated with @path or %NULL if
 * unknown.
 * @path: The path being served.
 * @address: IP address of the peer.
 * @agent: (nullable): The User-Agent header of the peer or %NULL if unknown.
 * @returns %TRUE if the peer is allowed, %FALSE otherwise
 *
 * Check whether an IP address is allowed to access this resource.
 *
 * Since: 0.20.11
 */
gboolean
gupnp_acl_is_allowed (GUPnPAcl     *self,
                      GUPnPDevice  *device,
                      GUPnPService *service,
                      const char   *path,
                      const char   *address,
                      const char   *agent)
{
        g_return_val_if_fail (GUPNP_IS_ACL (self), FALSE);

        return GUPNP_ACL_GET_IFACE (self)->is_allowed (self,
                                                       device,
                                                       service,
                                                       path,
                                                       address,
                                                       agent);
}

/**
 * gupnp_acl_is_allowed_async:
 * @self: a #GUPnPAcl
 * @device: (nullable): The [class@GUPnP.Device] associated with @path or %NULL if
 * unknown.
 * @service: (nullable): The [class@GUPnP.Service] associated with @path or %NULL if
 * unknown.
 * @path: The path being served.
 * @address: IP address of the peer
 * @agent: (nullable): The User-Agent header of the peer or %NULL if not
 * unknown.
 * @cancellable: (nullable): A cancellable which can be used to cancel the
 * operation.
 * @callback: Callback to call after the function is done.
 * @user_data: Some user data.
 *
 * Check asynchronously whether an IP address is allowed to access
 * this resource.
 *
 * This function is optional. [method@GUPnP.Acl.can_sync] should return %TRUE
 * if the implementing class supports it. If it is supported, GUPnP will
 * prefer to use this function over [method@GUPnP.Acl.is_allowed].
 *
 * Implement this function if the process of verifying the access right
 * is expected to take some time, for example when using D-Bus etc.
 *
 * Use [method@GUPnP.Acl.is_allowed_finish] to retrieve the result.
 *
* Since: 0.20.11
 */
void
gupnp_acl_is_allowed_async (GUPnPAcl           *self,
                            GUPnPDevice        *device,
                            GUPnPService       *service,
                            const char         *path,
                            const char         *address,
                            const char         *agent,
                            GCancellable       *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer            user_data)
{
        g_return_if_fail (GUPNP_IS_ACL (self));

        GUPNP_ACL_GET_IFACE (self)->is_allowed_async (self,
                                                      device,
                                                      service,
                                                      path,
                                                      address,
                                                      agent,
                                                      cancellable,
                                                      callback,
                                                      user_data);
}

/**
 * gupnp_acl_is_allowed_finish:
 * @self: An instance of #GUPnPAcl
 * @res: [iface@Gio.AsyncResult] obtained from the callback passed to [method@GUPnP.Acl.is_allowed_async]
 * @error: (inout)(optional)(nullable): A return location for a #GError describing the failure
 * @returns %TRUE if the authentication was successful, %FALSE otherwise and on
 * error. Check @error for details.
 *
 * Get the result of [method@GUPnP.Acl.is_allowed_async].
 *
 * Since: 0.20.11
 */
gboolean
gupnp_acl_is_allowed_finish (GUPnPAcl      *self,
                             GAsyncResult  *res,
                             GError       **error)
{
        g_return_val_if_fail (GUPNP_IS_ACL (self), FALSE);

        return GUPNP_ACL_GET_IFACE (self)->is_allowed_finish (self,
                                                              res,
                                                              error);
}

/**
 * gupnp_acl_can_sync:
 * @self: A #GUPnPAcl
 * @returns %TRUE, if gupnp_acl_is_allowed_async() is supported, %FALSE
 * otherwise.
 *
 * Check whether [method@GUPnP.Acl.is_allowed_async] is supported.
 *
 * Since: 0.20.11
 */
gboolean
gupnp_acl_can_sync (GUPnPAcl *self)
{
        g_return_val_if_fail (GUPNP_IS_ACL (self), FALSE);

        return GUPNP_ACL_GET_IFACE (self)->can_sync (self);
}

///////////////////////////////////////////////////////////////////
// Internal helper functions
//

/**
 * acl_server_handler_new:
 * @service: (nullable): A #GUPnPContext or %NULL if unknown
 * @context: The #GUPnPContext the server handler is run on.
 * @callback: The #SoupServerCallback we're wrapping.
 * @user_data: The user_data for @callback
 * @notify: (nullable): The #GDestroyNotify for @user_data or %NULL if none.
 * @returns: A newly allocated #AclServerHandler
 *
 * Allocate a new #AclServerHandler.
 *
 */
AclServerHandler *
acl_server_handler_new (GUPnPService *service,
                        GUPnPContext *context,
                        SoupServerCallback callback,
                        gpointer user_data,
                        GDestroyNotify notify)
{
        AclServerHandler *handler = g_new0 (AclServerHandler, 1);

        handler->service = service;
        handler->context = context;
        handler->callback = callback;
        handler->user_data = user_data;
        handler->notify = notify;

        return handler;
}

/**
 * acl_server_handler_free:
 * @handler: An #AclServerHandler instance.
 *
 * Free an #AclServerHandler previously allocated with acl_server_handler_new().
 *
 */
void
acl_server_handler_free (AclServerHandler *handler)
{
        handler->service = NULL;
        handler->context = NULL;

        if (handler->notify != NULL)
                handler->notify (handler->user_data);

        g_free (handler);
}

/**
 * acl_async_handler_new:
 * @server: A #SoupServer instance.
 * @message: The #SoupMessage we want to handle.
 * @path: The path we're trying to serve.
 * @query: (nullable): The query parameters of the request.
 * @client: The #SoupClientContext for this request.
 * @handler: The #AclServerHandler used with this request.
 * @returns: A new instance of #AclAsyncHandler.
 *
 * Create a new async closure.
 *
 */
AclAsyncHandler *
acl_async_handler_new (SoupServer *server,
                       SoupServerMessage *message,
                       const char *path,
                       GHashTable *query,
                       AclServerHandler *handler)
{
        AclAsyncHandler *data = g_slice_new0 (AclAsyncHandler);

        data->server = g_object_ref (server);
        data->message = g_object_ref (message);
        data->path = g_strdup (path);
        if (query != NULL)
                data->query = g_hash_table_ref (query);
        data->handler = handler;

        return data;
}

/**
 * acl_async_handler_free:
 * @handler: An instance allocated with acl_async_handler_new()
 *
 * Free an #AclAsyncHandler allocated with acl_async_handler_new().
 *
 */
void
acl_async_handler_free (AclAsyncHandler *handler)
{
        g_object_unref (handler->server);
        g_object_unref (handler->message);
        g_free (handler->path);
        if (handler->query != NULL)
                g_hash_table_unref (handler->query);
        // g_boxed_free (SOUP_TYPE_CLIENT_CONTEXT, handler->client);

        g_slice_free (AclAsyncHandler, handler);
}
