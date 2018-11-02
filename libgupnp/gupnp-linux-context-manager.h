/*
 * Copyright (C) 2011 Jens Georg.
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
