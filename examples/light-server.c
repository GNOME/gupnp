/*
 * Example UPnP device/service, implementing the BinaryLight device and
 * SwitchPower services to emulate a light switch.
 *
 * The user interface is as minimal as possible so that the GUPnP concepts and
 * best practises are more apparent.  For a better implementation of
 * BinaryLight, see gupnp-tools.
 *
 * This example code is in the public domain.
 */

#include <libgupnp/gupnp.h>
#include <stdlib.h>
#include <gmodule.h>

static gboolean status;
static gboolean quiet;

static GOptionEntry entries[] =
{
  { "quiet", 'q', 0, G_OPTION_ARG_NONE, &quiet, "Turn off output", NULL },
  { NULL }
};

G_BEGIN_DECLS
G_MODULE_EXPORT void set_target_cb (GUPnPService *service,
                                    GUPnPServiceAction *action,
                                    gpointer user_data);
G_MODULE_EXPORT void get_target_cb (GUPnPService *service,
                                    GUPnPServiceAction *action,
                                    gpointer user_data);
G_MODULE_EXPORT void get_status_cb (GUPnPService *service,
                                    GUPnPServiceAction *action,
                                    gpointer user_data);
G_MODULE_EXPORT void query_target_cb (GUPnPService *service, char *variable,
                                      GValue *value, gpointer user_data);
G_MODULE_EXPORT void query_status_cb (GUPnPService *service, char *variable,
                                      GValue *value, gpointer user_data);
G_END_DECLS
/*
 * Action handlers
 */

/* SetTarget */
G_MODULE_EXPORT void
set_target_cb (GUPnPService          *service,
               GUPnPServiceAction    *action,
               G_GNUC_UNUSED gpointer user_data)
{
  gboolean target;

  /* Get the new target value */
  gupnp_service_action_get (action,
                            "newTargetValue", G_TYPE_BOOLEAN, &target,
                            NULL);

  /* If the new target doesn't match the current status, change the status and
     emit a notification that the status has changed. */
  if (target != status) {
    status = target;
    gupnp_service_notify (service,
                          "Status", G_TYPE_BOOLEAN, status,
                          NULL);

    if (!quiet)
    {
      g_print ("The light is now %s.\n", status ? "on" : "off");
    }
  }

  /* Return success to the client */
  gupnp_service_action_return_success (action);
}

/* GetTarget */
G_MODULE_EXPORT void
get_target_cb (G_GNUC_UNUSED GUPnPService *service,
               GUPnPServiceAction         *action,
               G_GNUC_UNUSED gpointer      user_data)
{
  gupnp_service_action_set (action,
                            "RetTargetValue", G_TYPE_BOOLEAN, status,
                            NULL);
  gupnp_service_action_return_success (action);
}

/* GetStatus */
G_MODULE_EXPORT void
get_status_cb (G_GNUC_UNUSED GUPnPService *service,
               GUPnPServiceAction         *action,
               G_GNUC_UNUSED gpointer      user_data)
{
  gupnp_service_action_set (action,
                            "ResultStatus", G_TYPE_BOOLEAN, status,
                            NULL);
  gupnp_service_action_return_success (action);
}

/*
 * State Variable query handlers
 */

/* Target */
G_MODULE_EXPORT void
query_target_cb (G_GNUC_UNUSED GUPnPService *service,
                 G_GNUC_UNUSED char         *variable,
                 GValue                     *value,
                 G_GNUC_UNUSED gpointer      user_data)
{
  g_value_init (value, G_TYPE_BOOLEAN);
  g_value_set_boolean (value, status);
}

/* Status */
G_MODULE_EXPORT void
query_status_cb (G_GNUC_UNUSED GUPnPService *service,
                 G_GNUC_UNUSED char         *variable,
                 GValue                     *value,
                 G_GNUC_UNUSED gpointer      user_data)
{
  g_value_init (value, G_TYPE_BOOLEAN);
  g_value_set_boolean (value, status);
}


int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
  GOptionContext *optionContext;
  GMainLoop *main_loop;
  GError *error = NULL;
  GUPnPContext *context;
  GUPnPRootDevice *dev;
  GUPnPServiceInfo *service;
  
  optionContext = g_option_context_new (NULL);
  g_option_context_add_main_entries (optionContext, entries, NULL);
  if (!g_option_context_parse (optionContext, &argc, &argv, &error))
  {
    g_print ("option parsing failed: %s\n", error->message);
    exit (1);
  }

  /* By default the light is off */
  status = FALSE;
  if (!quiet)
  {
    g_print ("The light is now %s.\n", status ? "on" : "off");
  }

  /* Create the UPnP context */
  context = gupnp_context_new_for_address (NULL, 0, GSSDP_UDA_VERSION_1_0, &error);
  if (error) {
    g_printerr ("Error creating the GUPnP context: %s\n",
		error->message);
    g_error_free (error);

    return EXIT_FAILURE;
  }

  /* Create root device */
  dev = gupnp_root_device_new (context, "BinaryLight1.xml", ".", &error);
  if (error != NULL) {
    g_printerr ("Error creating the GUPnP root device: %s\n",
                error->message);

    g_error_free (error);

    return EXIT_FAILURE;
  }
  gupnp_root_device_set_available (dev, TRUE);

  /* Get the switch service from the root device */
  service = gupnp_device_info_get_service
    (GUPNP_DEVICE_INFO (dev), "urn:schemas-upnp-org:service:SwitchPower:1");
  if (!service) {
    g_printerr ("Cannot get SwitchPower1 service\n");

    return EXIT_FAILURE;
  }
  

  /* Autoconnect the action and state variable handlers.  This connects
     query_target_cb and query_status_cb to the Target and Status state
     variables query callbacks, and set_target_cb, get_target_cb and
     get_status_cb to SetTarget, GetTarget and GetStatus actions
     respectively. */
  gupnp_service_signals_autoconnect (GUPNP_SERVICE (service), NULL, &error);
  if (error) {
    g_printerr ("Failed to autoconnect signals: %s\n", error->message);
    g_error_free (error);

    return EXIT_FAILURE;
  }
  
  /* Run the main loop */
  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);

  /* Cleanup */
  g_main_loop_unref (main_loop);
  g_object_unref (service);
  g_object_unref (dev);
  g_object_unref (context);
  
  return EXIT_SUCCESS;
}
