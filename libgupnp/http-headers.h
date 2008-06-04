/*
 * Copyright (C) 2007, 2008 OpenedHand Ltd.
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

#ifndef __HTTP_HEADERS_H__
#define __HTTP_HEADERS_H__

#include <libsoup/soup-message.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL gboolean
http_request_get_range           (SoupMessage  *message,
                                  gboolean     *have_offset,
                                  gsize        *offset,
                                  gsize        *length);

G_GNUC_INTERNAL void
http_request_set_accept_language (SoupMessage  *message);

G_GNUC_INTERNAL GList *
http_request_get_accept_locales  (SoupMessage  *message);

G_GNUC_INTERNAL void
http_request_set_user_agent      (SoupMessage  *message);

G_GNUC_INTERNAL void
http_response_set_content_locale (SoupMessage  *message,
                                  const char   *locale);

G_GNUC_INTERNAL void
http_response_set_content_type   (SoupMessage  *message,
                                  const char   *path,
                                  const guchar *data,
                                  gsize         data_size);

G_GNUC_INTERNAL void
http_response_set_content_range  (SoupMessage  *message,
                                  gsize         offset,
                                  gsize         length,
                                  gsize         total);

G_END_DECLS

#endif /* __HTTP_HEADERS_H__ */
