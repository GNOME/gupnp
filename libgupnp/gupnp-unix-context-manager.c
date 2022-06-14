/*
 * Copyright (C) 2009 Nokia Corporation.
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#define G_LOG_DOMAIN "gupnp-unix-context-manager"

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
#include <glib/gstdio.h>
#include <libgssdp/gssdp-error.h>

#include "gupnp-unix-context-manager.h"
#include "gupnp-context.h"

struct _GUPnPUnixContextManager {
        GUPnPSimpleContextManager parent;
};

G_DEFINE_TYPE (GUPnPUnixContextManager,
               gupnp_unix_context_manager,
               GUPNP_TYPE_SIMPLE_CONTEXT_MANAGER)

/*
 * Create a context for all network interfaces that are up.
 */
static GList *
gupnp_unix_context_manager_get_interfaces (GUPnPSimpleContextManager *manager)
{
        struct ifaddrs *ifa_list, *ifa;
        GList *processed;

        g_return_val_if_fail (GUPNP_IS_UNIX_CONTEXT_MANAGER (manager), NULL);

        if (getifaddrs (&ifa_list) != 0) {
                g_warning ("Failed to retrieve list of network interfaces:%s\n",
                           strerror (errno));

                return FALSE;
        }

        processed = NULL;

        /* Create contexts for each up interface */
        for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next) {
                if (g_list_find_custom (processed,
                                        ifa->ifa_name,
                                        (GCompareFunc) strcmp) != NULL)
                        continue;

                if (ifa->ifa_flags & IFF_POINTOPOINT)
                        continue;

                if (ifa->ifa_flags & IFF_UP)
                        processed = g_list_append (processed,
                                                   g_strdup (ifa->ifa_name));
        }

        freeifaddrs (ifa_list);

        return processed;
}

static void
gupnp_unix_context_manager_init (G_GNUC_UNUSED GUPnPUnixContextManager *manager)
{
}

static void
gupnp_unix_context_manager_class_init (GUPnPUnixContextManagerClass *klass)
{
        GUPnPSimpleContextManagerClass *parent_class;

        parent_class = GUPNP_SIMPLE_CONTEXT_MANAGER_CLASS (klass);
        parent_class->get_interfaces =
                                    gupnp_unix_context_manager_get_interfaces;
}

