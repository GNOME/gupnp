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

#ifndef __GUPNP_XML_DOC_H__
#define __GUPNP_XML_DOC_H__

#include <libxml/tree.h>
#include <glib-object.h>

GType
gupnp_xml_doc_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_XML_DOC \
                (gupnp_xml_doc_get_type ())
#define GUPNP_XML_DOC(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_XML_DOC, \
                 GUPnPXMLDoc))
#define GUPNP_XML_DOC_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_XML_DOC, \
                 GUPnPXMLDocClass))
#define GUPNP_IS_XML_DOC(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_XML_DOC))
#define GUPNP_IS_XML_DOC_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_XML_DOC))
#define GUPNP_XML_DOC_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_XML_DOC, \
                 GUPnPXMLDocClass))

typedef struct _GUPnPXMLDoc GUPnPXMLDoc;
typedef struct _GUPnPXMLDocClass GUPnPXMLDocClass;

struct _GUPnPXMLDoc {
        GObject parent;
        /*< public >*/
        xmlDoc *doc;
};

struct _GUPnPXMLDocClass {
        GObjectClass parent_class;
};

GUPnPXMLDoc *
gupnp_xml_doc_new                       (xmlDoc         *xml_doc);

GUPnPXMLDoc *
gupnp_xml_doc_new_from_path             (const char     *path,
                                         GError        **error);

#endif /* __GUPNP_XML_DOC_H__ */
