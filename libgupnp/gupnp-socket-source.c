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
#include <glib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "gupnp-socket-source.h"
//#include "gupnp-protocol.h"

struct _GUPnPSocketSource {
        GSource source;

        GPollFD poll_fd;
};

static gboolean
gupnp_socket_source_prepare  (GSource    *source,
                              int        *timeout);
static gboolean
gupnp_socket_source_check    (GSource    *source);
static gboolean
gupnp_socket_source_dispatch (GSource    *source,
                              GSourceFunc callback,
                              gpointer    user_data);
static void
gupnp_socket_source_finalize (GSource    *source);

GSourceFuncs gupnp_socket_source_funcs = {
        gupnp_socket_source_prepare,
        gupnp_socket_source_check,
        gupnp_socket_source_dispatch,
        gupnp_socket_source_finalize
};

/**
 * gupnp_socket_source_new
 *
 * Return value: A new #GUPnPSocketSource
 **/
GUPnPSocketSource *
gupnp_socket_source_new (void)
{
        GSource *source;
        GUPnPSocketSource *socket_source;
        struct sockaddr_in addr;
        int res;

        /* Create source */
        source = g_source_new (&gupnp_socket_source_funcs,
                               sizeof (GUPnPSocketSource));

        socket_source = (GUPnPSocketSource *) source;

        /* Create socket */
        socket_source->poll_fd.fd = socket (AF_INET,
                                            SOCK_STREAM,
                                            IPPROTO_TCP);
        if (socket_source->poll_fd.fd == -1)
                goto error;
        
        socket_source->poll_fd.events = G_IO_IN | G_IO_ERR;

        g_source_add_poll (source, &socket_source->poll_fd);
        
        /* Bind to SSDP port */
        memset (&addr, 0, sizeof (addr));
                
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl (INADDR_ANY);
        addr.sin_port        = htons (8080); /* XXX */

        res = bind (socket_source->poll_fd.fd,
                    (struct sockaddr *) &addr,
                    sizeof (addr));
        if (res == -1)
                goto error;

        return socket_source;

error:
        g_source_destroy (source);
        
        return NULL;
}

static gboolean
gupnp_socket_source_prepare (GSource *source,
                             int     *timeout)
{
        return FALSE;
}

static gboolean
gupnp_socket_source_check (GSource *source)
{
        GUPnPSocketSource *socket_source;

        socket_source = (GUPnPSocketSource *) source;

        return socket_source->poll_fd.revents & (G_IO_IN | G_IO_ERR);
}

static gboolean
gupnp_socket_source_dispatch (GSource    *source,
                              GSourceFunc callback,
                              gpointer    user_data)
{
        GUPnPSocketSource *socket_source;

        socket_source = (GUPnPSocketSource *) source;

        if (socket_source->poll_fd.revents & G_IO_IN) {
                /* Ready to read */
                if (callback)
                        callback (user_data);
        } else if (socket_source->poll_fd.revents & G_IO_ERR) {
                /* Error */
                int value;
                socklen_t size_int;

                value = EINVAL;
                size_int = sizeof (int);
                
                /* Get errno from socket */
                getsockopt (socket_source->poll_fd.fd,
                            SOL_SOCKET,
                            SO_ERROR,
                            &value,
                            &size_int);

                g_warning ("Socket error %d received: %s",
                           value,
                           strerror (value));
        }

        return TRUE;
}

static void
gupnp_socket_source_finalize (GSource *source)
{
        GUPnPSocketSource *socket_source;

        socket_source = (GUPnPSocketSource *) source;
        
        /* Close the socket */
        close (socket_source->poll_fd.fd);
}

/**
 * gupnp_socket_source_get_fd
 *
 * Return value: The socket's FD.
 **/
int
gupnp_socket_source_get_fd (GUPnPSocketSource *socket_source)
{
        g_return_val_if_fail (socket_source != NULL, -1);
        
        return socket_source->poll_fd.fd;
}
