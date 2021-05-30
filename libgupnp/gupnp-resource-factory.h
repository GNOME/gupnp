/*
 * Copyright (C) 2007 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
