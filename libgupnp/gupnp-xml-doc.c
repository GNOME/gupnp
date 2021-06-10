/*
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *                               <zeeshan.ali@nokia.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

/**
 * SECTION:gupnp-xml-doc
 * @short_description: GObject wrapper for xmlDoc.
 *
 * GObject wrapper for xmlDoc, so that we can use refcounting and weak
 * references.
 *
 * Since: 0.13.0
 */

#include <config.h>
#include <string.h>
#include "gupnp-xml-doc.h"
#include "gupnp-error.h"

/**
 * GUPnPXMLDoc:
 * @doc: Pointer to the document.
 *
 * Reference-counting wrapper for libxml's #xmlDoc
 */
struct _GUPnPXMLDoc {
        GObject parent;
        xmlDoc *doc;
};

G_DEFINE_TYPE (GUPnPXMLDoc,
               gupnp_xml_doc,
               G_TYPE_OBJECT)

static void
gupnp_xml_doc_init (G_GNUC_UNUSED GUPnPXMLDoc *doc)
{
        /* Empty */
}

static void
gupnp_xml_doc_finalize (GObject *object)
{
        GUPnPXMLDoc *doc;

        doc = GUPNP_XML_DOC (object);

        g_clear_pointer (&doc->doc, xmlFreeDoc);

        G_OBJECT_CLASS (gupnp_xml_doc_parent_class)->finalize (object);
}

static void
gupnp_xml_doc_class_init (GUPnPXMLDocClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gupnp_xml_doc_finalize;
}

/**
 * gupnp_xml_doc_new:
 * @xml_doc:(transfer full): Pointer to #xmlDoc to wrap under this object
 *
 * Create a new #GUPnPXMLDoc for @xml_doc.
 *
 * Return value: A new #GUPnPXMLDoc, or %NULL on an error
 *
 * Since: 0.14.0
 **/
GUPnPXMLDoc *
gupnp_xml_doc_new (xmlDoc *xml_doc)
{
        GUPnPXMLDoc *doc;

        g_return_val_if_fail (xml_doc != NULL, NULL);

        doc = g_object_new (GUPNP_TYPE_XML_DOC, NULL);

        doc->doc = xml_doc;

        return doc;
}

/**
 * gupnp_xml_doc_new_from_path:
 * @path: Path to xml document
 * @error:(inout)(optional)(nullable): Location to put the error into
 *
 * Create a new #GUPnPXMLDoc for the XML document at @path.
 *
 * Return value:(nullable): A new #GUPnPXMLDoc, or %NULL on an error
 *
 * Since: 0.14.0
 **/
GUPnPXMLDoc *
gupnp_xml_doc_new_from_path (const char *path,
                             GError    **error)
{
        xmlDoc *doc;
        int flags;

        flags = XML_PARSE_PEDANTIC;

        if (!g_getenv ("GUPNP_DEBUG")) {
                flags |= XML_PARSE_NOWARNING | XML_PARSE_NOERROR;
        }

        g_return_val_if_fail (path != NULL, NULL);
        doc = xmlReadFile (path, NULL, flags);
        if (doc == NULL) {
                g_set_error (error,
                             GUPNP_XML_ERROR,
                             GUPNP_XML_ERROR_PARSE,
                             "Failed to parse %s\n",
                             path);

                return NULL;
        }

        return gupnp_xml_doc_new (doc);
}

/**
 * gupnp_xml_doc_get_doc:
 * @xml_doc: A #GUPnPXMLDoc
 *
 * Returns: a pointer to the wrapped #xmlDoc
 */
const xmlDoc *
gupnp_xml_doc_get_doc (GUPnPXMLDoc *xml_doc) {
    return xml_doc->doc;
}
