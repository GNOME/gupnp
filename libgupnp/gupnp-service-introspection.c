/* 
 * Copyright (C) 2007 Zeeshan Ali <zeenix@gstreamer.net>
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali <zeenix@gstreamer.net>
 *         Jorn Baayen <jorn@openedhand.com>
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
 * SECTION:gupnp-service-introspection
 * @short_description: Service introspection class.
 *
 * The #GUPnPServiceIntrospection class provides methods for service
 * introspection based on information contained in its service description
 * document (SCPD).
 **/

#include <libsoup/soup.h>
#include <string.h>

#include "gupnp-service-introspection.h"
#include "xml-util.h"

#define MAX_FIXED_14_4 99999999999999.9999

G_DEFINE_TYPE (GUPnPServiceIntrospection,
               gupnp_service_introspection,
               G_TYPE_OBJECT);

struct _GUPnPServiceIntrospectionPrivate {
        char *scpd_url;
        
        /* Intropection data */
        GHashTable *action_hash;
        GHashTable *variable_hash;
};

enum {
        PROP_0,
        PROP_SCPD_URL
};

static void
contstruct_introspection_info (GUPnPServiceIntrospection *introspection);

static void
gupnp_service_introspection_init (GUPnPServiceIntrospection *introspection)
{
        introspection->priv = 
                G_TYPE_INSTANCE_GET_PRIVATE (introspection,
                                             GUPNP_TYPE_SERVICE_INTROSPECTION,
                                             GUPnPServiceIntrospectionPrivate);
}

static void
gupnp_service_introspection_set_property (GObject      *object,
                                          guint        property_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
        GUPnPServiceIntrospection *introspection;

        introspection = GUPNP_SERVICE_INTROSPECTION (object);

        switch (property_id) {
        case PROP_SCPD_URL:
                introspection->priv->scpd_url =
                        g_value_dup_string (value);

                /* Contruct introspection data */
                contstruct_introspection_info (introspection);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_service_introspection_get_property (GObject    *object,
                                          guint      property_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
        GUPnPServiceIntrospection *introspection;

        introspection = GUPNP_SERVICE_INTROSPECTION (object);

        switch (property_id) {
        case PROP_SCPD_URL:
                g_value_set_string (value,
                                    introspection->priv->scpd_url);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static void
gupnp_service_introspection_finalize (GObject *object)
{
        GUPnPServiceIntrospection *introspection;

        introspection = GUPNP_SERVICE_INTROSPECTION (object);

        if (introspection->priv->action_hash)
                g_hash_table_destroy (introspection->priv->action_hash);

        if (introspection->priv->variable_hash)
                g_hash_table_destroy (introspection->priv->variable_hash);

        g_free (introspection->priv->scpd_url);
}

static void
gupnp_service_introspection_class_init (GUPnPServiceIntrospectionClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_service_introspection_set_property;
        object_class->get_property = gupnp_service_introspection_get_property;
        object_class->finalize     = gupnp_service_introspection_finalize;

        g_type_class_add_private (klass,
                                  sizeof (GUPnPServiceIntrospectionPrivate));

        /**
         * GUPnPServiceIntrospection:scpd_url
         *
         * The scpd_url of the device description file.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_SCPD_URL,
                 g_param_spec_string ("scpd-url",
                                      "SCPD-URL",
                                      "The URL of the SCPD from which "
                                      "introspection is built",
                                      NULL,
                                      G_PARAM_READWRITE |
                                      G_PARAM_CONSTRUCT_ONLY));
}

/**
 * 
 * Synchronously downloads and parses the SCPD from the URL @scpd_url and
 * returns the xmlDoc pointer to the resulting xml document tree.
 *
 **/
static xmlDoc * 
get_scpd (const char *scpd_url)
{
        SoupSession *session;
        xmlDoc *scpd = NULL;
        SoupMessage *msg;
        guint status;
        
        session = soup_session_sync_new ();
        
        msg = soup_message_new (SOUP_METHOD_GET, scpd_url);
        if (!msg) {
                g_warning ("failed to create request message for %s", scpd_url);

                goto EXIT_POINT;
        }

        status = soup_session_send_message (session, msg);
        if (!SOUP_STATUS_IS_SUCCESSFUL (status)) {
                g_warning ("failed to get %s. Reason: %s",
                            scpd_url,
                            soup_status_get_phrase (status));

                goto FAILED_GET;
        }

        scpd = xmlParseMemory (msg->response.body,
                                  msg->response.length);
        if (!scpd)
                g_warning ("failed to get parse SCPD document '%s'", scpd_url);

 FAILED_GET:
        g_object_unref (msg);
        g_object_unref (session);

 EXIT_POINT:
        return scpd;
}

static void
set_default_value (xmlNodePtr variable_node,
                   GUPnPServiceStateVariableInfo *variable)
{
        xmlChar *default_str;

        default_str = xml_util_get_child_element_content (variable_node,
                                                          "defaultValue");
        if (default_str) {
                GValue tmp;

                memset (&tmp, 0, sizeof (GValue));
                g_value_init (&tmp, G_TYPE_STRING);
                g_value_set_static_string (&tmp, (char *) default_str);
                g_value_transform (&tmp, &(variable->default_value));

                g_value_unset (&tmp);
                xmlFree (default_str);
        }
}

static void
set_string_value_limits (xmlNodePtr limit_node,
                         GSList     **limits)
{
        xmlNodePtr value_node;

        for (value_node = limit_node->children;
             value_node;
             value_node = value_node->next) {
                xmlChar *allowed_value;

                if (strcmp ("allowedValue", (char *) value_node->name) != 0)
                        continue;

                allowed_value = xmlNodeGetContent (value_node);
                if (!allowed_value)
                        continue;

                *limits = g_slist_append (*limits,
                                          g_strdup ((char *) allowed_value));
                xmlFree (allowed_value);
        }
}

static void
set_value_limit_by_name (xmlNodePtr limit_node,
                         GValue *limit,
                         char *limit_name)
{
        xmlChar *limit_str;

        limit_str = xml_util_get_child_element_content (limit_node, limit_name);
        if (limit_str) {
                GValue tmp;

                g_value_init (&tmp, G_TYPE_STRING);
                g_value_set_static_string (&tmp, (char *) limit_str);
                g_value_transform (&tmp, limit);

                g_value_unset (&tmp);
                xmlFree (limit_str);
        }
}

static void
set_variable_limits (xmlNodePtr                    variable_node,
                     GUPnPServiceStateVariableInfo *variable)
{
        xmlNodePtr limit_node;

        if (variable->is_numeric) {
                limit_node = xml_util_get_element (variable_node,
                                                   "allowedValueRange",
                                                   NULL);
                if (limit_node == NULL)
                        return;

                set_value_limit_by_name (limit_node,
                                &(variable->minimum),
                                "minimum");
                set_value_limit_by_name (limit_node,
                                &(variable->maximum),
                                "maximum");
                set_value_limit_by_name (limit_node,
                                &(variable->step),
                                "step");
        } else if (variable->gtype == G_TYPE_STRING) {
                limit_node = xml_util_get_element (variable_node,
                                                   "allowedValueList",
                                                   NULL);
                if (limit_node == NULL)
                        return;

                set_string_value_limits (limit_node,
                                         &(variable->allowed_values));
       }
}

static void
set_variable_type (GUPnPServiceStateVariableInfo *variable,
                   char                          *data_type)
{
        GType gtype;

        if (strcmp ("string", data_type) == 0) {
                gtype = G_TYPE_STRING;
        }
        
        else if (strcmp ("char", data_type) == 0) {
                gtype = G_TYPE_CHAR;
        }

        else if (strcmp ("boolean", data_type) == 0) {
                gtype = G_TYPE_BOOLEAN;
        }

        else if (strcmp ("i1", data_type) == 0) {
                gtype = G_TYPE_INT;
                g_value_init (&variable->minimum, gtype);
                g_value_set_int (&variable->minimum, G_MININT8);
                g_value_init (&variable->maximum, gtype);
                g_value_set_int (&variable->maximum, G_MAXINT8);
                variable->is_numeric = TRUE;
        }
         
        else if (strcmp ("i2", data_type) == 0) {
                gtype = G_TYPE_INT;
                g_value_init (&variable->minimum, gtype);
                g_value_set_int (&variable->minimum, G_MININT16);
                g_value_init (&variable->maximum, gtype);
                g_value_set_int (&variable->maximum, G_MAXINT16);
                variable->is_numeric = TRUE;
        }
        
        else if (strcmp ("i4", data_type) == 0 ||
                 strcmp ("int", data_type) == 0) {
                gtype = G_TYPE_INT;
                g_value_init (&variable->minimum, gtype);
                g_value_set_int (&variable->minimum, G_MININT32);
                g_value_init (&variable->maximum, gtype);
                g_value_set_int (&variable->maximum, G_MAXINT32);
                variable->is_numeric = TRUE;
        }

        else if (strcmp ("ui1", data_type) == 0) {
                gtype = G_TYPE_UINT;
                g_value_init (&variable->minimum, gtype);
                g_value_set_uint (&variable->minimum, 0);
                g_value_init (&variable->maximum, gtype);
                g_value_set_uint (&variable->maximum, G_MAXUINT8);
                variable->is_numeric = TRUE;
        }
        
        else if (strcmp ("ui2", data_type) == 0) {
                gtype = G_TYPE_UINT;
                g_value_init (&variable->minimum, gtype);
                g_value_set_uint (&variable->minimum, 0);
                g_value_init (&variable->maximum, gtype);
                g_value_set_uint (&variable->maximum, G_MAXUINT16);
                variable->is_numeric = TRUE;
        }
        
        else if (strcmp ("ui4", data_type) == 0) {
                gtype = G_TYPE_UINT;
                g_value_init (&variable->minimum, gtype);
                g_value_set_uint (&variable->minimum, 0);
                g_value_init (&variable->maximum, gtype);
                g_value_set_uint (&variable->maximum, G_MAXUINT32);
                variable->is_numeric = TRUE;
        }
        
        else if (strcmp ("r4", data_type) == 0) {
                gtype = G_TYPE_FLOAT;
                g_value_init (&variable->minimum, gtype);
                g_value_set_float (&variable->minimum, -G_MAXFLOAT);
                g_value_init (&variable->maximum, gtype);
                g_value_set_float (&variable->maximum, G_MAXFLOAT);
                variable->is_numeric = TRUE;
        }
        
        else if (strcmp ("r8", data_type) == 0 ||
                 strcmp ("number", data_type) == 0) {
                gtype = G_TYPE_DOUBLE;
                g_value_init (&variable->minimum, gtype);
                g_value_set_double (&variable->minimum,  -G_MAXDOUBLE);
                g_value_init (&variable->maximum, gtype);
                g_value_set_double (&variable->maximum, G_MAXDOUBLE);
                variable->is_numeric = TRUE;
        }
        
        else if (strcmp ("fixed.14.4", data_type) == 0) {
                /* Just how did this get into the UPnP specs? */
                gtype = G_TYPE_DOUBLE;
                g_value_init (&variable->minimum, gtype);
                g_value_set_double (&variable->minimum,  -MAX_FIXED_14_4);
                g_value_init (&variable->maximum, gtype);
                g_value_set_double (&variable->maximum, MAX_FIXED_14_4);
                variable->is_numeric = TRUE;
        }

        else {
                /* We treat rest of the types as strings */
                gtype = G_TYPE_STRING;
        }
        
        if (variable->is_numeric)
                g_value_init (&variable->step, gtype);
        g_value_init (&variable->default_value, gtype);
        variable->gtype = gtype;
        variable->data_type = data_type;
}

/**
 * 
 * Creates a #GUPnPServiceStateVariableInfo, constructed from the stateVariable
 * node @variable_node in the SCPD document
 *
 **/
static GUPnPServiceStateVariableInfo *
get_state_variable (xmlNodePtr variable_node)
{
        GUPnPServiceStateVariableInfo *variable;
        xmlChar *send_events;
        char *data_type;
        
        data_type = xml_util_get_child_element_content_glib (variable_node, 
                                                             "dataType");
        if (!data_type) {
                /* We can't report much about a state variable whose dataType
                 * is not specified so better not report anything at all */
                return NULL;
        }
        
        variable = g_slice_new0 (GUPnPServiceStateVariableInfo);

        set_variable_type (variable, data_type);

        set_variable_limits (variable_node, variable);
        set_default_value (variable_node, variable);

        send_events = xml_util_get_child_element_content
                                       (variable_node, "sendEventsAttribute");

        if (send_events) {
                if (strcmp ("yes", (char *) send_events) == 0)
                        variable->send_events = TRUE;
                xmlFree (send_events);
        }
        
        return variable;
}

/**
 * 
 * Creates a #GUPnPServiceActionArgInfo, constructed from the argument node
 * @argument_node in the SCPD document
 *
 **/
static GUPnPServiceActionArgInfo *
get_action_argument (xmlNodePtr argument_node)
{
        GUPnPServiceActionArgInfo *argument;
        char *name, *direction, *state_var;
        
        name      = xml_util_get_child_element_content_glib
                                        (argument_node, "name");
        direction = xml_util_get_child_element_content_glib
                                        (argument_node, "direction");
        state_var = xml_util_get_child_element_content_glib
                                        (argument_node, "relatedStateVariable");

        if (!name || !direction || !state_var) {
                g_free (name);
                g_free (direction);
                g_free (state_var);

                return NULL;
        }

        argument = g_slice_new0 (GUPnPServiceActionArgInfo);

        argument->name                   = name;
        argument->direction              = direction;
        argument->related_state_variable = state_var;

        if (xml_util_get_element (argument_node, "retval", NULL) != NULL)
                argument->retval = TRUE;

        return argument;
}

/**
 *
 * Creates a #GSList of all the arguments (of type #GUPnPServiceActionArgInfo)
 * from the action node @action_node in the SCPD document
 *
 **/
static GSList *
get_action_arguments (xmlNodePtr action_node)
{
        GSList *argument_list = NULL;
        xmlNodePtr arglist_node;
        xmlNodePtr argument_node;
        
        arglist_node = xml_util_get_element ((xmlNode *) action_node,
                                             "argumentList",
                                             NULL);
        if (!arglist_node)
                return NULL;

        /* Iterate over all the arguments */
        for (argument_node = arglist_node->children; 
             argument_node;
             argument_node = argument_node->next) {
                GUPnPServiceActionArgInfo *argument;
                
                if (strcmp ("argument", (char *) argument_node->name) != 0)
                        continue;

                argument = get_action_argument (argument_node);
                if (argument) {
                        argument_list = g_slist_append (argument_list,
                                                        argument);
                }
        }

        return argument_list;
}

/**
 *
 * Free's a #GSList of GUPnPServiceActionArgInfo
 *
 **/
static void
action_argument_list_free (GSList *argument_list)
{
        GSList *iter;

        for (iter = argument_list; iter; iter = iter->next) {
                gupnp_service_action_arg_info_free (
                                (GUPnPServiceActionArgInfo *) iter->data);
        }

        g_slist_free (argument_list);
}

/**
 *
 * Creates a #GHashTable of all the actions from the SCPD document with
 * keys being the names of the action and values being lists (of type #GSList)
 * of arguments (of type #GUPnPServiceActionArgInfo).
 *
 **/
static GHashTable *
get_actions (xmlNode *list_element)
{
        GHashTable *actions;
        xmlNodePtr action_node;
        actions = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         (GDestroyNotify) 
                                         action_argument_list_free);
        
        /* Iterate over all action elements */
        for (action_node = list_element->children; 
             action_node;
             action_node = action_node->next) {
                char *name;
                GSList *arguments;

                if (strcmp ("action", (char *) action_node->name) != 0)
                        continue;

                arguments = get_action_arguments (action_node);
                if (!arguments) {
                        continue;
                }

                name = xml_util_get_child_element_content_glib (action_node,
                                                                "name");
                if (!name)
                        continue;

                g_hash_table_insert (actions, name, arguments);
        }

        return actions;
}

/**
 *
 * Creates a #GHashTable of all the state variable from the SCPD document with
 * keys being the names of the variables and values being lists (of type 
 * #GSList) of state variable information (of type 
 * #GUPnPServiceStateVariableInfo).
 *
 **/
static GHashTable *
get_state_variables (xmlNode *list_element)
{
        GHashTable *variables;
        xmlNodePtr variable_node;

        variables = g_hash_table_new_full
                                (g_str_hash,
                                 g_str_equal,
                                 NULL,
                                 (GDestroyNotify)
                                        gupnp_service_state_variable_info_free);
        
        /* Iterate over all variable elements */
        for (variable_node = list_element->children; 
             variable_node;
             variable_node = variable_node->next) {
                char *name;
                GUPnPServiceStateVariableInfo *variable;

                if (strcmp ("stateVariable", (char *) variable_node->name) != 0)
                        continue;

                name = xml_util_get_child_element_content_glib (variable_node,
                                                                "name");
                if (!name)
                        continue;

                variable = get_state_variable (variable_node);
                if (!variable) {
                        g_free (name);

                        continue;
                }

                variable->name = name;

                g_hash_table_insert (variables, variable->name, variable);
        }

        return variables;
}

/*
 * Creates the #GHashTable's of action and state variable information
 *
 * */
static void
contstruct_introspection_info (GUPnPServiceIntrospection *introspection)
{
        xmlDoc *scpd;
        xmlNode *root_element, *element;

        g_return_if_fail (introspection->priv->scpd_url != NULL);

        scpd = get_scpd (introspection->priv->scpd_url);
        if (!scpd)
                return;

        root_element = xml_util_get_element ((xmlNode *) scpd,
                                             "scpd",
                                             NULL);

        /* Get actionList element */
        element = xml_util_get_element (root_element,
                                        "actionList",
                                        NULL);
        if (element)
                introspection->priv->action_hash = get_actions (element);

        /* Get actionList element */
        element = xml_util_get_element (root_element,
                                        "serviceStateTable",
                                        NULL);
        if (element)
                introspection->priv->variable_hash =
                        get_state_variables (element);

        xmlFreeDoc (scpd);
}

static void
collect_action_names (gpointer key,
                      gpointer value,
                      gpointer user_data)
{
        GSList **action_names = (GSList **) user_data;
        char *action_name;
       
        action_name = g_strdup ((char *) key);
        *action_names = g_slist_append (*action_names, action_name);
}

static void
collect_action_arguments (gpointer data,
                          gpointer user_data)
{
        GSList **argument_list = (GSList **) user_data;
        GUPnPServiceActionArgInfo *argument;
        GUPnPServiceActionArgInfo *tmp;
       
        argument = (GUPnPServiceActionArgInfo *) data;

        tmp = g_slice_new (GUPnPServiceActionArgInfo);

        tmp->name = g_strdup (argument->name);
        tmp->direction = g_strdup (argument->direction);
        tmp->related_state_variable =
                g_strdup (argument->related_state_variable);

        *argument_list = g_slist_append (*argument_list, tmp);
}

static void
collect_actions (gpointer key,
                 gpointer value,
                 gpointer user_data)
{
        GSList **action_list = (GSList **) user_data;
        GUPnPServiceActionInfo *action_info;
       
        action_info = g_slice_new0 (GUPnPServiceActionInfo);

        action_info->name = g_strdup ((char *) key);
        g_slist_foreach ((GSList *) value,
                         collect_action_arguments,
                         &action_info->arguments);
        
        *action_list = g_slist_append (*action_list, action_info);
}

static void
collect_allowed_values (gpointer data,
                        gpointer user_data)
{
        GSList **allowed_values = (GSList **) user_data;
        char *tmp;
       
        tmp = g_strdup ((char *) data);

        *allowed_values = g_slist_append (*allowed_values, tmp);
}

static GUPnPServiceStateVariableInfo *
copy_state_variable (GUPnPServiceStateVariableInfo *variable)
{
        GUPnPServiceStateVariableInfo *new_variable;

        new_variable = g_slice_new0 (GUPnPServiceStateVariableInfo);
        
        new_variable->name = g_strdup (variable->name);
        new_variable->send_events = variable->send_events;
        new_variable->data_type = g_strdup (variable->data_type);
        new_variable->gtype = variable->gtype;
        new_variable->is_numeric = variable->is_numeric;

        g_value_init (&new_variable->default_value, variable->gtype);
        g_value_copy (&variable->default_value, &new_variable->default_value);
        if (variable->is_numeric) {
                g_value_init (&new_variable->minimum, variable->gtype);
                g_value_init (&new_variable->maximum, variable->gtype);
                g_value_init (&new_variable->step, variable->gtype);

                g_value_copy (&variable->minimum, &new_variable->minimum);
                g_value_copy (&variable->maximum, &new_variable->maximum);
                g_value_copy (&variable->step, &new_variable->step);
        }
        g_slist_foreach (variable->allowed_values,
                         (GFunc) collect_allowed_values,
                         &new_variable->allowed_values);

        return new_variable;
}

static void
collect_state_variables (gpointer key, gpointer value, gpointer user_data)
{
        GSList **variable_list = (GSList **) user_data;
        GUPnPServiceStateVariableInfo *variable;
      
        variable = copy_state_variable (
                        (GUPnPServiceStateVariableInfo *) value);
        *variable_list = g_slist_append (*variable_list, variable);
}

/**
 * gupnp_service_introspection_new
 * @scpd_url: The URL to the SCPD of the service to create a introspection for
 *
 * Return value: A #GUPnPServiceIntrospection for the service with type @type
 * from the device with UDN @udn, as read from the device description file
 * specified by @doc.
 **/
GUPnPServiceIntrospection *
gupnp_service_introspection_new (const char *scpd_url)
{
        GUPnPServiceIntrospection *introspection;

        g_return_val_if_fail (scpd_url != NULL, NULL);

        introspection = g_object_new (GUPNP_TYPE_SERVICE_INTROSPECTION,
                              "scpd-url", scpd_url,
                              NULL);

        if (introspection->priv->action_hash == NULL) {
                g_warning ("Failed to create introspection data from SCPD "
                           "\"%s\".", scpd_url);

                g_object_unref (introspection);
                introspection = NULL;
        }

        return introspection;
}


/**
 * gupnp_service_introspection_list_action_names
 * @introspection: A #GUPnPServiceIntrospection
 *
 * Returns newly-allocated GSList of names of all the actions in this service.
 *
 * Note that this function retreives the needed information from the service
 * description document provided by the service and hence the list of actions
 * reported by this function is not guaranteed to be complete.
 * 
 * Return value: A GSList of names of all the actions or NULL. g_slist_free()
 * the list and g_free() the contents after use.
 **/
GSList *
gupnp_service_introspection_list_action_names (
                GUPnPServiceIntrospection *introspection)
{
        GSList *action_names = NULL;
      
        if (introspection->priv->action_hash == NULL)
                return NULL;

        g_hash_table_foreach (introspection->priv->action_hash,
                              collect_action_names,
                              &action_names);
        return action_names;
}

/**
 * gupnp_service_introspection_list_action_arguments
 * @introspection: A #GUPnPServiceIntrospection
 *
 * Returns newly-allocated GSList of all the arguments (of type
 * #GUPnPServiceActionArgInfo) of @action_name. 
 *
 * Note that this function retreives the needed information from the service
 * description document provided by the service and hence the list of arguments
 * reported by this function is not guaranteed to be complete.
 * 
 * Return value: A GSList of all the arguments of the @action_name or NULL.
 * g_slist_free() the list and gupnp_service_action_arg_info_free() the
 * list data after use.
**/
GSList *
gupnp_service_introspection_list_action_arguments (
                GUPnPServiceIntrospection *introspection,
                const char                *action_name)
{
        GSList *arguments = NULL;
        GSList *argument_list = NULL;
      
        if (introspection->priv->action_hash == NULL)
                return NULL;

        arguments = g_hash_table_lookup (introspection->priv->action_hash,
                                         action_name);
        g_slist_foreach (arguments,
                         collect_action_arguments,
                         &argument_list);
        
        return argument_list;
}

/**
 * gupnp_service_introspection_list_actions
 * @introspection: A #GUPnPServiceIntrospection
 *
 * Returns newly-allocated GSList of all the actions (of type
 * #GUPnPServiceActionInfo) in this service.
 *
 * Note that this function retreives the needed information from the service
 * description document provided by the service and hence the list of actions
 * reported by this function is not guaranteed to be complete.
 * 
 * Return value: A GSList of all the actions or
 * NULL. g_slist_free() the list and gupnp_service_action_info_free() the
 * contents after use.
 **/
GSList *
gupnp_service_introspection_list_actions (
                GUPnPServiceIntrospection *introspection)
{
        GSList *action_list = NULL;
      
        if (introspection->priv->action_hash == NULL)
                return NULL;

        g_hash_table_foreach (introspection->priv->action_hash,
                              collect_actions,
                              &action_list);
        return action_list;
}

/**
 * gupnp_service_action_arg_info_free
 * @argument: A #GUPnPServiceActionArgInfo
 *
 * Frees a #GUPnPServiceActionArgInfo.
 *
 **/
void
gupnp_service_action_arg_info_free (GUPnPServiceActionArgInfo *argument)
{
        g_free (argument->name);
        g_free (argument->direction);
        g_free (argument->related_state_variable);

        g_slice_free (GUPnPServiceActionArgInfo, argument);
}

/**
 * gupnp_service_action_info_free
 * @argument: A #GUPnPServiceActionInfo
 *
 * Frees a #GUPnPServiceActionInfo.
 *
 **/
void
gupnp_service_action_info_free (GUPnPServiceActionInfo *action_info)
{
        g_free (action_info->name);

        g_slist_foreach (action_info->arguments,
                         (GFunc) gupnp_service_action_arg_info_free,
                         NULL);

        g_slice_free (GUPnPServiceActionInfo, action_info);
}

/**
 * gupnp_service_introspection_list_state_variables
 * @introspection: A #GUPnPServiceIntrospection
 *
 * Returns newly-allocated GSList of all the state variables (of type
 * #GUPnPServiceStateVariableInfo) in this service.
 *
 * Note that this function retreives the needed information from the service
 * description document provided by the service and hence the list of state
 * variables reported by this function is not guaranteed to be complete.
 * 
 * Return value: A GSList of all the state variables or
 * NULL. g_slist_free() the list and gupnp_service_state_variable_info_free()
 * the contents after use.
 **/
GSList *
gupnp_service_introspection_list_state_variables (
                GUPnPServiceIntrospection *introspection)
{
        GSList *variable_list = NULL;
      
        if (introspection->priv->variable_hash == NULL)
                return NULL;

        g_hash_table_foreach (introspection->priv->variable_hash,
                              collect_state_variables,
                              &variable_list);
        return variable_list;
}

/**
 * gupnp_service_state_variable_info_free 
 * @argument: A #GUPnPServiceStateVariableInfo 
 *
 * Frees a #GUPnPServiceStateVariableInfo.
 *
 **/
void
gupnp_service_state_variable_info_free (GUPnPServiceStateVariableInfo *variable)
{
        g_free (variable->name);
        g_free (variable->data_type);
        g_value_unset (&variable->default_value);
        if (variable->is_numeric) {
                g_value_unset (&variable->minimum);
                g_value_unset (&variable->maximum);
                g_value_unset (&variable->step);
        }
        g_slist_foreach (variable->allowed_values,
                         (GFunc) g_free,
                         NULL);
        g_slist_free (variable->allowed_values);
        
        g_slice_free (GUPnPServiceStateVariableInfo, variable);
}
