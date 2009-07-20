/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 * Copyright (C) 2009 Nokia Corporation, all rights reserved.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *                               <zeeshan.ali@nokia.com>
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
 * SECTION:gupnp-context
 * @short_description: Context object wrapping shared networking bits.
 *
 * #GUPnPContext wraps the networking bits that are used by the various
 * GUPnP classes. It automatically starts a web server on demand.
 *
 * For debugging, it is possible to see the messages being sent and received by
 * exporting #GUPNP_DEBUG.
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
#include <libsoup/soup-address.h>
#include <glib/gstdio.h>

#include "gupnp-context.h"
#include "gupnp-context-private.h"
#include "gupnp-marshal.h"
#include "gena-protocol.h"
#include "http-headers.h"

G_DEFINE_TYPE (GUPnPContext,
               gupnp_context,
               GSSDP_TYPE_CLIENT);

struct _GUPnPContextPrivate {
        guint        port;

        guint        subscription_timeout;

        SoupSession *session;

        SoupServer  *server; /* Started on demand */
        char        *server_url;
};

enum {
        PROP_0,
        PROP_PORT,
        PROP_SERVER,
        PROP_SESSION,
        PROP_SUBSCRIPTION_TIMEOUT
};

typedef struct {
        char *local_path;
        char *server_path;
} HostPathData;

/*
 * Generates the default server ID.
 **/
static char *
make_server_id (void)
{
        struct utsname sysinfo;

        uname (&sysinfo);

        return g_strdup_printf ("%s/%s UPnP/1.0 GUPnP/%s",
                                sysinfo.sysname,
                                sysinfo.release,
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

        server_id = make_server_id ();
        gssdp_client_set_server_id (GSSDP_CLIENT (context), server_id);
        g_free (server_id);
}

static GObject *
gupnp_context_constructor (GType                  type,
                           guint                  n_props,
                           GObjectConstructParam *props)
{
	GObject *object;
	GUPnPContext *context;

	object = G_OBJECT_CLASS (gupnp_context_parent_class)->constructor
                (type, n_props, props);
	context = GUPNP_CONTEXT (object);

        context->priv->session = soup_session_async_new_with_options
                (SOUP_SESSION_IDLE_TIMEOUT,
                 60,
                 SOUP_SESSION_ASYNC_CONTEXT,
                 gssdp_client_get_main_context (GSSDP_CLIENT (context)),
                 NULL);

	if (g_getenv ("GUPNP_DEBUG")) {
		SoupLogger *logger;
		logger = soup_logger_new (SOUP_LOGGER_LOG_BODY, -1);
		soup_logger_attach (logger, context->priv->session);
	}

	return object;
}

static void
gupnp_context_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
        GUPnPContext *context;

        context = GUPNP_CONTEXT (object);

        switch (property_id) {
        case PROP_PORT:
                context->priv->port = g_value_get_uint (value);
                break;
        case PROP_SUBSCRIPTION_TIMEOUT:
                context->priv->subscription_timeout = g_value_get_uint (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_context_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
        GUPnPContext *context;

        context = GUPNP_CONTEXT (object);

        switch (property_id) {
        case PROP_PORT:
                g_value_set_uint (value,
                                  gupnp_context_get_port (context));
                break;
        case PROP_SERVER:
                g_value_set_object (value,
                                    gupnp_context_get_server (context));
                break;
        case PROP_SESSION:
                g_value_set_object (value,
                                    gupnp_context_get_session (context));
                break;
        case PROP_SUBSCRIPTION_TIMEOUT:
                g_value_set_uint (value,
                                  gupnp_context_get_subscription_timeout
                                                                   (context));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_context_dispose (GObject *object)
{
        GUPnPContext *context;
        GObjectClass *object_class;

        context = GUPNP_CONTEXT (object);

        if (context->priv->session) {
                g_object_unref (context->priv->session);
                context->priv->session = NULL;
        }

        if (context->priv->server) {
                g_object_unref (context->priv->server);
                context->priv->server = NULL;
        }

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_context_parent_class);
        object_class->dispose (object);
}

static void
gupnp_context_finalize (GObject *object)
{
        GUPnPContext *context;
        GObjectClass *object_class;

        context = GUPNP_CONTEXT (object);

        g_free (context->priv->server_url);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_context_parent_class);
        object_class->finalize (object);
}

static void
gupnp_context_class_init (GUPnPContextClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->constructor  = gupnp_context_constructor;
        object_class->set_property = gupnp_context_set_property;
        object_class->get_property = gupnp_context_get_property;
        object_class->dispose      = gupnp_context_dispose;
        object_class->finalize     = gupnp_context_finalize;

        g_type_class_add_private (klass, sizeof (GUPnPContextPrivate));

        /**
         * GUPnPContext:port
         *
         * The port to run on. Set to 0 if you don't care what port to run on.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_PORT,
                 g_param_spec_uint ("port",
                                    "Port",
                                    "Port to run on",
                                    0, G_MAXUINT, SOUP_ADDRESS_ANY_PORT,
                                    G_PARAM_READWRITE |
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_STATIC_NAME |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB));

        /**
         * GUPnPContext:server
         *
         * The #SoupServer HTTP server used by GUPnP.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_SERVER,
                 g_param_spec_object ("server",
                                      "SoupServer",
                                      "SoupServer HTTP server",
                                      SOUP_TYPE_SERVER,
                                      G_PARAM_READABLE |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPContext:session
         *
         * The #SoupSession object used by GUPnP.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_SESSION,
                 g_param_spec_object ("session",
                                      "SoupSession",
                                      "SoupSession object",
                                      SOUP_TYPE_SESSION,
                                      G_PARAM_READABLE |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));

        /**
         * GUPnPContext:subscription-timeout
         *
         * The preferred subscription timeout: the number of seconds after
         * which subscriptions are renewed. Set to '0' if subscriptions 
         * are never to time out.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_SUBSCRIPTION_TIMEOUT,
                 g_param_spec_uint ("subscription-timeout",
                                    "Subscription timeout",
                                    "Subscription timeout",
                                    0,
                                    GENA_MAX_TIMEOUT,
                                    GENA_DEFAULT_TIMEOUT,
                                    G_PARAM_READWRITE |
                                    G_PARAM_CONSTRUCT_ONLY |
                                    G_PARAM_STATIC_NAME |
                                    G_PARAM_STATIC_NICK |
                                    G_PARAM_STATIC_BLURB));
}

/**
 * gupnp_context_get_session
 * @context: A #GUPnPContext
 *
 * Get the #SoupSession object that GUPnP is using.
 *
 * Return value: The #SoupSession used by GUPnP. Do not unref this when
 * finished.
 **/
SoupSession *
gupnp_context_get_session (GUPnPContext *context)
{
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);

        return context->priv->session;
}

/*
 * Default server handler: Return 404 not found.
 **/
static void
default_server_handler (SoupServer        *server,
                        SoupMessage       *msg, 
                        const char        *path,
                        GHashTable        *query,
                        SoupClientContext *client,
                        gpointer           user_data)
{
        soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
}

/**
 * gupnp_context_get_server
 * @context: A #GUPnPContext
 *
 * Get the #SoupServer HTTP server that GUPnP is using.
 *
 * Return value: The #SoupServer used by GUPnP. Do not unref this when finished.
 **/
SoupServer *
gupnp_context_get_server (GUPnPContext *context)
{
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);

        if (context->priv->server == NULL) {
                context->priv->server = soup_server_new
                        (SOUP_SERVER_PORT,
                         context->priv->port,
                         SOUP_SERVER_ASYNC_CONTEXT,
                         gssdp_client_get_main_context (GSSDP_CLIENT (context)),
                         NULL);

                soup_server_add_handler (context->priv->server, NULL,
                                         default_server_handler, context,
                                         NULL);

                soup_server_run_async (context->priv->server);
        }

        return context->priv->server;
}

/*
 * Makes an URL that refers to our server.
 **/
static char *
make_server_url (GUPnPContext *context)
{
        SoupServer *server;
        guint port;

        /* What port are we running on? */
        server = gupnp_context_get_server (context);
        port = soup_server_get_port (server);

        /* Put it all together */
        return g_strdup_printf
                        ("http://%s:%u",
                         gssdp_client_get_host_ip (GSSDP_CLIENT (context)),
                         port);
}

const char *
_gupnp_context_get_server_url (GUPnPContext *context)
{
        if (context->priv->server_url == NULL)
                context->priv->server_url = make_server_url (context);

        return (const char *) context->priv->server_url;
}

/**
 * gupnp_context_new
 * @main_context: A #GMainContext, or %NULL to use the default one
 * @interface: The network interface to use, or %NULL to auto-detect.
 * @port: Port to run on, or 0 if you don't care what port is used.
 * @error: A location to store a #GError, or %NULL
 *
 * Create a new #GUPnPContext with the specified @main_context, @interface and
 * @port.
 *
 * Return value: A new #GUPnPContext object, or %NULL on an error
 **/
GUPnPContext *
gupnp_context_new (GMainContext *main_context,
                   const char   *interface,
                   guint         port,
                   GError      **error)
{
        return g_object_new (GUPNP_TYPE_CONTEXT,
                             "main-context", main_context,
                             "interface", interface,
                             "port", port,
                             "error", error,
                             NULL);
}

/**
 * gupnp_context_get_host_ip
 * @context: A #GUPnPContext
 *
 * Get the IP address we advertise ourselves as using.
 *
 * Return value: The IP address. This string should not be freed.
 *
 * Deprecated:0.12.7: The "host-ip" property has moved to the base class
 * #GSSDPClient so newer applications should use
 * #gssdp_client_get_host_ip instead.
 **/
const char *
gupnp_context_get_host_ip (GUPnPContext *context)
{
        return gssdp_client_get_host_ip (GSSDP_CLIENT (context));
}

/**
 * gupnp_context_get_port
 * @context: A #GUPnPContext
 *
 * Get the port that the SOAP server is running on.
 
 * Return value: The port the SOAP server is running on.
 **/
guint
gupnp_context_get_port (GUPnPContext *context)
{
        SoupServer *server;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), 0);

        server = gupnp_context_get_server (context);
        return soup_server_get_port (server);
}

/**
 * gupnp_context_set_subscription_timeout
 * @context: A #GUPnPContext
 * @timeout: Event subscription timeout in seconds
 *
 * Sets the event subscription timeout to @timeout. Use 0 if you don't
 * want subscriptions to time out. Note that any client side subscriptions
 * will automatically be renewed.
 **/
void
gupnp_context_set_subscription_timeout (GUPnPContext *context,
                                        guint         timeout)
{
        g_return_if_fail (GUPNP_IS_CONTEXT (context));

        context->priv->subscription_timeout = timeout;

        g_object_notify (G_OBJECT (context), "subscription-timeout");
}

/**
 * gupnp_context_get_subscription_timeout
 * @context: A #GUPnPContext
 *
 * Get the event subscription timeout (in seconds), or 0 meaning there is no
 * timeout.
 * 
 * Return value: The event subscription timeout in seconds.
 **/
guint
gupnp_context_get_subscription_timeout (GUPnPContext *context)
{
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), 0);

        return context->priv->subscription_timeout;
}

/* Construct a local path from @requested path, removing the last slash
 * if any to make sure we append the locale suffix in a canonical way. */
static char *
construct_local_path (const char   *requested_path,
                      HostPathData *host_path_data)
{
        GString *str;
        int len;

        if (!requested_path || *requested_path == 0)
                return g_strdup (host_path_data->local_path);

        if (*requested_path != '/')
                return NULL; /* Absolute paths only */

        str = g_string_new (host_path_data->local_path);

        /* Skip the length of the path relative to which @requested_path
         * is specified. */
        requested_path += strlen (host_path_data->server_path);

        /* Strip the last slashes to make sure we append the locale suffix
         * in a canonical way. */
        len = strlen (requested_path);
        while (requested_path[len - 1] == '/')
                len--;

        g_string_append_len (str,
                             requested_path,
                             len);

        return g_string_free (str, FALSE);
}

/* Append locale suffix to @local_path. */
static char *
append_locale (const char *local_path, GList *locales)
{
        if (!locales)
                return g_strdup (local_path);

        return g_strdup_printf ("%s.%s",
                                local_path,
                                (char *) locales->data);
}

/* Redirect @msg to the same URI, but with a slash appended. */
static void
redirect_to_folder (SoupMessage *msg)
{
        char *uri, *redir_uri;

        uri = soup_uri_to_string (soup_message_get_uri (msg),
                                  FALSE);
        redir_uri = g_strdup_printf ("%s/", uri);
        soup_message_headers_append (msg->response_headers,
                                     "Location", redir_uri);
        soup_message_set_status (msg,
                                 SOUP_STATUS_MOVED_PERMANENTLY);
        g_free (redir_uri);
        g_free (uri);
}

/* Serve @path. Note that we do not need to check for path including bogus
 * '..' as libsoup does this for us. */
static void
host_path_handler (SoupServer        *server,
                   SoupMessage       *msg, 
                   const char        *path,
                   GHashTable        *query,
                   SoupClientContext *client,
                   gpointer           user_data)
{
        char *local_path, *path_to_open;
        struct stat st;
        int status;
        GList *locales, *orig_locales;
        GMappedFile *mapped_file;
        GError *error;

        orig_locales = NULL;
        locales      = NULL;
        local_path   = NULL;
        path_to_open = NULL;

        if (msg->method != SOUP_METHOD_GET &&
            msg->method != SOUP_METHOD_HEAD) {
                soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);

                goto DONE;
        }

        /* Construct base local path */
        local_path = construct_local_path (path, (HostPathData *) user_data);
        if (!local_path) {
                soup_message_set_status (msg, SOUP_STATUS_BAD_REQUEST);

                goto DONE;
        }

        /* Get preferred locales */
        orig_locales = locales = http_request_get_accept_locales (msg);

 AGAIN:
        /* Add locale suffix if available */
        path_to_open = append_locale (local_path, locales);

        /* See what we've got */
        if (g_stat (path_to_open, &st) == -1) {
                if (errno == EPERM)
                        soup_message_set_status (msg, SOUP_STATUS_FORBIDDEN);
                else if (errno == ENOENT) {
                        if (locales) {
                                g_free (path_to_open);

                                locales = locales->next;

                                goto AGAIN;
                        } else
                                soup_message_set_status (msg,
                                                         SOUP_STATUS_NOT_FOUND);
                } else
                        soup_message_set_status
                                (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);

                goto DONE;
        }

        /* Handle directories */
        if (S_ISDIR (st.st_mode)) {
                if (!g_str_has_suffix (path, "/")) {
                        redirect_to_folder (msg);

                        goto DONE;
                }

                /* This incorporates the locale portion in the folder name
                 * intentionally. */
                g_free (local_path);
                local_path = g_build_filename (path_to_open,
                                               "index.html",
                                               NULL);

                g_free (path_to_open);

                goto AGAIN;
        }

        /* Map file */
        error = NULL;
        mapped_file = g_mapped_file_new (path_to_open, FALSE, &error);

        if (mapped_file == NULL) {
                g_warning ("Unable to map file %s: %s",
                           path_to_open, error->message);

                g_error_free (error);

                soup_message_set_status (msg,
                                         SOUP_STATUS_INTERNAL_SERVER_ERROR);

                goto DONE;
        }

        /* Handle method (GET or HEAD) */
        status = SOUP_STATUS_OK;

        if (msg->method == SOUP_METHOD_GET) {
                gsize offset, length;
                gboolean have_range;
                SoupBuffer *buffer;

                /* Find out range */
                have_range = FALSE;

                offset = 0;
                length = st.st_size;

                if (!http_request_get_range (msg,
                                             &have_range,
                                             &offset,
                                             &length)) {
                        soup_message_set_status
                                (msg,
                                 SOUP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE);

                        goto DONE;
                }

                if (have_range && (length < 0                   ||
                                   offset < 0                   ||
                                   length > st.st_size - offset ||
                                   offset >= st.st_size)) {
                        soup_message_set_status
                                (msg,
                                 SOUP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE);

                        goto DONE;
                }

                /* Add requested content */
                buffer = soup_buffer_new_with_owner
                             (g_mapped_file_get_contents (mapped_file) + offset,
                              length,
                              mapped_file,
                              (GDestroyNotify) g_mapped_file_free);

                soup_message_body_append_buffer (msg->response_body, buffer);

                soup_buffer_free (buffer);

                /* Set status */
                if (have_range) {
                        http_response_set_content_range (msg,
                                                         offset,
                                                         offset + length,
                                                         st.st_size);

                        status = SOUP_STATUS_PARTIAL_CONTENT;
                }

        } else if (msg->method == SOUP_METHOD_HEAD) {
                char *length;

                length = g_strdup_printf ("%lu", (gulong) st.st_size);
	        soup_message_headers_append (msg->response_headers,
                                             "Content-Length",
                                             length);
                g_free (length);

        } else {
                soup_message_set_status (msg,
                                         SOUP_STATUS_METHOD_NOT_ALLOWED);

                goto DONE;
        }

        /* Set Content-Type */
        http_response_set_content_type (msg,
                                        path_to_open, 
                                        (guchar *) g_mapped_file_get_contents
                                                                (mapped_file),
                                        st.st_size);

        /* Set Content-Language */
        if (locales)
               http_response_set_content_locale (msg, locales->data);

        /* Set Accept-Ranges */
        soup_message_headers_append (msg->response_headers,
                                     "Accept-Ranges",
                                     "bytes");

        /* Set status */
        soup_message_set_status (msg, status);

 DONE:
        /* Cleanup */
        g_free (path_to_open);
        g_free (local_path);

        while (orig_locales) {
                g_free (orig_locales->data);
                orig_locales = g_list_delete_link (orig_locales, orig_locales);
        }
}

HostPathData *
host_path_data_new (const char *local_path,
                    const char *server_path)
{
        HostPathData *path_data;

        path_data = g_slice_new (HostPathData);

        path_data->local_path  = g_strdup (local_path);
        path_data->server_path = g_strdup (server_path);

        return path_data;
}

void
host_path_data_free (HostPathData *path_data)
{
        g_free (path_data->local_path);
        g_free (path_data->server_path);

        g_slice_free (HostPathData, path_data);
}

/**
 * gupnp_context_host_path
 * @context: A #GUPnPContext
 * @local_path: Path to the local file or folder to be hosted
 * @server_path: Web server path where @local_path should be hosted
 *
 * Start hosting @local_path at @server_path. Files with the path
 * @local_path.LOCALE (if they exist) will be served up when LOCALE is
 * specified in the request's Accept-Language header.
 **/
void
gupnp_context_host_path (GUPnPContext *context,
                         const char   *local_path,
                         const char   *server_path)
{
        SoupServer *server;
        HostPathData *path_data;

        g_return_if_fail (GUPNP_IS_CONTEXT (context));
        g_return_if_fail (local_path != NULL);
        g_return_if_fail (server_path != NULL);

        server = gupnp_context_get_server (context);

        path_data = host_path_data_new (local_path, server_path);

        soup_server_add_handler (server,
                                 server_path,
                                 host_path_handler,
                                 path_data,
                                 (GDestroyNotify) host_path_data_free);
}

/**
 * gupnp_context_unhost_path
 * @context: A #GUPnPContext
 * @server_path: Web server path where the file or folder is hosted
 *
 * Stop hosting the file or folder at @server_path.
 **/
void
gupnp_context_unhost_path (GUPnPContext *context,
                           const char   *server_path)
{
        SoupServer *server;

        g_return_if_fail (GUPNP_IS_CONTEXT (context));
        g_return_if_fail (server_path != NULL);

        server = gupnp_context_get_server (context);

        soup_server_remove_handler (server, server_path);
}
