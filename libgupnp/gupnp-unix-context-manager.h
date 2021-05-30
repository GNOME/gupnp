/*
 * Copyright (C) 2009 Nokia Corporation.
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_UNIX_CONTEXT_MANAGER_H
#define GUPNP_UNIX_CONTEXT_MANAGER_H

#include "gupnp-simple-context-manager.h"

G_BEGIN_DECLS



#define GUPNP_TYPE_UNIX_CONTEXT_MANAGER \
                (gupnp_unix_context_manager_get_type ())
G_DECLARE_FINAL_TYPE (GUPnPUnixContextManager,
                      gupnp_unix_context_manager,
                      GUPNP,
                      UNIX_CONTEXT_MANAGER,
                      GUPnPSimpleContextManager)
G_END_DECLS

#endif /* GUPNP_UNIX_CONTEXT_MANAGER_H */
