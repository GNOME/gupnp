/*
 * Copyright (C) 2009,2011 Nokia Corporation.
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jens Georg <mail@jensge.org>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
