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

struct _GUPnPServiceProxyAction {
        GUPnPServiceProxy *proxy;
        char *name;
        gint header_pos;

        SoupMessage *msg;
        GString *msg_str;

        GUPnPServiceProxyActionCallback callback;
        gpointer user_data;

        GError *error;    /* If non-NULL, description of error that
                             occurred when preparing message */
};

G_GNUC_INTERNAL GUPnPServiceProxyAction *
gupnp_service_proxy_action_new_internal (const char *action);

G_GNUC_INTERNAL GUPnPServiceProxyAction *
gupnp_service_proxy_action_ref (GUPnPServiceProxyAction *action);

G_GNUC_INTERNAL void
gupnp_service_proxy_action_unref (GUPnPServiceProxyAction *action);


G_END_DECLS

#endif /* GUPNP_SERVICE_PROXY_ACTION_H */
