/*
 * GUUL - GUUL Unified UUID Library
 * Copyright (C) 2015 Jens Georg.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __GUUL_H
#define __GUUL_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <glib.h>

G_BEGIN_DECLS

#ifdef GUUL_INTERNAL
G_GNUC_INTERNAL
#endif
char *guul_get_uuid (void);

G_END_DECLS

#endif /* __GUUL_H */
