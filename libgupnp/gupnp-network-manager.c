/*
 * Copyright (C) 2009 Nokia Corporation, all rights reserved.
 *
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gupnp-network-manager
 * @short_description: NetworkManager-based implementation of
 * #GUPnPContextManager.
 *
 */

#include <config.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>

#include "gupnp-network-manager.h"
#include "gupnp-context.h"
#include "gupnp-marshal.h"

#define DBUS_TYPE_G_ARRAY_OF_OBJECT_PATH \
        (dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_OBJECT_PATH))

#define DBUS_SERVICE_NM "org.freedesktop.NetworkManager"
#define MANAGER_PATH "/org/freedesktop/NetworkManager"
#define MANAGER_INTERFACE "org.freedesktop.NetworkManager"
#define DEVICE_INTERFACE "org.freedesktop.NetworkManager.Device"

G_DEFINE_TYPE (GUPnPNetworkManager,
               gupnp_network_manager,
               GUPNP_TYPE_CONTEXT_MANAGER);

typedef enum
{
        NM_DEVICE_STATE_UNKNOWN,
        NM_DEVICE_STATE_UNMANAGED,
        NM_DEVICE_STATE_UNAVAILABLE,
        NM_DEVICE_STATE_DISCONNECTED,
        NM_DEVICE_STATE_PREPARE,
        NM_DEVICE_STATE_CONFIG,
        NM_DEVICE_STATE_NEED_AUTH,
        NM_DEVICE_STATE_IP_CONFIG,
        NM_DEVICE_STATE_ACTIVATED,
        NM_DEVICE_STATE_FAILED
} NMDeviceState;

typedef struct
{
        GUPnPNetworkManager *manager;

        GUPnPContext *context;

        DBusGProxy *device_proxy;
        DBusGProxy *prop_proxy;
} NMDevice;

struct _GUPnPNetworkManagerPrivate {
        DBusConnection *dbus_connection;
        DBusGConnection *connection;
        DBusGProxy *manager_proxy;

        GSource *idle_context_creation_src;

        GList *nm_devices;
};

static NMDevice *
nm_device_new (GUPnPNetworkManager *manager,
               const char          *device_path)
{
        NMDevice *nm_device;

        nm_device = g_slice_new0 (NMDevice);

        nm_device->manager = g_object_ref (manager);

        nm_device->prop_proxy = dbus_g_proxy_new_for_name (
                                        manager->priv->connection,
                                        DBUS_SERVICE_NM,
                                        device_path,
                                        DBUS_INTERFACE_PROPERTIES);

        nm_device->device_proxy = dbus_g_proxy_new_for_name (
                                                  manager->priv->connection,
                                                  DBUS_SERVICE_NM,
                                                  device_path,
                                                  DEVICE_INTERFACE);

        return nm_device;
}

static void
nm_device_free (NMDevice *nm_device)
{
        g_object_unref (nm_device->prop_proxy);
        g_object_unref (nm_device->device_proxy);

        if (nm_device->context != NULL) {
                g_signal_emit_by_name (nm_device->manager,
                                       "context-unavailable",
                                       nm_device->context);

                g_object_unref (nm_device->context);
        }

        g_object_unref (nm_device->manager);

        g_slice_free (NMDevice, nm_device);
}

#define LOOPBACK_IFACE "lo"

static gboolean
create_loopback_context (gpointer data)
{
        GUPnPNetworkManager *manager = (GUPnPNetworkManager *) data;
        GUPnPContext *context;
        GMainContext *main_context;
        guint port;
        GError *error = NULL;

        manager->priv->idle_context_creation_src = NULL;

        g_object_get (manager,
                      "main-context", &main_context,
                      "port", &port,
                      NULL);

        context = g_object_new (GUPNP_TYPE_CONTEXT,
                                "main-context", main_context,
                                "interface", LOOPBACK_IFACE,
                                "port", port,
                                "error", &error,
                                NULL);
        if (error) {
                g_warning ("Error creating GUPnP context: %s\n",
			   error->message);

                g_error_free (error);
                return FALSE;
        }

        g_signal_emit_by_name (manager, "context-available", context);

        g_object_unref (context);

        return FALSE;
}

static void
create_context_for_device (NMDevice *nm_device, const char *iface)
{
        GError *error = NULL;
        GMainContext *main_context;
        guint port;

        g_object_get (nm_device->manager,
                      "main-context", &main_context,
                      "port", &port,
                      NULL);

        nm_device->context = g_object_new (GUPNP_TYPE_CONTEXT,
                                           "main-context", main_context,
                                           "interface", iface,
                                           "port", port,
                                           "error", &error,
                                           NULL);
        if (error) {
                g_warning ("Error creating GUPnP context: %s\n",
			   error->message);

                g_error_free (error);

                return;
        }

        g_signal_emit_by_name (nm_device->manager,
                               "context-available",
                               nm_device->context);
}

static void
get_device_interface_cb (DBusGProxy     *proxy,
                         DBusGProxyCall *call,
                         void           *user_data)
{
        GValue value = {0,};
        GError *error = NULL;
        NMDevice *nm_device = (NMDevice *) user_data;

        if (!dbus_g_proxy_end_call (nm_device->prop_proxy,
                                    call,
                                    &error,
                                    G_TYPE_VALUE, &value,
                                    G_TYPE_INVALID)) {
                g_warning ("Error reading property from object: %s\n",
                           error->message);

                g_error_free (error);
                return;
        }

        create_context_for_device (nm_device, g_value_get_string (&value));
        g_value_unset (&value);
}

static void
get_device_state_cb (DBusGProxy     *proxy,
                     DBusGProxyCall *call,
                     void           *user_data)
{
        GValue value = {0,};
        GError *error = NULL;
        NMDevice *nm_device = (NMDevice *) user_data;

        if (!dbus_g_proxy_end_call (proxy,
                                    call,
                                    &error,
                                    G_TYPE_VALUE, &value,
                                    G_TYPE_INVALID)) {
                g_warning ("Error reading property: %s\n", error->message);

                g_error_free (error);
                return;
        }

        NMDeviceState state = g_value_get_uint (&value);

        if (state == NM_DEVICE_STATE_ACTIVATED) {
                dbus_g_proxy_begin_call (nm_device->prop_proxy,
                                         "Get",
                                         get_device_interface_cb,
                                         nm_device,
                                         NULL,
                                         G_TYPE_STRING, DEVICE_INTERFACE,
                                         G_TYPE_STRING, "Interface",
                                         G_TYPE_INVALID);
        }

        g_value_unset (&value);
}

static void
on_device_state_changed (DBusGProxy *proxy,
                         guint       new_state,
                         guint       old_state,
                         guint       reason,
                         void       *user_data)
{
        NMDevice *nm_device;

        nm_device = (NMDevice *) user_data;

        if (new_state == NM_DEVICE_STATE_ACTIVATED) {
                dbus_g_proxy_begin_call (nm_device->prop_proxy,
                                         "Get",
                                         get_device_interface_cb,
                                         nm_device,
                                         NULL,
                                         G_TYPE_STRING, DEVICE_INTERFACE,
                                         G_TYPE_STRING, "Interface",
                                         G_TYPE_INVALID);
        } else if (nm_device->context != NULL) {
                /* For all other states we just destroy the context */
                g_signal_emit_by_name (nm_device->manager,
                                       "context-unavailable",
                                       nm_device->context);

                g_object_unref (nm_device->context);
                nm_device->context = NULL;
        }
}

static void
add_device_from_path (char                *device_path,
                      GUPnPNetworkManager *manager)
{
        NMDevice *nm_device;

        nm_device = nm_device_new (manager, device_path);

        manager->priv->nm_devices = g_list_append (manager->priv->nm_devices,
                                                   nm_device);

        dbus_g_proxy_add_signal (nm_device->device_proxy,
                                 "StateChanged",
                                 G_TYPE_UINT,
                                 G_TYPE_UINT,
                                 G_TYPE_UINT,
                                 G_TYPE_INVALID);

        dbus_g_proxy_connect_signal (nm_device->device_proxy,
                                     "StateChanged",
                                     G_CALLBACK (on_device_state_changed),
                                     nm_device,
                                     NULL);

        dbus_g_proxy_begin_call (nm_device->prop_proxy,
                                 "Get",
                                 get_device_state_cb,
                                 nm_device,
                                 NULL,
                                 G_TYPE_STRING, DEVICE_INTERFACE,
                                 G_TYPE_STRING, "State",
                                 G_TYPE_INVALID);
}

static void
on_device_added (DBusGProxy *proxy,
                 char       *device_path,
                 gpointer user_data)
{
        add_device_from_path (device_path, GUPNP_NETWORK_MANAGER (user_data));
}

static int
compare_device_path (NMDevice *nm_device, char *device_path)
{
     const char *path;

     path = dbus_g_proxy_get_path (nm_device->device_proxy);
     if (G_UNLIKELY (path == NULL))
             return -1;

     return strcmp (path, device_path);
}

static void
on_device_removed (DBusGProxy *proxy,
                   char       *device_path,
                   gpointer    user_data)
{
        GList *device_node;
        NMDevice *nm_device;
        GUPnPNetworkManagerPrivate *priv;

        priv = GUPNP_NETWORK_MANAGER (user_data)->priv;

        device_node = g_list_find_custom (priv->nm_devices,
                                          device_path,
                                          (GCompareFunc) compare_device_path);
        if (G_UNLIKELY (device_node == NULL))
                return;

        nm_device = (NMDevice *) device_node->data;

        priv->nm_devices = g_list_remove (priv->nm_devices, nm_device);
        nm_device_free (nm_device);
}

static void
get_devices_cb (DBusGProxy     *proxy,
                DBusGProxyCall *call,
                void           *user_data)
{
        GPtrArray *device_paths;
        GError *error = NULL;

        if (!dbus_g_proxy_end_call (proxy,
                                    call,
                                    &error,
                                    DBUS_TYPE_G_ARRAY_OF_OBJECT_PATH,
                                    &device_paths,
                                    G_TYPE_INVALID)) {
                g_warning ("Error fetching list of devices: %s\n",
                           error->message);

                g_error_free (error);
                return;
        }

        g_ptr_array_foreach (device_paths,
                             (GFunc) add_device_from_path,
                             user_data);

        g_ptr_array_foreach (device_paths, (GFunc) g_free, NULL);
        g_ptr_array_free (device_paths, TRUE);
}

static void
schedule_loopback_context_creation (GUPnPNetworkManager *manager)
{
        GMainContext *main_context;

        g_object_get (manager,
                      "main-context", &main_context,
                      NULL);

        /* Create contexts in mainloop so that is happens after user has hooked
         * to the "context-available" signal.
         */
        manager->priv->idle_context_creation_src = g_idle_source_new ();
        g_source_attach (manager->priv->idle_context_creation_src,
            main_context);
        g_source_set_callback (manager->priv->idle_context_creation_src,
                               create_loopback_context,
                               manager,
                               NULL);
        g_source_unref (manager->priv->idle_context_creation_src);
}

static void
init_network_manager (GUPnPNetworkManager *manager)
{
        GUPnPNetworkManagerPrivate *priv;
        DBusError derror;
        GMainContext *main_context;

        priv = manager->priv;

        g_object_get (manager, "main-context", &main_context, NULL);

        /* Do fake open to initialize types */
        dbus_g_connection_open ("", NULL);
        dbus_error_init (&derror);
        priv->dbus_connection = dbus_bus_get_private (DBUS_BUS_SYSTEM, &derror);
        if (priv->dbus_connection == NULL) {
                g_warning ("Failed to connect to System Bus: %s",
                           derror.message);
                return;
        }

        dbus_connection_setup_with_g_main (priv->dbus_connection, main_context);

        priv->connection =
            dbus_connection_get_g_connection (priv->dbus_connection);

        priv->manager_proxy = dbus_g_proxy_new_for_name (priv->connection,
                                                         DBUS_SERVICE_NM,
                                                         MANAGER_PATH,
                                                         MANAGER_INTERFACE);

        dbus_g_proxy_add_signal (priv->manager_proxy,
                                 "DeviceAdded",
                                 DBUS_TYPE_G_OBJECT_PATH,
                                 G_TYPE_INVALID);
        dbus_g_proxy_connect_signal (priv->manager_proxy,
                                     "DeviceAdded",
                                     G_CALLBACK (on_device_added),
                                     manager,
                                     NULL);

        dbus_g_proxy_add_signal (priv->manager_proxy,
                                 "DeviceRemoved",
                                 DBUS_TYPE_G_OBJECT_PATH,
                                 G_TYPE_INVALID);
        dbus_g_proxy_connect_signal (priv->manager_proxy,
                                     "DeviceRemoved",
                                     G_CALLBACK (on_device_removed),
                                     manager,
                                     NULL);

        dbus_g_proxy_begin_call (priv->manager_proxy,
                                 "GetDevices",
                                 get_devices_cb,
                                 manager,
                                 NULL,
                                 G_TYPE_INVALID);
}

static void
gupnp_network_manager_init (GUPnPNetworkManager *manager)
{
        manager->priv =
                G_TYPE_INSTANCE_GET_PRIVATE (manager,
                                             GUPNP_TYPE_NETWORK_MANAGER,
                                             GUPnPNetworkManagerPrivate);
}

static void
gupnp_network_manager_constructed (GObject *object)
{
        GUPnPNetworkManager *manager;
        GUPnPNetworkManagerPrivate *priv;
        GObjectClass *object_class;

        manager = GUPNP_NETWORK_MANAGER (object);
        priv = manager->priv;

        priv->nm_devices = NULL;

        init_network_manager (manager);

        schedule_loopback_context_creation (manager);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_network_manager_parent_class);
        if (object_class->constructed != NULL) {
                object_class->constructed (object);
        }
}

static void
gupnp_network_manager_dispose (GObject *object)
{
        GUPnPNetworkManager *manager;
        GUPnPNetworkManagerPrivate *priv;
        GObjectClass *object_class;

        manager = GUPNP_NETWORK_MANAGER (object);
        priv = manager->priv;

        if (manager->priv->idle_context_creation_src) {
                g_source_destroy (manager->priv->idle_context_creation_src);
                manager->priv->idle_context_creation_src = NULL;
        }

        if (priv->manager_proxy != NULL) {
                g_object_unref (priv->manager_proxy);
                priv->manager_proxy = NULL;
        }

        if (priv->nm_devices != NULL) {
                g_list_foreach (priv->nm_devices, (GFunc) nm_device_free, NULL);
                g_list_free (priv->nm_devices);
                priv->nm_devices = NULL;
        }

        if (priv->connection)  {
          dbus_connection_close (priv->dbus_connection);
          dbus_g_connection_unref (priv->connection);
        }
        priv->connection = NULL;

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_network_manager_parent_class);
        object_class->dispose (object);
}

static void
gupnp_network_manager_class_init (GUPnPNetworkManagerClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->constructed  = gupnp_network_manager_constructed;
        object_class->dispose      = gupnp_network_manager_dispose;

        dbus_g_object_register_marshaller (gupnp_marshal_VOID__UINT_UINT_UINT,
                                           G_TYPE_NONE,
                                           G_TYPE_UINT,
                                           G_TYPE_UINT,
                                           G_TYPE_UINT,
                                           G_TYPE_INVALID);

        g_type_class_add_private (klass, sizeof (GUPnPNetworkManagerPrivate));
}

gboolean
gupnp_network_manager_is_available (GMainContext *main_context)
{
        DBusConnection *connection;
        DBusError derror;
        DBusGConnection *gconnection;
        DBusGProxy *dbus_proxy;
        GError *error = NULL;
        gboolean ret = FALSE;

        /* Do fake open to initialize types */
        dbus_g_connection_open ("", NULL);
        dbus_error_init (&derror);
        connection = dbus_bus_get_private (DBUS_BUS_SYSTEM, &derror);
        if (connection == NULL) {
                g_warning ("Failed to connect to System Bus: %s",
                           derror.message);
                return FALSE;
        }

        dbus_connection_setup_with_g_main (connection, main_context);
        gconnection = dbus_connection_get_g_connection (connection);

        dbus_proxy = dbus_g_proxy_new_for_name (gconnection,
                                                DBUS_SERVICE_DBUS,
                                                DBUS_PATH_DBUS,
                                                DBUS_INTERFACE_DBUS);

        if (!dbus_g_proxy_call (dbus_proxy,
                                "NameHasOwner",
                                &error,
                                G_TYPE_STRING, DBUS_SERVICE_NM,
                                G_TYPE_INVALID,
                                G_TYPE_BOOLEAN, &ret,
                                G_TYPE_INVALID)) {
                g_warning ("%s.NameHasOwner() failed: %s",
                           DBUS_INTERFACE_DBUS,
                           error->message);
                g_error_free (error);
        }

        g_object_unref (dbus_proxy);
        dbus_connection_close (connection);
        dbus_g_connection_unref (gconnection);

        return ret;
}
