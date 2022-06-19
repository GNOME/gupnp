/*
 * Copyright (C) 2009, 2010 Jens Georg
 * Copyright (C) 2009 Nokia Corporation.
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
 *         Jens Georg <mail@jensge.org>
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
 * SECTION:gupnp-windows-context-manager
 * @short_description: Windows-specific implementation of #GUPnPContextManager.
 */

#include <config.h>
#include <string.h>
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0502
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include "gupnp-windows-context-manager.h"
#include "gupnp-context.h"

G_DEFINE_TYPE (GUPnPWindowsContextManager,
               gupnp_windows_context_manager,
               GUPNP_TYPE_SIMPLE_CONTEXT_MANAGER);

/*
 * Create a context for all network interfaces that are up.
 */
static GList *
gupnp_windows_context_manager_get_interfaces
                                        (GUPnPSimpleContextManager *manager)
{
        GList *interfaces = NULL;
        ULONG flags = GAA_FLAG_INCLUDE_PREFIX |
                      GAA_FLAG_SKIP_DNS_SERVER |
                      GAA_FLAG_SKIP_MULTICAST;
        /* use 15k buffer initially as documented in MSDN */
        DWORD size = 0x3C00;
        DWORD ret;
        PIP_ADAPTER_ADDRESSES adapters_addresses;
        PIP_ADAPTER_ADDRESSES adapter;

        ULONG family = gupnp_context_manager_get_socket_family (
                GUPNP_CONTEXT_MANAGER (manager));

        do {
                adapters_addresses = (PIP_ADAPTER_ADDRESSES) g_malloc0 (size);
                ret = GetAdaptersAddresses (family,
                                            flags,
                                            NULL,
                                            adapters_addresses,
                                            &size);
                if (ret == ERROR_BUFFER_OVERFLOW) {
                        g_free (adapters_addresses);
                }
        } while (ret == ERROR_BUFFER_OVERFLOW);

        if (ret != ERROR_SUCCESS)
                return NULL;

        for (adapter = adapters_addresses;
             adapter != NULL;
             adapter = adapter->Next) {
                if (adapter->FirstUnicastAddress == NULL)
                        continue;
                if (adapter->OperStatus != IfOperStatusUp)
                        continue;
                interfaces = g_list_append (interfaces,
                                            g_strdup (adapter->AdapterName));
        }

        return interfaces;
}

static void
gupnp_windows_context_manager_init (GUPnPWindowsContextManager *manager)
{
}

static void
gupnp_windows_context_manager_class_init (GUPnPWindowsContextManagerClass *klass)
{
        GUPnPSimpleContextManagerClass *parent_class;

        parent_class = GUPNP_SIMPLE_CONTEXT_MANAGER_CLASS (klass);
        parent_class->get_interfaces =
                                gupnp_windows_context_manager_get_interfaces;
}
