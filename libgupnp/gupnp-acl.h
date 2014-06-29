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

#ifndef __GUPNP_ACL_H__
#define __GUPNP_ACL_H__

#include <glib.h>
#include <glib-object.h>
#include <libsoup/soup-session.h>

G_BEGIN_DECLS

GType
gupnp_acl_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_ACL (gupnp_acl_get_type())

#define GUPNP_ACL(obj)                  \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
     GUPNP_TYPE_ACL,                    \
     GUPnPAcl))

#define GUPNP_IS_ACL(obj)               \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj),  \
     GUPNP_TYPE_ACL))

#define GUPNP_ACL_GET_INTERFACE(obj)      \
    (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
     GUPNP_TYPE_ACL, GUPnPAclInterface))

/**
 * GUPnPAcl:
 *
 * Handle to an object implementing the #GUPnPAclInterface interface.
 */
typedef struct _GUPnPAcl GUPnPAcl;

/**
 * GUPnPAclInterface:
 * @parent: The parent interface.
 * @is_allowed: Check whether access to the resource is granted.
 * @is_allowed_async: Asynchronously check whether the access is granted.
 * @is_allowed_finish: Conclude the @is_allowed_async operation.
 * @can_sync: Whether the ACL can do sync queries.
 *
 * Implement a simple access control list for GUPnP.
 *
 * Since: 0.20.11
 */
typedef struct _GUPnPAclInterface GUPnPAclInterface;

/* Forward declarations to avoid recursive includes */
struct _GUPnPDevice;
struct _GUPnPService;

struct _GUPnPAclInterface {
    GTypeInterface parent;

    gboolean (*is_allowed) (GUPnPAcl     *self,
                            struct _GUPnPDevice  *device,
                            struct _GUPnPService *service,
                            const char   *path,
                            const char   *address,
                            const char   *agent);

    void     (*is_allowed_async) (GUPnPAcl           *self,
                                  struct _GUPnPDevice        *device,
                                  struct _GUPnPService       *service,
                                  const char         *path,
                                  const char         *address,
                                  const char         *agent,
                                  GCancellable       *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer            user_data);

    gboolean (*is_allowed_finish) (GUPnPAcl      *self,
                                   GAsyncResult  *res,
                                   GError       **error);

    gboolean (*can_sync)          (GUPnPAcl *self);


    /*< private >*/
    /* future padding */
    void (* _gupnp_reserved1) (void);
    void (* _gupnp_reserved2) (void);
    void (* _gupnp_reserved3) (void);
    void (* _gupnp_reserved4) (void);
};

gboolean
gupnp_acl_is_allowed (GUPnPAcl     *self,
                      struct _GUPnPDevice  *device,
                      struct _GUPnPService *service,
                      const char   *path,
                      const char   *address,
                      const char   *agent);

void
gupnp_acl_is_allowed_async (GUPnPAcl           *self,
                            struct _GUPnPDevice        *device,
                            struct _GUPnPService       *service,
                            const char         *path,
                            const char         *address,
                            const char         *agent,
                            GCancellable       *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer            user_data);

gboolean
gupnp_acl_is_allowed_finish (GUPnPAcl      *self,
                             GAsyncResult  *res,
                             GError       **error);

gboolean
gupnp_acl_can_sync (GUPnPAcl *self);

G_END_DECLS

#endif
