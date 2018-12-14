/*
 * Copyright (C) 2018,2019 The GUPnP maintainers.
 *
 * Author: Jens Georg <mail@jensge.org>
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

#ifndef GUPNP_SERVICE_PROXY_ACTION_H
#define GUPNP_SERVICE_PROXY_ACTION_H

#include <gobject/gvaluecollector.h>

G_BEGIN_DECLS

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


struct _GUPnPServiceProxyAction {
        GUPnPServiceProxy *proxy;
        char *name;
        gint header_pos;

        SoupMessage *msg;
        GString *msg_str;

        GCancellable *cancellable;
        gulong cancellable_connection_id;

        GUPnPServiceProxyActionCallback callback;
        gpointer user_data;

        GError *error;    /* If non-NULL, description of error that
                             occurred when preparing message */
};

G_GNUC_INTERNAL GUPnPServiceProxyAction *
gupnp_service_proxy_action_new_internal (const char *action);

G_GNUC_INTERNAL gboolean
gupnp_service_proxy_action_get_result_valist (GUPnPServiceProxyAction *action,
                                              GError                 **error,
                                              va_list                  var_args);

G_END_DECLS

#endif /* GUPNP_SERVICE_PROXY_ACTION_H */
