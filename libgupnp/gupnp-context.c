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
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "gupnp-context.h"
#include "gupnp-socket-source.h"
#include "gupnp-error.h"
#include "gupnp-marshal.h"

/* Size of the buffer used for reading from the socket */
#define BUF_SIZE 1024

G_DEFINE_TYPE (GUPnPContext,
               gupnp_context,
               GSSDP_TYPE_CLIENT);

struct _GUPnPContextPrivate {
        GUPnPSocketSource *socket_source;
};

enum {
        MESSAGE_RECEIVED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Function prototypes */
static gboolean
socket_source_cb (gpointer user_data);

static void
gupnp_context_init (GUPnPContext *context)
{
        context->priv = G_TYPE_INSTANCE_GET_PRIVATE
                                        (context,
                                         GUPNP_TYPE_CONTEXT,
                                         GUPnPContextPrivate);

        /* Set up socket (Will set errno if it failed) */
        context->priv->socket_source = gupnp_socket_source_new ();
        if (context->priv->socket_source != NULL) {
                g_source_set_callback ((GSource *) context->priv->socket_source,
                                       socket_source_cb,
                                       context,
                                       NULL);
        }
}

static void
gupnp_context_dispose (GObject *object)
{
        GUPnPContext *context;

        context = GUPNP_CONTEXT (object);

        /* Destroy the SocketSource */
        if (context->priv->socket_source) {
                g_source_destroy ((GSource *) context->priv->socket_source);
                context->priv->socket_source = NULL;
        }
}

static void
gupnp_context_class_init (GUPnPContextClass *klass)
{
        GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gupnp_context_dispose;

        g_type_class_add_private (klass, sizeof (GUPnPContextPrivate));

        signals[MESSAGE_RECEIVED] =
                g_signal_new ("message-received",
                              GUPNP_TYPE_CONTEXT,
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              gupnp_marshal_VOID__STRING_INT_POINTER,
                              G_TYPE_NONE,
                              3,
                              G_TYPE_STRING,
                              G_TYPE_INT,
                              G_TYPE_POINTER);
}

/**
 * gupnp_context_new
 * @main_context: The #GMainContext to associate with
 * @error: A location to return an error of type #GUPnP_ERROR_QUARK
 *
 * Return value: A new #GUPnPContext object.
 **/
GUPnPContext *
gupnp_context_new (GMainContext *main_context,
                   GError      **error)
{
        GUPnPContext *context;
        
        context = g_object_new (GUPNP_TYPE_CONTEXT,
                                "main-context", main_context,
                                NULL);

        if (context->priv->socket_source == NULL) {
                g_set_error (error,
                             GUPNP_ERROR_QUARK,
                             errno,
                             strerror (errno));

                g_object_unref (context);

                return NULL;
        }

        return context;
}

/**
 * _gupnp_context_send_message
 * @context: A #GUPnPContext
 * @dest_ip: The destination IP address, or NULL to broadcast
 * @message: The message to send
 *
 * Sends @message to @dest_ip.
 **/
void
_gupnp_context_send_message (GUPnPContext *context,
                            const char  *dest_ip,
                            const char  *message)
{
        struct sockaddr_in addr;
        int socket_fd, res;

        g_return_if_fail (GUPNP_IS_CONTEXT (context));
        g_return_if_fail (dest_ip != NULL);
        g_return_if_fail (message != NULL);

        socket_fd = gupnp_socket_source_get_fd (context->priv->socket_source);

        memset (&addr, 0, sizeof (addr));

        addr.sin_family      = AF_INET;
        addr.sin_port        = htons (8080); /* XXX */
        addr.sin_addr.s_addr = inet_addr (dest_ip);

        res = sendto (socket_fd,
                      message,
                      strlen (message),
                      0,
                      (struct sockaddr *) &addr,
                      sizeof (addr));

        if (res == -1) {
                g_warning ("sendto: Error %d sending message: %s",
                           errno, strerror (errno));
        }
}

/**
 * Free a GSList of strings
 **/
static void
string_list_free (gpointer ptr)
{
        GSList *list;

        list = ptr;

        while (list) {
                g_free (list->data);
                list = g_slist_delete_link (list, list);
        }
}

/**
 * Called when data can be read from the socket
 **/
static gboolean
socket_source_cb (gpointer user_data)
{
        GUPnPContext *context;
        int fd, type;
        size_t bytes;
        char buf[BUF_SIZE];
        struct sockaddr_in addr;
        socklen_t addr_size;
        GHashTable *hash;
#if 0
        char *req_method;
        guint status_code;
#endif

        context = GUPNP_CONTEXT (user_data);

        /* Get FD */
        fd = gupnp_socket_source_get_fd (context->priv->socket_source);

        /* Read data */
        addr_size = sizeof (addr);
        
        bytes = recvfrom (fd,
                          buf,
                          BUF_SIZE - 1, /* Leave space for trailing \0 */
                          MSG_TRUNC,
                          (struct sockaddr *) &addr,
                          &addr_size);

        if (bytes >= BUF_SIZE) {
                g_warning ("Received packet of %d bytes, but the maximum "
                           "buffer size is %d. Packed dropped.",
                           bytes, BUF_SIZE);

                return TRUE;
        }

        /* Add trailing \0 */
        buf[bytes] = '\0';
        
        /* Parse message */
        type = -1;

        hash = g_hash_table_new_full (g_str_hash,
                                      g_str_equal,
                                      g_free,
                                      string_list_free);

#if 0
        if (soup_headers_parse_request (buf,
                                        bytes,
                                        hash,
                                        &req_method,
                                        NULL,
                                        NULL)) {
                if (g_ascii_strncasecmp (req_method,
                                         SSDP_SEARCH_METHOD,
                                         strlen (SSDP_SEARCH_METHOD)) == 0)
                        type = _GUPnP_DISCOVERY_REQUEST;
                else if (g_ascii_strncasecmp (req_method,
                                              GENA_NOTIFY_METHOD,
                                              strlen (GENA_NOTIFY_METHOD)) == 0)
                        type = _GUPnP_ANNOUNCEMENT;
                else
                        g_warning ("Unhandled method '%s'", req_method);

                g_free (req_method);
        } else if (soup_headers_parse_response (buf,
                                                bytes,
                                                hash,
                                                NULL,
                                                &status_code,
                                                NULL)) {
                if (status_code == 200)
                        type = _GUPnP_DISCOVERY_RESPONSE;
                else
                        g_warning ("Unhandled status code '%d'", status_code);
        } else
                g_warning ("Unhandled message '%s'", buf);

        /* Emit signal if parsing succeeded */
        if (type >= 0) {
                g_signal_emit (context,
                               signals[MESSAGE_RECEIVED],
                               0,
                               inet_ntoa (addr.sin_addr),
                               type,
                               hash);
        }
#endif

        g_hash_table_destroy (hash);

        return TRUE;
}
