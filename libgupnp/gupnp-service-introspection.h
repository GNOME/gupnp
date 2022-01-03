/*
 * Copyright (C) 2007 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *         Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GUPNP_SERVICE_INTROSPECTION_H
#define GUPNP_SERVICE_INTROSPECTION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GUPNP_TYPE_SERVICE_INTROSPECTION \
                (gupnp_service_introspection_get_type ())

#define GUPNP_TYPE_SERVICE_ACTION_INFO (gupnp_service_action_info_get_type())
#define GUPNP_TYPE_SERVICE_ACTION_ARG_INFO (gupnp_service_action_arg_info_get_type())


G_DECLARE_FINAL_TYPE (GUPnPServiceIntrospection,
                      gupnp_service_introspection,
                      GUPNP,
                      SERVICE_INTROSPECTION,
                      GObject)

/**
 * GUPnPServiceActionArgDirection:
 * @GUPNP_SERVICE_ACTION_ARG_DIRECTION_IN: An "in" variable, to the service.
 * @GUPNP_SERVICE_ACTION_ARG_DIRECTION_OUT: An "out" variable, from the service.
 *
 * Represents the direction of a service state variable.
 **/
typedef enum
{
        GUPNP_SERVICE_ACTION_ARG_DIRECTION_IN,
        GUPNP_SERVICE_ACTION_ARG_DIRECTION_OUT
} GUPnPServiceActionArgDirection;

/**
 * GUPnPServiceActionArgInfo:
 * @name: The name of the action argument.
 * @direction: The direction of the action argument.
 * @related_state_variable: The name of the state variable associated with this
 * argument.
 * @retval: Whether this argument is the return value of the action.
 *
 * This structure contains information about the argument of service action.
 **/
typedef struct {
        char                          *name;
        GUPnPServiceActionArgDirection direction;
        char                          *related_state_variable;
        gboolean                       retval;
} GUPnPServiceActionArgInfo;
GType
gupnp_service_action_arg_info_get_type (void);

/**
 * GUPnPServiceActionInfo:
 * @name: The name of the action argument.
 * @arguments:(element-type GUPnP.ServiceActionArgInfo):A GList of all the arguments
 * (of type #GUPnPServiceActionArgInfo) of this action.
 *
 * This structure contains information about a service action.
 **/
typedef struct {
        char  *name;
        GList *arguments; /* list of #GUPnPServiceActionArgInfo */
} GUPnPServiceActionInfo;

GType
gupnp_service_action_info_get_type (void);

/**
 * GUPnPServiceStateVariableInfo:
 * @name: The name of the state variable.
 * @send_events: Whether this state variable can source events.
 * @is_numeric: Wether this state variable is a numeric type (integer and
 * float).
 * @type: The GType of this state variable.
 * @default_value: The default value of this state variable.
 * @minimum: The minimum value of this state variable. Only applies to numeric
 * data types.
 * @maximum: The maximum value of this state variable. Only applies to numeric
 * data types.
 * @step: The step value of this state variable. Only applies to numeric
 * data types.
 * @allowed_values: (element-type utf8): The allowed values of this state variable. Only applies to
 * string data types. Unlike the other fields in this structure, this field
 * contains a list of (char *) strings rather than GValues.
 *
 * This structure contains information about service state variable.
 **/
typedef struct {
        char    *name;
        gboolean send_events;
        gboolean is_numeric;
        GType    type;
        GValue   default_value;
        GValue   minimum;
        GValue   maximum;
        GValue   step;
        GList   *allowed_values;
} GUPnPServiceStateVariableInfo;

GType
gupnp_service_state_variable_info_get_type (void);

const GList *
gupnp_service_introspection_list_action_names
                                (GUPnPServiceIntrospection *introspection);

const GList *
gupnp_service_introspection_list_actions
                                (GUPnPServiceIntrospection *introspection);

const GUPnPServiceActionInfo *
gupnp_service_introspection_get_action
                                (GUPnPServiceIntrospection *introspection,
                                 const gchar               *action_name);

const GList *
gupnp_service_introspection_list_state_variable_names
                                (GUPnPServiceIntrospection *introspection);

const GList *
gupnp_service_introspection_list_state_variables
                                (GUPnPServiceIntrospection *introspection);

const GUPnPServiceStateVariableInfo *
gupnp_service_introspection_get_state_variable
                                (GUPnPServiceIntrospection *introspection,
                                 const gchar               *variable_name);

G_END_DECLS

#endif /* GUPNP_SERVICE_INTROSPECTION_H */
