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



#define GUPNP_TYPE_UNIX_CONTEXT_MANAGER \
                (gupnp_unix_context_manager_get_type ())
G_DECLARE_FINAL_TYPE (GUPnPUnixContextManager,
                      gupnp_unix_context_manager,
                      GUPNP,
                      UNIX_CONTEXT_MANAGER,
                      GUPnPSimpleContextManager)
G_END_DECLS

#endif /* GUPNP_UNIX_CONTEXT_MANAGER_H */
