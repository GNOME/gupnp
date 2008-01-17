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

#ifndef __ACCEPT_LANGUAGE_H__
#define __ACCEPT_LANGUAGE_H__

#include <libsoup/soup-message.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL char *
accept_language_get_header  (void);

G_GNUC_INTERNAL GList *
accept_language_get_locales (SoupMessage *message);

G_GNUC_INTERNAL int
http_language_from_locale   (char        *lang);

G_GNUC_INTERNAL int
locale_from_http_language   (char        *lang);

G_END_DECLS

#endif /* __ACCEPT_LANGUAGE_H__ */
