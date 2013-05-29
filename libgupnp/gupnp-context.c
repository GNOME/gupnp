/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 * Copyright (C) 2009 Nokia Corporation.
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:gupnp-context
 * @short_description: Context object wrapping shared networking bits.
 *
 * #GUPnPContext wraps the networking bits that are used by the various
 * GUPnP classes. It automatically starts a web server on demand.
 *
 * For debugging, it is possible to see the messages being sent and received by
 * exporting %GUPNP_DEBUG.
 */

#include <config.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#ifndef G_OS_WIN32
#include <sys/utsname.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <libsoup/soup-address.h>
#include <glib/gstdio.h>

#include "gupnp-context.h"
#include "gupnp-context-private.h"
#include "gupnp-error.h"
#include "gupnp-marshal.h"
#include "gena-protocol.h"
#include "http-headers.h"

#define GUPNP_CONTEXT_DEFAULT_LANGUAGE "en"

static void
gupnp_context_initable_iface_init (gpointer g_iface,
                                   gpointer iface_data);


G_DEFINE_TYPE_EXTENDED (GUPnPContext,
                        gupnp_context,
                        GSSDP_TYPE_CLIENT,
                        0,
                        G_IMPLEMENT_INTERFACE
                                (G_TYPE_INITABLE,
                                 gupnp_context_initable_iface_init));

struct _GUPnPContextPrivate {
        guint        port;

        guint        subscription_timeout;

        SoupSession *session;

        SoupServer  *server; /* Started on demand */
        char        *server_url;
        char        *default_language;

        GList       *host_path_datas;
};

enum {
        PROP_0,
        PROP_PORT,
        PROP_SERVER,
        PROP_SESSION,
        PROP_SUBSCRIPTION_TIMEOUT,
        PROP_DEFAULT_LANGUAGE
};

typedef struct {
        char *local_path;

        GRegex *regex;
} UserAgent;

typedef struct {
        char *local_path;
        char *server_path;
        char *default_language;

        GList *user_agents;
} HostPathData;

static GInitableIface* initable_parent_iface = NULL;

/*
 * Generates the default server ID.
 **/
static char *
make_server_id (void)
{
#ifdef G_OS_WIN32
        OSVERSIONINFO versioninfo;
        versioninfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
        if (GetVersionEx (&versioninfo)) {
                return g_strdup_printf ("Microsoft Windows/%ld.%ld"
                                        " UPnP/1.0 GUPnP/%s",
                                        versioninfo.dwMajorVersion,
                                        versioninfo.dwMinorVersion,
                                        VERSION);
        } else {
                return g_strdup_printf ("Microsoft Windows UPnP/1.0 GUPnP/%s",
                                        VERSION);
        }
#else
        struct utsname sysinfo;

        uname (&sysinfo);

        return g_strdup_printf ("%s/%s UPnP/1.0 GUPnP/%s",
                                sysinfo.sysname,
                                sysinfo.release,
                                VERSION);
#endif
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

static gboolean
gupnp_context_initable_init (GInitable     *initable,
                             GCancellable  *cancellable,
                             GError       **error)
{
        char *user_agent;
        GError *inner_error = NULL;
        GUPnPContext *context;

        if (!initable_parent_iface->init(initable,
                                         cancellable,
                                         &inner_error)) {
                g_propagate_error (error, inner_error);

                return FALSE;
        }

        context = GUPNP_CONTEXT (initable);

        context->priv->session = soup_session_async_new_with_options
                (SOUP_SESSION_IDLE_TIMEOUT,
                 60,
                 SOUP_SESSION_ASYNC_CONTEXT,
                 g_main_context_get_thread_default (),
                 NULL);

        user_agent = g_strdup_printf ("%s GUPnP/" VERSION " DLNADOC/1.50",
                                      g_get_application_name ()? : "");
        g_object_set (context->priv->session,
                      SOUP_SESSION_USER_AGENT,
                      user_agent,
                      NULL);
        g_free (user_agent);

        if (g_getenv ("GUPNP_DEBUG")) {
                SoupLogger *logger;
                logger = soup_logger_new (SOUP_LOGGER_LOG_BODY, -1);
                soup_session_add_feature (context->priv->session,
                                          SOUP_SESSION_FEATURE (logger));
        }

        soup_session_add_feature_by_type (context->priv->session,
                                          SOUP_TYPE_CONTENT_DECODER);

        /* Create the server already if the port is not null*/
        if (context->priv->port != 0) {
                gupnp_context_get_server (context);

                if (context->priv->server == NULL) {
                        g_object_unref (context->priv->session);
                        context->priv->session = NULL;

                        g_set_error (error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_OTHER,
                                     "Could not create HTTP server on port %d",
                                     context->priv->port);

                        return FALSE;
                }
        }

        return TRUE;
}

static void
gupnp_context_initable_iface_init (gpointer               g_iface,
                                   G_GNUC_UNUSED gpointer iface_data)
{
        GInitableIface *iface = (GInitableIface *)g_iface;
        initable_parent_iface = g_type_interface_peek_parent (iface);
        iface->init = gupnp_context_initable_init;
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
        case PROP_DEFAULT_LANGUAGE:
                gupnp_context_set_default_language (context,
                                                    g_value_get_string (value));
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
        case PROP_DEFAULT_LANGUAGE:
                g_value_set_string (value,
                                    gupnp_context_get_default_language
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

        while (context->priv->host_path_datas) {
                HostPathData *data;

                data = (HostPathData *) context->priv->host_path_datas->data;

                gupnp_context_unhost_path (context, data->server_path);
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

        if (context->priv->default_language) {
                g_free (context->priv->default_language);
                context->priv->default_language = NULL;
        }

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

        object_class->set_property = gupnp_context_set_property;
        object_class->get_property = gupnp_context_get_property;
        object_class->dispose      = gupnp_context_dispose;
        object_class->finalize     = gupnp_context_finalize;

        g_type_class_add_private (klass, sizeof (GUPnPContextPrivate));

        /**
         * GUPnPContext:port:
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
         * GUPnPContext:server:
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
         * GUPnPContext:session:
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
         * GUPnPContext:subscription-timeout:
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
        /**
         * GUPnPContext:default-language:
         *
         * The content of the Content-Language header id the client
         * sends Accept-Language and no language-specific pages to serve
         * exist. The property defaults to 'en'.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_DEFAULT_LANGUAGE,
                 g_param_spec_string ("default-language",
                                      "Default language",
                                      "Default language",
                                      GUPNP_CONTEXT_DEFAULT_LANGUAGE,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT |
                                      G_PARAM_STATIC_NAME |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB));
}

/**
 * gupnp_context_get_session:
 * @context: A #GUPnPContext
 *
 * Get the #SoupSession object that GUPnP is using.
 *
 * Return value: (transfer none): The #SoupSession used by GUPnP. Do not unref
 * this when finished.
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
default_server_handler (G_GNUC_UNUSED SoupServer        *server,
                        SoupMessage                     *msg,
                        G_GNUC_UNUSED const char        *path,
                        G_GNUC_UNUSED GHashTable        *query,
                        G_GNUC_UNUSED SoupClientContext *client,
                        G_GNUC_UNUSED gpointer           user_data)
{
        soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
}

/**
 * gupnp_context_get_server:
 * @context: A #GUPnPContext
 *
 * Get the #SoupServer HTTP server that GUPnP is using.
 *
 * Returns: (transfer none): The #SoupServer used by GUPnP. Do not unref this when finished.
 **/
SoupServer *
gupnp_context_get_server (GUPnPContext *context)
{
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);

        if (context->priv->server == NULL) {
                const char *ip;

                ip =  gssdp_client_get_host_ip (GSSDP_CLIENT (context));
                SoupAddress *addr = soup_address_new (ip, context->priv->port);
                soup_address_resolve_sync (addr, NULL);

                context->priv->server = soup_server_new
                        (SOUP_SERVER_PORT,
                         context->priv->port,
                         SOUP_SERVER_ASYNC_CONTEXT,
                         g_main_context_get_thread_default (),
                         SOUP_SERVER_INTERFACE,
                         addr,
                         NULL);
                g_object_unref (addr);

                if (context->priv->server) {
                        soup_server_add_handler (context->priv->server,
                                                 NULL,
                                                 default_server_handler,
                                                 context,
                                                 NULL);

                        soup_server_run_async (context->priv->server);
                }
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
 * gupnp_context_new:
 * @main_context: (allow-none): Deprecated: 0.17.2: Always set to %NULL. If you
 * want to use a different context, use g_main_context_push_thread_default().
 * @iface: (allow-none): The network interface to use, or %NULL to
 * auto-detect.
 * @port: Port to run on, or 0 if you don't care what port is used.
 * @error: A location to store a #GError, or %NULL
 *
 * Create a new #GUPnPContext with the specified @main_context, @iface and
 * @port.
 *
 * Return value: A new #GUPnPContext object, or %NULL on an error
 **/
GUPnPContext *
gupnp_context_new (GMainContext *main_context,
                   const char   *iface,
                   guint         port,
                   GError      **error)
{
        if (main_context)
                g_warning ("gupnp_context_new::main_context is deprecated."
                           " Use g_main_context_push_thread_default()"
                           " instead");

        return g_initable_new (GUPNP_TYPE_CONTEXT,
                               NULL,
                               error,
                               "interface", iface,
                               "port", port,
                               NULL);
}

/**
 * gupnp_context_get_host_ip:
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
 * gupnp_context_get_port:
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
 * gupnp_context_set_subscription_timeout:
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
 * gupnp_context_get_subscription_timeout:
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

static void
host_path_data_set_language (HostPathData *data, const char *language)
{
        char *old_language = data->default_language;

        if ((old_language != NULL) && (!strcmp (language, old_language)))
                return;

        data->default_language = g_strdup (language);


        if (old_language != NULL)
                g_free (old_language);
}

/**
 * gupnp_context_set_default_language:
 * @context: A #GUPnPContext
 * @language A language tag as defined in RFC 2616 3.10
 *
 * Set the default language for the Content-Length header to @language.
 *
 * If the client sends an Accept-Language header the UPnP HTTP server
 * is required to send a Content-Language header in return. If there are
 * no files hosted in languages which match the requested ones the
 * Content-Language header is set to this value. The default value is "en".
 */
void
gupnp_context_set_default_language (GUPnPContext *context,
                                    const char   *language)
{
        g_return_if_fail (GUPNP_IS_CONTEXT (context));
        g_return_if_fail (language != NULL);


        char *old_language = context->priv->default_language;

        if ((old_language != NULL) && (!strcmp (language, old_language)))
                return;

        context->priv->default_language = g_strdup (language);

        g_list_foreach (context->priv->host_path_datas,
                        (GFunc) host_path_data_set_language,
                        (gpointer) language);

        if (old_language != NULL)
                g_free (old_language);
}

/**
 * gupnp_context_get_default_language:
 * @context: A #GUPnPContext
 *
 * Get the default Content-Language header for this context.
 *
 * Returns: (transfer none): The default content of the Content-Language
 * header.
 */
const char *
gupnp_context_get_default_language (GUPnPContext *context)
{
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);

        return context->priv->default_language;
}

/* Construct a local path from @requested path, removing the last slash
 * if any to make sure we append the locale suffix in a canonical way. */
static char *
construct_local_path (const char   *requested_path,
                      const char   *user_agent,
                      HostPathData *host_path_data)
{
        GString *str;
        char *local_path;
        int len;

        local_path = NULL;

        if (user_agent != NULL) {
                GList *node;

                for (node = host_path_data->user_agents;
                     node;
                     node = node->next) {
                        UserAgent *agent;

                        agent = node->data;

                        if (g_regex_match (agent->regex,
                                           user_agent,
                                           0,
                                           NULL)) {
                                local_path = agent->local_path;
                        }
                }
        }

        if (local_path == NULL)
                local_path = host_path_data->local_path;

        if (!requested_path || *requested_path == 0)
                return g_strdup (local_path);

        if (*requested_path != '/')
                return NULL; /* Absolute paths only */

        str = g_string_new (local_path);

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
host_path_handler (G_GNUC_UNUSED SoupServer        *server,
                   SoupMessage                     *msg,
                   const char                      *path,
                   G_GNUC_UNUSED GHashTable        *query,
                   G_GNUC_UNUSED SoupClientContext *client,
                   gpointer                         user_data)
{
        char *local_path, *path_to_open;
        struct stat st;
        int status;
        GList *locales, *orig_locales;
        GMappedFile *mapped_file;
        GError *error;
        HostPathData *host_path_data;
        const char *user_agent;

        orig_locales = NULL;
        locales      = NULL;
        local_path   = NULL;
        path_to_open = NULL;
        host_path_data = (HostPathData *) user_data;

        if (msg->method != SOUP_METHOD_GET &&
            msg->method != SOUP_METHOD_HEAD) {
                soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);

                goto DONE;
        }

        user_agent = soup_message_headers_get_one (msg->request_headers,
                                                   "User-Agent");

        /* Construct base local path */
        local_path = construct_local_path (path, user_agent, host_path_data);
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
                gboolean have_range;
                SoupBuffer *buffer;
                SoupRange *ranges;
                int nranges;

                /* Find out range */
                have_range = FALSE;

                if (soup_message_headers_get_ranges (msg->request_headers,
                                                     st.st_size,
                                                     &ranges,
                                                     &nranges))
                        have_range = TRUE;

                /* We do not support mulipart/byteranges so only first first */
                /* range from request is handled */
                if (have_range && (ranges[0].end > st.st_size ||
                                   st.st_size < 0 ||
                                   ranges[0].start >= st.st_size ||
                                   ranges[0].start > ranges[0].end)) {
                        soup_message_set_status
                                (msg,
                                 SOUP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE);
                        soup_message_headers_free_ranges (msg->request_headers,
                                                          ranges);

                        goto DONE;
                }

                /* Add requested content */
                buffer = soup_buffer_new_with_owner
                             (g_mapped_file_get_contents (mapped_file),
                              g_mapped_file_get_length (mapped_file),
                              mapped_file,
                              (GDestroyNotify) g_mapped_file_unref);

                /* Set range and status */
                if (have_range) {
                        SoupBuffer *range_buffer;

                        soup_message_body_truncate (msg->response_body);
                        soup_message_headers_set_content_range (
                                                          msg->response_headers,
                                                          ranges[0].start,
                                                          ranges[0].end,
                                                          buffer->length);
                        range_buffer = soup_buffer_new_subbuffer (
                                           buffer,
                                           ranges[0].start,
                                           ranges[0].end - ranges[0].start + 1);
                        soup_message_body_append_buffer (msg->response_body,
                                                         range_buffer);
                        status = SOUP_STATUS_PARTIAL_CONTENT;

                        soup_message_headers_free_ranges (msg->request_headers,
                                                          ranges);
                        soup_buffer_free (range_buffer);
                } else
                        soup_message_body_append_buffer (msg->response_body, buffer);

                soup_buffer_free (buffer);
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
        else if (soup_message_headers_get_one (msg->request_headers,
                                               "Accept-Language")) {
                soup_message_headers_append (msg->response_headers,
                                             "Content-Language",
                                             host_path_data->default_language);
        }

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

static UserAgent *
user_agent_new (const char *local_path,
                GRegex     *regex)
{
        UserAgent *agent;

        agent = g_slice_new0 (UserAgent);

        agent->local_path = g_strdup (local_path);
        agent->regex = g_regex_ref (regex);

        return agent;
}

static void
user_agent_free (UserAgent *agent)
{
        g_free (agent->local_path);
        g_regex_unref (agent->regex);

        g_slice_free (UserAgent, agent);
}

static HostPathData *
host_path_data_new (const char *local_path,
                    const char *server_path,
                    const char *default_language)
{
        HostPathData *path_data;

        path_data = g_slice_new0 (HostPathData);

        path_data->local_path  = g_strdup (local_path);
        path_data->server_path = g_strdup (server_path);
        path_data->default_language = g_strdup (default_language);

        return path_data;
}

static void
host_path_data_free (HostPathData *path_data)
{
        g_free (path_data->local_path);
        g_free (path_data->server_path);
        g_free (path_data->default_language);

        while (path_data->user_agents) {
                UserAgent *agent;

                agent = path_data->user_agents->data;

                user_agent_free (agent);

                path_data->user_agents = g_list_delete_link (
                                path_data->user_agents,
                                path_data->user_agents);
        }

        g_slice_free (HostPathData, path_data);
}

/**
 * gupnp_context_host_path:
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

        path_data = host_path_data_new (local_path,
                                        server_path,
                                        context->priv->default_language);

        soup_server_add_handler (server,
                                 server_path,
                                 host_path_handler,
                                 path_data,
                                 NULL);

        context->priv->host_path_datas =
                g_list_append (context->priv->host_path_datas,
                               path_data);
}

static unsigned int
path_compare_func (HostPathData *path_data,
                   const char   *server_path)
{
        /* ignore default language */
        return strcmp (path_data->server_path, server_path);
}

/**
 * gupnp_context_host_path_for_agent:
 * @context: A #GUPnPContext
 * @local_path: Path to the local file or folder to be hosted
 * @server_path: Web server path already being hosted
 * @user_agent: The user-agent as a #GRegex.
 *
 * Use this method to serve different local path to specific user-agent(s). The
 * path @server_path must already be hosted by @context.
 *
 * Return value: %TRUE on success, %FALSE otherwise.
 **/
gboolean
gupnp_context_host_path_for_agent (GUPnPContext *context,
                                   const char   *local_path,
                                   const char   *server_path,
                                   GRegex       *user_agent)
{
        GList *node;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), FALSE);
        g_return_val_if_fail (local_path != NULL, FALSE);
        g_return_val_if_fail (server_path != NULL, FALSE);
        g_return_val_if_fail (user_agent != NULL, FALSE);

        node = g_list_find_custom (context->priv->host_path_datas,
                                   server_path,
                                   (GCompareFunc) path_compare_func);
        if (node != NULL) {
                HostPathData *path_data;
                UserAgent *agent;

                path_data = (HostPathData *) node->data;
                agent = user_agent_new (local_path, user_agent);

                path_data->user_agents = g_list_append (path_data->user_agents,
                                                        agent);

                return TRUE;
        } else
                return FALSE;
}

/**
 * gupnp_context_unhost_path:
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
        HostPathData *path_data;
        GList *node;

        g_return_if_fail (GUPNP_IS_CONTEXT (context));
        g_return_if_fail (server_path != NULL);

        server = gupnp_context_get_server (context);

        node = g_list_find_custom (context->priv->host_path_datas,
                                   server_path,
                                   (GCompareFunc) path_compare_func);
        g_return_if_fail (node != NULL);

        path_data = (HostPathData *) node->data;
        context->priv->host_path_datas = g_list_delete_link (
                        context->priv->host_path_datas,
                        node);

        soup_server_remove_handler (server, server_path);
        host_path_data_free (path_data);
}
