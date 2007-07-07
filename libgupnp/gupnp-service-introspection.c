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
#include "gupnp-service-introspection-private.h"
#include "xml-util.h"
#include "gupnp-types.h"

#define MAX_FIXED_14_4 99999999999999.9999

G_DEFINE_TYPE (GUPnPServiceIntrospection,
               gupnp_service_introspection,
               G_TYPE_OBJECT);

struct _GUPnPServiceIntrospectionPrivate {
        xmlDoc *scpd;
        
        /* Intropection data */
        GHashTable *action_hash;
        GHashTable *variable_hash;
        
        /* For caching purposes */
        GSList *variable_list;
        GSList *action_list;
        GSList *action_names;
};

enum {
        PROP_0,
        PROP_SCPD
};

static void
contstruct_introspection_info (GUPnPServiceIntrospection *introspection);

/**
 * gupnp_service_state_variable_info_free 
 * @argument: A #GUPnPServiceStateVariableInfo 
 *
 * Frees a #GUPnPServiceStateVariableInfo.
 *
 **/
static void
gupnp_service_state_variable_info_free
                                (GUPnPServiceStateVariableInfo *variable)
{
        g_free (variable->name);
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
        case PROP_SCPD:
                introspection->priv->scpd =
                        g_value_get_pointer (value);

                /* Contruct introspection data */
                contstruct_introspection_info (introspection);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

/**
 * gupnp_service_action_info_free
 * @argument: A #GUPnPServiceActionInfo
 *
 * Frees a #GUPnPServiceActionInfo.
 *
 **/
static void
gupnp_service_action_info_free (GUPnPServiceActionInfo *action_info)
{
        g_slist_free (action_info->arguments);
        g_slice_free (GUPnPServiceActionInfo, action_info);
}

static void
gupnp_service_introspection_finalize (GObject *object)
{
        GUPnPServiceIntrospection *introspection;

        introspection = GUPNP_SERVICE_INTROSPECTION (object);

        if (introspection->priv->variable_list)
                g_slist_free (introspection->priv->variable_list);
        
        if (introspection->priv->action_list) {
                g_slist_foreach (introspection->priv->action_list,
                                 (GFunc) gupnp_service_action_info_free,
                                 NULL);
                g_slist_free (introspection->priv->action_list);
        }
        
        if (introspection->priv->action_names)
                g_slist_free (introspection->priv->action_names);
        
        if (introspection->priv->action_hash)
                g_hash_table_destroy (introspection->priv->action_hash);

        if (introspection->priv->variable_hash)
                g_hash_table_destroy (introspection->priv->variable_hash);

        xmlFreeDoc (introspection->priv->scpd);
}

static void
gupnp_service_introspection_class_init (GUPnPServiceIntrospectionClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_service_introspection_set_property;
        object_class->finalize     = gupnp_service_introspection_finalize;

        g_type_class_add_private (klass,
                                  sizeof (GUPnPServiceIntrospectionPrivate));

        /**
         * GUPnPServiceIntrospection:scpd
         *
         * The scpd of the device description file.
         **/
        g_object_class_install_property
                (object_class,
                 PROP_SCPD,
                 g_param_spec_pointer ("scpd",
                                       "SCPD",
                                       "Pointer to SCPD",
                                       G_PARAM_WRITABLE |
                                       G_PARAM_CONSTRUCT_ONLY));
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

        limit_str = xml_util_get_child_element_content (limit_node,
                                                        limit_name);
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
        } else if (variable->type == G_TYPE_STRING) {
                limit_node = xml_util_get_element (variable_node,
                                                   "allowedValueList",
                                                   NULL);
                if (limit_node == NULL)
                        return;

                set_string_value_limits (limit_node,
                                         &(variable->allowed_values));
       }
}

static gboolean
set_variable_type (GUPnPServiceStateVariableInfo *variable,
                   char                          *data_type)
{
        GType type;

        if (strcmp ("string", data_type) == 0) {
                type = G_TYPE_STRING;
        }
        
        else if (strcmp ("char", data_type) == 0) {
                type = G_TYPE_CHAR;
        }

        else if (strcmp ("boolean", data_type) == 0) {
                type = G_TYPE_BOOLEAN;
        }

        else if (strcmp ("i1", data_type) == 0) {
                type = G_TYPE_INT;
                g_value_init (&variable->minimum, type);
                g_value_set_int (&variable->minimum, G_MININT8);
                g_value_init (&variable->maximum, type);
                g_value_set_int (&variable->maximum, G_MAXINT8);
                variable->is_numeric = TRUE;
        }
         
        else if (strcmp ("i2", data_type) == 0) {
                type = G_TYPE_INT;
                g_value_init (&variable->minimum, type);
                g_value_set_int (&variable->minimum, G_MININT16);
                g_value_init (&variable->maximum, type);
                g_value_set_int (&variable->maximum, G_MAXINT16);
                variable->is_numeric = TRUE;
        }
        
        else if (strcmp ("i4", data_type) == 0 ||
                 strcmp ("int", data_type) == 0) {
                type = G_TYPE_INT;
                g_value_init (&variable->minimum, type);
                g_value_set_int (&variable->minimum, G_MININT32);
                g_value_init (&variable->maximum, type);
                g_value_set_int (&variable->maximum, G_MAXINT32);
                variable->is_numeric = TRUE;
        }

        else if (strcmp ("ui1", data_type) == 0) {
                type = G_TYPE_UINT;
                g_value_init (&variable->minimum, type);
                g_value_set_uint (&variable->minimum, 0);
                g_value_init (&variable->maximum, type);
                g_value_set_uint (&variable->maximum, G_MAXUINT8);
                variable->is_numeric = TRUE;
        }
        
        else if (strcmp ("ui2", data_type) == 0) {
                type = G_TYPE_UINT;
                g_value_init (&variable->minimum, type);
                g_value_set_uint (&variable->minimum, 0);
                g_value_init (&variable->maximum, type);
                g_value_set_uint (&variable->maximum, G_MAXUINT16);
                variable->is_numeric = TRUE;
        }
        
        else if (strcmp ("ui4", data_type) == 0) {
                type = G_TYPE_UINT;
                g_value_init (&variable->minimum, type);
                g_value_set_uint (&variable->minimum, 0);
                g_value_init (&variable->maximum, type);
                g_value_set_uint (&variable->maximum, G_MAXUINT32);
                variable->is_numeric = TRUE;
        }
        
        else if (strcmp ("r4", data_type) == 0) {
                type = G_TYPE_FLOAT;
                g_value_init (&variable->minimum, type);
                g_value_set_float (&variable->minimum, -G_MAXFLOAT);
                g_value_init (&variable->maximum, type);
                g_value_set_float (&variable->maximum, G_MAXFLOAT);
                variable->is_numeric = TRUE;
        }
        
        else if (strcmp ("r8", data_type) == 0 ||
                 strcmp ("number", data_type) == 0) {
                type = G_TYPE_DOUBLE;
                g_value_init (&variable->minimum, type);
                g_value_set_double (&variable->minimum,  -G_MAXDOUBLE);
                g_value_init (&variable->maximum, type);
                g_value_set_double (&variable->maximum, G_MAXDOUBLE);
                variable->is_numeric = TRUE;
        }
        
        else if (strcmp ("fixed.14.4", data_type) == 0) {
                /* Just how did this get into the UPnP specs? */
                type = G_TYPE_DOUBLE;
                g_value_init (&variable->minimum, type);
                g_value_set_double (&variable->minimum,  -MAX_FIXED_14_4);
                g_value_init (&variable->maximum, type);
                g_value_set_double (&variable->maximum, MAX_FIXED_14_4);
                variable->is_numeric = TRUE;
        }

        else {
                type = gupnp_data_type_to_gtype (data_type);
        }

        if (type == G_TYPE_INVALID) {
                g_warning ("Unkown type '%s' in the SCPD", data_type);
                return FALSE;
        }

        if (variable->is_numeric)
                g_value_init (&variable->step, type);
        g_value_init (&variable->default_value, type);
        variable->type = type;
        
        return TRUE;
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
        gboolean success;

        data_type = xml_util_get_child_element_content_glib (variable_node,
                                                             "dataType");
        if (!data_type) {
                /* We can't report much about a state variable whose dataType
                 * is not specified so better not report anything at all */
                return NULL;
        }
        
        variable = g_slice_new0 (GUPnPServiceStateVariableInfo);

        success = set_variable_type (variable, data_type);
        g_free (data_type);
        if (!success)
                return NULL;

        set_variable_limits (variable_node, variable);
        set_default_value (variable_node, variable);

        send_events = xml_util_get_child_element_content
                                       (variable_node, "sendEventsAttribute");
        if (send_events == NULL) {
                /* Some documents put it as attribute of the tag */
                send_events = xml_util_get_attribute_contents (variable_node,
                                                               "sendEvents");
        }

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
        char *name, *state_var;
        xmlChar *direction;
        
        name      = xml_util_get_child_element_content_glib
                                       (argument_node, "name");
        state_var = xml_util_get_child_element_content_glib
                                       (argument_node, "relatedStateVariable");
        direction = xml_util_get_child_element_content
                                       (argument_node, "direction");

        if (!name || !state_var || !direction) {
                g_free (name);
                g_free (state_var);

                xmlFree (direction);

                return NULL;
        }

        argument = g_slice_new0 (GUPnPServiceActionArgInfo);

        argument->name = name;
        argument->related_state_variable = state_var;

        if (strcmp ("in", (char *) direction) == 0)
                argument->direction = GUPNP_SERVICE_ACTION_ARG_DIRECTION_IN;
        else
                argument->direction = GUPNP_SERVICE_ACTION_ARG_DIRECTION_OUT;
        xmlFree (direction);

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
        g_free (argument->related_state_variable);

        g_slice_free (GUPnPServiceActionArgInfo, argument);
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

                if (strcmp ("stateVariable",
                            (char *) variable_node->name) != 0)
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
        xmlNode *root_element, *element;

        g_return_if_fail (introspection->priv->scpd != NULL);

        root_element = xml_util_get_element (
                        (xmlNode *) introspection->priv->scpd,
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
}

static void
collect_action_names (gpointer key,
                      gpointer value,
                      gpointer user_data)
{
        GSList **action_names = (GSList **) user_data;
        char *action_name;
       
        action_name = (char *) key;
        *action_names = g_slist_append (*action_names, action_name);
}

static void
collect_action_arguments (gpointer data,
                          gpointer user_data)
{
        GSList **argument_list = (GSList **) user_data;
        GUPnPServiceActionArgInfo *argument;
       
        argument = (GUPnPServiceActionArgInfo *) data;

        *argument_list = g_slist_append (*argument_list, argument);
}

static void
collect_actions (gpointer key,
                 gpointer value,
                 gpointer user_data)
{
        GSList **action_list = (GSList **) user_data;
        GUPnPServiceActionInfo *action_info;
       
        action_info = g_slice_new0 (GUPnPServiceActionInfo);

        action_info->name = (char *) key;
        g_slist_foreach ((GSList *) value,
                         collect_action_arguments,
                         &action_info->arguments);
        
        *action_list = g_slist_append (*action_list, action_info);
}

static void
collect_state_variables (gpointer key, gpointer value, gpointer user_data)
{
        GSList **variable_list = (GSList **) user_data;
        GUPnPServiceStateVariableInfo *variable;
      
        variable = (GUPnPServiceStateVariableInfo *) value;
        *variable_list = g_slist_append (*variable_list, variable);
}

/**
 * gupnp_service_introspection_new
 * @scpd: Pointer to the SCPD of the service to create a introspection for
 *
 * Return value: A #GUPnPServiceIntrospection for the service created from the
 * SCPD @scpd or NULL.
 *
 **/
GUPnPServiceIntrospection *
gupnp_service_introspection_new (xmlDoc *scpd)
{
        GUPnPServiceIntrospection *introspection;

        g_return_val_if_fail (scpd != NULL, NULL);

        introspection = g_object_new (GUPNP_TYPE_SERVICE_INTROSPECTION,
                                      "scpd", scpd,
                                      NULL);

        if (introspection->priv->action_hash == NULL) {
                g_object_unref (introspection);
                introspection = NULL;
        }

        return introspection;
}

/**
 * gupnp_service_introspection_list_action_names
 * @introspection: A #GUPnPServiceIntrospection
 *
 * Returns a GSList of names of all the actions in this service.
 *
 * Note that this function retreives the needed information from the service
 * description document provided by the service and hence the list of actions
 * reported by this function is not guaranteed to be complete.
 * 
 * Return value: A GSList of names of all the actions or NULL. Do not modify
 * or free it or it's contents.
 **/
const GSList *
gupnp_service_introspection_list_action_names (
                GUPnPServiceIntrospection *introspection)
{
        if (introspection->priv->action_hash == NULL)
                return NULL;

        if (introspection->priv->action_names == NULL) {
                g_hash_table_foreach (introspection->priv->action_hash,
                                      collect_action_names,
                                      &introspection->priv->action_names);
        }

        return introspection->priv->action_names;
}

/**
 * gupnp_service_introspection_list_action_arguments
 * @introspection: A #GUPnPServiceIntrospection
 *
 * Returns a GSList of all the arguments (of type #GUPnPServiceActionArgInfo)
 * of @action_name. 
 *
 * Note that this function retreives the needed information from the service
 * description document provided by the service and hence the list of arguments
 * reported by this function is not guaranteed to be complete.
 * 
 * Return value: A GSList of all the arguments of the @action_name or NULL. Do
 * not modify or free it or it's contents.
 *
**/
const GSList *
gupnp_service_introspection_list_action_arguments (
                GUPnPServiceIntrospection *introspection,
                const char                *action_name)
{
        if (introspection->priv->action_hash == NULL)
                return NULL;

        return g_hash_table_lookup (introspection->priv->action_hash,
                                    action_name);
}

/**
 * gupnp_service_introspection_list_actions
 * @introspection: A #GUPnPServiceIntrospection
 *
 * Returns a GSList of all the actions (of type #GUPnPServiceActionInfo) in
 * this service.
 *
 * Note that this function retreives the needed information from the service
 * description document provided by the service and hence the list of actions
 * reported by this function is not guaranteed to be complete.
 * 
 * Return value: A GSList of all the actions or NULL. Do not modify or free it
 * or it's contents.
 *
 **/
const GSList *
gupnp_service_introspection_list_actions (
                GUPnPServiceIntrospection *introspection)
{
        if (introspection->priv->action_hash == NULL)
                return NULL;
        
        if (introspection->priv->action_list == NULL) {
                g_hash_table_foreach (introspection->priv->action_hash,
                                      collect_actions,
                                      &introspection->priv->action_list);
        }

        return introspection->priv->action_list;
}

/**
 * gupnp_service_introspection_list_state_variables
 * @introspection: A #GUPnPServiceIntrospection
 *
 * Returns a GSList of all the state variables (of type
 * #GUPnPServiceStateVariableInfo) in this service.
 *
 * Note that this function retreives the needed information from the service
 * description document provided by the service and hence the list of state
 * variables reported by this function is not guaranteed to be complete.
 * 
 * Return value: A GSList of all the state variables or NULL. Do not modify or
 * free it or it's contents.
 * 
 **/
const GSList *
gupnp_service_introspection_list_state_variables (
                GUPnPServiceIntrospection *introspection)
{
        if (introspection->priv->variable_hash == NULL)
                return NULL;

        if (introspection->priv->variable_list == NULL) {
                g_hash_table_foreach (introspection->priv->variable_hash,
                                      collect_state_variables,
                                      &introspection->priv->variable_list);
        }

        return introspection->priv->variable_list;
}

/**
 * gupnp_service_introspection_get_state_variable
 * @introspection: A #GUPnPServiceIntrospection
 *
 * Returns a pointer to the state variable (of type #GUPnPServiceStateVariable)
 * in this service with the name @variable_name.
 *
 * Note that this function retreives the needed information from the service
 * description document provided by the service and hence the list of arguments
 * reported by this function is not guaranteed to be complete.
 * 
 * Return value: The pointer to the variable by the name @action_name or NULL.
 * Do not modify or free it.
 *
**/
const GUPnPServiceStateVariableInfo *
gupnp_service_introspection_get_state_variable
                                (GUPnPServiceIntrospection *introspection,
                                 const char                *variable_name)
{
        if (introspection->priv->action_hash == NULL)
                return NULL;

        return g_hash_table_lookup (introspection->priv->variable_hash,
                                    variable_name);
}


