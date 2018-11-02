/*
 * Copyright (C) 2009,2011 Nokia Corporation.
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jens Georg <mail@jensge.org>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
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

#ifndef GUPNP_SIMPLE_CONTEXT_MANAGER_H
#define GUPNP_SIMPLE_CONTEXT_MANAGER_H

#include "gupnp-context-manager.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL GType
gupnp_simple_context_manager_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_SIMPLE_CONTEXT_MANAGER \
                (gupnp_simple_context_manager_get_type ())

G_GNUC_INTERNAL
G_DECLARE_DERIVABLE_TYPE (GUPnPSimpleContextManager,
                          gupnp_simple_context_manager,
                          GUPNP,
                          SIMPLE_CONTEXT_MANAGER,
                          GUPnPContextManager)

struct _GUPnPSimpleContextManagerClass {
        GUPnPContextManagerClass parent_class;

        /* vfuncs */
        GList *(* get_interfaces) (GUPnPSimpleContextManager *context_manager);

        /* future padding */
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
};

G_END_DECLS

#endif /* GUPNP_SIMPLE_CONTEXT_MANAGER_H */
