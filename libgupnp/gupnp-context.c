/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *                               <zeeshan.ali@nokia.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

/**
 * GUPnPContext:
 *
 * Context object wrapping shared networking bits.
 *
 * #GUPnPContext wraps the networking bits that are used by the various
 * GUPnP classes. It automatically starts a web server on demand.
 *
 * For debugging, it is possible to see the messages being sent and received by
 * setting the environment variable `GUPNP_DEBUG`.
 */

#define G_LOG_DOMAIN "gupnp-context"

#include <config.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#ifdef G_OS_WIN32
#include <windows.h>
#else
#include <sys/utsname.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <glib/gstdio.h>

#include "gupnp-acl.h"
#include "gupnp-acl-private.h"
#include "gupnp-context.h"
#include "gupnp-context-private.h"
#include "gupnp-error.h"
#include "gena-protocol.h"
#include "http-headers.h"
#include "gupnp-device.h"

#define GUPNP_CONTEXT_DEFAULT_LANGUAGE "en"

static void
gupnp_acl_server_handler (SoupServer *server,
                          SoupServerMessage *msg,
                          const char *path,
                          GHashTable *query,
                          gpointer user_data);

static void
gupnp_context_initable_iface_init (gpointer g_iface,
                                   gpointer iface_data);

struct _GUPnPContextPrivate {
        guint        subscription_timeout;

        SoupSession *session;

        SoupServer  *server; /* Started on demand */
        GUri *server_uri;
        char        *default_language;

        GList       *host_path_datas;

        GUPnPAcl    *acl;
};
typedef struct _GUPnPContextPrivate GUPnPContextPrivate;

G_DEFINE_TYPE_EXTENDED (GUPnPContext,
                        gupnp_context,
                        GSSDP_TYPE_CLIENT,
                        0,
                        G_ADD_PRIVATE(GUPnPContext)
                        G_IMPLEMENT_INTERFACE
                                (G_TYPE_INITABLE,
                                 gupnp_context_initable_iface_init))

enum
{
        PROP_0,
        PROP_SERVER,
        PROP_SESSION,
        PROP_SUBSCRIPTION_TIMEOUT,
        PROP_DEFAULT_LANGUAGE,
        PROP_ACL,
};

typedef struct {
        char *local_path;

        GRegex *regex;
} UserAgent;

typedef struct {
        char         *local_path;
        char         *server_path;
        char         *default_language;

        GList        *user_agents;
        GUPnPContext *context;
} HostPathData;

static GInitableIface* initable_parent_iface = NULL;

static const char *GSSDP_UDA_VERSION_STRINGS[] = {
        [GSSDP_UDA_VERSION_1_0] = "1.0",
        [GSSDP_UDA_VERSION_1_1] = "1.1"
};

/*
 * Generates the default server ID
 */
static char *
make_server_id (GSSDPUDAVersion uda_version)
{
#ifdef G_OS_WIN32
        OSVERSIONINFO versioninfo;
        versioninfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
        if (GetVersionEx (&versioninfo)) {
                return g_strdup_printf ("Microsoft Windows/%ld.%ld UPnP/%s GUPnP/%s",
                                        versioninfo.dwMajorVersion,
                                        versioninfo.dwMinorVersion,
                                        GSSDP_UDA_VERSION_STRINGS[uda_version],
                                        VERSION);
        } else {
                return g_strdup_printf ("Microsoft Windows/Unknown UPnP/%s GUPnP/%s",
                                        GSSDP_UDA_VERSION_STRINGS[uda_version],
                                        VERSION);
        }
#else
        struct utsname sysinfo;

        uname (&sysinfo);

        return g_strdup_printf ("%s/%s UPnP/%s GUPnP/%s",
                                sysinfo.sysname,
                                sysinfo.release,
                                GSSDP_UDA_VERSION_STRINGS[uda_version],
                                VERSION);
#endif
}
static void
gupnp_context_init (GUPnPContext *context)
{
}

static gboolean
gupnp_context_initable_init (GInitable     *initable,
                             GCancellable  *cancellable,
                             GError       **error)
{
        char *user_agent;
        char *server_id;
        GError *inner_error = NULL;
        GUPnPContext *context;
        GUPnPContextPrivate *priv;
        GSSDPUDAVersion version;

        if (!initable_parent_iface->init(initable,
                                         cancellable,
                                         &inner_error)) {
                g_propagate_error (error, inner_error);

                return FALSE;
        }

        context = GUPNP_CONTEXT (initable);
        priv = gupnp_context_get_instance_private (context);

        version = gssdp_client_get_uda_version (GSSDP_CLIENT (context));
        server_id = make_server_id (version);
        gssdp_client_set_server_id (GSSDP_CLIENT (context), server_id);
        g_free (server_id);

        priv->session = soup_session_new ();

        user_agent = g_strdup_printf ("%s GUPnP/" VERSION " DLNADOC/1.50",
                                      g_get_prgname()? : "");

        soup_session_set_user_agent (priv->session, user_agent);
        g_free (user_agent);

        if (g_getenv ("GUPNP_DEBUG")) {
                SoupLogger *logger;
                logger = soup_logger_new (SOUP_LOGGER_LOG_BODY);
                soup_session_add_feature (priv->session,
                                          SOUP_SESSION_FEATURE (logger));
        }

        /* Create the server already if the port is not null*/
        guint port = gssdp_client_get_port (GSSDP_CLIENT (context));
        if (port != 0) {
                // Create the server
                gupnp_context_get_server (context);

                if (priv->server == NULL) {
                        g_object_unref (priv->session);
                        priv->session = NULL;

                        g_set_error (error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_OTHER,
                                     "Could not create HTTP server on port %d",
                                     port);

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
        GUPnPContextPrivate *priv;

        context = GUPNP_CONTEXT (object);
        priv = gupnp_context_get_instance_private (context);

        switch (property_id) {
        case PROP_SUBSCRIPTION_TIMEOUT:
                priv->subscription_timeout = g_value_get_uint (value);
                break;
        case PROP_DEFAULT_LANGUAGE:
                gupnp_context_set_default_language (context,
                                                    g_value_get_string (value));
                break;
        case PROP_ACL:
                gupnp_context_set_acl (context, g_value_get_object (value));

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
        case PROP_ACL:
                g_value_set_object (value,
                                    gupnp_context_get_acl (context));

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
        GUPnPContextPrivate *priv;
        GObjectClass *object_class;

        context = GUPNP_CONTEXT (object);
        priv = gupnp_context_get_instance_private (context);

        g_clear_object (&priv->session);

        while (priv->host_path_datas) {
                HostPathData *data;

                data = (HostPathData *) priv->host_path_datas->data;

                gupnp_context_unhost_path (context, data->server_path);
        }

        g_clear_object (&priv->server);
        g_clear_object (&priv->acl);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_context_parent_class);
        object_class->dispose (object);
}

static void
gupnp_context_finalize (GObject *object)
{
        GUPnPContext *context;
        GUPnPContextPrivate *priv;
        GObjectClass *object_class;

        context = GUPNP_CONTEXT (object);
        priv = gupnp_context_get_instance_private (context);

        g_free (priv->default_language);

        if (priv->server_uri)
                g_uri_unref (priv->server_uri);

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

        /**
         * GUPnPContext:server:(attributes org.gtk.Property.get=gupnp_context_get_server)
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
         * GUPnPContext:session:(attributes org.gtk.Property.get=gupnp_context_get_session)
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
         * GUPnPContext:subscription-timeout:(attributes org.gtk.Property.get=gupnp_context_get_subscription_timeout org.gtk.Property.set=gupnp_context_set_subscription_timeout)
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
         * GUPnPContext:default-language:(attributes org.gtk.Property.get=gupnp_context_get_default_language org.gtk.Property.set=gupnp_context_set_default_language)
         *
         * The content of the Content-Language header id the client
         * sends Accept-Language and no language-specific pages to serve
         * exist. The property defaults to 'en'.
         *
         * Since: 0.18.0
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

        /**
         * GUPnPContext:acl:(attributes org.gtk.Property.get=gupnp_context_get_acl org.gtk.Property.set=gupnp_context_set_acl)
         *
         * An access control list.
         *
         * Since: 0.20.11
         */
        g_object_class_install_property
                (object_class,
                 PROP_ACL,
                 g_param_spec_object ("acl",
                                      "Access control list",
                                      "Access control list",
                                      GUPNP_TYPE_ACL,
                                      G_PARAM_CONSTRUCT |
                                      G_PARAM_READWRITE |
                                      G_PARAM_STATIC_STRINGS));
}

/**
 * gupnp_context_get_session:(attributes org.gtk.Method.get_property=session)
 * @context: A #GUPnPContext
 *
 * Get the #SoupSession object that GUPnP is using.
 *
 * Return value: (transfer none): The #SoupSession used by GUPnP. Do not unref
 * this when finished.
 *
 * Since: 0.12.3
 **/
SoupSession *
gupnp_context_get_session (GUPnPContext *context)
{
        GUPnPContextPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);

        priv = gupnp_context_get_instance_private (context);

        return priv->session;
}

/*
 * Default server handler: Return 404 not found.
 **/
static void
default_server_handler (G_GNUC_UNUSED SoupServer *server,
                        SoupServerMessage *msg,
                        G_GNUC_UNUSED const char *path,
                        G_GNUC_UNUSED GHashTable *query,
                        G_GNUC_UNUSED gpointer user_data)
{
        soup_server_message_set_status (msg,
                                        SOUP_STATUS_NOT_FOUND,
                                        "Not found");
}

/**
 * gupnp_context_get_server:(attributes org.gtk.Method.get_property=server)
 * @context: A #GUPnPContext
 *
 * Get the #SoupServer HTTP server that GUPnP is using.
 *
 * Returns: (transfer none): The #SoupServer used by GUPnP. Do not unref this when finished.
 **/
SoupServer *
gupnp_context_get_server (GUPnPContext *context)
{
        GUPnPContextPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);
        priv = gupnp_context_get_instance_private (context);

        if (priv->server == NULL) {
                const char *ip = NULL;
                GSocketAddress *addr = NULL;
                GInetAddress *inet_addr = NULL;
                GError *error = NULL;

                priv->server = soup_server_new (NULL, NULL);

                soup_server_add_handler (priv->server,
                                         NULL,
                                         default_server_handler,
                                         context,
                                         NULL);

                ip = gssdp_client_get_host_ip (GSSDP_CLIENT (context));
                inet_addr = gssdp_client_get_address (GSSDP_CLIENT (context));
                guint port = gssdp_client_get_port (GSSDP_CLIENT (context));
                if (g_inet_address_get_family (inet_addr) == G_SOCKET_FAMILY_IPV6 &&
                    g_inet_address_get_is_link_local (inet_addr)) {
                        guint scope =
                                gssdp_client_get_index (GSSDP_CLIENT (context));
                        addr = g_object_new (G_TYPE_INET_SOCKET_ADDRESS,
                                             "address", inet_addr,
                                             "port", port,
                                             "scope-id", scope,
                                             NULL);
                } else {
                        addr = g_inet_socket_address_new (inet_addr, port);
                }
                g_object_unref (inet_addr);

                if (! soup_server_listen (priv->server,
                                          addr, (SoupServerListenOptions) 0, &error)) {
                        g_clear_object (&priv->server);
                        g_warning ("Unable to listen on %s:%u %s", ip, port, error->message);
                        g_error_free (error);
                }

                g_object_unref (addr);
        }

        return priv->server;
}

/*
 * Makes a SoupURI that refers to our server.
 **/
static GUri *
make_server_uri (GUPnPContext *context)
{
        SoupServer *server = gupnp_context_get_server (context);
        GSList *uris = soup_server_get_uris (server);
        if (uris)
        {
                GUri *uri = g_uri_ref (uris->data);
                g_slist_free_full (uris, (GDestroyNotify) g_uri_unref);

                return uri;
        }
        return NULL;
}

GUri *
_gupnp_context_get_server_uri (GUPnPContext *context)
{
        GUPnPContextPrivate *priv;

        priv = gupnp_context_get_instance_private (context);
        if (priv->server_uri == NULL)
                priv->server_uri = make_server_uri (context);

        if (priv->server_uri)
                return g_uri_ref (priv->server_uri);

        return NULL;
}

/**
 * gupnp_context_new:
 * @iface: (nullable): The network interface to use, or %NULL to
 * auto-detect.
 * @port: Port to run on, or 0 if you don't care what port is used.
 * @error: (inout)(optional)(nullable): A location to store a #GError, or %NULL
 *
 * Create a new #GUPnPContext with the specified @iface and
 * @port.
 *
 * Deprecated: 1.6. Use [ctor@GUPnP.Context.new_for_address] instead
 *
 * Return value: A new #GUPnPContext object, or %NULL on an error
 **/
GUPnPContext *
gupnp_context_new (const char   *iface,
                   guint         port,
                   GError      **error)
{
        return g_initable_new (GUPNP_TYPE_CONTEXT,
                               NULL,
                               error,
                               "interface", iface,
                               "port", port,
                               NULL);
}

/**
 * gupnp_context_new_full:
 * @iface: (nullable): the name of a network interface
 * @addr: (nullable): an IP address or %NULL for auto-detection. If you do not
 * care about the address, but want to specify an address family, use
 * [ctor@Glib.InetAddress.new_any] with the appropriate family instead.
 * @port: The network port to use for M-SEARCH requests or 0 for
 * random.
 * @uda_version: The UDA version this client will adhere to
 * @error: (allow-none): Location to store error, or %NULL.
 *
 * Creates a GUPnP context with address @addr on network interface @iface. If
 * neither is specified, GUPnP will chose the address it deems most suitable.
 *
 * Since: 1.6.0
 *
 * Return value: (nullable):  A new #GSSDPClient object or %NULL on error.
 */
GUPnPContext *
gupnp_context_new_full (const char *iface,
                       GInetAddress *addr,
                       guint16 port,
                       GSSDPUDAVersion uda_version,
                       GError **error)
{
        return g_initable_new (GUPNP_TYPE_CONTEXT,
                               NULL,
                               error,
                               "interface",
                               iface,
                               "address",
                               addr,
                               "port",
                               port,
                               "uda-version",
                               uda_version,
                               NULL);
}

/**
 * gupnp_context_new_for_address
 * @addr: (nullable): an IP address or %NULL for auto-detection. If you do not
 * care about the address, but want to specify an address family, use
 * [ctor@Glib.InetAddress.new_any] with the appropriate family instead.
 * @port: The network port to use for M-SEARCH requests or 0 for
 * random.
 * @uda_version: The UDA version this client will adhere to
 * @error: (allow-none): Location to store error, or %NULL.
 *
 * Creates a GUPnP context with address @addr. If none is specified, GUPnP
 * will chose the address it deems most suitable.
 *
 * Since: 1.6.0
 *
 * Return value: (nullable):  A new #GSSDPClient object or %NULL on error.
 */
GUPnPContext *
gupnp_context_new_for_address (GInetAddress *addr,
                                guint16 port,
                                GSSDPUDAVersion uda_version,
                                GError **error)
{
        return gupnp_context_new_full (NULL, addr, port, uda_version, error);
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
        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), 0);

        GUri *uri = _gupnp_context_get_server_uri (context);
        g_uri_unref (uri);

        return g_uri_get_port (uri);
}

/**
 * gupnp_context_set_subscription_timeout:(attributes org.gtk.Method.set_property=subscription-timeout)
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
        GUPnPContextPrivate *priv;

        g_return_if_fail (GUPNP_IS_CONTEXT (context));
        priv = gupnp_context_get_instance_private (context);

        priv->subscription_timeout = timeout;

        g_object_notify (G_OBJECT (context), "subscription-timeout");
}

/**
 * gupnp_context_get_subscription_timeout:(attributes org.gtk.Method.get_property=subscription-timeout)
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
        GUPnPContextPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), 0);

        priv = gupnp_context_get_instance_private (context);

        return priv->subscription_timeout;
}

static void
host_path_data_set_language (HostPathData *data, const char *language)
{
        char *old_language = data->default_language;

        if ((old_language != NULL) && (!strcmp (language, old_language)))
                return;

        data->default_language = g_strdup (language);

        g_free (old_language);
}

/**
 * gupnp_context_set_default_language:(attributes org.gtk.Method.set_property=default-language)
 * @context: A #GUPnPContext
 * @language: A language tag as defined in RFC 2616 3.10
 *
 * Set the default language for the Content-Language header to @language.
 *
 * If the client sends an Accept-Language header the UPnP HTTP server
 * is required to send a Content-Language header in return. If there are
 * no files hosted in languages which match the requested ones the
 * Content-Language header is set to this value. The default value is "en".
 *
 * Since: 0.18.0
 */
void
gupnp_context_set_default_language (GUPnPContext *context,
                                    const char   *language)
{
        GUPnPContextPrivate *priv;
        char *old_language = NULL;

        g_return_if_fail (GUPNP_IS_CONTEXT (context));
        g_return_if_fail (language != NULL);
        priv = gupnp_context_get_instance_private (context);


        old_language = priv->default_language;

        if ((old_language != NULL) && (!strcmp (language, old_language)))
                return;

        priv->default_language = g_strdup (language);

        g_list_foreach (priv->host_path_datas,
                        (GFunc) host_path_data_set_language,
                        (gpointer) language);

        g_free (old_language);
}

/**
 * gupnp_context_get_default_language:(attributes org.gtk.Method.get_property=default-language)
 * @context: A #GUPnPContext
 *
 * Get the default Content-Language header for this context.
 *
 * Returns: (transfer none): The default content of the Content-Language
 * header.
 *
 * Since: 0.18.0
 */
const char *
gupnp_context_get_default_language (GUPnPContext *context)
{
        GUPnPContextPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);

        priv = gupnp_context_get_instance_private (context);

        return priv->default_language;
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
redirect_to_folder (SoupServerMessage *msg)
{
        char *uri, *redir_uri;

        uri = g_uri_to_string_partial (soup_server_message_get_uri (msg),
                                       G_URI_HIDE_PASSWORD);
        redir_uri = g_strdup_printf ("%s/", uri);
        soup_message_headers_append (
                soup_server_message_get_response_headers (msg),
                "Location",
                redir_uri);
        soup_server_message_set_status (msg,
                                        SOUP_STATUS_MOVED_PERMANENTLY,
                                        "Moved permanently");
        g_free (redir_uri);
        g_free (uri);
}

static void
update_client_cache (GUPnPContext *context,
                     const char   *host,
                     const char   *user_agent)
{
        const char *entry;
        GSSDPClient *client;

        if (user_agent == NULL)
                return;

        client = GSSDP_CLIENT (context);

        entry = gssdp_client_guess_user_agent (client, host);
        if (!entry) {
                gssdp_client_add_cache_entry (client,
                                              host,
                                              user_agent);
        }
}

/* Serve @path. Note that we do not need to check for path including bogus
 * '..' as libsoup does this for us. */
static void
host_path_handler (G_GNUC_UNUSED SoupServer *server,
                   SoupServerMessage *msg,
                   const char *path,
                   G_GNUC_UNUSED GHashTable *query,
                   gpointer user_data)
{
        char *local_path, *path_to_open;
        GStatBuf st;
        int status;
        GList *locales, *orig_locales;
        GMappedFile *mapped_file;
        GError *error;
        HostPathData *host_path_data;
        const char *user_agent;
        const char *host;
        GBytes *buffer = NULL;

        orig_locales = NULL;
        locales      = NULL;
        local_path   = NULL;
        path_to_open = NULL;
        host_path_data = (HostPathData *) user_data;

        if (soup_server_message_get_method (msg) != SOUP_METHOD_GET &&
            soup_server_message_get_method (msg) != SOUP_METHOD_HEAD) {
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_NOT_IMPLEMENTED,
                                                "Not implemented");

                goto DONE;
        }

        /* Always send HTTP 1.1 for device description requests
         * Also set Connection: close header, since the request originated
         * from a HTTP 1.0 client
         */
        if (soup_server_message_get_http_version (msg) == SOUP_HTTP_1_0) {
                soup_server_message_set_http_version (msg, SOUP_HTTP_1_1);
                soup_message_headers_append (
                        soup_server_message_get_response_headers (msg),
                        "Connection",
                        "close");
        }

        user_agent = soup_message_headers_get_one (
                soup_server_message_get_request_headers (msg),
                "User-Agent");
        host = soup_server_message_get_remote_host (msg);

        /* If there was no User-Agent in the request, try to guess from the
         * discovery message and put it into the response headers for further
         * processing. Otherwise use the agent to populate the cache.
         */
        if (user_agent == NULL) {
                GSSDPClient *client = GSSDP_CLIENT (host_path_data->context);

                user_agent = gssdp_client_guess_user_agent (client, host);

                if (user_agent != NULL) {
                        soup_message_headers_append (
                                soup_server_message_get_response_headers (msg),
                                "User-Agent",
                                user_agent);
                }
        } else {
                update_client_cache (host_path_data->context, host, user_agent);
        }

        /* Construct base local path */
        local_path = construct_local_path (path, user_agent, host_path_data);
        if (!local_path) {
                soup_server_message_set_status (msg,
                                                SOUP_STATUS_BAD_REQUEST,
                                                "Bad request");

                goto DONE;
        }

        /* Get preferred locales */
        orig_locales = locales = http_request_get_accept_locales (
                soup_server_message_get_request_headers (msg));

 AGAIN:
        /* Add locale suffix if available */
        path_to_open = append_locale (local_path, locales);

        /* See what we've got */
        if (g_stat (path_to_open, &st) == -1) {
                if (errno == EPERM)
                        soup_server_message_set_status (msg,
                                                        SOUP_STATUS_FORBIDDEN,
                                                        "Forbidden");
                else if (errno == ENOENT) {
                        if (locales) {
                                g_free (path_to_open);

                                locales = locales->next;

                                goto AGAIN;
                        } else
                                soup_server_message_set_status (
                                        msg,
                                        SOUP_STATUS_NOT_FOUND,
                                        "Not found");
                } else
                        soup_server_message_set_status (
                                msg,
                                SOUP_STATUS_INTERNAL_SERVER_ERROR,
                                "Internal server error");

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

                soup_server_message_set_status (
                        msg,
                        SOUP_STATUS_INTERNAL_SERVER_ERROR,
                        "Internal server error");

                goto DONE;
        }

        /* Handle method (GET or HEAD) */
        status = SOUP_STATUS_OK;
        SoupMessageHeaders *response_headers =
                soup_server_message_get_response_headers (msg);
        SoupMessageHeaders *request_headers =
                soup_server_message_get_request_headers (msg);

        /* Add requested content */
        // Creating the buffer here regardless of whether we use it.
        // It will take ownership of the mapped file and we can unref it on exit
        // This will prevent leaking the mapped file in other cases
        buffer = g_bytes_new_with_free_func (
                g_mapped_file_get_contents (mapped_file),
                g_mapped_file_get_length (mapped_file),
                (GDestroyNotify) g_mapped_file_unref,
                mapped_file);

        if (soup_server_message_get_method (msg) == SOUP_METHOD_GET) {
                gboolean have_range;
                SoupRange *ranges;
                int nranges;

                /* Find out range */
                have_range = FALSE;

                if (soup_message_headers_get_ranges (request_headers,
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
                        soup_server_message_set_status (
                                msg,
                                SOUP_STATUS_REQUESTED_RANGE_NOT_SATISFIABLE,
                                "Range not satisfyable");
                        soup_message_headers_free_ranges (request_headers,
                                                          ranges);

                        goto DONE;
                }

                SoupMessageBody *message_body =
                        soup_server_message_get_response_body (msg);

                /* Set range and status */
                if (have_range) {
                        GBytes *range_buffer;

                        soup_message_body_truncate (message_body);
                        soup_message_headers_set_content_range (
                                response_headers,
                                ranges[0].start,
                                ranges[0].end,
                                g_bytes_get_size (buffer));
                        range_buffer = g_bytes_new_from_bytes (
                                buffer,
                                ranges[0].start,
                                ranges[0].end - ranges[0].start + 1);
                        soup_message_body_append_bytes (message_body,
                                                        range_buffer);
                        status = SOUP_STATUS_PARTIAL_CONTENT;

                        soup_message_headers_free_ranges (request_headers,
                                                          ranges);
                        g_bytes_unref (range_buffer);
                } else
                        soup_message_body_append_bytes (message_body, buffer);

        } else if (soup_server_message_get_method (msg) == SOUP_METHOD_HEAD) {
                char *length;

                length = g_strdup_printf ("%zu", st.st_size);
                soup_message_headers_append (response_headers,
                                             "Content-Length",
                                             length);
                g_free (length);

        } else {
                g_assert_not_reached ();
                goto DONE;
        }

        /* Set Content-Type */
        http_response_set_content_type (
                response_headers,
                path_to_open,
                (guchar *) g_mapped_file_get_contents (mapped_file),
                st.st_size);

        /* Set Content-Language */
        if (locales)
                http_response_set_content_locale (response_headers,
                                                  locales->data);
        else if (soup_message_headers_get_one (request_headers,
                                               "Accept-Language")) {
                soup_message_headers_append (response_headers,
                                             "Content-Language",
                                             host_path_data->default_language);
        }

        /* Set Accept-Ranges */
        soup_message_headers_append (response_headers,
                                     "Accept-Ranges",
                                     "bytes");

        /* Set status */
        soup_server_message_set_status (msg, status, NULL);

 DONE:
        /* Cleanup */
        g_bytes_unref (buffer);
        g_free (path_to_open);
        g_free (local_path);

        g_list_free_full (orig_locales, g_free);
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
                    const char *default_language,
                    GUPnPContext *context)
{
        HostPathData *path_data;

        path_data = g_slice_new0 (HostPathData);

        path_data->local_path  = g_strdup (local_path);
        path_data->server_path = g_strdup (server_path);
        path_data->default_language = g_strdup (default_language);
        path_data->context = context;

        return path_data;
}

static void
host_path_data_free (HostPathData *path_data)
{
        g_free (path_data->local_path);
        g_free (path_data->server_path);
        g_free (path_data->default_language);

        g_list_free_full (path_data->user_agents,
                          (GDestroyNotify) user_agent_free);

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
        GUPnPContextPrivate *priv;

        g_return_if_fail (GUPNP_IS_CONTEXT (context));
        g_return_if_fail (local_path != NULL);
        g_return_if_fail (server_path != NULL);

        priv = gupnp_context_get_instance_private (context);

        server = gupnp_context_get_server (context);

        path_data = host_path_data_new (local_path,
                                        server_path,
                                        priv->default_language,
                                        context);

        soup_server_add_handler (server,
                                 server_path,
                                 host_path_handler,
                                 path_data,
                                 NULL);

        priv->host_path_datas =
                g_list_append (priv->host_path_datas,
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
 *
 * Since: 0.14.0
 **/
gboolean
gupnp_context_host_path_for_agent (GUPnPContext *context,
                                   const char   *local_path,
                                   const char   *server_path,
                                   GRegex       *user_agent)
{
        GUPnPContextPrivate *priv;
        GList *node;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), FALSE);
        g_return_val_if_fail (local_path != NULL, FALSE);
        g_return_val_if_fail (server_path != NULL, FALSE);
        g_return_val_if_fail (user_agent != NULL, FALSE);

        priv = gupnp_context_get_instance_private (context);
        node = g_list_find_custom (priv->host_path_datas,
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
        GUPnPContextPrivate *priv;
        GList *node;

        g_return_if_fail (GUPNP_IS_CONTEXT (context));
        g_return_if_fail (server_path != NULL);

        priv = gupnp_context_get_instance_private (context);
        server = gupnp_context_get_server (context);

        node = g_list_find_custom (priv->host_path_datas,
                                   server_path,
                                   (GCompareFunc) path_compare_func);
        g_return_if_fail (node != NULL);

        path_data = (HostPathData *) node->data;
        priv->host_path_datas = g_list_delete_link (
                        priv->host_path_datas,
                        node);

        soup_server_remove_handler (server, server_path);
        host_path_data_free (path_data);
}

/**
 * gupnp_context_get_acl:(attributes org.gtk.Method.get_property=acl)
 * @context: A #GUPnPContext
 *
 * Access the #GUPnPAcl associated with this client. If there isn't any,
 * retturns %NULL. The returned ACL must not be freed.
 *
 * Returns:(transfer none): The access control list associated with this context or %NULL
 * if no acl is set.
 *
 * Since: 0.20.11
 **/
GUPnPAcl *
gupnp_context_get_acl (GUPnPContext *context)
{
        GUPnPContextPrivate *priv;

        g_return_val_if_fail (GUPNP_IS_CONTEXT (context), NULL);

        priv = gupnp_context_get_instance_private (context);
        return priv->acl;
}

/**
 * gupnp_context_set_acl:(attributes org.gtk.Method.set_property=acl)
 * @context: A #GUPnPContext
 * @acl: (nullable): The new access control list or %NULL to remove the
 * current list.
 *
 * Attach or remove the assoicated access control list to this context. If
 * @acl is %NULL, the current access control list will be removed.
 *
 * Since: 0.20.11
 **/
void
gupnp_context_set_acl (GUPnPContext *context, GUPnPAcl *acl)
{
        GUPnPContextPrivate *priv;

        g_return_if_fail (GUPNP_IS_CONTEXT (context));

        priv = gupnp_context_get_instance_private (context);
        g_clear_object (&priv->acl);

        if (acl != NULL)
                priv->acl = g_object_ref (acl);

        g_object_notify (G_OBJECT (context), "acl");
}

static void
gupnp_acl_async_callback (GUPnPAcl *acl,
                          GAsyncResult *res,
                          AclAsyncHandler *data)
{
        gboolean allowed;
        GError *error = NULL;

        allowed = gupnp_acl_is_allowed_finish (acl, res, &error);
#if SOUP_CHECK_VERSION(3,1,2)
        soup_server_message_unpause (data->message);
#else
        soup_server_unpause_message (data->server, data->message);
#endif
        if (!allowed)
                soup_server_message_set_status (data->message,
                                                SOUP_STATUS_FORBIDDEN,
                                                "Forbidden");
        else
                data->handler->callback (data->server,
                                         data->message,
                                         data->path,
                                         data->query,
                                         data->handler->user_data);

        acl_async_handler_free (data);
}

static void
gupnp_acl_server_handler (SoupServer *server,
                          SoupServerMessage *msg,
                          const char *path,
                          GHashTable *query,
                          gpointer user_data)
{
        AclServerHandler *handler = (AclServerHandler *) user_data;
        const char *agent;
        const char *host;
        GUPnPDevice *device = NULL;
        GUPnPContextPrivate *priv;

        priv = gupnp_context_get_instance_private (handler->context);
        host = soup_server_message_get_remote_host (msg);

        if (handler->service) {
                g_object_get (handler->service,
                              "root-device", &device,
                              NULL);

                // g_object_get will give us an additional reference here.
                // drop that immediately
                if (device != NULL) {
                        g_object_unref (device);
                }
        }

        agent = soup_message_headers_get_one (
                soup_server_message_get_request_headers (msg),
                "User-Agent");
        if (agent == NULL) {
                agent = gssdp_client_guess_user_agent
                                (GSSDP_CLIENT (handler->context),
                                 host);
        }

        if (priv->acl != NULL) {
                if (gupnp_acl_can_sync (priv->acl)) {
                        if (!gupnp_acl_is_allowed (priv->acl,
                                                   device,
                                                   handler->service,
                                                   path,
                                                   host,
                                                   agent)) {
                                soup_server_message_set_status (
                                        msg,
                                        SOUP_STATUS_FORBIDDEN,
                                        "Forbidden");

                                return;
                        }
                } else {
                        AclAsyncHandler *data;

                        data = acl_async_handler_new (server,
                                                      msg,
                                                      path,
                                                      query,
                                                      handler);

#if SOUP_CHECK_VERSION(3,1,2)
                        soup_server_message_pause (msg);
#else
                        soup_server_pause_message (server, msg);
#endif
                        // Since we drop the additional reference above, coverity seems to think this is
                        // use-after-free, but the service is still holding a reference here.

                        // coverity[pass_freed_arg]
                        gupnp_acl_is_allowed_async (
                                priv->acl,
                                device,
                                handler->service,
                                path,
                                host,
                                agent,
                                NULL,
                                (GAsyncReadyCallback) gupnp_acl_async_callback,
                                data);

                        return;
                }
        }

        /* Delegate to orignal callback */
        handler->callback (server, msg, path, query, handler->user_data);
}

/**
 * gupnp_context_add_server_handler:
 * @context: a #GUPnPContext
 * @use_acl: %TRUE, if the path should query the GUPnPContext::acl before
 * serving the resource, %FALSE otherwise.
 * @path: the toplevel path for the handler.
 * @callback: callback to invoke for requests under @path
 * @user_data: the user_data passed to @callback
 * @destroy: (nullable): A #GDestroyNotify for @user_data or %NULL if none.
 *
 * Add a #SoupServerCallback to the #GUPnPContext<!-- -->'s #SoupServer.
 *
 * Since: 0.20.11
 */
void
gupnp_context_add_server_handler (GUPnPContext *context,
                                  gboolean use_acl,
                                  const char *path,
                                  SoupServerCallback callback,
                                  gpointer user_data,
                                  GDestroyNotify destroy)
{
        GUPnPContextPrivate *priv;

        g_return_if_fail (GUPNP_IS_CONTEXT (context));

        priv = gupnp_context_get_instance_private (context);
        if (use_acl) {
                AclServerHandler *handler;
                handler = acl_server_handler_new (NULL, context, callback, user_data, destroy);
                soup_server_add_handler (priv->server,
                                         path,
                                         gupnp_acl_server_handler,
                                         handler,
                                         (GDestroyNotify) acl_server_handler_free);
        } else
                soup_server_add_handler (priv->server,
                                         path,
                                         callback,
                                         user_data,
                                         destroy);
}

void
_gupnp_context_add_server_handler_with_data (GUPnPContext *context,
                                             const char *path,
                                             AclServerHandler *handler)
{
        GUPnPContextPrivate *priv;

        g_return_if_fail (GUPNP_IS_CONTEXT (context));

        priv = gupnp_context_get_instance_private (context);
        soup_server_add_handler (priv->server,
                                 path,
                                 gupnp_acl_server_handler,
                                 handler,
                                 (GDestroyNotify) acl_server_handler_free);
}

/**
 * gupnp_context_remove_server_handler:
 * @context: a #GUPnPContext
 * @path: the toplevel path for the handler.
 *
 * Remove a #SoupServerCallback from the #GUPnPContext<!-- -->'s #SoupServer.
 *
 * Since: 0.20.11
 */
void
gupnp_context_remove_server_handler (GUPnPContext *context, const char *path)
{
        GUPnPContextPrivate *priv;

        g_return_if_fail (GUPNP_IS_CONTEXT (context));

        priv = gupnp_context_get_instance_private (context);
        soup_server_remove_handler (priv->server, path);
}

/**
 * gupnp_context_rewrite_uri:
 * @context: a #GUPnPContext
 * @uri: an uri to rewrite if necessary
 *
 * Utility function to re-write an uri to the IPv6 link-local form which has
 * the zone index appended to the IP address.
 *
 * Returns: A re-written version of the @uri if the context is on a link-local
 * IPv6 address, a copy of the @uri otherwise or %NULL if @uri was invalid
 *
 * Since: 1.2.0
 */
char *
gupnp_context_rewrite_uri (GUPnPContext *context, const char *uri)
{
        GUri *soup_uri = NULL;
        char *retval = NULL;

        soup_uri = gupnp_context_rewrite_uri_to_uri (context, uri);
        if (soup_uri == NULL) {
                // We already issued a warning above, so just return
                return NULL;
        }

        retval = g_uri_to_string_partial (soup_uri, G_URI_HIDE_PASSWORD);
        g_uri_unref (soup_uri);

        return retval;
}

/**
 * gupnp_context_rewrite_uri_to_uri:
 * @context: a #GUPnPContext
 * @uri: an uri to rewrite if necessary
 *
 * Utility function to re-write an uri to the IPv6 link-local form which has
 * the zone index appended to the IP address.
 *
 * Returns: A re-written version of the @uri if the context is on a link-local
 * IPv6 address or @uri converted to a #SoupURI or %NULL if @uri is invalid
 *
 * Since: 1.2.3
 * Stability: Private
 */
GUri *
gupnp_context_rewrite_uri_to_uri (GUPnPContext *context, const char *uri)
{
        const char *host = NULL;
        GUri *soup_uri = NULL;
        GInetAddress *addr = NULL;
        int index = -1;
        GError *error = NULL;

        soup_uri = g_uri_parse (uri, G_URI_FLAGS_NONE, &error);

        if (error != NULL) {
                g_warning ("Invalid call-back url: %s (%s)",
                           uri,
                           error->message);

                g_clear_error (&error);

                return NULL;
        }

        host = g_uri_get_host (soup_uri);
        addr = g_inet_address_new_from_string (host);

        if (addr == NULL) {
                return soup_uri;
        }

        index = gssdp_client_get_index (GSSDP_CLIENT (context));

        if (g_inet_address_get_is_link_local (addr) &&
            g_inet_address_get_family (addr) == G_SOCKET_FAMILY_IPV6) {
                char *new_host;

                new_host = g_strdup_printf ("%s%%%d",
                                            host,
                                            index);
                GUri *new_uri =
                        soup_uri_copy (soup_uri, SOUP_URI_HOST, new_host, NULL);
                g_free (new_host);
                g_uri_unref (soup_uri);
                soup_uri = new_uri;
        }

        if (g_inet_address_get_family (addr) !=
            gssdp_client_get_family (GSSDP_CLIENT (context))) {
                g_warning ("Address family mismatch while trying to rewrite "
                           "URI %s",
                           uri);
                g_uri_unref (soup_uri);
                soup_uri = NULL;
        }

        g_object_unref (addr);

        return soup_uri;
}

gboolean
validate_host_header (const char *host_header,
                      const char *host_ip,
                      guint context_port)
{

        gboolean retval = FALSE;
        // Be lazy and let GUri do the heavy lifting here, such as stripping the
        // [] from v6 addresses, splitting of the port etc.
        char *uri_from_host = g_strconcat ("http://", host_header, NULL);

        char *host = NULL;
        int port = 0;
        GError *error = NULL;

        g_uri_split_network (uri_from_host,
                             G_URI_FLAGS_NONE,
                             NULL,
                             &host,
                             &port,
                             &error);

        if (error != NULL) {
                g_debug ("Failed to parse HOST header from request: %s",
                         error->message);
                goto out;
        }

        // -1 means there was no :port; according to UDA this is allowed and
        // defaults to 80, the HTTP port then
        if (port == -1) {
                port = 80;
        }

        if (!g_str_equal (host, host_ip)) {
                g_debug ("Mismatch between host header and host IP (%s, "
                         "expected: %s)",
                         host,
                         host_ip);
        }

        if (port != context_port) {
                g_debug ("Mismatch between host header and host port (%d, "
                         "expected %d)",
                         port,
                         context_port);
        }

        retval = g_str_equal (host, host_ip) && port == context_port;

out:
        g_clear_error (&error);
        g_free (host);
        g_free (uri_from_host);

        return retval;
}

gboolean
gupnp_context_validate_host_header (GUPnPContext *context,
                                    const char *host_header)
{
        return validate_host_header (
                host_header,
                gssdp_client_get_host_ip (GSSDP_CLIENT (context)),
                gupnp_context_get_port (context));
}
