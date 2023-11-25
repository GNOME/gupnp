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

#define G_LOG_DOMAIN "gupnp-xml-doc"

#include <config.h>
#include <string.h>
#include <gio/gio.h>

#include <libxml/parser.h>

#include "gupnp-xml-doc.h"
#include "gupnp-error.h"

/**
 * GUPnPXMLDoc:
 * Reference-counting wrapper for libxml's #xmlDoc
 *
 * Since: 0.14.0
 */
struct _GUPnPXMLDoc {
        GObject parent;
        xmlDoc *doc;
        gboolean initialized;
        char *path;
};

static GInitableIface *initable_parent_iface = NULL;
static void
gupnp_xml_doc_initable_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_EXTENDED (
        GUPnPXMLDoc,
        gupnp_xml_doc,
        G_TYPE_OBJECT,
        0,
        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                               gupnp_xml_doc_initable_iface_init))

enum
{
        PROP_0,
        PROP_DOC,
        PROP_PATH
};

static gboolean
gupnp_xml_doc_initable_init (GInitable *initable,
                             GCancellable *cancellable,
                             GError **error)
{
        GUPnPXMLDoc *self = GUPNP_XML_DOC (initable);
        if (self->initialized == TRUE)
                return TRUE;

        if (self->doc != NULL)
                return TRUE;

        if (self->path == NULL) {
                g_set_error_literal (error,
                                     GUPNP_XML_ERROR,
                                     GUPNP_XML_ERROR_OTHER,
                                     "Neither path nor document passed when "
                                     "creating GUPnPXMLDoc");
                return FALSE;
        }

        int flags = XML_PARSE_PEDANTIC;
        if (!g_getenv ("GUPNP_DEBUG")) {
                flags |= XML_PARSE_NOWARNING | XML_PARSE_NOERROR;
        }

        self->doc = xmlReadFile (self->path, NULL, flags);
        if (self->doc == NULL) {
                g_set_error (error,
                             GUPNP_XML_ERROR,
                             GUPNP_XML_ERROR_PARSE,
                             "Failed to parse %s\n",
                             self->path);

                return FALSE;
        }

        return TRUE;
}


static void
gupnp_xml_doc_initable_iface_init (gpointer g_iface, gpointer iface_data)
{
        (void) iface_data;

        GInitableIface *iface = (GInitableIface *)g_iface;
        initable_parent_iface = g_type_interface_peek_parent (iface);
        iface->init = gupnp_xml_doc_initable_init;
}

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
        g_free (doc->path);

        G_OBJECT_CLASS (gupnp_xml_doc_parent_class)->finalize (object);
}

static void
gupnp_xml_doc_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
        GUPnPXMLDoc *self = GUPNP_XML_DOC (object);
        switch (property_id) {
        case PROP_DOC:
                self->doc = g_value_get_pointer (value);
                break;
        case PROP_PATH:
                self->path = g_value_dup_string (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_xml_doc_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
        (void) value;
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
gupnp_xml_doc_class_init (GUPnPXMLDocClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_xml_doc_set_property;
        object_class->get_property = gupnp_xml_doc_get_property;
        object_class->finalize = gupnp_xml_doc_finalize;

        g_object_class_install_property (
                object_class,
                PROP_DOC,
                g_param_spec_pointer ("doc",
                                      "doc",
                                      "doc",
                                      G_PARAM_CONSTRUCT_ONLY |
                                              G_PARAM_WRITABLE |
                                              G_PARAM_STATIC_STRINGS));
        g_object_class_install_property (
                object_class,
                PROP_PATH,
                g_param_spec_string ("path",
                                     "path",
                                     "path",
                                     NULL,
                                     G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                                             G_PARAM_STATIC_STRINGS));
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
        return g_initable_new (GUPNP_TYPE_XML_DOC,
                               NULL,
                               NULL,
                               "doc",
                               xml_doc,
                               NULL);}

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
        return g_initable_new (GUPNP_TYPE_XML_DOC,
                               NULL,
                               error,
                               "path",
                               path,
                               NULL);
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
