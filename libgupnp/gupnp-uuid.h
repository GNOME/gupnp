/*
 * Copyright (C) 2015 Jens Georg
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_UUID_H
#define GUPNP_UUID_H

#include <glib.h>

G_BEGIN_DECLS

G_GNUC_DEPRECATED_FOR(g_uuid_string_random) char *
gupnp_get_uuid (void);

G_END_DECLS

#endif /* GUPNP_UUID_H */
