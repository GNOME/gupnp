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

static GMainLoop *main_loop;

static enum {
  OFF = 0,
  ON = 1,
  TOGGLE
} mode;

static gboolean quiet;
static gint repeat_counter = 1;
static gint repeat_delay;

static GOptionEntry entries[] =
{
  { "quiet", 'q', 0, G_OPTION_ARG_NONE, &quiet, "Turn off output", NULL },
  { "repeat-counter", 'c', 0, G_OPTION_ARG_INT, &repeat_counter, "Repeat counter", "value" },
  { "repeat-delay", 'd', 0, G_OPTION_ARG_INT, &repeat_delay, "Delay in [ms] between each iteration", "value" },
  { NULL }
};


static void
send_cmd (GUPnPServiceProxy *proxy)
{
  GError *error = NULL;
  gboolean target;

  GUPnPServiceProxyAction *action;

  if (mode == TOGGLE) {
    /* We're toggling, so first fetch the current status */

    action = gupnp_service_proxy_action_new ("GetStatus", NULL);

    gupnp_service_proxy_call_action (proxy, action, NULL, &error);
    if (error != NULL)
      goto error;

    gupnp_service_proxy_action_get_result (action,
                                           &error,
                                           "ResultStatus", G_TYPE_BOOLEAN, &target, NULL);
    g_clear_pointer (&action, gupnp_service_proxy_action_unref);

    if (error != NULL)
      goto error;

    /* And then toggle it */
    target = ! target;

  } else {
    /* Mode is a boolean, so the target is the mode thanks to our well chosen
       enumeration values. */
    target = mode;
  }

  /* Set the target */

  action = gupnp_service_proxy_action_new ("SetTarget",
                                           "newTargetValue", G_TYPE_BOOLEAN, target, NULL);
  gupnp_service_proxy_call_action (proxy, action, NULL, &error);
  g_clear_pointer (&action, gupnp_service_proxy_action_unref);
  if (error != NULL) {
          goto error;
  } else {
    if (!quiet) {
      g_print ("Set switch to %s.\n", target ? "on" : "off");
    }
  }
  
 done:
  /* Only manipulate the first light switch that is found */
  if (--repeat_counter <= 0) {
    g_main_loop_quit (main_loop);
  }
  return;

 error:
  g_clear_pointer (&action, gupnp_service_proxy_action_unref);
  g_printerr ("Cannot set switch: %s\n", error->message);
  g_error_free (error);
  goto done;
}

static gboolean timeout_func (gpointer user_data)
{
  GUPnPServiceProxy *proxy = (GUPnPServiceProxy *) user_data;

  send_cmd (proxy);
  return TRUE;
}

static void
service_proxy_available_cb (G_GNUC_UNUSED GUPnPControlPoint *cp,
                            GUPnPServiceProxy               *proxy)
{
  if (repeat_counter>0)
  {
    send_cmd (proxy);
    g_timeout_add (repeat_delay, timeout_func, proxy);
  } else {
    g_main_loop_quit (main_loop);
  }
}

static void
usage (GOptionContext *optionContext)
{
  gchar *help_text;

  help_text = g_option_context_get_help (optionContext, TRUE, NULL);
  g_printerr ("%s", help_text);
  g_free (help_text);
}

int
main (int argc, char **argv)
{
  GOptionContext *optionContext;
  GError *error = NULL;
  GUPnPContext *context;
  GUPnPControlPoint *cp;

  optionContext = g_option_context_new ("[on|off|toggle]");
  g_option_context_add_main_entries (optionContext, entries, NULL);
  if (!g_option_context_parse (optionContext, &argc, &argv, &error))
  {
    g_print ("option parsing failed: %s\n", error->message);
    exit (1);
  }

  /* Check and parse command line arguments */
  if (argc != 2) {
    usage (optionContext);
    return EXIT_FAILURE;
  }
  
  if (g_str_equal (argv[1], "on")) {
    mode = ON;
  } else if (g_str_equal (argv[1], "off")) {
    mode = OFF;
  } else if (g_str_equal (argv[1], "toggle")) {
    mode = TOGGLE;
  } else {
    usage (optionContext);
    return EXIT_FAILURE;
  }

  /* Create the UPnP context */
  context = gupnp_context_new_for_address (NULL,
                                           0,
                                           GSSDP_UDA_VERSION_1_0,
                                           &error);
  if (error) {
    g_printerr ("Error creating the GUPnP context: %s\n",
		error->message);
    g_error_free (error);

    return EXIT_FAILURE;
  }

  /* Create the control point, searching for SwitchPower services */
  cp = gupnp_control_point_new (context,
				"urn:schemas-upnp-org:service:SwitchPower:1");
  
  /* Connect to the service-found callback */
  g_signal_connect (cp,
                    "service-proxy-available",
                    G_CALLBACK (service_proxy_available_cb),
                    NULL);
  
  /* Start searching when the main loop runs */
  gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

  /* Run the main loop */
  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);

  /* Cleanup */
  g_main_loop_unref (main_loop);
  g_object_unref (cp);
  g_object_unref (context);
  
  return EXIT_SUCCESS;
}
