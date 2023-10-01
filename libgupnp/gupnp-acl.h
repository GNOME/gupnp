/*
 * Copyright (C) 2013,2014 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_ACL_H
#define GUPNP_ACL_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GUPNP_TYPE_ACL (gupnp_acl_get_type())
G_DECLARE_INTERFACE (GUPnPAcl, gupnp_acl, GUPNP, ACL, GObject)


typedef struct _GUPnPAcl GUPnPAcl;
typedef struct _GUPnPAclInterface GUPnPAclInterface;

/* Forward declarations to avoid recursive includes */
typedef struct _GUPnPDevice GUPnPDevice;
typedef struct _GUPnPService GUPnPService;

struct _GUPnPAclInterface {
    GTypeInterface parent;

    gboolean (*is_allowed) (GUPnPAcl *self,
                            GUPnPDevice *device,
                            GUPnPService *service,
                            const char *path,
                            const char *address,
                            const char *agent);

    void (*is_allowed_async) (GUPnPAcl *self,
                              GUPnPDevice *device,
                              GUPnPService *service,
                              const char *path,
                              const char *address,
                              const char *agent,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data);

    gboolean (*is_allowed_finish) (GUPnPAcl      *self,
                                   GAsyncResult  *res,
                                   GError       **error);

    gboolean (*can_sync)          (GUPnPAcl *self);


    /*< private >*/
#ifndef GOBJECT_INTROSPECTION_SKIP
    /* future padding */
    void (* _gupnp_reserved1) (void);
    void (* _gupnp_reserved2) (void);
    void (* _gupnp_reserved3) (void);
    void (* _gupnp_reserved4) (void);
#endif
};

gboolean
gupnp_acl_is_allowed (GUPnPAcl *self,
                      GUPnPDevice *device,
                      GUPnPService *service,
                      const char *path,
                      const char *address,
                      const char *agent);

void
gupnp_acl_is_allowed_async (GUPnPAcl *self,
                            GUPnPDevice *device,
                            GUPnPService *service,
                            const char *path,
                            const char *address,
                            const char *agent,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data);

gboolean
gupnp_acl_is_allowed_finish (GUPnPAcl      *self,
                             GAsyncResult  *res,
                             GError       **error);

gboolean
gupnp_acl_can_sync (GUPnPAcl *self);

G_END_DECLS

#endif /* GUPNP_ACL_H */
