/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author: Ludovic Ferrandis <ludovic.ferrandis@intel.com>
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
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
 * SECTION:gupnp-connman-manager
 * @short_description: Connman-based implementation of
 * #GUPnPContextManager.
 *
 */

#include "gupnp-connman-manager.h"
#include "gupnp-context.h"
#include "gupnp-marshal.h"

typedef enum
{
        CM_SERVICE_STATE_ACTIVE   = 1,
        CM_SERVICE_STATE_INACTIVE = 2

} CMServiceState;

typedef struct {
        GUPnPConnmanManager *manager;
        GUPnPContext        *context;
        GDBusProxy          *proxy;
        CMServiceState      current;
        guint               sig_prop_id;
        guint               port;
        gchar               *iface;
        gchar               *name;

} CMService;

struct _GUPnPConnmanManagerPrivate {
        GDBusProxy *manager_proxy;
        GSource    *idle_context_creation_src;
        GHashTable *cm_services;
        guint      sig_change_id;
        GDBusConnection *system_bus;
};

#define CM_DBUS_CONNMAN_NAME      "net.connman"
#define CM_DBUS_MANAGER_PATH      "/"
#define CM_DBUS_MANAGER_INTERFACE "net.connman.Manager"
#define CM_DBUS_SERVICE_INTERFACE "net.connman.Service"

#define DBUS_SERVICE_DBUS   "org.freedesktop.DBus"
#define DBUS_PATH_DBUS      "/org/freedesktop/DBus"
#define DBUS_INTERFACE_DBUS "org.freedesktop.DBus"

#define LOOPBACK_IFACE "lo"

G_DEFINE_TYPE (GUPnPConnmanManager,
               gupnp_connman_manager,
               GUPNP_TYPE_CONTEXT_MANAGER);

static gboolean
loopback_context_create (gpointer data)
{
        GUPnPConnmanManager *manager;
        GUPnPContext        *context;
        GError              *error = NULL;
        guint               port;

        manager = GUPNP_CONNMAN_MANAGER (data);
        manager->priv->idle_context_creation_src = NULL;

        g_object_get (manager, "port", &port, NULL);

        context = g_initable_new (GUPNP_TYPE_CONTEXT,
                                  NULL,
                                  &error,
                                  "interface", LOOPBACK_IFACE,
                                  "port", port,
                                  NULL);

        if (error != NULL) {
                g_warning ("Error creating GUPnP context: %s", error->message);
                g_error_free (error);

                return FALSE;
        }

        g_signal_emit_by_name (manager, "context-available", context);
        g_object_unref (context);

        return FALSE;
}

static gboolean
service_context_create (CMService *cm_service)
{
        GError  *error = NULL;

        cm_service->context = g_initable_new (GUPNP_TYPE_CONTEXT,
                                              NULL,
                                              &error,
                                              "interface", cm_service->iface,
                                              "network", cm_service->name,
                                              "port", cm_service->port,
                                              NULL);

        if (error != NULL) {
                g_warning ("Error creating GUPnP context: %s", error->message);
                g_error_free (error);

                return FALSE;
        }

        g_signal_emit_by_name (cm_service->manager,
                               "context-available",
                               cm_service->context);

        return TRUE;
}

static void
service_context_delete (CMService *cm_service)
{
        // For other states we just destroy the context
        g_signal_emit_by_name (cm_service->manager,
                               "context-unavailable",
                               cm_service->context);

        g_object_unref (cm_service->context);
        cm_service->context = NULL;
}

static void
service_context_update (CMService *cm_service, CMServiceState new_state)
{
        if (cm_service->current != new_state) {
                if (new_state == CM_SERVICE_STATE_ACTIVE) {
                        if (service_context_create (cm_service) == FALSE)
                                new_state = CM_SERVICE_STATE_INACTIVE;

                } else if ((new_state == CM_SERVICE_STATE_INACTIVE) &&
                           (cm_service->context != NULL)) {
                                service_context_delete (cm_service);
                }

                cm_service->current = new_state;
        }
}

static void
on_service_property_signal (GDBusConnection *connection,
                            const gchar     *sender_name,
                            const gchar     *object_path,
                            const gchar     *interface_name,
                            const gchar     *signal_name,
                            GVariant        *parameters,
                            gpointer        user_data)
{
        CMService      *cm_service;
        GVariant       *value;
        gchar          *name;
        const gchar    *state_str;
        CMServiceState new_state;

        cm_service = (CMService *) user_data;
        g_variant_get (parameters, "(&sv)", &name, &value);

        if (g_strcmp0 (name, "Name") == 0) {
                g_free (cm_service->name);
                g_variant_get (value, "s", &cm_service->name);

                if (cm_service->context != NULL)
                        g_object_set (G_OBJECT (cm_service->context),
                                               "network",
                                               cm_service->name,
                                               NULL);

        } else if (g_strcmp0 (name, "Ethernet") == 0) {
                g_free (cm_service->iface);
                g_variant_lookup (value, "Interface", "s", &cm_service->iface);

                if (cm_service->context != NULL)
                        g_object_set (G_OBJECT (cm_service->context),
                                               "interface",
                                               cm_service->iface,
                                               NULL);

        } else if (g_strcmp0 (name, "State") == 0) {
                state_str = g_variant_get_string (value, 0);

                if ((g_strcmp0 (state_str, "online") == 0) ||
                    (g_strcmp0 (state_str, "ready") == 0))
                        new_state = CM_SERVICE_STATE_ACTIVE;
                else
                        new_state = CM_SERVICE_STATE_INACTIVE;

                service_context_update (cm_service, new_state);
        }

        g_variant_unref (value);
}

static CMService *
cm_service_new (GUPnPConnmanManager *manager,
                GDBusProxy          *service_proxy)
{
        CMService *cm_service;

        cm_service = g_slice_new0 (CMService);

        cm_service->manager = manager;
        cm_service->proxy   = service_proxy;
        cm_service->current = CM_SERVICE_STATE_INACTIVE;

        return cm_service;
}

static void
cm_service_free (CMService *cm_service)
{
        GDBusConnection *cnx;

        cnx = g_dbus_proxy_get_connection (cm_service->proxy);

        if (cm_service->sig_prop_id) {
                g_dbus_connection_signal_unsubscribe (cnx,
                                                      cm_service->sig_prop_id);
                cm_service->sig_prop_id = 0;
        }

        g_object_unref (cm_service->proxy);

        if (cm_service->context != NULL) {
                g_signal_emit_by_name (cm_service->manager,
                                       "context-unavailable",
                                       cm_service->context);

                g_object_unref (cm_service->context);
        }

        g_free (cm_service->iface);
        g_free (cm_service->name);
        g_slice_free (CMService, cm_service);
}

static void
cm_service_use (GUPnPConnmanManager *manager,
                CMService           *cm_service)
{
        GDBusConnection *connection;

        connection = g_dbus_proxy_get_connection (cm_service->proxy);

        cm_service->sig_prop_id = g_dbus_connection_signal_subscribe (
                                connection,
                                CM_DBUS_CONNMAN_NAME,
                                CM_DBUS_SERVICE_INTERFACE,
                                "PropertyChanged",
                                g_dbus_proxy_get_object_path (cm_service->proxy),
                                NULL,
                                G_DBUS_SIGNAL_FLAGS_NONE,
                                on_service_property_signal,
                                cm_service,
                                NULL);

        if (cm_service->current == CM_SERVICE_STATE_ACTIVE)
                if (service_context_create (cm_service) == FALSE)
                        cm_service->current = CM_SERVICE_STATE_INACTIVE;
}
static void
cm_service_update (CMService *cm_service, GVariant *dict, guint port)
{
        CMServiceState new_state;
        GVariant       *eth;
        gchar          *iface;
        gchar          *name;
        gchar          *state;
        gboolean       is_name;
        gboolean       is_iface;

        is_iface = FALSE;
        iface    = NULL;
        name     = NULL;

        is_name = g_variant_lookup (dict, "Name", "s", &name);

        eth = g_variant_lookup_value (dict, "Ethernet", G_VARIANT_TYPE_VARDICT);

        if (eth != NULL) {
                is_iface = g_variant_lookup (eth, "Interface", "s", &iface);
                g_variant_unref (eth);
        }

        new_state = CM_SERVICE_STATE_INACTIVE;

        if (g_variant_lookup (dict, "State", "&s", &state) != FALSE)
                if ((g_strcmp0 (state, "online") == 0) ||
                    (g_strcmp0 (state, "ready") == 0))
                        new_state = CM_SERVICE_STATE_ACTIVE;

        if (is_name && (g_strcmp0 (cm_service->name, name) == 0)) {
                g_free (cm_service->name);
                cm_service->name = name;

                if (cm_service->context != NULL)
                        g_object_set (G_OBJECT (cm_service->context),
                                      "network",
                                      cm_service->name,
                                      NULL);
        }

        if (is_iface && (g_strcmp0 (cm_service->iface, iface) == 0)) {
                g_free (cm_service->iface);
                cm_service->iface = iface;

                if (cm_service->context != NULL)
                        g_object_set (G_OBJECT (cm_service->context),
                                      "interface",
                                      cm_service->iface,
                                      NULL);
        }

        cm_service->port = port;
        service_context_update (cm_service, new_state);
}

static void
service_proxy_new_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer     user_data)
{
        GUPnPConnmanManager *manager;
        GDBusProxy          *service_proxy;
        GError              *error = NULL;
        CMService           *cm_service;
        const gchar         *path;

        service_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

        if (error != NULL) {
                g_warning ("Failed to create D-Bus proxy: %s", error->message);
                g_error_free (error);

                return;
        }

        manager = GUPNP_CONNMAN_MANAGER (user_data);
        path = g_dbus_proxy_get_object_path (service_proxy);
        cm_service = g_hash_table_lookup (manager->priv->cm_services, path);

        if (cm_service == NULL) {
		g_object_unref (service_proxy);

                return;
	}

        cm_service->proxy = service_proxy;
        cm_service_use (manager, cm_service);
}

static void
cm_service_add (GUPnPConnmanManager *manager,
                GVariant            *dict,
                gchar               *path,
                guint               port)
{
        CMServiceState new_state;
        CMService      *cm_service;
        GVariant       *eth;
        gchar          *iface;
        gchar          *name;
        gchar          *state;

        iface   = NULL;
        name    = NULL;

        g_variant_lookup (dict, "Name", "s", &name);

        eth = g_variant_lookup_value (dict, "Ethernet", G_VARIANT_TYPE_VARDICT);

        if (eth != NULL) {
                g_variant_lookup (eth, "Interface", "s", &iface);
                g_variant_unref (eth);
        }

        new_state = CM_SERVICE_STATE_INACTIVE;

        if (g_variant_lookup (dict, "State", "&s", &state) != FALSE)
                if ((g_strcmp0 (state, "online") == 0) ||
                    (g_strcmp0 (state, "ready") == 0))
                        new_state = CM_SERVICE_STATE_ACTIVE;

        cm_service = cm_service_new (manager, NULL);

        cm_service->name    = name;
        cm_service->iface   = iface;
        cm_service->port    = port;
        cm_service->current = new_state;

        g_hash_table_insert (manager->priv->cm_services,
                             g_strdup (path),
                             cm_service);

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                  G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                  NULL,
                                  CM_DBUS_CONNMAN_NAME,
                                  path,
                                  CM_DBUS_SERVICE_INTERFACE,
                                  NULL,
                                  service_proxy_new_cb,
                                  manager);
}

static void
services_array_add (GUPnPConnmanManager *manager, GVariant *data)
{
        GVariant     *dict;
        CMService    *cm_service;
        gchar        *path;
        GVariantIter iter;
        GVariantIter dict_iter;
        guint        port;

        g_object_get (manager, "port", &port, NULL);
        g_variant_iter_init (&iter, data);

        while (g_variant_iter_loop (&iter, "(&o@a{sv})", &path, &dict)) {

                if (path == NULL)
                        continue;

                if (dict == NULL)
                        continue;

                if (g_variant_iter_init (&dict_iter, dict) == 0)
                        continue;

                cm_service = g_hash_table_lookup (manager->priv->cm_services,
                                                  path);

                if (cm_service == NULL)
                        cm_service_add (manager, dict, path, port);
                else
                        cm_service_update (cm_service, dict, port);
        }
}

static void
services_array_remove (GUPnPConnmanManager *manager, GVariant *data)
{
        GUPnPConnmanManagerPrivate *priv;
        CMService                  *cm_service;
        char                       *path;
        GVariantIter               iter;

        priv = manager->priv;
        g_variant_iter_init (&iter, data);

        while (g_variant_iter_next (&iter, "&o", &path)) {
                if (path == NULL)
                        continue;

                cm_service = g_hash_table_lookup (priv->cm_services, path);

                if (cm_service == NULL)
                        continue;

                g_hash_table_remove (priv->cm_services, path);
        }
}

static void
on_manager_svc_changed_signal (GDBusConnection *connection,
                               const gchar     *sender_name,
                               const gchar     *object_path,
                               const gchar     *interface_name,
                               const gchar     *signal_name,
                               GVariant        *parameters,
                               gpointer        user_data)
{
        GUPnPConnmanManager *manager;
        GVariant            *add_array;
        GVariant            *remove_array;

        manager = GUPNP_CONNMAN_MANAGER (user_data);

        add_array = g_variant_get_child_value (parameters, 0);
        remove_array = g_variant_get_child_value (parameters, 1);

        if ((add_array != NULL) && (g_variant_n_children (add_array) > 0))
                services_array_add (manager, add_array);

        if ((remove_array != NULL) && (g_variant_n_children (remove_array) > 0))
                services_array_remove (manager, remove_array);

        g_variant_unref (add_array);
        g_variant_unref (remove_array);
}

static void
get_services_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer     user_data)
{
        GUPnPConnmanManager *manager;
        GVariant            *ret;
        GVariant            *services_array;
        GError              *error = NULL;

        ret = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                        res,
                                        &error);

        if (error != NULL) {
                g_warning ("Error fetching service list: %s", error->message);
                g_error_free (error);

                return;
        }

        if (ret == NULL) {
                g_warning ("Failed fetching list of services but no error");

                return;
        }

        if (g_variant_is_container (ret) != TRUE)
        {
                g_warning ("Wrong format result");
		g_variant_unref (ret);

                return;
        }

        manager = GUPNP_CONNMAN_MANAGER (user_data);
        services_array = g_variant_get_child_value (ret, 0);
        services_array_add (manager, services_array);

        g_variant_unref (services_array);
        g_variant_unref (ret);
}

static void
schedule_loopback_context_creation (GUPnPConnmanManager *manager)
{
        /* Create contexts in mainloop so that it happens after user has hooked
         * to the "context-available" signal.
         */
        manager->priv->idle_context_creation_src = g_idle_source_new ();

        g_source_attach (manager->priv->idle_context_creation_src,
                         g_main_context_get_thread_default ());

        g_source_set_callback (manager->priv->idle_context_creation_src,
                               loopback_context_create,
                               manager,
                               NULL);

        g_source_unref (manager->priv->idle_context_creation_src);
}

static void
init_connman_manager (GUPnPConnmanManager *manager)
{
        GUPnPConnmanManagerPrivate *priv;
        GError                     *error = NULL;
        GDBusConnection            *connection;

        priv = manager->priv;

        priv->manager_proxy = g_dbus_proxy_new_for_bus_sync (
                                G_BUS_TYPE_SYSTEM,
                                G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                NULL,
                                CM_DBUS_CONNMAN_NAME,
                                CM_DBUS_MANAGER_PATH,
                                CM_DBUS_MANAGER_INTERFACE,
                                NULL,
                                &error);

        if (error != NULL) {
                g_warning ("Failed to connect to Connman: %s", error->message);
                g_error_free (error);

                return;
        }

        connection = g_dbus_proxy_get_connection (priv->manager_proxy);

        if (connection == NULL) {
                g_warning ("Failed to get DBus Connection");
                g_object_unref (priv->manager_proxy);
                priv->manager_proxy = NULL;

                return;
        }

        g_dbus_proxy_call (priv->manager_proxy,
                           "GetServices",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           get_services_cb,
                           manager);

        priv->sig_change_id = g_dbus_connection_signal_subscribe (
                                                connection,
                                                CM_DBUS_CONNMAN_NAME,
                                                CM_DBUS_MANAGER_INTERFACE,
                                                "ServicesChanged",
                                                CM_DBUS_MANAGER_PATH,
                                                NULL,
                                                G_DBUS_SIGNAL_FLAGS_NONE,
                                                on_manager_svc_changed_signal,
                                                manager,
                                                NULL);
}

static void
gupnp_connman_manager_dispose (GObject *object)
{
        GUPnPConnmanManager        *manager;
        GUPnPConnmanManagerPrivate *priv;
        GObjectClass               *object_class;
        GDBusConnection            *cnx;

        manager = GUPNP_CONNMAN_MANAGER (object);
        priv = manager->priv;
        cnx = g_dbus_proxy_get_connection (priv->manager_proxy);

        if (priv->sig_change_id) {
                g_dbus_connection_signal_unsubscribe (cnx, priv->sig_change_id);
                priv->sig_change_id = 0;
        }

        if (priv->idle_context_creation_src) {
                g_source_destroy (priv->idle_context_creation_src);
                priv->idle_context_creation_src = NULL;
        }

        if (priv->manager_proxy != NULL) {
                g_object_unref (priv->manager_proxy);
                priv->manager_proxy = NULL;
        }

        if (priv->cm_services) {
                g_hash_table_destroy (priv->cm_services);
                priv->cm_services = NULL;
        }

        g_clear_object (&(priv->system_bus));

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_connman_manager_parent_class);
        object_class->dispose (object);
}

static void
gupnp_connman_manager_constructed (GObject *object)
{
        GUPnPConnmanManager *manager;
        GObjectClass        *object_class;

        manager = GUPNP_CONNMAN_MANAGER (object);

        manager->priv->system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM,
                                                    NULL,
                                                    NULL);

        init_connman_manager (manager);

        schedule_loopback_context_creation (manager);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_connman_manager_parent_class);

        if (object_class->constructed != NULL) {
                object_class->constructed (object);
        }
}

static void
gupnp_connman_manager_init (GUPnPConnmanManager *manager)
{
        manager->priv = G_TYPE_INSTANCE_GET_PRIVATE (
                                                manager,
                                                GUPNP_TYPE_CONNMAN_MANAGER,
                                                GUPnPConnmanManagerPrivate);

        manager->priv->cm_services = g_hash_table_new_full (
                                        g_str_hash,
                                        g_str_equal,
                                        (GDestroyNotify) g_free,
                                        (GDestroyNotify) cm_service_free);
}

static void
gupnp_connman_manager_class_init (GUPnPConnmanManagerClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->constructed  = gupnp_connman_manager_constructed;
        object_class->dispose      = gupnp_connman_manager_dispose;

        g_type_class_add_private (klass, sizeof (GUPnPConnmanManagerPrivate));
}

gboolean
gupnp_connman_manager_is_available (void)
{
        GDBusProxy *dbus_proxy;
        GVariant   *ret_values;
        GError     *error = NULL;
        gboolean   ret = FALSE;

        dbus_proxy = g_dbus_proxy_new_for_bus_sync (
                        G_BUS_TYPE_SYSTEM,
                        G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                        NULL,
                        DBUS_SERVICE_DBUS,
                        DBUS_PATH_DBUS,
                        DBUS_INTERFACE_DBUS,
                        NULL,
                        &error);


        if (error != NULL) {
                g_warning ("Failed to connect to Connman: %s", error->message);
                g_error_free (error);

                return ret;
        }

        ret_values = g_dbus_proxy_call_sync (
                                dbus_proxy,
                                "NameHasOwner",
                                g_variant_new ("(s)", CM_DBUS_CONNMAN_NAME),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL,
                                &error);

        if (error != NULL) {
                g_warning ("%s.NameHasOwner() failed: %s",
                           DBUS_INTERFACE_DBUS,
                           error->message);

                g_error_free (error);

        } else {
                g_variant_get_child (ret_values, 0, "b", &ret);
                g_variant_unref (ret_values);
        }

        g_object_unref (dbus_proxy);

        return ret;
}
