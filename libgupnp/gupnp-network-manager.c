/*
 * Copyright (C) 2009 Nokia Corporation.
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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
#include <gio/gio.h>

#include "gupnp-network-manager.h"
#include "gupnp-context.h"

#define DBUS_TYPE_G_ARRAY_OF_OBJECT_PATH \
        (dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_OBJECT_PATH))

#define DBUS_SERVICE_NM "org.freedesktop.NetworkManager"
#define MANAGER_PATH "/org/freedesktop/NetworkManager"
#define MANAGER_INTERFACE "org.freedesktop.NetworkManager"
#define AP_INTERFACE "org.freedesktop.NetworkManager.AccessPoint"
#define DEVICE_INTERFACE "org.freedesktop.NetworkManager.Device"
#define WIFI_INTERFACE "org.freedesktop.NetworkManager.Device.Wireless"

#define DBUS_SERVICE_DBUS "org.freedesktop.DBus"
#define DBUS_PATH_DBUS "/org/freedesktop/DBus"
#define DBUS_INTERFACE_DBUS "org.freedesktop.DBus"

struct _GUPnPNetworkManager {
        GUPnPContextManager parent;
};

struct _GUPnPNetworkManagerPrivate {
        GDBusProxy *manager_proxy;

        GSource *idle_context_creation_src;

        GList *nm_devices;

        GCancellable *cancellable;

        GDBusConnection *system_bus;
};
typedef struct _GUPnPNetworkManagerPrivate GUPnPNetworkManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GUPnPNetworkManager,
                            gupnp_network_manager,
                            GUPNP_TYPE_CONTEXT_MANAGER);

typedef enum
{
        NM_DEVICE_STATE_UNKNOWN = 0,
        NM_OLD_DEVICE_STATE_UNMANAGED = 1,
        NM_OLD_DEVICE_STATE_UNAVAILABLE = 2,
        NM_OLD_DEVICE_STATE_DISCONNECTED = 3,
        NM_OLD_DEVICE_STATE_PREPARE = 4,
        NM_OLD_DEVICE_STATE_CONFIG = 5,
        NM_OLD_DEVICE_STATE_NEED_AUTH = 6,
        NM_OLD_DEVICE_STATE_IP_CONFIG = 7,
        NM_OLD_DEVICE_STATE_ACTIVATED = 8,
        NM_OLD_DEVICE_STATE_FAILED = 9,
        NM_DEVICE_STATE_UNMANAGED = 10,
        NM_DEVICE_STATE_UNAVAILABLE = 20,
        NM_DEVICE_STATE_DISCONNECTED = 30,
        NM_DEVICE_STATE_PREPARE = 40,
        NM_DEVICE_STATE_CONFIG = 50,
        NM_DEVICE_STATE_NEED_AUTH = 60,
        NM_DEVICE_STATE_IP_CONFIG = 70,
        NM_DEVICE_STATE_IP_CHECK = 80,
        NM_DEVICE_STATE_SECONDARIES = 90,
        NM_DEVICE_STATE_ACTIVATED = 100,
        NM_DEVICE_STATE_DEACTIVATING = 110,
        NM_DEVICE_STATE_FAILED = 120
} NMDeviceState;

typedef enum
{
        NM_DEVICE_TYPE_UNKNOWN,
        NM_DEVICE_TYPE_ETHERNET,
        NM_DEVICE_TYPE_WIFI,
        NM_OLD_DEVICE_TYPE_GSM,
        NM_OLD_DEVICE_TYPE_CDMA,
        NM_DEVICE_TYPE_BT,
        NM_DEVICE_TYPE_OLPC_MESH,
        NM_DEVICE_TYPE_WIMAX,
        NM_DEVICE_TYPE_MODEM
} NMDeviceType;

typedef struct
{
        gint ref_count;

        GUPnPNetworkManager *manager;

        // element-type GUPnPContext
        GList *contexts;

        GDBusProxy *proxy;
        GDBusProxy *wifi_proxy;
        GDBusProxy *ap_proxy;
} NMDevice;

static NMDevice *
nm_device_new (GUPnPNetworkManager *manager,
               GDBusProxy          *device_proxy)
{
        NMDevice *nm_device;

        nm_device = g_slice_new0 (NMDevice);

        g_atomic_int_set (&nm_device->ref_count, 1);
        nm_device->manager = manager;
        nm_device->proxy = g_object_ref (device_proxy);

        return nm_device;
}

static NMDevice *
nm_device_ref (NMDevice *nm_device)
{
        g_atomic_int_inc (&nm_device->ref_count);

        return nm_device;
}

static void
nm_device_unref (NMDevice *nm_device)
{
       if (!g_atomic_int_dec_and_test (&nm_device->ref_count))
          return;

        g_object_unref (nm_device->proxy);
        if (nm_device->wifi_proxy != NULL)
                g_object_unref (nm_device->wifi_proxy);
        if (nm_device->ap_proxy != NULL)
                g_object_unref (nm_device->ap_proxy);

        while (nm_device->contexts != NULL) {
                if (nm_device->manager != NULL) {
                        g_signal_emit_by_name (nm_device->manager,
                                               "context-unavailable",
                                               nm_device->contexts->data);
                }
                GList *entry = nm_device->contexts;
                g_object_unref (entry->data);
                nm_device->contexts =
                        g_list_remove_link (nm_device->contexts, entry);
                g_list_free (entry);
        }

        g_slice_free (NMDevice, nm_device);
}

static void
create_context_for_device (NMDevice *nm_device)
{
        GError *error = NULL;
        guint port;
        GVariant *value;
        char *iface;
        char *ssid = NULL;

        g_object_get (nm_device->manager,
                      "port", &port,
                      NULL);

        value = g_dbus_proxy_get_cached_property (nm_device->proxy,
                                                  "Interface");
        if (G_UNLIKELY (value == NULL))
                return;

        if (G_UNLIKELY (!g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))) {
                g_variant_unref (value);

                return;
        }

        iface = g_variant_dup_string (value, NULL);
        g_variant_unref (value);

        if (nm_device->ap_proxy != NULL) {
                value = g_dbus_proxy_get_cached_property (nm_device->ap_proxy,
                                                          "Ssid");
                if (G_LIKELY (value != NULL)) {
                        ssid = g_strndup (g_variant_get_data (value),
                                          g_variant_get_size (value));
                        g_variant_unref (value);
                }
        }

        GSocketFamily family = gupnp_context_manager_get_socket_family (
                GUPNP_CONTEXT_MANAGER (nm_device->manager));

        if (family == G_SOCKET_FAMILY_INVALID ||
            family == G_SOCKET_FAMILY_IPV4) {

                GUPnPContext *context = g_initable_new (GUPNP_TYPE_CONTEXT,
                                                        NULL,
                                                        &error,
                                                        "interface",
                                                        iface,
                                                        "network",
                                                        ssid,
                                                        "port",
                                                        port,
                                                        "address-family",
                                                        G_SOCKET_FAMILY_IPV4,
                                                        NULL);
                if (error) {
                        g_warning ("Error creating GUPnP context: %s\n",
                                   error->message);

                        g_clear_error (&error);
                } else {

                        g_signal_emit_by_name (nm_device->manager,
                                               "context-available",
                                               context);

                        nm_device->contexts =
                                g_list_prepend (nm_device->contexts, context);
                }
        }

        if (family == G_SOCKET_FAMILY_INVALID ||
            family == G_SOCKET_FAMILY_IPV6) {

                GUPnPContext *context = g_initable_new (GUPNP_TYPE_CONTEXT,
                                                        NULL,
                                                        &error,
                                                        "interface",
                                                        iface,
                                                        "network",
                                                        ssid,
                                                        "port",
                                                        port,
                                                        "address-family",
                                                        G_SOCKET_FAMILY_IPV6,
                                                        NULL);
                if (error) {
                        g_warning ("Error creating GUPnP context: %s\n",
                                   error->message);

                        g_clear_error (&error);
                } else {

                        g_signal_emit_by_name (nm_device->manager,
                                               "context-available",
                                               context);

                        nm_device->contexts =
                                g_list_prepend (nm_device->contexts, context);
                }
        }

        g_free (iface);
        g_free (ssid);
}

static void
ap_proxy_new_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
        NMDevice *nm_device;
        GError *error;

        nm_device = (NMDevice *) user_data;
        error = NULL;

        nm_device->ap_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (G_UNLIKELY (error != NULL)) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_message ("Failed to create D-Bus proxy: %s", error->message);
                else
                        nm_device->manager = NULL;
                g_error_free (error);
                goto done;
        }

        create_context_for_device (nm_device);

done:
        nm_device_unref (nm_device);
}

static void
on_wifi_device_activated (NMDevice *nm_device)
{
        GVariant *value;
        const char *ap_path;
        GUPnPNetworkManagerPrivate *priv;

        value = g_dbus_proxy_get_cached_property (nm_device->wifi_proxy,
                                                  "ActiveAccessPoint");
        if (G_UNLIKELY (value == NULL))
                return;

        if (G_UNLIKELY (!g_variant_is_of_type (value,
                                               G_VARIANT_TYPE_OBJECT_PATH))) {
                g_variant_unref (value);

                return;
        }

        priv = gupnp_network_manager_get_instance_private (nm_device->manager);
        ap_path = g_variant_get_string (value, NULL);
        if (G_UNLIKELY (ap_path == NULL))
                create_context_for_device (nm_device);
        else {
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          DBUS_SERVICE_NM,
                                          ap_path,
                                          AP_INTERFACE,
                                          priv->cancellable,
                                          ap_proxy_new_cb,
                                          nm_device_ref (nm_device));
	}

        g_variant_unref (value);
}

static void
on_device_activated (NMDevice *nm_device)
{
        if (nm_device->wifi_proxy != NULL)
                on_wifi_device_activated (nm_device);
        else
                create_context_for_device (nm_device);
}

static void
on_device_signal (GDBusProxy *proxy,
                  char       *sender_name,
                  char       *signal_name,
                  GVariant   *parameters,
                  gpointer    user_data)
{
        NMDevice *nm_device;
        unsigned int new_state;

        if (g_strcmp0 (signal_name, "StateChanged") != 0)
                return;

        nm_device = (NMDevice *) user_data;
        g_variant_get_child (parameters, 0, "u", &new_state);

        if (new_state == NM_OLD_DEVICE_STATE_ACTIVATED ||
            new_state == NM_DEVICE_STATE_ACTIVATED)
                on_device_activated (nm_device);
        else if (nm_device->contexts != NULL) {
                /* For all other states we just destroy the context */
                while (nm_device->contexts != NULL) {
                        g_signal_emit_by_name (nm_device->manager,
                                               "context-unavailable",
                                               nm_device->contexts->data);
                        g_object_unref (nm_device->contexts->data);
                        nm_device->contexts =
                                g_list_remove_link (nm_device->contexts,
                                                    nm_device->contexts);
                }
        }
}

static void
use_new_device (GUPnPNetworkManager *manager,
                NMDevice            *nm_device)
{
        NMDeviceState state;
        GVariant *value;
        GUPnPNetworkManagerPrivate *priv;

        priv = gupnp_network_manager_get_instance_private (manager);

        priv->nm_devices = g_list_append (priv->nm_devices,
                                          nm_device_ref (nm_device));

        g_signal_connect (nm_device->proxy,
                          "g-signal",
                          G_CALLBACK (on_device_signal),
                          nm_device);

        value = g_dbus_proxy_get_cached_property (nm_device->proxy, "State");
        if (G_UNLIKELY (value == NULL))
                return;

        if (G_UNLIKELY (!g_variant_is_of_type (value, G_VARIANT_TYPE_UINT32))) {
                g_variant_unref (value);

                return;
        }

        state = g_variant_get_uint32 (value);
        g_variant_unref (value);

        if (state == NM_OLD_DEVICE_STATE_ACTIVATED ||
            state == NM_DEVICE_STATE_ACTIVATED)
                on_device_activated (nm_device);
}

static void
wifi_proxy_new_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data) {
        NMDevice *nm_device;
        GError *error;

        nm_device = (NMDevice *) user_data;
        error = NULL;

        nm_device->wifi_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (G_UNLIKELY (error != NULL)) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_message ("Failed to create D-Bus proxy: %s", error->message);
                else
                        nm_device->manager = NULL;
                g_error_free (error);
                goto done;
        }

        use_new_device (nm_device->manager, nm_device);

done:
        nm_device_unref (nm_device);
}

static void
device_proxy_new_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data) {
        GUPnPNetworkManager *manager;
        GDBusProxy *device_proxy;
        NMDevice *nm_device = NULL;
        NMDeviceType type;
        GVariant *value;
        GError *error;
        GUPnPNetworkManagerPrivate *priv;

        error = NULL;

        device_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (G_UNLIKELY (error != NULL)) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_message ("Failed to create D-Bus proxy: %s", error->message);
                g_error_free (error);

                goto done;
        }

        manager = GUPNP_NETWORK_MANAGER (user_data);

        value = g_dbus_proxy_get_cached_property (device_proxy, "DeviceType");
        if (G_UNLIKELY (value == NULL)) {
                goto done;
        }

        if (G_UNLIKELY (!g_variant_is_of_type (value, G_VARIANT_TYPE_UINT32))) {
                g_variant_unref (value);

                goto done;
        }

        type = g_variant_get_uint32 (value);
        g_variant_unref (value);

        priv = gupnp_network_manager_get_instance_private (manager);
        nm_device = nm_device_new (manager, device_proxy);

        if (type == NM_DEVICE_TYPE_WIFI) {
                const char *path;

                path = g_dbus_proxy_get_object_path (nm_device->proxy);
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          DBUS_SERVICE_NM,
                                          path,
                                          WIFI_INTERFACE,
                                          priv->cancellable,
                                          wifi_proxy_new_cb,
                                          nm_device_ref (nm_device));
        } else
                use_new_device (manager, nm_device);

done:
        g_clear_pointer (&nm_device, nm_device_unref);
        g_clear_object (&device_proxy);
}

static int
compare_device_path (NMDevice *nm_device, char *device_path)
{
     const char *path;

     path = g_dbus_proxy_get_object_path (nm_device->proxy);
     if (G_UNLIKELY (path == NULL))
             return -1;

     return strcmp (path, device_path);
}

static void
on_manager_signal (GDBusProxy *proxy,
                   char       *sender_name,
                   char       *signal_name,
                   GVariant   *parameters,
                   gpointer    user_data)
{
        GUPnPNetworkManager *manager;
        GUPnPNetworkManagerPrivate *priv;

        manager = GUPNP_NETWORK_MANAGER (user_data);
        priv = gupnp_network_manager_get_instance_private (manager);

        if (g_strcmp0 (signal_name, "DeviceAdded") == 0) {
                char *device_path = NULL;

                g_variant_get_child (parameters, 0, "o", &device_path);
                if (G_UNLIKELY (device_path == NULL))
                        return;


                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          DBUS_SERVICE_NM,
                                          device_path,
                                          DEVICE_INTERFACE,
                                          priv->cancellable,
                                          device_proxy_new_cb,
                                          manager);
                g_free (device_path);
        } else if (g_strcmp0 (signal_name, "DeviceRemoved") == 0) {
                GList *device_node;
                NMDevice *nm_device;
                GUPnPNetworkManagerPrivate *priv;
                char *device_path = NULL;

                g_variant_get_child (parameters, 0, "o", &device_path);
                if (G_UNLIKELY (device_path == NULL))
                        return;

                priv = gupnp_network_manager_get_instance_private (manager);

                device_node = g_list_find_custom (
                                            priv->nm_devices,
                                            device_path,
                                            (GCompareFunc) compare_device_path);
                if (G_UNLIKELY (device_node == NULL)) {
                        g_free (device_path);

                        return;
                }

                nm_device = (NMDevice *) device_node->data;

                priv->nm_devices = g_list_remove (priv->nm_devices, nm_device);
                nm_device_unref (nm_device);
                g_free (device_path);
        }
}

static void
get_devices_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
        GUPnPNetworkManager *manager;
        GVariant *ret;
        GVariantIter *device_iter;
        char* device_path;
        GError *error = NULL;
        GUPnPNetworkManagerPrivate *priv;

        ret = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                        res,
                                        &error);
        if (error != NULL) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Error fetching list of devices: %s",
                                   error->message);

                g_error_free (error);
                return;
        }

        manager = GUPNP_NETWORK_MANAGER (user_data);
        priv = gupnp_network_manager_get_instance_private (manager);

        g_variant_get_child (ret, 0, "ao", &device_iter);
        while (g_variant_iter_loop (device_iter, "o", &device_path))
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          DBUS_SERVICE_NM,
                                          device_path,
                                          DEVICE_INTERFACE,
                                          priv->cancellable,
                                          device_proxy_new_cb,
                                          user_data);
        g_variant_iter_free (device_iter);

        g_variant_unref (ret);
}

static void
init_network_manager (GUPnPNetworkManager *manager)
{
        GUPnPNetworkManagerPrivate *priv;
        GError *error;

        priv = gupnp_network_manager_get_instance_private (manager);

        error = NULL;
        priv->manager_proxy = g_dbus_proxy_new_for_bus_sync (
                        G_BUS_TYPE_SYSTEM,
                        G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                        NULL,
                        DBUS_SERVICE_NM,
                        MANAGER_PATH,
                        MANAGER_INTERFACE,
                        priv->cancellable,
                        &error);
        if (error != NULL) {
                g_message ("Failed to connect to NetworkManager: %s",
                           error->message);
                g_error_free (error);

                return;
        }

        g_signal_connect (priv->manager_proxy,
                          "g-signal",
                          G_CALLBACK (on_manager_signal),
                          manager);


        g_dbus_proxy_call (priv->manager_proxy,
                           "GetDevices",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           priv->cancellable,
                           get_devices_cb,
                           manager);
}

static void
gupnp_network_manager_init (GUPnPNetworkManager *manager)
{
}

static void
gupnp_network_manager_constructed (GObject *object)
{
        GUPnPNetworkManager *manager;
        GUPnPNetworkManagerPrivate *priv;
        GObjectClass *object_class;

        manager = GUPNP_NETWORK_MANAGER (object);
        priv = gupnp_network_manager_get_instance_private (manager);

        priv->cancellable = g_cancellable_new ();
        priv->nm_devices = NULL;
        priv->system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

        init_network_manager (manager);

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
        priv = gupnp_network_manager_get_instance_private (manager);

        if (priv->idle_context_creation_src) {
                g_source_destroy (priv->idle_context_creation_src);
                priv->idle_context_creation_src = NULL;
        }

        if (priv->manager_proxy != NULL) {
                g_object_unref (priv->manager_proxy);
                priv->manager_proxy = NULL;
        }

        g_list_free_full (priv->nm_devices, (GDestroyNotify) nm_device_unref);

        if (priv->cancellable != NULL)  {
                g_cancellable_cancel (priv->cancellable);
                g_object_unref (priv->cancellable);
                priv->cancellable = NULL;
        }

        g_clear_object (&(priv->system_bus));

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
}

gboolean
gupnp_network_manager_is_available (void)
{
        GDBusProxy *dbus_proxy;
        GVariant *ret_values;
        GError *error = NULL;
        gboolean ret = FALSE;

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
                g_message ("Failed to connect to NetworkManager: %s",
                           error->message);
                g_error_free (error);

                return ret;
        }

        ret_values = g_dbus_proxy_call_sync (dbus_proxy,
                                             "NameHasOwner",
                                             g_variant_new ("(s)",
                                                            DBUS_SERVICE_NM),
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
