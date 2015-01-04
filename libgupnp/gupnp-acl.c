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

/**
 * SECTION:gupnp-acl
 * @short_description: Object providing a simple access control list for
 * GUPnP.
 *
 * #GUPnPAcl provides either synchronous or asynchronous functions to check
 * whether a peer sould be able to access a resource or not.
 *
 * Since: 0.20.11
 */

#include "gupnp-acl.h"
#include "gupnp-acl-private.h"
#include "gupnp-device.h"

G_DEFINE_INTERFACE(GUPnPAcl, gupnp_acl, G_TYPE_OBJECT)

static void
gupnp_acl_default_init (GUPnPAclInterface *klass)
{
}

/**
 * gupnp_acl_is_allowed:
 * @self: an instance of #GUPnPAcl
 * @device: (allow-none): The #GUPnPDevice associated with @path or %NULL if
 * unknown.
 * @service: (allow-none): The #GUPnPService associated with @path or %NULL if
 * unknown.
 * @path: The path being served.
 * @address: IP address of the peer.
 * @agent: (allow-none): The User-Agent header of the peer or %NULL if not
 * unknown.
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

        return GUPNP_ACL_GET_INTERFACE (self)->is_allowed (self,
                                                           device,
                                                           service,
                                                           path,
                                                           address,
                                                           agent);
}

/**
 * gupnp_acl_is_allowed_async:
 * @self: a #GUPnPAcl
 * @device: (allow-none): The #GUPnPDevice associated with @path or %NULL if
 * unknown.
 * @service: (allow-none): The #GUPnPService associated with @path or %NULL if
 * unknown.
 * @path: The path being served.
 * @address: IP address of the peer
 * @agent: (allow-none): The User-Agent header of the peer or %NULL if not
 * unknown.
 * @cancellable: (allow-none): A #GCancellable which can be used to cancel the
 * operation.
 * @callback: Callback to call after the function is done.
 * @user_data: Some user data.
 *
 * Optional. Check asynchronously whether an IP address is allowed to access
 * this resource. Use this function if the process of verifying the access right
 * is expected to take some time, for example when using D-Bus etc.
 *
 * If this function is supported, gupnp_acl_can_sync() should return %TRUE.
 *
 * Use gupnp_acl_is_allowed_finish() to retrieve the result.
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

        GUPNP_ACL_GET_INTERFACE (self)->is_allowed_async (self,
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
 * @res: %GAsyncResult obtained from the callback in gupnp_acl_is_allowed_async()
 * @error: (allow-none): A return location for a #GError describing the failure
 * @returns %TRUE if the authentication was successful, %FALSE otherwise and on
 * error. Check @error for details.
 *
 * Since: 0.20.11
 */
gboolean
gupnp_acl_is_allowed_finish (GUPnPAcl      *self,
                             GAsyncResult  *res,
                             GError       **error)
{
        g_return_val_if_fail (GUPNP_IS_ACL (self), FALSE);

        return GUPNP_ACL_GET_INTERFACE (self)->is_allowed_finish (self,
                                                                  res,
                                                                  error);
}

/**
 * gupnp_acl_can_sync:
 * @self: A #GUPnPAcl
 * @returns %TRUE, if gupnp_acl_is_allowed_async() is supported, %FALSE
 * otherwise.
 *
 * Check whether gupnp_acl_is_allowed_async() is supported.
 *
 * Since: 0.20.11
 */
gboolean
gupnp_acl_can_sync (GUPnPAcl *self)
{
        g_return_val_if_fail (GUPNP_IS_ACL (self), FALSE);

        return GUPNP_ACL_GET_INTERFACE (self)->can_sync (self);
}

/**
 * acl_server_handler_new:
 * @service: (allow-none): A #GUPnPContext or %NULL if unknown
 * @context: The #GUPnPContext the server handler is run on.
 * @callback: The #SoupServerCallback we're wrapping.
 * @user_data: The user_data for @callback
 * @notify: (allow-none): The #GDestroyNotify for @user_data or %NULL if none.
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
 * @query: (allow-none): The query parameters of the request.
 * @client: The #SoupClientContext for this request.
 * @handler: The #AclServerHandler used with this request.
 * @returns: A new instance of #AclAsyncHandler.
 *
 * Create a new async closure.
 *
 */
AclAsyncHandler *
acl_async_handler_new (SoupServer *server,
                       SoupMessage *message,
                       const char *path,
                       GHashTable *query,
                       SoupClientContext *client,
                       AclServerHandler *handler)
{
        AclAsyncHandler *data = g_slice_new0 (AclAsyncHandler);

        data->server = g_object_ref (server);
        data->message = g_object_ref (message);
        data->path = g_strdup (path);
        if (query != NULL)
                data->query = g_hash_table_ref (query);
        data->client = g_boxed_copy (SOUP_TYPE_CLIENT_CONTEXT, client);
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
        g_boxed_free (SOUP_TYPE_CLIENT_CONTEXT, handler->client);

        g_slice_free (AclAsyncHandler, handler);
}
