/*
 * Copyright (C) 2007 Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
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

#ifndef __GUPNP_SERVICE_INTROSPECTION_H__
#define __GUPNP_SERVICE_INTROSPECTION_H__

#include <glib-object.h>

G_BEGIN_DECLS

GType
gupnp_service_introspection_get_type (void) G_GNUC_CONST;

#define GUPNP_TYPE_SERVICE_INTROSPECTION \
                (gupnp_service_introspection_get_type ())
#define GUPNP_SERVICE_INTROSPECTION(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 GUPNP_TYPE_SERVICE_INTROSPECTION, \
                 GUPnPServiceIntrospection))
#define GUPNP_SERVICE_INTROSPECTION_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_CAST ((obj), \
                 GUPNP_TYPE_SERVICE_INTROSPECTION, \
                 GUPnPServiceIntrospectionClass))
#define GUPNP_IS_SERVICE_INTROSPECTION(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 GUPNP_TYPE_SERVICE_INTROSPECTION))
#define GUPNP_IS_SERVICE_INTROSPECTION_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 GUPNP_TYPE_SERVICE_INTROSPECTION))
#define GUPNP_SERVICE_INTROSPECTION_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 GUPNP_TYPE_SERVICE_INTROSPECTION, \
                 GUPnPServiceIntrospectionClass))

/**
 * GUPnPServiceActionArgDirection
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

/**
 * GUPnPServiceActionInfo:
 * @name: The name of the action argument.
 * @arguments: A GList of all the arguments
 * (of type #GUPnPServiceActionArgInfo) of this action.
 *
 * This structure contains information about a service action.
 **/
typedef struct {
        char  *name;
        GList *arguments; /* list of #GUPnPServiceActionArgInfo */
} GUPnPServiceActionInfo;

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
 * @allowed_values: The allowed values of this state variable. Only applies to
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

typedef struct _GUPnPServiceIntrospectionPrivate
                GUPnPServiceIntrospectionPrivate;

/**
 * GUPnPServiceIntrospection:
 *
 * This struct contains private data only, and should be accessed using the
 * functions below.
 */
typedef struct {
        GObject parent;

        GUPnPServiceIntrospectionPrivate *priv;
} GUPnPServiceIntrospection;

typedef struct {
        GObjectClass parent_class;
} GUPnPServiceIntrospectionClass;

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

#endif /* __GUPNP_SERVICE_INTROSPECTION_H__ */
