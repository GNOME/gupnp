/* 
 * Copyright (C) 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
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
 * SECTION:gupnp-service
 * @short_description: Class for service implementations.
 *
 * #GUPnPService allows for implementing services. #GUPnPService implements
 * the #GUPnPServiceInfo interface.
 */

#include "gupnp-service.h"
#include "gupnp-marshal.h"

G_DEFINE_TYPE (GUPnPService,
               gupnp_service,
               GUPNP_TYPE_SERVICE_INFO);

struct _GUPnPServicePrivate {
};

enum {
        ACTION_INVOKED,
        QUERY_PROPERTY,
        NOTIFY_FAILED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _GUPnPServiceAction {
};

/**
 * gupnp_service_action_get_name
 * @action: A #GUPnPServiceAction
 *
 * Return value: The name of @action
 **/
const char *
gupnp_service_action_get_name (GUPnPServiceAction *action)
{
        g_return_val_if_fail (action != NULL, NULL);

        return NULL;
}

/**
 * gupnp_service_action_get_locale
 * @action: A #GUPnPServiceAction
 *
 * Return value: The locale @action requests.
 **/
const char *
gupnp_service_action_get_locale (GUPnPServiceAction *action)
{
        g_return_val_if_fail (action != NULL, NULL);

        return NULL;
}

/**
 * gupnp_service_action_get
 * @action: A #GUPnPServiceAction
 * @Varargs: tuples of argument name, argument type, and argument value, 
 * terminated with NULL.
 *
 * Retrieves the specified action arguments.
 **/
void
gupnp_service_action_get (GUPnPServiceAction *action,
                          ...)
{
        g_return_if_fail (action != NULL);
}

/**
 * gupnp_service_action_get_valist
 * @action: A #GUPnPServiceAction
 * @var_args: va_list of tuples of argument name, argument type, and argument
 * value.
 *
 * See gupnp_service_action_get(); this version takes a va_list for
 * use by language bindings.
 **/
void
gupnp_service_action_get_valist (GUPnPServiceAction *action,
                                 va_list             var_args)
{
        g_return_if_fail (action != NULL);
}

/**
 * gupnp_service_action_get_value
 * @action: A #GUPnPServiceAction
 * @argument: The name of the argument to retrieve
 * @value: The #GValue to store the value of the argument
 *
 * Retrieves the value of @argument into @value.
 **/
void
gupnp_service_action_get_value (GUPnPServiceAction *action,
                                const char         *argument,
                                GValue             *value)
{
        g_return_if_fail (action != NULL);
        g_return_if_fail (argument != NULL);
        g_return_if_fail (value != NULL);
}

/**
 * gupnp_service_action_set
 * @action: A #GUPnPServiceAction
 * @Varargs: tuples of return value name, return value type, and
 * actual return value, terminated with NULL.
 *
 * Sets the specified action return values.
 **/
void
gupnp_service_action_set (GUPnPServiceAction *action,
                          ...)
{
        g_return_if_fail (action != NULL);
}

/**
 * gupnp_service_action_set_valist
 * @action: A #GUPnPServiceAction
 * @var_args: va_list of tuples of return value name, return value type, and
 * actual return value.
 *
 * See gupnp_service_action_set(); this version takes a va_list for
 * use by language bindings.
 **/
void
gupnp_service_action_set_valist (GUPnPServiceAction *action,
                                 va_list             var_args)
{
        g_return_if_fail (action != NULL);
}

/**
 * gupnp_service_action_set_value
 * @action: A #GUPnPServiceAction
 * @argument: The name of the return value to retrieve
 * @value: The #GValue to store the return value
 *
 * Sets the value of @argument to @value.
 **/
void
gupnp_service_action_set_value (GUPnPServiceAction *action,
                                const char         *argument,
                                const GValue       *value)
{
        g_return_if_fail (action != NULL);
        g_return_if_fail (argument != NULL);
        g_return_if_fail (value != NULL);
}

/**
 * gupnp_service_action_return
 * @action: A #GUPnPServiceAction
 *
 * Return succesfully.
 **/
void
gupnp_service_action_return (GUPnPServiceAction *action)
{
        g_return_if_fail (action != NULL);
}

/**
 * gupnp_service_action_return_error
 * @action: A #GUPnPServiceAction
 * @error: The #GError
 *
 * Return @error.
 **/
void
gupnp_service_action_return_error (GUPnPServiceAction *action,
                                   GError             *error)
{
        g_return_if_fail (action != NULL);
        g_return_if_fail (error != NULL);
}

static void
gupnp_service_init (GUPnPService *proxy)
{
        proxy->priv = G_TYPE_INSTANCE_GET_PRIVATE (proxy,
                                                   GUPNP_TYPE_SERVICE,
                                                   GUPnPServicePrivate);
}

static void
gupnp_service_dispose (GObject *object)
{
        GUPnPService *proxy;
        GObjectClass *object_class;

        proxy = GUPNP_SERVICE (object);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_service_parent_class);
        object_class->dispose (object);
}

static void
gupnp_service_finalize (GObject *object)
{
        GUPnPService *proxy;
        GObjectClass *object_class;

        proxy = GUPNP_SERVICE (object);

        /* Call super */
        object_class = G_OBJECT_CLASS (gupnp_service_parent_class);
        object_class->finalize (object);
}

static void
gupnp_service_class_init (GUPnPServiceClass *klass)
{
        GObjectClass *object_class;
        GUPnPServiceInfoClass *info_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->dispose  = gupnp_service_dispose;
        object_class->finalize = gupnp_service_finalize;

        info_class = GUPNP_SERVICE_INFO_CLASS (klass);
        
        g_type_class_add_private (klass, sizeof (GUPnPServicePrivate));

        /**
         * GUPnPService::action-invoked
         * @service: The #GUPnPService that received the signal
         * @action: The invoked #GUPnPAction
         *
         * Emitted whenever an action is invoked. Handler should process
         * @action.
         **/
        signals[ACTION_INVOKED] =
                g_signal_new ("action-invoked",
                              GUPNP_TYPE_SERVICE,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GUPnPServiceClass,
                                               action_invoked),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_POINTER);

        /**
         * GUPnPService::query-property
         * @service: The #GUPnPService that received the signal
         * @property: The property that is being queried
         * @value: The location of the #GValue of the property
         *
         * Emitted whenever @service needs to know the value of @property.
         * Handler should fill @value with the value of @property.
         **/
        signals[QUERY_PROPERTY] =
                g_signal_new ("query-property",
                              GUPNP_TYPE_SERVICE,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GUPnPServiceClass,
                                               query_property),
                              NULL,
                              NULL,
                              gupnp_marshal_VOID__STRING_POINTER,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_STRING,
                              G_TYPE_POINTER);

        /**
         * GUPnPService::notify-failed
         * @service: The #GUPnPService that received the signal
         * @notify_url: The notification URL
         * @reason: A pointer to a #GError describing why the notify failed
         *
         * Emitted whenever notification of a client fails.
         **/
        signals[NOTIFY_FAILED] =
                g_signal_new ("notify-failed",
                              GUPNP_TYPE_SERVICE,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GUPnPServiceClass,
                                               notify_failed),
                              NULL,
                              NULL,
                              gupnp_marshal_VOID__STRING_POINTER,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_STRING,
                              G_TYPE_POINTER);
}

/**
 * gupnp_service_notify
 * @service: A #GUPnPService
 * @Varargs: Tuples of property name, property type, and property value,
 * terminated with NULL.
 *
 * Notifies listening clients that the properties listed in @Varargs
 * have changed to the specified values.
 **/
void
gupnp_service_notify (GUPnPService *service,
                      ...)
{
        g_return_if_fail (GUPNP_IS_SERVICE (service));
}

/**
 * gupnp_service_notify_valist
 * @service: A #GUPnPService
 * @var_args: A va_list of tuples of property name, property type, and property
 * value, terminated with NULL.
 *
 * See gupnp_service_notify_valist(); this version takes a va_list for
 * use by language bindings.
 **/
void
gupnp_service_notify_valist (GUPnPService *service,
                             va_list       var_args)
{
        g_return_if_fail (GUPNP_IS_SERVICE (service));
}

/**
 * gupnp_service_notify_value
 * @service: A #GUPnPService
 * @property: The name of the property to notify
 * @value: The value of the property
 *
 * Notifies listening clients that @property has changed to @value.
 **/
void
gupnp_service_notify_value (GUPnPService *service,
                            const char   *property,
                            const GValue *value)
{
        g_return_if_fail (GUPNP_IS_SERVICE (service));
        g_return_if_fail (property != NULL);
        g_return_if_fail (G_IS_VALUE (value));
}

/**
 * gupnp_service_freeze_notify
 * @service: A #GUPnPService
 *
 * Causes new notifications to be queued up until gupnp_service_thaw_notify()
 * is called.
 **/
void
gupnp_service_freeze_notify (GUPnPService *service)
{
        g_return_if_fail (GUPNP_IS_SERVICE (service));
}

/**
 * gupnp_service_thaw_notify
 * @service: A #GUPnPService
 *
 * Sends out any pending notifications, and stops queuing of new ones.
 **/
void
gupnp_service_thaw_notify (GUPnPService *service)
{
        g_return_if_fail (GUPNP_IS_SERVICE (service));
}
