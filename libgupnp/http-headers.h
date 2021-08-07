/*
 * Copyright (C) 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_HTTP_HEADERS_H
#define GUPNP_HTTP_HEADERS_H

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
http_request_get_accept_locales (SoupMessageHeaders *message);

G_GNUC_INTERNAL void
http_response_set_content_locale (SoupMessageHeaders *message,
                                  const char *locale);

G_GNUC_INTERNAL void
http_response_set_content_type (SoupMessageHeaders *response_headers,
                                const char *path,
                                const guchar *data,
                                gsize data_size);

G_GNUC_INTERNAL void
http_response_set_content_range  (SoupMessage  *message,
                                  gsize         offset,
                                  gsize         length,
                                  gsize         total);

G_GNUC_INTERNAL void
http_response_set_body_gzip (SoupServerMessage *msg,
                             const char *body,
                             const gsize length);

G_END_DECLS

#endif /* GUPNP_HTTP_HEADERS_H */
