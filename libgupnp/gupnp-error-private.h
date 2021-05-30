/*
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_ERROR_PRIVATE_H
#define GUPNP_ERROR_PRIVATE_H

#include <libsoup/soup-message.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL void
_gupnp_error_set_server_error (GError     **error,
                               SoupMessage *msg);

G_GNUC_INTERNAL GError *
_gupnp_error_new_server_error (SoupMessage *msg);

G_END_DECLS

#endif /* GUPNP_ERROR_PRIVATE_H */
