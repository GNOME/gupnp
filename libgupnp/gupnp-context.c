/* 
 * Copyright (C) 2006 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
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

#include <config.h>
#include <sys/utsname.h>

#include "gupnp-context.h"
#include "gupnp-context-private.h"

G_DEFINE_TYPE (GUPnPContext,
               gupnp_context,
               GSSDP_TYPE_CLIENT);

struct _GUPnPContextPrivate {
        SoupSession *session;
};

/**
 * Generates the default server ID
 **/
static char *
make_server_id (void)
{
        struct utsname sysinfo;

        uname (&sysinfo);
        
        return g_strdup_printf ("%s/%s UPnP/1.0 GUPnP/%s",
                                sysinfo.sysname,
                                sysinfo.version,
                                VERSION);
}

static void
gupnp_context_init (GUPnPContext *context)
{
        char *server_id;

        context->priv =
                G_TYPE_INSTANCE_GET_PRIVATE (context,
                                             GUPNP_TYPE_CONTEXT,
                                             GUPnPContextPrivate);

        context->priv->session = soup_session_async_new ();

        server_id = make_server_id ();
        gssdp_client_set_server_id (GSSDP_CLIENT (context), server_id);
        g_free (server_id);
}

static void
gupnp_context_dispose (GObject *object)
{
        GUPnPContext *context;

        context = GUPNP_CONTEXT (object);

        if (context->priv->session) {
                g_object_unref (context->priv->session);
                context->priv->session = NULL;
        }
}

static void
gupnp_context_class_init (GUPnPContextClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = gupnp_context_dispose;

        g_type_class_add_private (klass, sizeof (GUPnPContextPrivate));
}

SoupSession *
_gupnp_context_get_session (GUPnPContext *context)
{
        return context->priv->session;
}

/**
 * gupnp_context_new
 * @main_context: A #GMainContext, or NULL to use the default one
 * @error: A location to store a #GError, or NULL
 *
 * Return value: A new #GUPnPContext object.
 **/
GUPnPContext *
gupnp_context_new (GMainContext *main_context,
                   GError      **error)
{
        return g_object_new (GUPNP_TYPE_CONTEXT,
                             "main-context", main_context,
                             NULL);
        /* XXX error */
}
