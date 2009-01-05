/*
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GENA_PROTOCOL_H__
#define __GENA_PROTOCOL_H__

G_BEGIN_DECLS

#define GENA_METHOD_SUBSCRIBE   "SUBSCRIBE"
#define GENA_METHOD_UNSUBSCRIBE "UNSUBSCRIBE"
#define GENA_METHOD_NOTIFY      "NOTIFY"

#define GENA_MIN_TIMEOUT     1800
#define GENA_MAX_TIMEOUT     604800 /* 7 days */
#define GENA_DEFAULT_TIMEOUT 1800

G_END_DECLS

#endif /* __GENA_PROTOCOL_H__ */
