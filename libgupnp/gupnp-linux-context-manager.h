/*
 * Copyright (C) 2011 Jens Georg.
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_LINUX_CONTEXT_MANAGER_H
#define GUPNP_LINUX_CONTEXT_MANAGER_H

#include <glib.h>
#include <glib-object.h>

#include "gupnp-context-manager.h"

G_BEGIN_DECLS

#define GUPNP_TYPE_LINUX_CONTEXT_MANAGER \
                (gupnp_linux_context_manager_get_type ())

G_DECLARE_FINAL_TYPE (GUPnPLinuxContextManager,
                      gupnp_linux_context_manager,
                      GUPNP,
                      LINUX_CONTEXT_MANAGER,
                      GUPnPContextManager)

G_GNUC_INTERNAL gboolean gupnp_linux_context_manager_is_available (void);

G_END_DECLS

#endif /* GUPNP_LINUX_CONTEXT_MANAGER_H */
