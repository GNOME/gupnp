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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef GUPNP_RESOURCE_FACTORY_H
#define GUPNP_RESOURCE_FACTORY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GUPNP_TYPE_RESOURCE_FACTORY \
                (gupnp_resource_factory_get_type ())
G_DECLARE_DERIVABLE_TYPE (GUPnPResourceFactory,
                          gupnp_resource_factory,
                          GUPNP,
                          RESOURCE_FACTORY,
                          GObject)

struct _GUPnPResourceFactoryClass {
        GObjectClass parent_class;

        /* future padding */
        void (* _gupnp_reserved1) (void);
        void (* _gupnp_reserved2) (void);
        void (* _gupnp_reserved3) (void);
        void (* _gupnp_reserved4) (void);
};

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

#endif /* GUPNP_RESOURCE_FACTORY_H */
