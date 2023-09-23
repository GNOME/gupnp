/*
 * Copyright (C) 2007 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */


#include <config.h>
#include <libsoup/soup.h>
#include <string.h>

#include "gupnp-service-introspection.h"
#include "gupnp-service-introspection-private.h"
#include "gupnp-error.h"
#include "xml-util.h"
#include "gvalue-util.h"
#include "gupnp-types.h"
#include "gupnp-types-private.h"

#define MAX_FIXED_14_4 99999999999999.9999

struct _GUPnPServiceIntrospection {
        GObject parent;
};

struct _GUPnPServiceIntrospectionPrivate {
        xmlDoc *scpd;
        GList *variables;
        GList *actions;

        /* For caching purposes */
        GList *action_names;
        GList *variable_names;
};

typedef struct _GUPnPServiceIntrospectionPrivate
                GUPnPServiceIntrospectionPrivate;


static GInitableIface *initable_parent_iface = NULL;
static void
gupnp_service_introspection_initable_iface_init (gpointer g_iface, gpointer iface_data);


/**
 * GUPnPServiceIntrospection:
 *
 * Introspection of local and remote services..
 *
 * The #GUPnPServiceIntrospection class provides methods for service
 * introspection based on information contained in its service description
 * document (SCPD). There is no constructor provided for this class, please use
 * [method@GUPnP.ServiceInfo.introspect_async] to create a
 * #GUPnPServiceIntrospection object for a specific service.
 *
 * Note that all the introspection information is retrieved from the service
 * description document (SCPD) provided by the service and hence can not be
 * guaranteed to be complete. An UPnP service is required to provide a SCPD but
 * unfortunately, many services either do not provide this document or the
 * document does not provide any or all of the introspection information.
 *
 * This class exposes internals of the UPnP protocol and should not need
 * to be used for regular device or control point development.
 *
 **/
G_DEFINE_TYPE_EXTENDED (
        GUPnPServiceIntrospection,
        gupnp_service_introspection,
        G_TYPE_OBJECT,
        0,
        G_ADD_PRIVATE (GUPnPServiceIntrospection) G_IMPLEMENT_INTERFACE (
                G_TYPE_INITABLE,
                gupnp_service_introspection_initable_iface_init))
enum {
        PROP_0,
        PROP_SCPD
};

static gboolean
construct_introspection_info (GUPnPServiceIntrospection *introspection,
                              GError                    **error);

/**
 * gupnp_service_state_variable_info_free:
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

        g_list_free_full (variable->allowed_values, g_free);
}

static void
gupnp_service_state_variable_info_free (
        GUPnPServiceStateVariableInfo *variable);

static void
gupnp_service_state_variable_info_relase (GUPnPServiceStateVariableInfo *info)
{
        g_rc_box_release_full (info, (GDestroyNotify)gupnp_service_state_variable_info_free);
}

G_DEFINE_BOXED_TYPE(GUPnPServiceStateVariableInfo,
                     gupnp_service_state_variable_info,
                     g_rc_box_acquire,
                     gupnp_service_state_variable_info_relase)

static void
gupnp_service_introspection_init (GUPnPServiceIntrospection *introspection)
{
}

static void
gupnp_service_introspection_set_property (GObject      *object,
                                          guint        property_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
        GUPnPServiceIntrospection *introspection;

        introspection = GUPNP_SERVICE_INTROSPECTION (object);
        GUPnPServiceIntrospectionPrivate *priv =
                gupnp_service_introspection_get_instance_private (introspection);

        switch (property_id) {
        case PROP_SCPD:
                priv->scpd = g_value_get_pointer (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}


/**
 * gupnp_service_action_arg_info_free:
 * @argument: A #GUPnPServiceActionArgInfo
 *
 * Frees a #GUPnPServiceActionArgInfo.
 *
 **/
static void
gupnp_service_action_arg_info_free (GUPnPServiceActionArgInfo *argument)
{
        g_free (argument->name);
        g_free (argument->related_state_variable);
}

static void
gupnp_service_action_arg_info_unref (GUPnPServiceActionArgInfo *argument)
{
        g_rc_box_release_full (
                argument,
                (GDestroyNotify) gupnp_service_action_arg_info_free);
}

G_DEFINE_BOXED_TYPE (GUPnPServiceActionArgInfo,
                     gupnp_service_action_arg_info,
                     g_rc_box_acquire,
                     gupnp_service_action_arg_info_unref)


/**
 * gupnp_service_action_info_free:
 * @argument: A #GUPnPServiceActionInfo
 *
 * Frees a #GUPnPServiceActionInfo.
 *
 **/
static void
gupnp_service_action_info_free (GUPnPServiceActionInfo *action_info)
{
        g_free (action_info->name);
        g_list_free_full (action_info->arguments,
                          (GDestroyNotify) gupnp_service_action_arg_info_unref);
}


static void
gupnp_service_action_info_unref (GUPnPServiceActionInfo *argument)
{
        g_rc_box_release_full (
                argument,
                (GDestroyNotify) gupnp_service_action_info_free);
}

G_DEFINE_BOXED_TYPE (GUPnPServiceActionInfo,
                     gupnp_service_action_info,
                     g_rc_box_acquire,
                     gupnp_service_action_info_unref)

static void
gupnp_service_introspection_finalize (GObject *object)
{
        GUPnPServiceIntrospection *introspection;
        GUPnPServiceIntrospectionPrivate *priv;

        introspection = GUPNP_SERVICE_INTROSPECTION (object);
        priv = gupnp_service_introspection_get_instance_private (introspection);

        g_list_free_full (priv->variables,
                          (GDestroyNotify) gupnp_service_state_variable_info_relase);

        g_list_free_full (priv->actions,
                          (GDestroyNotify) gupnp_service_action_info_unref);

        /* Contents don't need to be freed, they were owned by priv->variables
         */
        if (priv->variable_names)
                g_list_free (priv->variable_names);

        /* Contents don't need to be freed, they were owned by priv->actions
         */
        if (priv->action_names)
                g_list_free (priv->action_names);

        GObjectClass *object_class =
                G_OBJECT_CLASS (gupnp_service_introspection_parent_class);
        object_class->finalize (object);
}

static gboolean
gupnp_service_introspection_initable_init (GInitable *initable,
                                           GCancellable *cancellable,
                                           GError **error)
{
        GUPnPServiceIntrospection *self =
                GUPNP_SERVICE_INTROSPECTION (initable);
        GUPnPServiceIntrospectionPrivate *priv =
                gupnp_service_introspection_get_instance_private (self);

        if (priv->scpd == NULL) {
                g_set_error_literal (
                        error,
                        GUPNP_SERVICE_INTROSPECTION_ERROR,
                        GUPNP_SERVICE_INTROSPECTION_ERROR_OTHER,
                        "SCDP is required for creating a ServiceIntrospection");

                return FALSE;
        }

        if (priv->actions != NULL || priv->variables != NULL)
                return TRUE;

        return construct_introspection_info (self, error);
}

static void
gupnp_service_introspection_initable_iface_init (gpointer g_iface,
                                                 gpointer iface_data)
{
        GInitableIface *iface = (GInitableIface *) g_iface;
        initable_parent_iface = g_type_interface_peek_parent (iface);
        iface->init = gupnp_service_introspection_initable_init;
}


static void
gupnp_service_introspection_class_init (GUPnPServiceIntrospectionClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = gupnp_service_introspection_set_property;
        object_class->finalize     = gupnp_service_introspection_finalize;

        /**
         * GUPnPServiceIntrospection:scpd:
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
set_default_value (xmlNodePtr                     variable_node,
                   GUPnPServiceStateVariableInfo *variable)
{
        xmlChar *default_str;

        default_str = xml_util_get_child_element_content (variable_node,
                                                          "defaultValue");
        if (default_str) {
                gvalue_util_set_value_from_string (&variable->default_value,
                                                   (char *) default_str);

                xmlFree (default_str);
        }
}

static void
set_string_value_limits (xmlNodePtr   limit_node,
                         GList     **limits)
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

                *limits = g_list_append (*limits,
                                          g_strdup ((char *) allowed_value));
                xmlFree (allowed_value);
        }
}

static void
set_value_limit_by_name (xmlNodePtr  limit_node,
                         GValue     *limit,
                         const char *limit_name)
{
        xmlChar *limit_str;

        limit_str = xml_util_get_child_element_content (limit_node,
                                                        limit_name);
        if (limit_str) {
                gvalue_util_set_value_from_string (limit, (char *) limit_str);

                xmlFree (limit_str);
        }
}

static void
set_variable_limits (xmlNodePtr                     variable_node,
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
                g_value_init (&variable->step, type);
                g_value_set_int (&variable->step, 1);
                variable->is_numeric = TRUE;
        }

        else if (strcmp ("i2", data_type) == 0) {
                type = G_TYPE_INT;
                g_value_init (&variable->minimum, type);
                g_value_set_int (&variable->minimum, G_MININT16);
                g_value_init (&variable->maximum, type);
                g_value_set_int (&variable->maximum, G_MAXINT16);
                g_value_init (&variable->step, type);
                g_value_set_int (&variable->step, 1);
                variable->is_numeric = TRUE;
        }

        else if (strcmp ("i4", data_type) == 0 ||
                 strcmp ("int", data_type) == 0) {
                type = G_TYPE_INT;
                g_value_init (&variable->minimum, type);
                g_value_set_int (&variable->minimum, G_MININT32);
                g_value_init (&variable->maximum, type);
                g_value_set_int (&variable->maximum, G_MAXINT32);
                g_value_init (&variable->step, type);
                g_value_set_int (&variable->step, 1);
                variable->is_numeric = TRUE;
        }

        else if (strcmp ("ui1", data_type) == 0) {
                type = G_TYPE_UINT;
                g_value_init (&variable->minimum, type);
                g_value_set_uint (&variable->minimum, 0);
                g_value_init (&variable->maximum, type);
                g_value_set_uint (&variable->maximum, G_MAXUINT8);
                g_value_init (&variable->step, type);
                g_value_set_uint (&variable->step, 1);
                variable->is_numeric = TRUE;
        }

        else if (strcmp ("ui2", data_type) == 0) {
                type = G_TYPE_UINT;
                g_value_init (&variable->minimum, type);
                g_value_set_uint (&variable->minimum, 0);
                g_value_init (&variable->maximum, type);
                g_value_set_uint (&variable->maximum, G_MAXUINT16);
                g_value_init (&variable->step, type);
                g_value_set_uint (&variable->step, 1);
                variable->is_numeric = TRUE;
        }

        else if (strcmp ("ui4", data_type) == 0) {
                type = G_TYPE_UINT;
                g_value_init (&variable->minimum, type);
                g_value_set_uint (&variable->minimum, 0);
                g_value_init (&variable->maximum, type);
                g_value_set_uint (&variable->maximum, G_MAXUINT32);
                g_value_init (&variable->step, type);
                g_value_set_uint (&variable->step, 1);
                variable->is_numeric = TRUE;
        }

        else if (strcmp ("r4", data_type) == 0) {
                type = G_TYPE_FLOAT;
                g_value_init (&variable->minimum, type);
                g_value_set_float (&variable->minimum, -G_MAXFLOAT);
                g_value_init (&variable->maximum, type);
                g_value_set_float (&variable->maximum, G_MAXFLOAT);
                g_value_init (&variable->step, type);
                g_value_set_float (&variable->step, 1.0);
                variable->is_numeric = TRUE;
        }

        else if (strcmp ("r8", data_type) == 0 ||
                 strcmp ("number", data_type) == 0) {
                type = G_TYPE_DOUBLE;
                g_value_init (&variable->minimum, type);
                g_value_set_double (&variable->minimum,  -G_MAXDOUBLE);
                g_value_init (&variable->maximum, type);
                g_value_set_double (&variable->maximum, G_MAXDOUBLE);
                g_value_init (&variable->step, type);
                g_value_set_double (&variable->step, 1.0);
                variable->is_numeric = TRUE;
        }

        else if (strcmp ("fixed.14.4", data_type) == 0) {
                /* Just how did this get into the UPnP specs? */
                type = G_TYPE_DOUBLE;
                g_value_init (&variable->minimum, type);
                g_value_set_double (&variable->minimum,  -MAX_FIXED_14_4);
                g_value_init (&variable->maximum, type);
                g_value_set_double (&variable->maximum, MAX_FIXED_14_4);
                g_value_init (&variable->step, type);
                g_value_set_double (&variable->step, 1.0);
                variable->is_numeric = TRUE;
        }
        /* TODO: "float", "int" */

        else {
                type = gupnp_data_type_to_gtype (data_type);
        }

        if (type == G_TYPE_INVALID) {
                g_warning ("Unknown type '%s' in the SCPD", data_type);
                return FALSE;
        }

        g_value_init (&variable->default_value, type);
        variable->type = type;

        return TRUE;
}

/*
 *
 * Creates a #GUPnPServiceStateVariableInfo, constructed from the stateVariable
 * node @variable_node in the SCPD document
 *
 */
static GUPnPServiceStateVariableInfo *
get_state_variable (xmlNodePtr variable_node)
{
        GUPnPServiceStateVariableInfo *variable;
        char *send_events;
        char *data_type;
        gboolean success;

        data_type = xml_util_get_child_element_content_glib (variable_node,
                                                             "dataType");
        if (!data_type) {
                /* We can't report much about a state variable whose dataType
                 * is not specified so better not report anything at all */
                return NULL;
        }

        variable = g_rc_box_new0(GUPnPServiceStateVariableInfo);

        success = set_variable_type (variable, data_type);
        g_free (data_type);
        if (!success) {
                return NULL;
        }

        set_variable_limits (variable_node, variable);
        set_default_value (variable_node, variable);

        send_events =
                xml_util_get_child_element_content_glib (variable_node,
                                                         "sendEventsAttribute");
        if (send_events == NULL) {
                xmlChar *attr = xml_util_get_attribute_contents (variable_node,
                                                                 "sendEvents");
                /* Some documents put it as attribute of the tag */
                send_events = g_strdup ((char *) attr);
                xmlFree (attr);
        }

        if (send_events) {
                variable->send_events =
                        g_str_equal ("yes", g_strstrip (send_events));
                g_free (send_events);
        }

        return variable;
}

/*
 *
 * Creates a #GUPnPServiceActionArgInfo, constructed from the argument node
 * @argument_node in the SCPD document
 *
 */
static GUPnPServiceActionArgInfo *
get_action_argument (xmlNodePtr argument_node)
{
        GUPnPServiceActionArgInfo *argument;
        char *name = NULL;
        char *state_var = NULL;
        char *direction = NULL;

        name = xml_util_get_child_element_content_glib (argument_node, "name");
        state_var = xml_util_get_child_element_content_glib (
                argument_node,
                "relatedStateVariable");
        direction = xml_util_get_child_element_content_glib (argument_node,
                                                             "direction");

        if (!name || !state_var || !direction) {
                g_free (name);
                g_free (state_var);
                g_free (direction);

                return NULL;
        }

        argument = g_rc_box_new0 (GUPnPServiceActionArgInfo);

        argument->name = g_strstrip (name);
        argument->related_state_variable = g_strstrip (state_var);

        if (strcmp ("in", g_strstrip (direction)) == 0) {
                argument->direction = GUPNP_SERVICE_ACTION_ARG_DIRECTION_IN;
        } else {
                argument->direction = GUPNP_SERVICE_ACTION_ARG_DIRECTION_OUT;
        }
        g_free (direction);

        if (xml_util_get_element (argument_node, "retval", NULL) != NULL) {
                argument->retval = TRUE;
        }

        return argument;
}

/*
 *
 * Creates a #GList of all the arguments (of type #GUPnPServiceActionArgInfo)
 * from the action node @action_node in the SCPD document
 *
 */
static GList *
get_action_arguments (xmlNodePtr action_node)
{
        GList *argument_list = NULL;
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
                        argument_list = g_list_append (argument_list,
                                                        argument);
                }
        }

        return argument_list;
}

/*
 *
 * Creates a #GList of all the actions (of type #GUPnPServiceActionInfo) from
 * the SCPD document.
 *
 */
static GList *
get_actions (xmlNode *list_element)
{
        GList *actions = NULL;
        xmlNodePtr action_node;

        /* Iterate over all action elements */
        for (action_node = list_element->children;
             action_node;
             action_node = action_node->next) {
                GUPnPServiceActionInfo *action_info;
                char *name;

                if (strcmp ("action", (char *) action_node->name) != 0)
                        continue;

                name = xml_util_get_child_element_content_glib (action_node,
                                                                "name");
                if (!name)
                        continue;

                action_info = g_rc_box_new0 (GUPnPServiceActionInfo);
                action_info->name = name;
                action_info->arguments = get_action_arguments (action_node);

                actions = g_list_append (actions, action_info);
        }

        return actions;
}

/*
 *
 * Creates a #GList of all the state variables (of type
 * #GUPnPServiceStateVariableInfo) from the SCPD document.
 *
 */
static GList *
get_state_variables (xmlNode *list_element)
{
        GList *variables = NULL;
        xmlNodePtr variable_node;

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

                variable->name = g_strstrip (name);
                variables = g_list_append (variables, variable);
        }

        return variables;
}

/*
 * Creates the #GHashTable's of action and state variable information
 *
 * */
static gboolean
construct_introspection_info (GUPnPServiceIntrospection *introspection,
                              GError **error)
{
        xmlNode *element;
        GUPnPServiceIntrospectionPrivate *priv;

        priv = gupnp_service_introspection_get_instance_private (introspection);
        /* Get actionList element */
        element = xml_util_get_element ((xmlNode *) priv->scpd,
                                        "scpd",
                                        "actionList",
                                        NULL);
        if (element)
                priv->actions = get_actions (element);

        /* Get serviceStateTable element */
        element = xml_util_get_element ((xmlNode *) priv->scpd,
                                        "scpd",
                                        "serviceStateTable",
                                        NULL);
        if (element)
                priv->variables = get_state_variables (element);

        if (priv->actions == NULL && priv->variables == NULL) {
                g_set_error_literal (error,
                                     GUPNP_SERVICE_INTROSPECTION_ERROR,
                                     GUPNP_SERVICE_INTROSPECTION_ERROR_OTHER,
                                     "Service description has neither actions "
                                     "nor variables");
                return FALSE;
        }

        return TRUE;
}

static void
collect_action_names (gpointer data,
                      gpointer user_data)
{
        GList **action_names = (GList **) user_data;
        GUPnPServiceActionInfo *action_info = (GUPnPServiceActionInfo *) data;

        *action_names = g_list_append (*action_names, action_info->name);
}

static void
collect_variable_names (gpointer data,
                        gpointer user_data)
{
        GList **variable_names = (GList **) user_data;
        GUPnPServiceStateVariableInfo *variable =
                (GUPnPServiceStateVariableInfo *) data;

        *variable_names = g_list_append (*variable_names, variable->name);
}

/**
 * gupnp_service_introspection_new:
 * @scpd: Pointer to the SCPD of the service to create an introspection for
 *
 * Create a new #GUPnPServiceIntrospection for the service created from the
 * SCPD @scpd or %NULL.
 *
 * Return value: A new #GUPnPServiceIntrospection.
 **/
GUPnPServiceIntrospection *
gupnp_service_introspection_new (xmlDoc *scpd, GError **error)
{
        g_return_val_if_fail (scpd != NULL, NULL);

        return g_initable_new (GUPNP_TYPE_SERVICE_INTROSPECTION,
                                        NULL,
                                        error,
                                        "scpd",
                                        scpd,
                                        NULL);
}

/**
 * gupnp_service_introspection_list_action_names:
 * @introspection: A #GUPnPServiceIntrospection
 *
 * Returns a GList of names of all the actions in this service.
 *
 * Return value: (transfer none) (element-type utf8) : A GList of names of all
 * the actions or %NULL. Do not modify or free it or its contents.
 **/
const GList *
gupnp_service_introspection_list_action_names
                        (GUPnPServiceIntrospection *introspection)
{
        GUPnPServiceIntrospectionPrivate *priv;

        priv = gupnp_service_introspection_get_instance_private (introspection);

        if (priv->actions == NULL)
                return NULL;

        if (priv->action_names == NULL) {
                g_list_foreach (priv->actions,
                                collect_action_names,
                                &priv->action_names);
        }

        return priv->action_names;
}

/**
 * gupnp_service_introspection_list_actions:
 * @introspection: A #GUPnPServiceIntrospection
 *
 * Returns a #GList of all the actions (of type #GUPnPServiceActionInfo) in
 * this service.
 *
 * Return value: (element-type GUPnP.ServiceActionInfo) (transfer none): A
 * #GList of all the actions or %NULL. Do not modify or free it or its
 * contents.
 **/
const GList *
gupnp_service_introspection_list_actions
                        (GUPnPServiceIntrospection *introspection)
{
        GUPnPServiceIntrospectionPrivate *priv;

        priv = gupnp_service_introspection_get_instance_private (introspection);

        return priv->actions;
}

/**
 * gupnp_service_introspection_list_state_variables:
 * @introspection: A #GUPnPServiceIntrospection
 *
 * Returns a GList of all the state variables (of type
 * #GUPnPServiceStateVariableInfo) in this service.
 *
 * Return value: (element-type GUPnP.ServiceStateVariableInfo) (transfer none):
 * A #GList of all the state variables or %NULL. Do not modify or free it or
 * its contents.
 *
 **/
const GList *
gupnp_service_introspection_list_state_variables
                        (GUPnPServiceIntrospection *introspection)
{
        GUPnPServiceIntrospectionPrivate *priv;

        priv = gupnp_service_introspection_get_instance_private (introspection);

        return priv->variables;
}

/**
 * gupnp_service_introspection_list_state_variable_names:
 * @introspection: A #GUPnPServiceIntrospection
 *
 * Returns a #GList of names of all the state variables in this service.
 *
 * Return value: (element-type utf8) (transfer none): A #GList of names of all
 * the state variables or %NULL. Do not modify or free it or its contents.
 **/
const GList *
gupnp_service_introspection_list_state_variable_names
                        (GUPnPServiceIntrospection *introspection)
{
        GUPnPServiceIntrospectionPrivate *priv;

        priv = gupnp_service_introspection_get_instance_private (introspection);

        if (priv->variables == NULL)
                return NULL;

        if (priv->variable_names == NULL) {
                g_list_foreach (priv->variables,
                                collect_variable_names,
                                &priv->variable_names);
        }

        return priv->variable_names;
}

static gint
state_variable_search_func (GUPnPServiceStateVariableInfo *variable,
                            const gchar                   *variable_name)
{
        return strcmp (variable->name, variable_name);
}

/**
 * gupnp_service_introspection_get_state_variable:
 * @introspection: A #GUPnPServiceIntrospection
 * @variable_name: The name of the variable to retrieve
 *
 * Returns the state variable by the name @variable_name in this service.
 *
 * Return value: (transfer none)(nullable): the state variable or %NULL. Do not modify or
 * free it.
 **/
const GUPnPServiceStateVariableInfo *
gupnp_service_introspection_get_state_variable
                        (GUPnPServiceIntrospection *introspection,
                         const gchar               *variable_name)
{
        GList *variable_node;
        GUPnPServiceIntrospectionPrivate *priv;

        priv = gupnp_service_introspection_get_instance_private (introspection);

        if (priv->variables == NULL)
                return NULL;

        variable_node = g_list_find_custom (
                                priv->variables,
                                (gpointer) variable_name,
                                (GCompareFunc) state_variable_search_func);
        if (variable_node == NULL)
                return NULL;

        return (GUPnPServiceStateVariableInfo *) variable_node->data;
}

static gint
action_search_func (GUPnPServiceActionInfo *action,
                    const gchar            *action_name)
{
        return strcmp (action->name, action_name);
}

/**
 * gupnp_service_introspection_get_action:
 * @introspection: A #GUPnPServiceIntrospection
 * @action_name: The name of the action to retrieve
 *
 * Returns the action by the name @action_name in this service.
 *
 * Return value: (transfer none)(nullable): the action or %NULL. Do not modify or free
 * it.
 **/
const GUPnPServiceActionInfo *
gupnp_service_introspection_get_action
                        (GUPnPServiceIntrospection *introspection,
                         const gchar               *action_name)
{
        GList *action_node;
        GUPnPServiceIntrospectionPrivate *priv;

        priv = gupnp_service_introspection_get_instance_private (introspection);

        if (priv->variables == NULL)
                return NULL;

        action_node = g_list_find_custom (priv->actions,
                                          (gpointer) action_name,
                                          (GCompareFunc) action_search_func);
        if (action_node == NULL)
                return NULL;

        return (GUPnPServiceActionInfo *) action_node->data;
}
