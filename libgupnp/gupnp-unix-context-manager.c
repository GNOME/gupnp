/*
 * Copyright (C) 2009 Nokia Corporation, all rights reserved.
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
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

/**
 * SECTION:gupnp-unix-context-manager
 * @short_description: Unix-specific implementation of #GUPnPContextManager.
 *
 */

#include <config.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <libsoup/soup-address.h>
#include <glib/gstdio.h>

#include "gupnp-unix-context-manager.h"
#include "gupnp-context.h"

G_DEFINE_TYPE (GUPnPUnixContextManager,
               gupnp_unix_context_manager,
               GUPNP_TYPE_CONTEXT_MANAGER);

/* FIXME: We currently don't deal with IPv6 because libsoup doesn't seem to
 * like it and we get the screen full of warnings and errors. Please fix that
 * and uncomment the IPv6 handling here.
 */
static char *
get_host_ip_from_iface (struct ifaddrs *ifa)
{
        char ip[INET6_ADDRSTRLEN];
        const char *host_ip = NULL;
        struct sockaddr_in *s4;
        /*struct sockaddr_in6 *s6;*/

        if (ifa->ifa_addr == NULL)
                return NULL;

        if (!(ifa->ifa_flags & IFF_UP))
                return NULL;

        switch (ifa->ifa_addr->sa_family) {
                case AF_INET:
                        s4 = (struct sockaddr_in *) ifa->ifa_addr;
                        host_ip = inet_ntop (AF_INET,
                                        &s4->sin_addr, ip, sizeof (ip));
                        break;
                /*case AF_INET6:
                        s6 = (struct sockaddr_in6 *) ifa->ifa_addr;
                        host_ip = inet_ntop (AF_INET6,
                                        &s6->sin6_addr, ip, sizeof (ip));
                        break;*/
                default:
                        break; /* Unknown: ignore */
        }

        return g_strdup (host_ip);
}

void
create_and_signal_context (GUPnPUnixContextManager *manager,
                           const char              *host_ip)
{
        GUPnPContext *context;
        GMainContext *main_context;
        guint port;

        GError *error;

        if (host_ip == NULL)
                return;

        g_object_get (manager,
                      "main-context", &main_context,
                      "port", &port,
                      NULL);

        error = NULL;
        context = g_object_new (GUPNP_TYPE_CONTEXT,
                                "main-context", main_context,
                                "host-ip", host_ip,
                                "port", port,
                                "error", &error,
                                NULL);
        if (error != NULL) {
                g_warning ("Failed to create context for host/IP '%s': %s\n",
                           host_ip,
                           error->message);

                g_error_free (error);

                return;
        }

        g_signal_emit_by_name (manager,
                               "context-available",
                               context);

        g_object_unref (context);
}

/*
 * Create a context for all network interfaces that are up.
 */
static gboolean
create_contexts (gpointer data)
{
        GUPnPUnixContextManager *manager = (GUPnPUnixContextManager *) data;
        struct ifaddrs *ifa_list, *ifa;
        char *ret;

        ret = NULL;

        if (getifaddrs (&ifa_list) != 0) {
                g_warning ("Failed to retrieve list of network interfaces:%s\n",
                           strerror (errno));

                return FALSE;
        }

        for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next) {
                char *host_ip;

                host_ip = get_host_ip_from_iface (ifa);

                if (host_ip != NULL) {
                        create_and_signal_context (manager, host_ip);

                        g_free (host_ip);
                }
        }

        freeifaddrs (ifa_list);

        return FALSE;
}

static void
schedule_contexts_creation (GUPnPUnixContextManager *manager)
{
        GMainContext *main_context;
        GSource *source;

        g_object_get (manager,
                      "main-context", &main_context,
                      NULL);

        /* Create contexts in mainloop so that is happens after user has hooked
         * to the "context-available" signal.
         */
        source = g_idle_source_new ();
        g_source_attach (source, main_context);
        g_source_set_callback (source,
                               create_contexts,
                               manager,
                               NULL);
}

static void
gupnp_unix_context_manager_init (GUPnPUnixContextManager *manager)
{
}

static void
gupnp_unix_context_manager_constructed (GObject *object)
{
        GObjectClass *parent_class;
        GUPnPUnixContextManager *manager;

        manager = GUPNP_UNIX_CONTEXT_MANAGER (object);

        schedule_contexts_creation (manager);

        /* Chain-up */
        parent_class = G_OBJECT_CLASS (gupnp_unix_context_manager_parent_class);
        if (parent_class->constructed != NULL) {
                parent_class->constructed (object);
        }
}

static void
gupnp_unix_context_manager_dispose (GObject *object)
{
        GUPnPUnixContextManager *manager;
        GObjectClass *object_class;

        manager = GUPNP_UNIX_CONTEXT_MANAGER (object);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_unix_context_manager_parent_class);
        object_class->dispose (object);
}

static void
gupnp_unix_context_manager_class_init (GUPnPUnixContextManagerClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->constructed  = gupnp_unix_context_manager_constructed;
        object_class->dispose      = gupnp_unix_context_manager_dispose;
}

