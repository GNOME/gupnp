/*
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_GVALUE_UTIL_H
#define GUPNP_GVALUE_UTIL_H

#include <glib-object.h>
#include <libxml/tree.h>

G_GNUC_INTERNAL gboolean
gvalue_util_set_value_from_string      (GValue       *value,
                                        const char   *str);

G_GNUC_INTERNAL gboolean
gvalue_util_set_value_from_xml_node    (GValue       *value,
                                        xmlNode      *node);

G_GNUC_INTERNAL gboolean
gvalue_util_value_append_to_xml_string (const GValue *value,
                                        GString      *str);

G_GNUC_INTERNAL void
gvalue_free                            (gpointer data);

#endif /* GUPNP_GVALUE_UTIL_H */
