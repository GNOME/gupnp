/*
 * Copyright (C) 2007 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
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

#ifndef __GUPNP_RESOURCE_FACTORY_H__
#define __GUPNP_RESOURCE_FACTORY_H__

#include <glib-object.h>

G_BEGIN_DECLS

GType
gupnp_resource_factory_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_RESOURCE_FACTORY \
                (gupnp_resource_factory_get_type ())
#define GUPNP_RESOURCE_FACTORY(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_RESOURCE_FACTORY, \
                 GUPnPResourceFactory))
#define GUPNP_RESOURCE_FACTORY_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_RESOURCE_FACTORY, \
                 GUPnPResourceFactoryClass))
#define GUPNP_IS_RESOURCE_FACTORY(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_RESOURCE_FACTORY))
#define GUPNP_IS_RESOURCE_FACTORY_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_RESOURCE_FACTORY))
#define GUPNP_RESOURCE_FACTORY_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_RESOURCE_FACTORY, \
                 GUPnPResourceFactoryClass))

typedef struct _GUPnPResourceFactoryPrivate GUPnPResourceFactoryPrivate;

/**
 * GUPnPResourceFactory:
 *
 * This struct contains private data only, and should be accessed using the
 * functions below.
 */
typedef struct {
        GObject parent;

        GUPnPResourceFactoryPrivate *priv;
} GUPnPResourceFactory;

typedef struct {
        GObjectClass parent_class;

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
} GUPnPResourceFactoryClass;

GUPnPResourceFactory *
gupnp_resource_factory_new         (void);

GUPnPResourceFactory *
gupnp_resource_factory_get_default (void);

void
gupnp_resource_factory_register_resource_type
                                   (GUPnPResourceFactory *factory,
                                    const char           *upnp_type,
                                    GType                 type);

gboolean
gupnp_resource_factory_unregister_resource_type
                                   (GUPnPResourceFactory *factory,
                                    const char           *upnp_type);

void
gupnp_resource_factory_register_resource_proxy_type
                                   (GUPnPResourceFactory *factory,
                                    const char           *upnp_type,
                                    GType                 type);

gboolean
gupnp_resource_factory_unregister_resource_proxy_type
                                   (GUPnPResourceFactory *factory,
                                    const char           *upnp_type);

G_END_DECLS

#endif /* __GUPNP_RESOURCE_FACTORY_H__ */
