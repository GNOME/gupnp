/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *                               <zeeshan.ali@nokia.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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
