/*
 * Copyright (C) 2009,2011 Nokia Corporation.
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jens Georg <mail@jensge.org>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
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

/**
 * SECTION:gupnp-simple-context-manager
 * @short_description: Abstract implementation of basic #GUPnPContextManager.
 *
 */

#include <config.h>
#include <errno.h>
#include <gio/gio.h>
#include <libgssdp/gssdp-error.h>

#include "gupnp-simple-context-manager.h"
#include "gupnp-context.h"

G_DEFINE_ABSTRACT_TYPE (GUPnPSimpleContextManager,
                        gupnp_simple_context_manager,
                        GUPNP_TYPE_CONTEXT_MANAGER);

struct _GUPnPSimpleContextManagerPrivate {
        GList *contexts; /* List of GUPnPContext instances */

        GSource *idle_context_creation_src;
};

static GList*
gupnp_simple_context_manager_get_interfaces (GUPnPSimpleContextManager *manager)
{
        GUPnPSimpleContextManagerClass *object_class =
                              GUPNP_SIMPLE_CONTEXT_MANAGER_GET_CLASS (manager);

        return object_class->get_interfaces (manager);
}

static void
create_and_signal_context (const char                *interface,
                           GUPnPSimpleContextManager *manager)
{
        GUPnPContext *context;
        guint port;

        GError *error;

        g_object_get (manager,
                      "port", &port,
                      NULL);

        error = NULL;
        context = g_initable_new (GUPNP_TYPE_CONTEXT,
                                  NULL,
                                  &error,
                                  "interface", interface,
                                  "port", port,
                                  NULL);
        if (error != NULL) {
                if (!(error->domain == GSSDP_ERROR &&
                      error->code == GSSDP_ERROR_NO_IP_ADDRESS))
                        g_warning
                           ("Failed to create context for interface '%s': %s",
                            interface,
                            error->message);

                g_error_free (error);

                return;
        }

        g_signal_emit_by_name (manager,
                               "context-available",
                               context);

        manager->priv->contexts = g_list_append (manager->priv->contexts,
                                                 context);
}

/*
 * Create a context for all network interfaces that are up.
 */
static gboolean
create_contexts (gpointer data)
{
        GUPnPSimpleContextManager *manager = (GUPnPSimpleContextManager *) data;
        GList *ifaces;

        manager->priv->idle_context_creation_src = NULL;

        if (manager->priv->contexts != NULL)
               return FALSE;

        ifaces = gupnp_simple_context_manager_get_interfaces (manager);
        while (ifaces) {
                create_and_signal_context ((char *) ifaces->data, manager);
                g_free (ifaces->data);
                ifaces = g_list_delete_link (ifaces, ifaces);
        }

        return FALSE;
}

static void
destroy_contexts (GUPnPSimpleContextManager *manager)
{
        while (manager->priv->contexts) {
                GUPnPContext *context;

                context = GUPNP_CONTEXT (manager->priv->contexts->data);

                g_signal_emit_by_name (manager,
                                       "context-unavailable",
                                       context);
                g_object_unref (context);

                manager->priv->contexts = g_list_delete_link
                                        (manager->priv->contexts,
                                         manager->priv->contexts);
        }
}

static void
schedule_contexts_creation (GUPnPSimpleContextManager *manager)
{
        manager->priv->idle_context_creation_src = NULL;

        /* Create contexts in mainloop so that is happens after user has hooked
         * to the "context-available" signal.
         */
        manager->priv->idle_context_creation_src = g_idle_source_new ();
        g_source_attach (manager->priv->idle_context_creation_src,
                         g_main_context_get_thread_default ());
        g_source_set_callback (manager->priv->idle_context_creation_src,
                               create_contexts,
                               manager,
                               NULL);
        g_source_unref (manager->priv->idle_context_creation_src);
}

static void
gupnp_simple_context_manager_init (GUPnPSimpleContextManager *manager)
{
        manager->priv =
                G_TYPE_INSTANCE_GET_PRIVATE (manager,
                                             GUPNP_TYPE_SIMPLE_CONTEXT_MANAGER,
                                             GUPnPSimpleContextManagerPrivate);
}

static void
gupnp_simple_context_manager_constructed (GObject *object)
{
        GObjectClass *parent_class;
        GUPnPSimpleContextManager *manager;

        manager = GUPNP_SIMPLE_CONTEXT_MANAGER (object);
        schedule_contexts_creation (manager);

        /* Chain-up */
        parent_class = G_OBJECT_CLASS (gupnp_simple_context_manager_parent_class);
        if (parent_class->constructed != NULL)
                parent_class->constructed (object);
}

static void
gupnp_simple_context_manager_dispose (GObject *object)
{
        GUPnPSimpleContextManager *manager;
        GObjectClass *object_class;

        manager = GUPNP_SIMPLE_CONTEXT_MANAGER (object);

        destroy_contexts (manager);

        if (manager->priv->idle_context_creation_src) {
                g_source_destroy (manager->priv->idle_context_creation_src);
                manager->priv->idle_context_creation_src = NULL;
        }


        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_simple_context_manager_parent_class);
        object_class->dispose (object);
}

static void
gupnp_simple_context_manager_class_init (GUPnPSimpleContextManagerClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->constructed  = gupnp_simple_context_manager_constructed;
        object_class->dispose      = gupnp_simple_context_manager_dispose;

        g_type_class_add_private (klass,
                                  sizeof (GUPnPSimpleContextManagerPrivate));
}
