/*
 * Copyright (C) 2009,2011 Nokia Corporation.
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Jens Georg <mail@jensge.org>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#define G_LOG_DOMAIN "gupnp-simple-context-manager"

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

struct _GUPnPSimpleContextManagerPrivate {
        GList *contexts; /* List of GUPnPContext instances */

        GSource *idle_context_creation_src;
};
typedef struct _GUPnPSimpleContextManagerPrivate GUPnPSimpleContextManagerPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GUPnPSimpleContextManager,
                                     gupnp_simple_context_manager,
                                     GUPNP_TYPE_CONTEXT_MANAGER)

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
        GUPnPSimpleContextManagerPrivate *priv;
        guint port;

        GError *error;

        priv = gupnp_simple_context_manager_get_instance_private (manager);
        g_object_get (manager,
                      "port", &port,
                      NULL);

        error = NULL;
        context = g_initable_new (GUPNP_TYPE_CONTEXT,
                                  NULL,
                                  &error,
                                  "interface",
                                  interface,
                                  "port",
                                  port,
                                  "address-family",
                                  gupnp_context_manager_get_socket_family (
                                          GUPNP_CONTEXT_MANAGER (manager)),

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

        priv->contexts = g_list_append (priv->contexts,
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
        GUPnPSimpleContextManagerPrivate *priv;

        priv = gupnp_simple_context_manager_get_instance_private (manager);

        priv->idle_context_creation_src = NULL;

        if (priv->contexts != NULL)
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
        GUPnPSimpleContextManagerPrivate *priv;

        priv = gupnp_simple_context_manager_get_instance_private (manager);
        while (priv->contexts) {
                GUPnPContext *context;

                context = GUPNP_CONTEXT (priv->contexts->data);

                g_signal_emit_by_name (manager,
                                       "context-unavailable",
                                       context);
                g_object_unref (context);

                priv->contexts = g_list_delete_link (priv->contexts,
                                                     priv->contexts);
        }
}

static void
schedule_contexts_creation (GUPnPSimpleContextManager *manager)
{
        GUPnPSimpleContextManagerPrivate *priv;

        priv = gupnp_simple_context_manager_get_instance_private (manager);
        priv->idle_context_creation_src = NULL;

        /* Create contexts in mainloop so that is happens after user has hooked
         * to the "context-available" signal.
         */
        priv->idle_context_creation_src = g_idle_source_new ();
        g_source_attach (priv->idle_context_creation_src,
                         g_main_context_get_thread_default ());
        g_source_set_callback (priv->idle_context_creation_src,
                               create_contexts,
                               manager,
                               NULL);
        g_source_unref (priv->idle_context_creation_src);
}

static void
gupnp_simple_context_manager_init (GUPnPSimpleContextManager *manager)
{
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
        GUPnPSimpleContextManagerPrivate *priv;

        manager = GUPNP_SIMPLE_CONTEXT_MANAGER (object);
        priv = gupnp_simple_context_manager_get_instance_private (manager);

        destroy_contexts (manager);

        if (priv->idle_context_creation_src) {
                g_source_destroy (priv->idle_context_creation_src);
                priv->idle_context_creation_src = NULL;
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
}
