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
 * SECTION:gupnp-xml-doc-wrapper
 * @short_description: GObject wrapper for xmlDoc.
 *
 * GObject wrapper for xmlDoc, so that we can use refcounting and weak
 * references.
 */

#include <string.h>
#include "gupnp-xml-doc-wrapper.h"

G_DEFINE_TYPE (GUPnPXMLDocWrapper,
               gupnp_xml_doc_wrapper,
               G_TYPE_INITIALLY_UNOWNED);

static void
gupnp_xml_doc_wrapper_init (GUPnPXMLDocWrapper *wrapper)
{
        /* Empty */
}

static void
gupnp_xml_doc_wrapper_finalize (GObject *object)
{
        GUPnPXMLDocWrapper *wrapper;

        wrapper = GUPNP_XML_DOC_WRAPPER (object);

        xmlFreeDoc (wrapper->doc);
}

static void
gupnp_xml_doc_wrapper_class_init (GUPnPXMLDocWrapperClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gupnp_xml_doc_wrapper_finalize;
}

/* Takes ownership of @doc */
GUPnPXMLDocWrapper *
gupnp_xml_doc_wrapper_new (xmlDoc *doc)
{
        GUPnPXMLDocWrapper *wrapper;

        g_return_val_if_fail (doc != NULL, NULL);

        wrapper = g_object_new (GUPNP_TYPE_XML_DOC_WRAPPER, NULL);

        wrapper->doc = doc;

        return wrapper;
}
