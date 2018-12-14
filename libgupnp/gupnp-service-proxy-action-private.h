#ifndef GUPNP_SERVICE_PROXY_ACTION_H
#define GUPNP_SERVICE_PROXY_ACTION_H

G_BEGIN_DECLS

struct _GUPnPServiceProxyAction {
        volatile gint ref_count;
        GUPnPServiceProxy *proxy;
        char *action_name;

        SoupMessage *msg;
        GString *msg_str;

        GUPnPServiceProxyActionCallback callback;
        gpointer user_data;

        GError *error;    /* If non-NULL, description of error that
                             occurred when preparing message */
};

G_GNUC_INTERNAL GUPnPServiceProxyAction *
gupnp_service_proxy_action_new (const char *action);

G_GNUC_INTERNAL GUPnPServiceProxyAction *
gupnp_service_proxy_action_ref (GUPnPServiceProxyAction *action);

G_GNUC_INTERNAL void
gupnp_service_proxy_action_unref (GUPnPServiceProxyAction *action);


G_END_DECLS

#endif /* GUPNP_SERVICE_PROXY_ACTION_H */
