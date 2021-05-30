/*
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_GENA_PROTOCOL_H
#define GUPNP_GENA_PROTOCOL_H

G_BEGIN_DECLS

#define GENA_METHOD_SUBSCRIBE   "SUBSCRIBE"
#define GENA_METHOD_UNSUBSCRIBE "UNSUBSCRIBE"
#define GENA_METHOD_NOTIFY      "NOTIFY"

#define GENA_MIN_TIMEOUT     1800
#define GENA_MAX_TIMEOUT     604800 /* 7 days */
#define GENA_DEFAULT_TIMEOUT 1800

G_END_DECLS

#endif /* GUPNP_GENA_PROTOCOL_H */
