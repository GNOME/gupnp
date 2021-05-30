/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *                               <zeeshan.ali@nokia.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_XML_DOC_H
#define GUPNP_XML_DOC_H

#include <libxml/tree.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GUPNP_TYPE_XML_DOC \
                (gupnp_xml_doc_get_type ())

G_DECLARE_FINAL_TYPE (GUPnPXMLDoc,
                      gupnp_xml_doc,
                      GUPNP,
                      XML_DOC,
                      GObject)

GUPnPXMLDoc *
gupnp_xml_doc_new                       (xmlDoc         *xml_doc);

GUPnPXMLDoc *
gupnp_xml_doc_new_from_path             (const char     *path,
                                         GError        **error);

const xmlDoc *
gupnp_xml_doc_get_doc                   (GUPnPXMLDoc    *xml_doc);

G_END_DECLS

#endif /* GUPNP_XML_DOC_H */
