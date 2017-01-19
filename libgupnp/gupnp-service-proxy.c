/*
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:gupnp-service-proxy
 * @short_description: Proxy class for remote services.
 *
 * #GUPnPServiceProxy sends commands to a remote UPnP service and handles
 * incoming event notifications. #GUPnPServiceProxy implements the
 * #GUPnPServiceInfo interface.
 */

#include <libsoup/soup.h>
#include <gobject/gvaluecollector.h>
#include <string.h>
#include <locale.h>
#include <errno.h>

#include "gupnp-service-proxy.h"
#include "gupnp-context-private.h"
#include "gupnp-error.h"
#include "gupnp-error-private.h"
#include "gupnp-types.h"
#include "xml-util.h"
#include "gena-protocol.h"
#include "http-headers.h"
#include "gvalue-util.h"

G_DEFINE_TYPE (GUPnPServiceProxy,
               gupnp_service_proxy,
               GUPNP_TYPE_SERVICE_INFO);

struct _GUPnPServiceProxyPrivate {
        gboolean subscribed;

        GList *pending_actions;

        char *path; /* Path to this proxy */

        char *sid; /* Subscription ID */
        GSource *subscription_timeout_src;

        guint32 seq; /* Event sequence number */

        GHashTable *notify_hash;

        GList *pending_messages; /* Pending SoupMessages from this proxy */

        GList *pending_notifies; /* Pending notifications to be sent (xmlDoc) */
        GSource *notify_idle_src; /* Idle handler src of notification emiter */
};

enum {
        PROP_0,
        PROP_SUBSCRIBED
};

enum {
        SUBSCRIPTION_LOST,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _GUPnPServiceProxyAction {
        volatile gint ref_count;
        GUPnPServiceProxy *proxy;

        SoupMessage *msg;
        GString *msg_str;

        GUPnPServiceProxyActionCallback callback;
        gpointer user_data;

        GError *error;    /* If non-NULL, description of error that
                             occurred when preparing message */
};

typedef struct {
        GType type;       /* type of the variable this notification is for */

        GList *callbacks; /* list of CallbackData */
        GList *next_emit; /* pointer into callbacks as which callback to call next*/
} NotifyData;

typedef struct {
        GUPnPServiceProxyNotifyCallback callback;
        GDestroyNotify notify;
        gpointer user_data;
} CallbackData;

typedef struct {
        char *sid;
        guint32 seq;

        xmlDoc *doc;
} EmitNotifyData;

static void
subscribe_got_response (SoupSession       *session,
                        SoupMessage       *msg,
                        GUPnPServiceProxy *proxy);
static void
subscribe (GUPnPServiceProxy *proxy);
static void
unsubscribe (GUPnPServiceProxy *proxy);

static GUPnPServiceProxyAction *
gupnp_service_proxy_action_ref (GUPnPServiceProxyAction *action);

static void
gupnp_service_proxy_action_unref (GUPnPServiceProxyAction *action);

static GUPnPServiceProxyAction *
gupnp_service_proxy_action_ref (GUPnPServiceProxyAction *action)
{
        g_return_val_if_fail (action, NULL);
        g_return_val_if_fail (action->ref_count > 0, NULL);

        g_atomic_int_inc (&action->ref_count);

        return action;
}

static void
gupnp_service_proxy_action_unref (GUPnPServiceProxyAction *action)
{

        g_return_if_fail (action);
        g_return_if_fail (action->ref_count > 0);


        if (g_atomic_int_dec_and_test (&action->ref_count)) {
                if (action->proxy != NULL) {
                        g_object_remove_weak_pointer (G_OBJECT (action->proxy),
                                                      (gpointer *)&(action->proxy));
                        action->proxy->priv->pending_actions =
                                g_list_remove (action->proxy->priv->pending_actions, action);
                }

                if (action->msg != NULL)
                        g_object_unref (action->msg);

                g_slice_free (GUPnPServiceProxyAction, action);
        }
}

G_DEFINE_BOXED_TYPE (GUPnPServiceProxyAction, gupnp_service_proxy_action, gupnp_service_proxy_action_ref, gupnp_service_proxy_action_unref)

static void
callback_data_free (CallbackData *data)
{
        if (data->notify)
                data->notify (data->user_data);

        g_slice_free (CallbackData, data);
}

static void
notify_data_free (NotifyData *data)
{
        g_list_free_full (data->callbacks,
                          (GDestroyNotify) callback_data_free);

        g_slice_free (NotifyData, data);
}

/* Steals doc reference */
static EmitNotifyData *
emit_notify_data_new (const char *sid,
                      guint32     seq,
                      xmlDoc     *doc)
{
        EmitNotifyData *data;

        data = g_slice_new (EmitNotifyData);

        data->sid = g_strdup (sid);
        data->seq = seq;
        data->doc = doc;

        return data;
}

static void
emit_notify_data_free (EmitNotifyData *data)
{
        g_free (data->sid);
        xmlFreeDoc (data->doc);

        g_slice_free (EmitNotifyData, data);
}

static void
gupnp_service_proxy_init (GUPnPServiceProxy *proxy)
{
        static int proxy_counter = 0;

        proxy->priv = G_TYPE_INSTANCE_GET_PRIVATE (proxy,
                                                   GUPNP_TYPE_SERVICE_PROXY,
                                                   GUPnPServiceProxyPrivate);

        /* Generate unique path */
        proxy->priv->path = g_strdup_printf ("/ServiceProxy%d", proxy_counter);
        proxy_counter++;

        /* Set up notify hash */
        proxy->priv->notify_hash =
                g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       (GDestroyNotify) notify_data_free);
}

static void
gupnp_service_proxy_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
        GUPnPServiceProxy *proxy;

        proxy = GUPNP_SERVICE_PROXY (object);

        switch (property_id) {
        case PROP_SUBSCRIBED:
                gupnp_service_proxy_set_subscribed
                                (proxy, g_value_get_boolean (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_service_proxy_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
        GUPnPServiceProxy *proxy;

        proxy = GUPNP_SERVICE_PROXY (object);

        switch (property_id) {
        case PROP_SUBSCRIBED:
                g_value_set_boolean (value,
                                     proxy->priv->subscribed);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_service_proxy_dispose (GObject *object)
{
        GUPnPServiceProxy *proxy;
        GObjectClass *object_class;
        GUPnPContext *context;
        SoupSession *session;

        proxy = GUPNP_SERVICE_PROXY (object);

        /* Unsubscribe */
        if (proxy->priv->subscribed) {
                unsubscribe (proxy);

                proxy->priv->subscribed = FALSE;
        }

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));

        /* Remove server handler */
        if (context) {
            SoupServer *server;

            server = gupnp_context_get_server (context);
            soup_server_remove_handler (server, proxy->priv->path);
        }

        /* Cancel pending actions */
        while (proxy->priv->pending_actions) {
                GUPnPServiceProxyAction *action;

                action = proxy->priv->pending_actions->data;
                proxy->priv->pending_actions =
                        g_list_delete_link (proxy->priv->pending_actions,
                                            proxy->priv->pending_actions);

                gupnp_service_proxy_cancel_action (proxy, action);
        }

        /* Cancel pending messages */
        if (context)
                session = gupnp_context_get_session (context);
        else
                session = NULL; /* Not the first time dispose is called. */

        while (proxy->priv->pending_messages) {
                SoupMessage *msg;

                msg = proxy->priv->pending_messages->data;

                soup_session_cancel_message (session,
                                             msg,
                                             SOUP_STATUS_CANCELLED);

                proxy->priv->pending_messages =
                        g_list_delete_link (proxy->priv->pending_messages,
                                            proxy->priv->pending_messages);
        }

        /* Cancel pending notifications */
        if (proxy->priv->notify_idle_src) {
                g_source_destroy (proxy->priv->notify_idle_src);
                proxy->priv->notify_idle_src = NULL;
        }

        g_list_free_full (proxy->priv->pending_notifies,
                          (GDestroyNotify) emit_notify_data_free);
        proxy->priv->pending_notifies = NULL;

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_service_proxy_parent_class);
        object_class->dispose (object);
}

static void
gupnp_service_proxy_finalize (GObject *object)
{
        GUPnPServiceProxy *proxy;
        GObjectClass *object_class;

        proxy = GUPNP_SERVICE_PROXY (object);

        g_free (proxy->priv->path);

        g_hash_table_destroy (proxy->priv->notify_hash);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_service_proxy_parent_class);
        object_class->finalize (object);
}

static void
gupnp_service_proxy_class_init (GUPnPServiceProxyClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_service_proxy_set_property;
        object_class->get_property = gupnp_service_proxy_get_property;
        object_class->dispose      = gupnp_service_proxy_dispose;
        object_class->finalize     = gupnp_service_proxy_finalize;

        g_type_class_add_private (klass, sizeof (GUPnPServiceProxyPrivate));

        /**
         * GUPnPServiceProxy:subscribed:
         *
         * Whether we are subscribed to this service.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_SUBSCRIBED,
                 g_param_spec_boolean ("subscribed",
                                       "Subscribed",
                                       "Whether we are subscribed to this "
                                       "service",
                                       FALSE,
                                       G_PARAM_READWRITE |
                                       G_PARAM_STATIC_NAME |
                                       G_PARAM_STATIC_NICK |
                                       G_PARAM_STATIC_BLURB));

        /**
         * GUPnPServiceProxy::subscription-lost:
         * @proxy: The #GUPnPServiceProxy that received the signal
         * @error: (type GError):A pointer to a #GError describing why the subscription has
         * been lost
         *
         * Emitted whenever the subscription to this service has been lost due
         * to an error condition.
         **/
        signals[SUBSCRIPTION_LOST] =
                g_signal_new ("subscription-lost",
                              GUPNP_TYPE_SERVICE_PROXY,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GUPnPServiceProxyClass,
                                               subscription_lost),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_POINTER);
}

/**
 * gupnp_service_proxy_send_action:
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @error: (allow-none): The location where to store any error, or %NULL
 * @...: tuples of in parameter name, in parameter type, and in parameter
 * value, followed by %NULL, and then tuples of out parameter name,
 * out parameter type, and out parameter value location, terminated with %NULL
 *
 * Sends action @action with parameters @Varargs to the service exposed by
 * @proxy synchronously. If an error occurred, @error will be set. In case of
 * a UPnPError the error code will be the same in @error.
 *
 * Return value: %TRUE if sending the action was succesful.
 **/
gboolean
gupnp_service_proxy_send_action (GUPnPServiceProxy *proxy,
                                 const char        *action,
                                 GError           **error,
                                 ...)
{
        va_list var_args;
        gboolean ret;

        va_start (var_args, error);
        ret = gupnp_service_proxy_send_action_valist (proxy,
                                                      action,
                                                      error,
                                                      var_args);
        va_end (var_args);

        return ret;
}

static void
stop_main_loop (G_GNUC_UNUSED GUPnPServiceProxy       *proxy,
                G_GNUC_UNUSED GUPnPServiceProxyAction *action,
                gpointer                               user_data)
{
        g_main_loop_quit ((GMainLoop *) user_data);
}

/* This is a skip variant of G_VALUE_LCOPY, same as there is
 * G_VALUE_COLLECT_SKIP for G_VALUE_COLLECT.
 */
#define VALUE_LCOPY_SKIP(value_type, var_args) \
        G_STMT_START { \
                GTypeValueTable *_vtable = g_type_value_table_peek (value_type); \
                const gchar *_lcopy_format = _vtable->lcopy_format; \
         \
                while (*_lcopy_format) { \
                        switch (*_lcopy_format++) { \
                        case G_VALUE_COLLECT_INT: \
                                va_arg ((var_args), gint); \
                                break; \
                        case G_VALUE_COLLECT_LONG: \
                                va_arg ((var_args), glong); \
                                break; \
                        case G_VALUE_COLLECT_INT64: \
                                va_arg ((var_args), gint64); \
                                break; \
                        case G_VALUE_COLLECT_DOUBLE: \
                                va_arg ((var_args), gdouble); \
                                break; \
                        case G_VALUE_COLLECT_POINTER: \
                                va_arg ((var_args), gpointer); \
                                break; \
                        default: \
                                g_assert_not_reached (); \
                        } \
                } \
        } G_STMT_END

/* Initializes hash table to hold arg names as keys and GValues of
 * given type, but without any specific value. Note that if you are
 * going to use OUT_HASH_TABLE_TO_VAR_ARGS then you have to store a
 * copy of var_args with G_VA_COPY before using this macro.
 */
#define VAR_ARGS_TO_OUT_HASH_TABLE(var_args, hash) \
        G_STMT_START { \
                const gchar *arg_name = va_arg (var_args, const gchar *); \
         \
                while (arg_name != NULL) { \
                        GValue *value = g_new0 (GValue, 1); \
                        GType type = va_arg (var_args, GType); \
         \
                        VALUE_LCOPY_SKIP (type, var_args); \
                        g_value_init (value, type); \
                        g_hash_table_insert (hash, g_strdup (arg_name), value); \
                        arg_name = va_arg (var_args, const gchar *); \
                } \
        } G_STMT_END

/* Initializes hash table to hold arg names as keys and GValues of
 * given type and value.
 */
#define VAR_ARGS_TO_IN_LIST(var_args, names, values) \
        G_STMT_START { \
                const gchar *arg_name = va_arg (var_args, const gchar *); \
         \
                while (arg_name != NULL) { \
                        GValue *value = g_new0 (GValue, 1); \
                        gchar *__error = NULL; \
                        GType type = va_arg (var_args, GType); \
         \
                        G_VALUE_COLLECT_INIT (value, \
                                              type, \
                                              var_args, \
                                              G_VALUE_NOCOPY_CONTENTS, \
                                              &__error); \
                        if (__error == NULL) { \
                                names = g_list_prepend (names, g_strdup (arg_name)); \
                                values = g_list_prepend (values, value); \
                        } else { \
                                g_warning ("Failed to collect value of type %s for %s: %s", \
                                           g_type_name (type), \
                                           arg_name, \
                                           __error); \
                                g_free (__error); \
                        } \
                        arg_name = va_arg (var_args, const gchar *); \
                } \
                names = g_list_reverse (names); \
                values = g_list_reverse (values); \
        } G_STMT_END

/* Puts values stored in hash table with GValues into var args.
 */
#define OUT_HASH_TABLE_TO_VAR_ARGS(hash, var_args) \
        G_STMT_START { \
                const gchar *arg_name = va_arg (var_args, const gchar *); \
         \
                while (arg_name != NULL) { \
                        GValue *value = g_hash_table_lookup (hash, arg_name); \
                        GType type = va_arg (var_args, GType); \
         \
                        if (value == NULL) { \
                                g_warning ("No value for %s", arg_name); \
                                G_VALUE_COLLECT_SKIP (type, var_args); \
                        } else if (G_VALUE_TYPE (value) != type) { \
                                g_warning ("Different GType in value (%s) and in var args (%s) for %s.", \
                                           G_VALUE_TYPE_NAME (value), \
                                           g_type_name (type), \
                                           arg_name); \
                        } else { \
                                gchar *__error = NULL; \
         \
                                G_VALUE_LCOPY (value, var_args, 0, &__error); \
                                if (__error != NULL) { \
                                        g_warning ("Failed to lcopy the value of type %s for %s: %s", \
                                                   g_type_name (type), \
                                                   arg_name, \
                                                   __error); \
                                        g_free (__error); \
                                } \
                        } \
                        arg_name = va_arg (var_args, const gchar *); \
                } \
        } G_STMT_END

/* GDestroyNotify for GHashTable holding GValues.
 */
static void
value_free (gpointer data)
{
  GValue *value = (GValue *) data;

  g_value_unset (value);
  g_free (value);
}

/**
 * gupnp_service_proxy_send_action_valist:
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @error: (allow-none): The location where to store any error, or %NULL
 * @var_args: va_list of tuples of in parameter name, in parameter type, and in
 * parameter value, followed by %NULL, and then tuples of out parameter name,
 * out parameter type, and out parameter value location
 *
 * See gupnp_service_proxy_send_action().
 *
 * Return value: %TRUE if sending the action was succesful.
 **/
gboolean
gupnp_service_proxy_send_action_valist (GUPnPServiceProxy *proxy,
                                        const char        *action,
                                        GError           **error,
                                        va_list            var_args)
{
        GList *in_names = NULL, *in_values = NULL;
        GHashTable *out_hash = NULL;
        va_list var_args_copy;
        gboolean result;
        GError *local_error;
        GMainLoop *main_loop;
        GUPnPServiceProxyAction *handle;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);


        main_loop = g_main_loop_new (g_main_context_get_thread_default (),
                                     TRUE);

        VAR_ARGS_TO_IN_LIST (var_args, in_names, in_values);
        G_VA_COPY (var_args_copy, var_args);
        out_hash = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          value_free);
        VAR_ARGS_TO_OUT_HASH_TABLE (var_args, out_hash);

        local_error = NULL;
        handle = gupnp_service_proxy_begin_action_list (proxy,
                                                        action,
                                                        in_names,
                                                        in_values,
                                                        stop_main_loop,
                                                        main_loop);
        if (!handle) {
                g_main_loop_unref (main_loop);
                result = FALSE;

                goto out;
        }

        /* Loop till we get a reply (or time out) */
        if (g_main_loop_is_running (main_loop))
                g_main_loop_run (main_loop);

        g_main_loop_unref (main_loop);

        result = gupnp_service_proxy_end_action_hash (proxy,
                                                      handle,
                                                      &local_error,
                                                      out_hash);

        if (local_error == NULL) {
                OUT_HASH_TABLE_TO_VAR_ARGS (out_hash, var_args_copy);
        } else {
                g_propagate_error (error, local_error);
        }
out:
        va_end (var_args_copy);
        g_list_free_full (in_names, g_free);
        g_list_free_full (in_values, value_free);
        g_hash_table_unref (out_hash);

        return result;
}

/**
 * gupnp_service_proxy_send_action_hash: (skip)
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @error: (allow-none): The location where to store any error, or %NULL
 * @in_hash: (element-type utf8 GValue) (transfer none): A #GHashTable of in
 * parameter name and #GValue pairs
 * @out_hash: (inout) (element-type utf8 GValue) (transfer full): A #GHashTable
 * of out parameter name and initialized #GValue pairs
 *
 * See gupnp_service_proxy_send_action(); this version takes a pair of
 * #GHashTable<!-- -->s for runtime determined parameter lists.
 *
 * Do not use this function in newly written code; it cannot guarantee the
 * order of arguments and thus breaks interaction with many devices.
 *
 * Return value: %TRUE if sending the action was succesful.
 *
 * Deprecated: 0.20.9: Use gupnp_service_proxy_send_action() or
 * gupnp_service_proxy_send_action_list()
 **/
gboolean
gupnp_service_proxy_send_action_hash (GUPnPServiceProxy *proxy,
                                      const char        *action,
                                      GError           **error,
                                      GHashTable        *in_hash,
                                      GHashTable        *out_hash)
{
        GMainLoop *main_loop;
        GUPnPServiceProxyAction *handle;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);

        main_loop = g_main_loop_new (g_main_context_get_thread_default (),
                                     TRUE);

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        handle = gupnp_service_proxy_begin_action_hash (proxy,
                                                        action,
                                                        stop_main_loop,
                                                        main_loop,
                                                        in_hash);
        G_GNUC_END_IGNORE_DEPRECATIONS
        if (!handle) {
                g_main_loop_unref (main_loop);

                return FALSE;
        }

        /* Loop till we get a reply (or time out) */
        if (g_main_loop_is_running (main_loop))
                g_main_loop_run (main_loop);

        g_main_loop_unref (main_loop);

        if (!gupnp_service_proxy_end_action_hash (proxy,
                                                  handle,
                                                  error,
                                                  out_hash))
                return FALSE;

        return TRUE;
}

/**
 * gupnp_service_proxy_send_action_list_gi: (rename-to gupnp_service_proxy_send_action_list)
 * @proxy: (transfer none) : A #GUPnPServiceProxy
 * @action: An action
 * @error: The location where to store any error, or %NULL
 * @in_names: (element-type utf8) (transfer none): #GList of 'in' parameter
 * names (as strings)
 * @in_values: (element-type GValue) (transfer none): #GList of values (as
 * #GValue) that line up with @in_names
 * @out_names: (element-type utf8) (transfer none): #GList of 'out' parameter
 * names (as strings)
 * @out_types: (element-type GType) (transfer none): #GList of types (as #GType)
 * that line up with @out_names
 * @out_values: (element-type GValue) (transfer full) (out): #GList of values
 * (as #GValue) that line up with @out_names and @out_types.
 *
 * The synchronous variant of #gupnp_service_proxy_begin_action_list and
 * #gupnp_service_proxy_end_action_list.
 *
 * Return value: %TRUE if sending the action was succesful.
 *
 **/
gboolean
gupnp_service_proxy_send_action_list_gi (GUPnPServiceProxy *proxy,
                                         const char        *action,
                                         GList             *in_names,
                                         GList             *in_values,
                                         GList             *out_names,
                                         GList             *out_types,
                                         GList            **out_values,
                                         GError           **error)
{
    return gupnp_service_proxy_send_action_list (proxy, action, error, in_names, in_values, out_names, out_types, out_values);
}

/**
 * gupnp_service_proxy_send_action_list: (skip)
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @error: (allow-none): The location where to store any error, or %NULL
 * @in_names: (element-type utf8) (transfer none): #GList of 'in' parameter
 * names (as strings)
 * @in_values: (element-type GValue) (transfer none): #GList of values (as
 * #GValue) that line up with @in_names
 * @out_names: (element-type utf8) (transfer none): #GList of 'out' parameter
 * names (as strings)
 * @out_types: (element-type GType) (transfer none): #GList of types (as #GType)
 * that line up with @out_names
 * @out_values: (element-type GValue) (transfer full) (out): #GList of values
 * (as #GValue) that line up with @out_names and @out_types.
 *
 * The synchronous variant of #gupnp_service_proxy_begin_action_list and
 * #gupnp_service_proxy_end_action_list.
 *
 * Return value: %TRUE if sending the action was succesful.
 *
 * Since: 0.13.3
 **/
gboolean
gupnp_service_proxy_send_action_list (GUPnPServiceProxy *proxy,
                                      const char        *action,
                                      GError           **error,
                                      GList             *in_names,
                                      GList             *in_values,
                                      GList             *out_names,
                                      GList             *out_types,
                                      GList            **out_values)
{
        GMainLoop *main_loop;
        GUPnPServiceProxyAction *handle;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);

        main_loop = g_main_loop_new (g_main_context_get_thread_default (),
                                     TRUE);

        handle = gupnp_service_proxy_begin_action_list (proxy,
                                                        action,
                                                        in_names,
                                                        in_values,
                                                        stop_main_loop,
                                                        main_loop);
        if (!handle) {
                g_main_loop_unref (main_loop);

                return FALSE;
        }

        /* Loop till we get a reply (or time out) */
        if (g_main_loop_is_running (main_loop))
                g_main_loop_run (main_loop);

        g_main_loop_unref (main_loop);

        if (!gupnp_service_proxy_end_action_list (proxy,
                                                  handle,
                                                  error,
                                                  out_names,
                                                  out_types,
                                                  out_values))
                return FALSE;

        return TRUE;
}

/**
 * gupnp_service_proxy_begin_action:
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @callback: (scope async): The callback to call when sending the action has succeeded
 * or failed
 * @user_data: User data for @callback
 * @...: tuples of in parameter name, in parameter type, and in parameter
 * value, terminated with %NULL
 *
 * Sends action @action with parameters @Varargs to the service exposed by
 * @proxy asynchronously, calling @callback on completion. From @callback, call
 * gupnp_service_proxy_end_action() to check for errors, to retrieve return
 * values, and to free the #GUPnPServiceProxyAction.
 *
 * Returns: (transfer none): A #GUPnPServiceProxyAction handle. This will be freed when
 * gupnp_service_proxy_cancel_action() or
 * gupnp_service_proxy_end_action_valist().
 **/
GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action (GUPnPServiceProxy              *proxy,
                                  const char                     *action,
                                  GUPnPServiceProxyActionCallback callback,
                                  gpointer                        user_data,
                                  ...)
{
        va_list var_args;
        GUPnPServiceProxyAction *ret;

        va_start (var_args, user_data);
        ret = gupnp_service_proxy_begin_action_valist (proxy,
                                                       action,
                                                       callback,
                                                       user_data,
                                                       var_args);
        va_end (var_args);

        return ret;
}

/* Begins a basic action message */
static GUPnPServiceProxyAction *
begin_action_msg (GUPnPServiceProxy              *proxy,
                  const char                     *action,
                  GUPnPServiceProxyActionCallback callback,
                  gpointer                        user_data)
{
        GUPnPServiceProxyAction *ret;
        char *control_url, *full_action;
        const char *service_type;

        /* Create action structure */
        ret = g_slice_new (GUPnPServiceProxyAction);
        ret->ref_count = 1;

        ret->proxy = proxy;
        g_object_add_weak_pointer (G_OBJECT (proxy), (gpointer *)&(ret->proxy));

        ret->callback  = callback;
        ret->user_data = user_data;

        ret->msg = NULL;

        ret->error = NULL;

        proxy->priv->pending_actions =
                g_list_prepend (proxy->priv->pending_actions, ret);

        /* Make sure we have a service type */
        service_type = gupnp_service_info_get_service_type
                                        (GUPNP_SERVICE_INFO (proxy));
        if (service_type == NULL) {
                ret->error = g_error_new (GUPNP_SERVER_ERROR,
                                          GUPNP_SERVER_ERROR_OTHER,
                                          "No service type defined");

                return ret;
        }

        /* Create message */
        control_url = gupnp_service_info_get_control_url
                                        (GUPNP_SERVICE_INFO (proxy));

        if (control_url != NULL) {
                ret->msg = soup_message_new (SOUP_METHOD_POST, control_url);

                g_free (control_url);
        }

        if (ret->msg == NULL) {
                ret->error = g_error_new (GUPNP_SERVER_ERROR,
                                          GUPNP_SERVER_ERROR_INVALID_URL,
                                          "No valid control URL defined");

                return ret;
        }

        /* Specify action */
        full_action = g_strdup_printf ("\"%s#%s\"", service_type, action);
        soup_message_headers_append (ret->msg->request_headers,
                                     "SOAPAction",
                                     full_action);
        g_free (full_action);

        /* Specify language */
        http_request_set_accept_language (ret->msg);

        /* Accept gzip encoding */
        soup_message_headers_append (ret->msg->request_headers,
				     "Accept-Encoding", "gzip");

        /* Set up envelope */
        ret->msg_str = xml_util_new_string ();

        g_string_append (ret->msg_str,
                         "<?xml version=\"1.0\"?>"
                         "<s:Envelope xmlns:s="
                                "\"http://schemas.xmlsoap.org/soap/envelope/\" "
                          "s:encodingStyle="
                                "\"http://schemas.xmlsoap.org/soap/encoding/\">"
                         "<s:Body>");

        g_string_append (ret->msg_str, "<u:");
        g_string_append (ret->msg_str, action);
        g_string_append (ret->msg_str, " xmlns:u=\"");
        g_string_append (ret->msg_str, service_type);
        g_string_append (ret->msg_str, "\">");

        return ret;
}

/* Received response to action message */
static void
action_got_response (SoupSession             *session,
                     SoupMessage             *msg,
                     GUPnPServiceProxyAction *action)
{
        const char *full_action;

        switch (msg->status_code) {
        case SOUP_STATUS_CANCELLED:
                /* Silently return */
                break;

        case SOUP_STATUS_METHOD_NOT_ALLOWED:
                /* Retry with M-POST */
                msg->method = "M-POST";

                soup_message_headers_append
                        (msg->request_headers,
                         "Man",
                         "\"http://schemas.xmlsoap.org/soap/envelope/\"; ns=s");

                /* Rename "SOAPAction" to "s-SOAPAction" */
                full_action = soup_message_headers_get_one
                        (msg->request_headers,
                         "SOAPAction");
                soup_message_headers_append (msg->request_headers,
                                             "s-SOAPAction",
                                             full_action);
                soup_message_headers_remove (msg->request_headers,
                                            "SOAPAction");

                /* And re-queue */
                soup_session_requeue_message (session, msg);

                break;

        default:
                /* Success: Call callback */
                action->callback (action->proxy, action, action->user_data);

                break;
        }
}

/* Finishes an action message and sends it off */
static void
finish_action_msg (GUPnPServiceProxyAction *action,
                   const char              *action_name)
{
        GUPnPContext *context;
        SoupSession *session;

        /* Finish message */
        g_string_append (action->msg_str, "</u:");
        g_string_append (action->msg_str, action_name);
        g_string_append_c (action->msg_str, '>');

        g_string_append (action->msg_str,
                         "</s:Body>"
                         "</s:Envelope>");

        soup_message_set_request (action->msg,
                                  "text/xml; charset=\"utf-8\"",
                                  SOUP_MEMORY_TAKE,
                                  action->msg_str->str,
                                  action->msg_str->len);

        g_string_free (action->msg_str, FALSE);

        /* We need to keep our own reference to the message as well,
         * in order for send_action() to work. */
        g_object_ref (action->msg);

        /* Send the message */
        context = gupnp_service_info_get_context
                                (GUPNP_SERVICE_INFO (action->proxy));
        session = gupnp_context_get_session (context);

        soup_session_queue_message (session,
                                    action->msg,
                                    (SoupSessionCallback) action_got_response,
                                    action);
}

/* Writes a parameter name and GValue pair to @msg */
static void
write_in_parameter (const char *arg_name,
                    GValue     *value,
                    GString    *msg_str)
{
        /* Write parameter pair */
        xml_util_start_element (msg_str, arg_name);
        gvalue_util_value_append_to_xml_string (value, msg_str);
        xml_util_end_element (msg_str, arg_name);
}

static gboolean
action_error_idle_cb (gpointer user_data)
{
        GUPnPServiceProxyAction *action = (GUPnPServiceProxyAction *) user_data;

        action->callback (action->proxy, action, action->user_data);

        return FALSE;
}

/**
 * gupnp_service_proxy_begin_action_valist:
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @callback: (scope async) : The callback to call when sending the action has succeeded
 * or failed
 * @user_data: User data for @callback
 * @var_args: A va_list of tuples of in parameter name, in parameter type, and
 * in parameter value
 *
 * See gupnp_service_proxy_begin_action().
 *
 * Returns: (transfer none): A #GUPnPServiceProxyAction handle. This will
 * be freed when calling gupnp_service_proxy_cancel_action() or
 * gupnp_service_proxy_end_action_valist().
 **/
GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action_valist
                                   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GUPnPServiceProxyActionCallback callback,
                                    gpointer                        user_data,
                                    va_list                         var_args)
{
        GUPnPServiceProxyAction *ret;
        GList *in_names = NULL, *in_values = NULL;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), NULL);
        g_return_val_if_fail (action, NULL);
        g_return_val_if_fail (callback, NULL);

        VAR_ARGS_TO_IN_LIST (var_args, in_names, in_values);
        ret = gupnp_service_proxy_begin_action_list (proxy,
                                                     action,
                                                     in_names,
                                                     in_values,
                                                     callback,
                                                     user_data);
        g_list_free_full (in_names, g_free);
        g_list_free_full (in_values, value_free);

        return ret;
}

/**
 * gupnp_service_proxy_begin_action_list:
 * @proxy: (transfer none): A #GUPnPServiceProxy
 * @action: An action
 * @in_names: (element-type utf8) (transfer none): #GList of 'in' parameter
 * names (as strings)
 * @in_values: (element-type GValue) (transfer none): #GList of values (as
 * #GValue) that line up with @in_names
 * @callback: (scope async) : The callback to call when sending the action has succeeded or
 * failed
 * @user_data: User data for @callback
 *
 * A variant of #gupnp_service_proxy_begin_action that takes lists of
 * in-parameter names, types and values.
 *
 * Return value: (transfer none): A #GUPnPServiceProxyAction handle. This will
 * be freed when calling gupnp_service_proxy_cancel_action() or
 * gupnp_service_proxy_end_action_list().
 *
 * Since: 0.13.3
 **/
GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action_list
                                   (GUPnPServiceProxy               *proxy,
                                    const char                      *action,
                                    GList                           *in_names,
                                    GList                           *in_values,
                                    GUPnPServiceProxyActionCallback  callback,
                                    gpointer                         user_data)
{
        GUPnPServiceProxyAction *ret;
        GList *names;
        GList *values;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), NULL);
        g_return_val_if_fail (action, NULL);
        g_return_val_if_fail (callback, NULL);
        g_return_val_if_fail (g_list_length (in_names) ==
                              g_list_length (in_values),
                              NULL);

        /* Create message */
        ret = begin_action_msg (proxy, action, callback, user_data);

        if (ret->error) {
                g_idle_add (action_error_idle_cb, ret);

                return ret;
        }

        /* Arguments */
        values = in_values;

        for (names = in_names; names; names=names->next) {
                GValue* val = values->data;

                write_in_parameter (names->data,
                                    val,
                                    ret->msg_str);

                values = values->next;
        }

        /* Finish and send off */
        finish_action_msg (ret, action);

        return ret;
}

/**
 * gupnp_service_proxy_begin_action_hash: (skip)
 * @proxy: A #GUPnPServiceProxy
 * @action: An action
 * @callback: (scope async): The callback to call when sending the action has succeeded
 * or failed
 * @user_data: User data for @callback
 * @hash: (element-type utf8 GValue): A #GHashTable of in parameter name and #GValue pairs
 *
 * See gupnp_service_proxy_begin_action(); this version takes a #GHashTable
 * for runtime generated parameter lists.
 *
 * Do not use this function in newly written code; it cannot guarantee the
 * order of arguments and thus breaks interaction with many devices.
 *
 * Return value: (transfer none): A #GUPnPServiceProxyAction handle. This will
 * be freed when calling gupnp_service_proxy_cancel_action() or
 * gupnp_service_proxy_end_action_hash().
 *
 * Deprecated: 0.20.9: Use gupnp_service_proxy_send_action() or
 * gupnp_service_proxy_send_action_list()
 *
 **/
GUPnPServiceProxyAction *
gupnp_service_proxy_begin_action_hash
                                   (GUPnPServiceProxy              *proxy,
                                    const char                     *action,
                                    GUPnPServiceProxyActionCallback callback,
                                    gpointer                        user_data,
                                    GHashTable                     *hash)
{
        GUPnPServiceProxyAction *ret;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), NULL);
        g_return_val_if_fail (action, NULL);
        g_return_val_if_fail (callback, NULL);

        /* Create message */
        ret = begin_action_msg (proxy, action, callback, user_data);

        if (ret->error) {
                g_idle_add (action_error_idle_cb, ret);

                return ret;
        }

        /* Arguments */
        g_hash_table_foreach (hash, (GHFunc) write_in_parameter, ret->msg_str);

        /* Finish and send off */
        finish_action_msg (ret, action);

        return ret;
}

/**
 * gupnp_service_proxy_end_action:
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 * @error: (allow-none): The location where to store any error, or %NULL
 * @...: tuples of out parameter name, out parameter type, and out parameter
 * value location, terminated with %NULL. The out parameter values should be
 * freed after use
 *
 * Retrieves the result of @action. The out parameters in @Varargs will be
 * filled in, and if an error occurred, @error will be set. In case of
 * an UPnP error the error code will be the same in @error.
 *
 * Return value: %TRUE on success.
 **/
gboolean
gupnp_service_proxy_end_action (GUPnPServiceProxy       *proxy,
                                GUPnPServiceProxyAction *action,
                                GError                 **error,
                                ...)
{
        va_list var_args;
        gboolean ret;

        va_start (var_args, error);
        ret = gupnp_service_proxy_end_action_valist (proxy,
                                                     action,
                                                     error,
                                                     var_args);
        va_end (var_args);

        return ret;
}

/* Checks an action response for errors and returns the parsed
 * xmlDoc object. */
static xmlDoc *
check_action_response (G_GNUC_UNUSED GUPnPServiceProxy *proxy,
                       GUPnPServiceProxyAction         *action,
                       xmlNode                        **params,
                       GError                         **error)
{
        xmlDoc *response;
        int code;

        /* Check for errors */
        switch (action->msg->status_code) {
        case SOUP_STATUS_OK:
        case SOUP_STATUS_INTERNAL_SERVER_ERROR:
                break;
        default:
                _gupnp_error_set_server_error (error, action->msg);

                return NULL;
        }

        /* Parse response */
        response = xmlRecoverMemory (action->msg->response_body->data,
                                     action->msg->response_body->length);

        if (!response) {
                if (action->msg->status_code == SOUP_STATUS_OK) {
                        g_set_error (error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                     "Could not parse SOAP response");
                } else {
                        g_set_error_literal
                                    (error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INTERNAL_SERVER_ERROR,
                                     action->msg->reason_phrase);
                }

                return NULL;
        }

        /* Get parameter list */
        *params = xml_util_get_element ((xmlNode *) response,
                                        "Envelope",
                                        NULL);
        if (*params != NULL)
                *params = xml_util_real_node ((*params)->children);

        if (*params != NULL) {
                if (strcmp ((const char *) (*params)->name, "Header") == 0)
                        *params = xml_util_real_node ((*params)->next);

                if (*params != NULL)
                        if (strcmp ((const char *) (*params)->name, "Body") != 0)
                                *params = NULL;
        }

        if (*params != NULL)
                *params = xml_util_real_node ((*params)->children);

        if (*params == NULL) {
                g_set_error (error,
                             GUPNP_SERVER_ERROR,
                             GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                             "Invalid Envelope");

                xmlFreeDoc (response);

                return NULL;
        }

        /* Check whether we have a Fault */
        if (action->msg->status_code == SOUP_STATUS_INTERNAL_SERVER_ERROR) {
                xmlNode *param;
                char *desc;

                param = xml_util_get_element (*params,
                                              "detail",
                                              "UPnPError",
                                              NULL);

                if (!param) {
                        g_set_error (error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                     "Invalid Fault");

                        xmlFreeDoc (response);

                        return NULL;
                }

                /* Code */
                code = xml_util_get_child_element_content_int
                                        (param, "errorCode");
                if (code == -1) {
                        g_set_error (error,
                                     GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_RESPONSE,
                                     "Invalid Fault");

                        xmlFreeDoc (response);

                        return NULL;
                }

                /* Description */
                desc = xml_util_get_child_element_content_glib
                                        (param, "errorDescription");
                if (desc == NULL)
                        desc = g_strdup (action->msg->reason_phrase);

                g_set_error_literal (error,
                                     GUPNP_CONTROL_ERROR,
                                     code,
                                     desc);

                g_free (desc);

                xmlFreeDoc (response);

                return NULL;
        }

        return response;
}

/* Reads a value into the parameter name and initialised GValue pair
 * from @response */
static void
read_out_parameter (const char *arg_name,
                    GValue     *value,
                    xmlNode    *params)
{
        xmlNode *param;

        /* Try to find a matching parameter in the response*/
        param = xml_util_get_element (params,
                                      arg_name,
                                      NULL);
        if (!param) {
                g_warning ("Could not find variable \"%s\" in response",
                           arg_name);

                return;
        }

        gvalue_util_set_value_from_xml_node (value, param);
}

/**
 * gupnp_service_proxy_end_action_valist:
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 * @error: (allow-none): The location where to store any error, or %NULL
 * @var_args: A va_list of tuples of out parameter name, out parameter type,
 * and out parameter value location. The out parameter values should be
 * freed after use
 *
 * See gupnp_service_proxy_end_action().
 *
 * Return value: %TRUE on success.
 **/
gboolean
gupnp_service_proxy_end_action_valist (GUPnPServiceProxy       *proxy,
                                       GUPnPServiceProxyAction *action,
                                       GError                 **error,
                                       va_list                  var_args)
{
        GHashTable *out_hash;
        va_list var_args_copy;
        gboolean result;
        GError *local_error;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);
        g_return_val_if_fail (proxy == action->proxy, FALSE);

        out_hash = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          value_free);
        G_VA_COPY (var_args_copy, var_args);
        VAR_ARGS_TO_OUT_HASH_TABLE (var_args, out_hash);
        local_error = NULL;
        result = gupnp_service_proxy_end_action_hash (proxy,
                                                      action,
                                                      &local_error,
                                                      out_hash);

        if (local_error == NULL) {
                OUT_HASH_TABLE_TO_VAR_ARGS (out_hash, var_args_copy);
        } else {
                g_propagate_error (error, local_error);
        }
        va_end (var_args_copy);
        g_hash_table_unref (out_hash);

        return result;
}

/**
 * gupnp_service_proxy_end_action_list_gi: (rename-to gupnp_service_proxy_end_action_list)
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 * @error: The location where to store any error, or %NULL
 * @out_names: (element-type utf8) (transfer none): #GList of 'out' parameter
 * names (as strings)
 * @out_types: (element-type GType) (transfer none): #GList of types (as #GType)
 * that line up with @out_names
 * @out_values: (element-type GValue) (transfer full) (out): #GList of values
 * (as #GValue) that line up with @out_names and @out_types.
 *
 * A variant of #gupnp_service_proxy_end_action that takes lists of
 * out-parameter names, types and place-holders for values. The returned list
 * in @out_values must be freed using #g_list_free and each element in it using
 * #g_value_unset and #g_slice_free.
 *
 * Return value : %TRUE on success.
 *
 **/
gboolean
gupnp_service_proxy_end_action_list_gi (GUPnPServiceProxy       *proxy,
                                        GUPnPServiceProxyAction *action,
                                        GList                   *out_names,
                                        GList                   *out_types,
                                        GList                  **out_values,
                                        GError                 **error)
{
    return gupnp_service_proxy_end_action_list (proxy, action, error, out_names, out_types, out_values);
}

/**
 * gupnp_service_proxy_end_action_list: (skip)
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 * @error: (allow-none): The location where to store any error, or %NULL
 * @out_names: (element-type utf8) (transfer none): #GList of 'out' parameter
 * names (as strings)
 * @out_types: (element-type GType) (transfer none): #GList of types (as #GType)
 * that line up with @out_names
 * @out_values: (element-type GValue) (transfer full) (out): #GList of values
 * (as #GValue) that line up with @out_names and @out_types.
 *
 * A variant of #gupnp_service_proxy_end_action that takes lists of
 * out-parameter names, types and place-holders for values. The returned list
 * in @out_values must be freed using #g_list_free and each element in it using
 * #g_value_unset and #g_slice_free.
 *
 * Returns: %TRUE on success.
 *
 **/
gboolean
gupnp_service_proxy_end_action_list (GUPnPServiceProxy       *proxy,
                                     GUPnPServiceProxyAction *action,
                                     GError                 **error,
                                     GList                   *out_names,
                                     GList                   *out_types,
                                     GList                  **out_values)
{
        xmlDoc *response;
        xmlNode *params;
        GList *names;
        GList *types;
        GList *out_values_list;

        out_values_list = NULL;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);
        g_return_val_if_fail (proxy == action->proxy, FALSE);

        /* Check for saved error from begin_action() */
        if (action->error) {
                g_propagate_error (error, action->error);
                gupnp_service_proxy_action_unref (action);

                return FALSE;
        }

        /* Check response for errors and do initial parsing */
        response = check_action_response (proxy, action, &params, error);
        if (response == NULL) {
                gupnp_service_proxy_action_unref (action);

                return FALSE;
        }

        /* Read arguments */
        types = out_types;
        for (names = out_names; names; names=names->next) {
                GValue *val;

                val = g_slice_new0 (GValue);
                g_value_init (val, (GType) types->data);

                read_out_parameter (names->data, val, params);

                out_values_list = g_list_append (out_values_list, val);

                types = types->next;
        }

        *out_values = out_values_list;

        /* Cleanup */
        gupnp_service_proxy_action_unref (action);

        xmlFreeDoc (response);

        return TRUE;
}

/**
 * gupnp_service_proxy_end_action_hash_gi: (rename-to gupnp_service_proxy_end_action_hash)
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 * @error: The location where to store any error, or %NULL
 * @hash: (element-type utf8 GValue) (inout) (transfer none): A #GHashTable of
 * out parameter name and initialised #GValue pairs
 *
 * See gupnp_service_proxy_end_action(); this version takes a #GHashTable for
 * runtime generated parameter lists.
 *
 * Return value: %TRUE on success.
 *
 **/
gboolean
gupnp_service_proxy_end_action_hash_gi (GUPnPServiceProxy       *proxy,
                                        GUPnPServiceProxyAction *action,
                                        GHashTable              *hash,
                                        GError                 **error)
{
    return gupnp_service_proxy_end_action_hash (proxy, action, error, hash);
}

/**
 * gupnp_service_proxy_end_action_hash:
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 * @error: The location where to store any error, or %NULL
 * @hash: (element-type utf8 GValue) (out caller-allocates) (transfer none): A #GHashTable of
 * out parameter name and initialised #GValue pairs
 *
 * See gupnp_service_proxy_end_action(); this version takes a #GHashTable for
 * runtime generated parameter lists.
 *
 * Return value: %TRUE on success.
 **/
gboolean
gupnp_service_proxy_end_action_hash (GUPnPServiceProxy       *proxy,
                                     GUPnPServiceProxyAction *action,
                                     GError                 **error,
                                     GHashTable              *hash)
{
        xmlDoc *response;
        xmlNode *params;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (action, FALSE);
        g_return_val_if_fail (proxy == action->proxy, FALSE);

        /* Check for saved error from begin_action() */
        if (action->error) {
                g_propagate_error (error, action->error);
                gupnp_service_proxy_action_unref (action);

                return FALSE;
        }

        /* Check response for errors and do initial parsing */
        response = check_action_response (proxy, action, &params, error);
        if (response == NULL) {
                gupnp_service_proxy_action_unref (action);

                return FALSE;
        }

        /* Read arguments */
        g_hash_table_foreach (hash, (GHFunc) read_out_parameter, params);

        /* Cleanup */
        gupnp_service_proxy_action_unref (action);

        xmlFreeDoc (response);

        return TRUE;
}

/**
 * gupnp_service_proxy_cancel_action:
 * @proxy: A #GUPnPServiceProxy
 * @action: A #GUPnPServiceProxyAction handle
 *
 * Cancels @action, freeing the @action handle.
 **/
void
gupnp_service_proxy_cancel_action (GUPnPServiceProxy       *proxy,
                                   GUPnPServiceProxyAction *action)
{
        GUPnPContext *context;
        SoupSession *session;

        g_return_if_fail (GUPNP_IS_SERVICE_PROXY (proxy));
        g_return_if_fail (action);
        g_return_if_fail (proxy == action->proxy);

        if (action->msg != NULL) {
                context = gupnp_service_info_get_context
                                        (GUPNP_SERVICE_INFO (proxy));
                session = gupnp_context_get_session (context);

                soup_session_cancel_message (session,
                                             action->msg,
                                             SOUP_STATUS_CANCELLED);
        }

        if (action->error != NULL)
                g_error_free (action->error);

        gupnp_service_proxy_action_unref (action);
}

/**
 * gupnp_service_proxy_add_notify: (skip)
 * @proxy: A #GUPnPServiceProxy
 * @variable: The variable to add notification for
 * @type: The type of the variable
 * @callback: (scope async): The callback to call when @variable changes
 * @user_data: User data for @callback
 *
 * Sets up @callback to be called whenever a change notification for
 * @variable is recieved.
 *
 * Return value: %TRUE on success.
 **/
gboolean
gupnp_service_proxy_add_notify (GUPnPServiceProxy              *proxy,
                                const char                     *variable,
                                GType                           type,
                                GUPnPServiceProxyNotifyCallback callback,
                                gpointer                        user_data)

{
        return gupnp_service_proxy_add_notify_full (proxy,
                                                    variable,
                                                    type,
                                                    callback,
                                                    user_data,
                                                    NULL);
}

/**
 * gupnp_service_proxy_add_notify_full: (rename-to gupnp_service_proxy_add_notify)
 * @proxy: A #GUPnPServiceProxy
 * @variable: The variable to add notification for
 * @type: The type of the variable
 * @callback: (scope notified): The callback to call when @variable changes
 * @user_data: User data for @callback
 * @notify: (allow-none): Function to call when the notification is removed, or %NULL
 *
 * Sets up @callback to be called whenever a change notification for
 * @variable is recieved.
 *
 * Since: 0.20.12
 *
 * Return value: %TRUE on success.
 **/
gboolean
gupnp_service_proxy_add_notify_full (GUPnPServiceProxy              *proxy,
                                     const char                     *variable,
                                     GType                           type,
                                     GUPnPServiceProxyNotifyCallback callback,
                                     gpointer                        user_data,
                                     GDestroyNotify                  notify)
{
        NotifyData *data;
        CallbackData *callback_data;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (variable, FALSE);
        g_return_val_if_fail (type, FALSE);
        g_return_val_if_fail (callback, FALSE);

        /* See if we already have notifications set up for this variable */
        data = g_hash_table_lookup (proxy->priv->notify_hash, variable);
        if (data == NULL) {
                /* No, create one */
                data = g_slice_new (NotifyData);

                data->type       = type;
                data->callbacks  = NULL;
                data->next_emit   = NULL;

                g_hash_table_insert (proxy->priv->notify_hash,
                                     g_strdup (variable),
                                     data);
        } else {
                /* Yes, check that everything is sane .. */
                if (data->type != type) {
                        g_warning ("A notification already exists for %s, but "
                                   "has type %s, not %s.",
                                   variable,
                                   g_type_name (data->type),
                                   g_type_name (type));

                        return FALSE;
                }
        }

        /* Append callback */
        callback_data = g_slice_new (CallbackData);

        callback_data->callback  = callback;
        callback_data->user_data = user_data;
        callback_data->notify    = notify;

        data->callbacks = g_list_append (data->callbacks, callback_data);

        if (data->next_emit == NULL)
                data->next_emit = g_list_last (data->callbacks);

        return TRUE;
}

/**
 * gupnp_service_proxy_add_raw_notify:
 * @proxy: A #GUPnPServiceProxy
 * @callback: (scope notified): The callback to call when the peer issues any
 * variable notification.
 * @user_data: User data for @callback
 * @notify: (allow-none): A #GDestroyNotify for @user_data
 *
 * Get a notification for anything that happens on the peer. @value in
 * @callback will be of type #G_TYPE_POINTER and contain the pre-parsed
 * #xmlDoc. Do NOT free or modify this document.
 *
 * Return value: %TRUE on success.
 *
 * Since: 0.20.12
 **/
gboolean
gupnp_service_proxy_add_raw_notify (GUPnPServiceProxy              *proxy,
                                    GUPnPServiceProxyNotifyCallback callback,
                                    gpointer                        user_data,
                                    GDestroyNotify                  notify)
{
        return gupnp_service_proxy_add_notify_full (proxy,
                                                    "*",
                                                    G_TYPE_NONE,
                                                    callback,
                                                    user_data,
                                                    notify);
}


/**
 * gupnp_service_proxy_remove_notify:
 * @proxy: A #GUPnPServiceProxy
 * @variable: The variable to add notification for
 * @callback: (scope call): The callback to call when @variable changes
 * @user_data: User data for @callback
 *
 * Cancels the variable change notification for @callback and @user_data.
 *
 * Up to version 0.20.9 this function must not be called directlya or
 * indirectly from a #GUPnPServiceProxyNotifyCallback associated with this
 * service proxy, even if it is for another variable. In later versions such
 * calls are allowed.
 *
 * Return value: %TRUE on success.
 **/
gboolean
gupnp_service_proxy_remove_notify (GUPnPServiceProxy              *proxy,
                                   const char                     *variable,
                                   GUPnPServiceProxyNotifyCallback callback,
                                   gpointer                        user_data)
{
        NotifyData *data;
        gboolean found;
        GList *l;

        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);
        g_return_val_if_fail (variable, FALSE);
        g_return_val_if_fail (callback, FALSE);

        /* Look up NotifyData for variable */
        data = g_hash_table_lookup (proxy->priv->notify_hash, variable);
        if (data == NULL) {
                g_warning ("No notifications found for variable %s",
                           variable);

                return FALSE;
        }

        /* Look up our callback-user_data pair */
        found = FALSE;

        for (l = data->callbacks; l; l = l->next) {
                CallbackData *callback_data;

                callback_data = l->data;

                if (callback_data->callback  == callback &&
                    callback_data->user_data == user_data) {

                        /* Gotcha! */
                        callback_data_free (callback_data);

                        if (data->next_emit == l)
                                data->next_emit = data->next_emit->next;

                        data->callbacks =
                                g_list_delete_link (data->callbacks, l);
                        if (data->callbacks == NULL) {
                                /* No callbacks left: Remove from hash */
                                g_hash_table_remove (proxy->priv->notify_hash,
                                                     variable);
                        }

                        found = TRUE;

                        break;
                }
        }

        if (found == FALSE)
                g_warning ("No such callback-user_data pair was found");

        return found;
}

/**
 * gupnp_service_proxy_remove_raw_notify:
 * @proxy: A #GUPnPServiceProxy
 * @callback: (scope call): The callback to call when @variable changes
 * @user_data: User data for @callback
 *
 * Cancels the variable change notification for @callback and @user_data.
 *
 * This function must not be called directly or indirectly from a
 * #GUPnPServiceProxyNotifyCallback associated with this service proxy, even
 * if it is for another variable.
 *
 * Return value: %TRUE on success.
 **/
gboolean
gupnp_service_proxy_remove_raw_notify (GUPnPServiceProxy              *proxy,
                                       GUPnPServiceProxyNotifyCallback callback,
                                       gpointer                        user_data)
{
        return gupnp_service_proxy_remove_notify (proxy,
                                                  "*",
                                                  callback,
                                                  user_data);
}

static void
emit_notification (GUPnPServiceProxy *proxy,
                   xmlNode           *var_node)
{
        NotifyData *data;
        GValue value = {0, };
        GList *l;

        data = g_hash_table_lookup (proxy->priv->notify_hash, var_node->name);
        if (data == NULL)
                return;

        /* Make a GValue of the desired type */
        g_value_init (&value, data->type);

        if (!gvalue_util_set_value_from_xml_node (&value, var_node)) {
                g_value_unset (&value);

                return;
        }

        /* Call callbacks. Note that data->next_emit may change if
         * callback calls remove_notify() or add_notify() */
        for (l = data->callbacks; l; l = data->next_emit) {
                CallbackData *callback_data;

                callback_data = l->data;
                data->next_emit = l->next;

                callback_data->callback (proxy,
                                         (const char *) var_node->name,
                                         &value,
                                         callback_data->user_data);
        }

        /* Cleanup */
        g_value_unset (&value);
}

static void
emit_notifications_for_doc (GUPnPServiceProxy *proxy,
                            xmlDoc            *doc)
{
        NotifyData *data = NULL;
        xmlNode *node;

        node = xmlDocGetRootElement (doc);

        /* Iterate over all provided properties */
        for (node = node->children; node; node = node->next) {
                xmlNode *var_node;

                /* Although according to the UPnP specs, there should be only
                 * one variable node inside a 'property' node, we still need to
                 * entertain the possibility of multiple variables inside it to
                 * be compatible with implementations using older GUPnP.
                 */
                for (var_node = node->children;
                     var_node;
                     var_node = var_node->next) {
                        if (strcmp ((char *) node->name, "property") == 0)
                                emit_notification (proxy, var_node);
                }
        }

        data = g_hash_table_lookup (proxy->priv->notify_hash, "*");
        if (data != NULL) {
                GValue value = {0, };
                GList *it = NULL;

                g_value_init (&value, G_TYPE_POINTER);
                g_value_set_pointer (&value, doc);

                for (it = data->callbacks; it != NULL; it = it->next) {
                        CallbackData *callback_data = it->data;

                        callback_data->callback (proxy,
                                                 "*",
                                                 &value,
                                                 callback_data->user_data);
                }

                g_value_unset (&value);
        }
}

/* Emit pending notifications. See comment below on why we do this. */
static gboolean
emit_notifications (gpointer user_data)
{
        GUPnPServiceProxy *proxy = user_data;
        GList *pending_notify;
        gboolean resubscribe = FALSE;

        g_assert (user_data);

        if (proxy->priv->sid == NULL)
                /* No SID */
                if (G_LIKELY (proxy->priv->subscribed))
                        /* subscription in progress, delay emision! */
                        return TRUE;

        for (pending_notify = proxy->priv->pending_notifies;
             pending_notify != NULL;
             pending_notify = pending_notify->next) {
                EmitNotifyData *emit_notify_data;

                emit_notify_data = pending_notify->data;

                if (emit_notify_data->seq > proxy->priv->seq) {
                        /* Error procedure on missed event according to
                         * UDA 1.0, section 4.2, §5:
                         * Re-subscribe to get a new SID and SEQ */
                        resubscribe = TRUE;

                        break;
                }

                /* Increment our own event sequence number */
                /* UDA 1.0, section 4.2, §3: To prevent overflow, SEQ is set to
                 * 1, NOT 0, when encountering G_MAXUINT32. SEQ == 0 always
                 * indicates the initial event message. */
                if (proxy->priv->seq < G_MAXUINT32)
                        proxy->priv->seq++;
                else
                        proxy->priv->seq = 1;

                if (G_LIKELY (proxy->priv->sid != NULL &&
                              strcmp (emit_notify_data->sid,
                                      proxy->priv->sid) == 0))
                        /* Our SID, entertain! */
                        emit_notifications_for_doc (proxy,
                                                    emit_notify_data->doc);
        }

        /* Cleanup */
        g_list_free_full (proxy->priv->pending_notifies,
                          (GDestroyNotify) emit_notify_data_free);
        proxy->priv->pending_notifies = NULL;

        proxy->priv->notify_idle_src = NULL;

        if (resubscribe) {
                unsubscribe (proxy);
                subscribe (proxy);
        }

        return FALSE;
}

/*
 * HTTP server received a message. Handle, if this was a NOTIFY
 * message with our SID.
 */
static void
server_handler (G_GNUC_UNUSED SoupServer        *soup_server,
                SoupMessage                     *msg,
                G_GNUC_UNUSED const char        *server_path,
                G_GNUC_UNUSED GHashTable        *query,
                G_GNUC_UNUSED SoupClientContext *soup_client,
                gpointer                         user_data)
{
        GUPnPServiceProxy *proxy;
        const char *hdr, *nt, *nts;
        guint32 seq;
        guint64 seq_parsed;
        xmlDoc *doc;
        xmlNode *node;
        EmitNotifyData *emit_notify_data;

        proxy = GUPNP_SERVICE_PROXY (user_data);

        if (strcmp (msg->method, GENA_METHOD_NOTIFY) != 0) {
                /* We don't implement this method */
                soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);

                return;
        }

        nt = soup_message_headers_get_one (msg->request_headers, "NT");
        nts = soup_message_headers_get_one (msg->request_headers, "NTS");
        if (nt == NULL || nts == NULL) {
                /* Required header is missing */
                soup_message_set_status (msg, SOUP_STATUS_BAD_REQUEST);

                return;
        }

        if (strcmp (nt, "upnp:event") != 0 ||
            strcmp (nts, "upnp:propchange") != 0) {
                /* Unexpected header content */
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        hdr = soup_message_headers_get_one (msg->request_headers, "SEQ");
        if (hdr == NULL) {
                /* No SEQ header */
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        errno = 0;
        seq_parsed = strtoul (hdr, NULL, 10);
        if (errno != 0 || seq_parsed > G_MAXUINT32) {
                /* Invalid SEQ header value */
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        seq = (guint32) seq_parsed;

        hdr = soup_message_headers_get_one (msg->request_headers, "SID");
        if (hdr == NULL ||
            strlen (hdr) <= strlen ("uuid:") ||
            strncmp (hdr, "uuid:", strlen ("uuid:")) != 0) {
                /* No SID */
                soup_message_set_status (msg, SOUP_STATUS_PRECONDITION_FAILED);

                return;
        }

        /* Parse the actual XML message content */
        doc = xmlRecoverMemory (msg->request_body->data,
                                msg->request_body->length);
        if (doc == NULL) {
                /* Failed */
                g_warning ("Failed to parse NOTIFY message body");

                soup_message_set_status (msg,
                                         SOUP_STATUS_INTERNAL_SERVER_ERROR);

                return;
        }

        /* Get root propertyset element */
        node = xmlDocGetRootElement (doc);
        if (node == NULL || strcmp ((char *) node->name, "propertyset")) {
                /* Empty or unsupported */
                xmlFreeDoc (doc);

                soup_message_set_status (msg, SOUP_STATUS_OK);

                return;
        }

        /*
         * Some UPnP stacks (hello, myigd/1.0) block when sending a NOTIFY, so
         * call the callbacks in an idle handler so that if the client calls the
         * device in the notify callback the server can actually respond.
         */
        emit_notify_data = emit_notify_data_new (hdr, seq, doc);

        proxy->priv->pending_notifies =
                g_list_append (proxy->priv->pending_notifies, emit_notify_data);
        if (!proxy->priv->notify_idle_src) {
                proxy->priv->notify_idle_src = g_idle_source_new();
                g_source_set_callback (proxy->priv->notify_idle_src,
                                       emit_notifications,
                                       proxy, NULL);
                g_source_attach (proxy->priv->notify_idle_src,
                                 g_main_context_get_thread_default ());

                g_source_unref (proxy->priv->notify_idle_src);
        }

        /* Everything went OK */
        soup_message_set_status (msg, SOUP_STATUS_OK);
}

/*
 * Generates a timeout header for the subscription timeout specified
 * in our GUPnPContext.
 */
static char *
make_timeout_header (GUPnPContext *context)
{
        guint timeout;

        timeout = gupnp_context_get_subscription_timeout (context);
        if (timeout > 0)
                return g_strdup_printf ("Second-%d", timeout);
        else
                return g_strdup ("infinite");
}

/*
 * Subscription expired.
 */
static gboolean
subscription_expire (gpointer user_data)
{
        GUPnPServiceProxy *proxy;
        GUPnPContext *context;
        SoupMessage *msg;
        SoupSession *session;
        char *sub_url, *timeout;

        proxy = GUPNP_SERVICE_PROXY (user_data);

        /* Reset timeout ID */
        proxy->priv->subscription_timeout_src = NULL;

        /* Send renewal message */
        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));

        /* Create subscription message */
        sub_url = gupnp_service_info_get_event_subscription_url
                                                (GUPNP_SERVICE_INFO (proxy));

        msg = soup_message_new (GENA_METHOD_SUBSCRIBE, sub_url);

        g_free (sub_url);

        g_return_val_if_fail (msg != NULL, FALSE);

        /* Add headers */
        soup_message_headers_append (msg->request_headers,
                                    "SID",
                                    proxy->priv->sid);

        timeout = make_timeout_header (context);
        soup_message_headers_append (msg->request_headers,
                                     "Timeout",
                                     timeout);
        g_free (timeout);

        /* And send it off */
        proxy->priv->pending_messages =
                g_list_prepend (proxy->priv->pending_messages, msg);

        session = gupnp_context_get_session (context);

        soup_session_queue_message (session,
                                    msg,
                                    (SoupSessionCallback)
                                        subscribe_got_response,
                                    proxy);

        return FALSE;
}

/*
 * Received subscription response.
 */
static void
subscribe_got_response (G_GNUC_UNUSED SoupSession *session,
                        SoupMessage               *msg,
                        GUPnPServiceProxy         *proxy)
{
        GError *error;

        /* Cancelled? */
        if (msg->status_code == SOUP_STATUS_CANCELLED)
                return;

        /* Remove from pending messages list */
        proxy->priv->pending_messages =
                g_list_remove (proxy->priv->pending_messages, msg);

        /* Check whether the subscription is still wanted */
        if (!proxy->priv->subscribed)
                return;

        /* Reset SID */
        g_free (proxy->priv->sid);
        proxy->priv->sid = NULL;

        /* Check message status */
        if (SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
                /* Success. */
                const char *hdr;
                int timeout;

                /* Save SID. */
                hdr = soup_message_headers_get_one (msg->response_headers,
                                                    "SID");
                if (hdr == NULL) {
                        error = g_error_new
                                        (GUPNP_EVENTING_ERROR,
                                         GUPNP_EVENTING_ERROR_SUBSCRIPTION_LOST,
                                         "No SID in SUBSCRIBE response");

                        goto hdr_err;
                }

                proxy->priv->sid = g_strdup (hdr);

                /* Figure out when the subscription times out */
                hdr = soup_message_headers_get_one (msg->response_headers,
                                                    "Timeout");
                if (hdr == NULL) {
                        g_warning ("No Timeout in SUBSCRIBE response.");

                        return;
                }

                if (strncmp (hdr, "Second-", strlen ("Second-")) == 0) {
                        /* We have a finite timeout */
                        timeout = atoi (hdr + strlen ("Second-"));

                        if (timeout < 0) {
                                g_warning ("Invalid time-out specified. "
                                           "Assuming default value of %d.",
                                           GENA_DEFAULT_TIMEOUT);

                                timeout = GENA_DEFAULT_TIMEOUT;
                        }

                        /* We want to resubscribe before the subscription
                         * expires. */
                        timeout = g_random_int_range (1, timeout / 2);

                        /* Add actual timeout */
                        proxy->priv->subscription_timeout_src =
                                g_timeout_source_new_seconds (timeout);
                        g_source_set_callback
                                (proxy->priv->subscription_timeout_src,
                                 subscription_expire,
                                 proxy, NULL);
                        g_source_attach (proxy->priv->subscription_timeout_src,
                                         g_main_context_get_thread_default ());

                        g_source_unref (proxy->priv->subscription_timeout_src);
                }
        } else {
                GUPnPContext *context;
                SoupServer *server;

                /* Subscription failed. */
                error = g_error_new_literal
                                (GUPNP_EVENTING_ERROR,
                                 GUPNP_EVENTING_ERROR_SUBSCRIPTION_FAILED,
                                 msg->reason_phrase);

hdr_err:
                /* Remove listener */
                context = gupnp_service_info_get_context
                                        (GUPNP_SERVICE_INFO (proxy));

                server = gupnp_context_get_server (context);
                soup_server_remove_handler (server, proxy->priv->path);

                proxy->priv->subscribed = FALSE;

                g_object_notify (G_OBJECT (proxy), "subscribed");

                /* Emit subscription-lost */
                g_signal_emit (proxy,
                               signals[SUBSCRIPTION_LOST],
                               0,
                               error);

                g_error_free (error);
        }
}

/*
 * Subscribe to this service.
 */
static void
subscribe (GUPnPServiceProxy *proxy)
{
        GUPnPContext *context;
        SoupMessage *msg;
        SoupSession *session;
        SoupServer *server;
        SoupURI *uri;
        char *uri_string;
        char *sub_url, *delivery_url, *timeout;

        /* Remove subscription timeout */
        if (proxy->priv->subscription_timeout_src) {
                g_source_destroy (proxy->priv->subscription_timeout_src);
                proxy->priv->subscription_timeout_src = NULL;
        }

	context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));

        /* Create subscription message */
        sub_url = gupnp_service_info_get_event_subscription_url
                                                (GUPNP_SERVICE_INFO (proxy));

        msg = NULL;
        if (sub_url != NULL) {
                msg = soup_message_new (GENA_METHOD_SUBSCRIBE, sub_url);

                g_free (sub_url);
        }

        if (msg == NULL) {
                GError *error;

                /* Subscription failed. */
                proxy->priv->subscribed = FALSE;

                g_object_notify (G_OBJECT (proxy), "subscribed");

                /* Emit subscription-lost */
                error = g_error_new (GUPNP_SERVER_ERROR,
                                     GUPNP_SERVER_ERROR_INVALID_URL,
                                     "No valid subscription URL defined");

                g_signal_emit (proxy,
                               signals[SUBSCRIPTION_LOST],
                               0,
                               error);

                g_error_free (error);

                return;
        }

        /* Add headers */
        uri = _gupnp_context_get_server_uri (context);
        soup_uri_set_path (uri, proxy->priv->path);
        uri_string = soup_uri_to_string (uri, FALSE);
        soup_uri_free (uri);
        delivery_url = g_strdup_printf ("<%s>", uri_string);
        g_free (uri_string);

        soup_message_headers_append (msg->request_headers,
                                     "Callback",
                                     delivery_url);
        g_free (delivery_url);

        soup_message_headers_append (msg->request_headers,
                                     "NT",
                                     "upnp:event");

        timeout = make_timeout_header (context);
        soup_message_headers_append (msg->request_headers,
                                     "Timeout",
                                     timeout);
        g_free (timeout);

        /* Listen for events */
        server = gupnp_context_get_server (context);

        soup_server_add_handler (server,
                                 proxy->priv->path,
                                 server_handler,
                                 proxy,
                                 NULL);

        /* And send our subscription message off */
        proxy->priv->pending_messages =
                g_list_prepend (proxy->priv->pending_messages, msg);

        session = gupnp_context_get_session (context);

        soup_session_queue_message (session,
                                    msg,
                                    (SoupSessionCallback)
                                        subscribe_got_response,
                                    proxy);
}

/*
 * Unsubscribe from this service.
 */
static void
unsubscribe (GUPnPServiceProxy *proxy)
{
        GUPnPContext *context;
        SoupSession *session;
        SoupServer *server;

        context = gupnp_service_info_get_context (GUPNP_SERVICE_INFO (proxy));

        /* Remove server handler */
        server = gupnp_context_get_server (context);
        soup_server_remove_handler (server, proxy->priv->path);

        if (proxy->priv->sid != NULL) {
                SoupMessage *msg;
                char *sub_url;

                /* Create unsubscription message */
                sub_url = gupnp_service_info_get_event_subscription_url
                                                   (GUPNP_SERVICE_INFO (proxy));

                msg = soup_message_new (GENA_METHOD_UNSUBSCRIBE, sub_url);

                g_free (sub_url);

                if (msg != NULL) {
                        /* Add headers */
                        soup_message_headers_append (msg->request_headers,
                                                     "SID",
                                                     proxy->priv->sid);

                        /* And queue it */
                        session = gupnp_context_get_session (context);

                        soup_session_queue_message (session, msg, NULL, NULL);
                }

                /* Reset SID */
                g_free (proxy->priv->sid);
                proxy->priv->sid = NULL;

                /* Reset sequence number */
                proxy->priv->seq = 0;
        }

        /* Remove subscription timeout */
        if (proxy->priv->subscription_timeout_src) {
                g_source_destroy (proxy->priv->subscription_timeout_src);
                proxy->priv->subscription_timeout_src = NULL;
        }
}

/**
 * gupnp_service_proxy_set_subscribed:
 * @proxy: A #GUPnPServiceProxy
 * @subscribed: %TRUE to subscribe to this service
 *
 * (Un)subscribes to this service.
 *
 * Note that the relevant messages are not immediately sent but queued.
 * If you want to unsubcribe from this service because the application
 * is quitting, rely on automatic synchronised unsubscription on object
 * destruction instead.
 **/
void
gupnp_service_proxy_set_subscribed (GUPnPServiceProxy *proxy,
                                    gboolean           subscribed)
{
        g_return_if_fail (GUPNP_IS_SERVICE_PROXY (proxy));

        if (proxy->priv->subscribed == subscribed)
                return;

        proxy->priv->subscribed = subscribed;

        if (subscribed)
                subscribe (proxy);
        else
                unsubscribe (proxy);

        g_object_notify (G_OBJECT (proxy), "subscribed");
}

/**
 * gupnp_service_proxy_get_subscribed:
 * @proxy: A #GUPnPServiceProxy
 *
 * Returns if we are subscribed to this service.
 *
 * Return value: %TRUE if we are subscribed to this service, otherwise %FALSE.
 **/
gboolean
gupnp_service_proxy_get_subscribed (GUPnPServiceProxy *proxy)
{
        g_return_val_if_fail (GUPNP_IS_SERVICE_PROXY (proxy), FALSE);

        return proxy->priv->subscribed;
}
