#include <libgupnp/gupnp-device-proxy-private.h>

int
main (int argc, char **argv)
{
        GUPnPDeviceProxy *proxy;
        
        g_type_init ();

        proxy = _gupnp_device_proxy_new (argv[1]);

        g_print ("Type: %s\n",
                 gupnp_device_info_get_device_type (GUPNP_DEVICE_INFO (proxy)));

        g_object_unref (proxy);

        return 0;
}
