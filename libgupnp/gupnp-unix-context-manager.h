/*
 * Copyright (C) 2009 Nokia Corporation.
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
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

#ifndef GUPNP_UNIX_CONTEXT_MANAGER_H
#define GUPNP_UNIX_CONTEXT_MANAGER_H

#include "gupnp-simple-context-manager.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL GType
gupnp_unix_context_manager_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_UNIX_CONTEXT_MANAGER \
                (gupnp_unix_context_manager_get_type ())
#define GUPNP_UNIX_CONTEXT_MANAGER(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_UNIX_CONTEXT_MANAGER, \
                 GUPnPUnixContextManager))
#define GUPNP_UNIX_CONTEXT_MANAGER_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_UNIX_CONTEXT_MANAGER, \
                 GUPnPUnixContextManagerClass))
#define GUPNP_IS_UNIX_CONTEXT_MANAGER(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_UNIX_CONTEXT_MANAGER))
#define GUPNP_IS_UNIX_CONTEXT_MANAGER_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_UNIX_CONTEXT_MANAGER))
#define GUPNP_UNIX_CONTEXT_MANAGER_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_UNIX_CONTEXT_MANAGER, \
                 GUPnPUnixContextManagerClass))

typedef struct _GUPnPUnixContextManagerPrivate GUPnPUnixContextManagerPrivate;

typedef struct {
        GUPnPSimpleContextManager parent;
} GUPnPUnixContextManager;

typedef struct {
        GUPnPSimpleContextManagerClass parent_class;

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
} GUPnPUnixContextManagerClass;

G_END_DECLS

#endif /* GUPNP_UNIX_CONTEXT_MANAGER_H */
