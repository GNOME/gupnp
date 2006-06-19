#include <libgupnp/gupnp.h>

int
main (int argc, char **argv)
{
        GSSDPClient *client;
        GUPnPControlPoint *cp;
        GMainLoop *main_loop;
        
        g_type_init ();

        client = gssdp_client_new (NULL, NULL);

        cp = gupnp_control_point_new (client, "upnp:rootdevice");
        gssdp_service_browser_set_active (GSSDP_SERVICE_BROWSER (cp), TRUE);

        main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_object_unref (cp);
        g_object_unref (client);

        return 0;
}
