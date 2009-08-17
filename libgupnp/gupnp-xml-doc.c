/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 * Copyright (C) 2009 Nokia Corporation, all rights reserved.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gupnp-xml-doc-
 * @short_description: GObject for xmlDoc.
 *
 * GObject for xmlDoc, so that we can use refcounting and weak
 * references.
 */

#include <string.h>
#include "gupnp-xml-doc.h"

G_DEFINE_TYPE (GUPnPXMLDoc,
               gupnp_xml_doc,
               G_TYPE_INITIALLY_UNOWNED);

static void
gupnp_xml_doc_init (GUPnPXMLDoc *doc)
{
        /* Empty */
}

static void
gupnp_xml_doc_finalize (GObject *object)
{
        GUPnPXMLDoc *doc;

        doc = GUPNP_XML_DOC (object);

        xmlFreeDoc (doc->doc);
}

static void
gupnp_xml_doc_class_init (GUPnPXMLDocClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gupnp_xml_doc_finalize;
}

/* Takes ownership of @doc */
GUPnPXMLDoc *
gupnp_xml_doc_new (xmlDoc *xml_doc)
{
        GUPnPXMLDoc *doc;

        g_return_val_if_fail (xml_doc != NULL, NULL);

        doc = g_object_new (GUPNP_TYPE_XML_DOC, NULL);

        doc->doc = xml_doc;

        return doc;
}
