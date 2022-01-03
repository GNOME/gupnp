---
Title: Implementing UPnP devices
---

# UPnP Server Tutorial

This chapter explains how to implement an UPnP service using GUPnP. For
this example we will create a virtual UPnP-enabled light bulb.

Before any code can be written, the device and services that it implement
need to be described in XML.  Although this can be frustrating, if you are
implementing a standardised service then the service description is
already written for you and the device description is trivial.  UPnP has
standardised Lighting Controls, so we'll be using the device and service types defined
there.

## Defining the Device

The first step is to write the _device description_
file.  This is a short XML document which describes the device and what
services it provides (for more details see the [UPnP Device Architecture specification](http://upnp.org/specs/arch/UPnP-arch-DeviceArchitecture-v1.0.pdf), section 2.1).  We'll be using
the `BinaryLight1` device type, but if none of the
existing device types are suitable a custom device type can be created.

```xml
<?xml version="1.0" encoding="utf-8"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
  <specVersion>
    <major>1</major>
    <minor>0</minor>
  </specVersion>

  <device>
    <deviceType>urn:schemas-upnp-org:device:BinaryLight:1</deviceType>
    <friendlyName>Kitchen Lights</friendlyName>
    <manufacturer>OpenedHand</manufacturer>
    <modelName>Virtual Light</modelName>
    <UDN>uuid:cc93d8e6-6b8b-4f60-87ca-228c36b5b0e8</UDN>
    
    <serviceList>
      <service>
        <serviceType>urn:schemas-upnp-org:service:SwitchPower:1</serviceType>
        <serviceId>urn:upnp-org:serviceId:SwitchPower:1</serviceId>
        <SCPDURL>/SwitchPower1.xml</SCPDURL>
        <controlURL>/SwitchPower/Control</controlURL>
        <eventSubURL>/SwitchPower/Event</eventSubURL>
      </service>
    </serviceList>
  </device>
</root>
```
The `<specVersion>` tag defines what version of the UPnP
Device Architecture the document conforms to.  At the time of writing the
only version is 1.0.

Next there is the root `<device>` tag.  This contains
metadata about the device, lists the services it provides and any
sub-devices present (there are none in this example).  The
`<deviceType>` tag specifies the type of the device.

Next we have `<friendlyName>`, `<manufacturer>` and `<modelName>`. The
friendly name is a human-readable name for the device, the manufacturer
and model name are self-explanatory.

Next there is the UDN, or _Unique Device Name_. This
is an identifier which is unique for each device but persistent for each
particular device.  Although it has to start with `uuid:`
note that it doesn't have to be an UUID.  There are several alternatives
here: for example it could be computed at built-time if the software will
only be used on a single machine, or it could be calculated using the
device's serial number or MAC address.

Finally we have the `<serviceList>` which describes the
services this device provides.  Each service has a service type (again
there are types defined for standardised services or you can create your
own), service identifier, and three URLs.  As a service type we're using
the standard `SwitchPower1` service.  The
`<SCPDURL>` field specifies where the _Service
Control Protocol Document_ can be found, this describes the
service in more detail and will be covered next.  Finally there are the
control and event URLs, which need to be unique on the device and will be
managed by GUPnP.

## Defining Services

Because we are using a standard service we can use the service description
from the specification.  This is the `SwitchPower1`
service description file:

```xml
<?xml version="1.0" encoding="utf-8"?>
<scpd xmlns="urn:schemas-upnp-org:service-1-0">
  <specVersion>
    <major>1</major>
    <minor>0</minor>
  </specVersion>
  <actionList>
    <action>
      <name>SetTarget</name>
      <argumentList>
        <argument>
          <name>newTargetValue</name>
          <relatedStateVariable>Target</relatedStateVariable>
          <direction>in</direction>
        </argument>
      </argumentList>
    </action>
    <action>
      <name>GetTarget</name>
      <argumentList>
        <argument>
          <name>RetTargetValue</name>
          <relatedStateVariable>Target</relatedStateVariable>
          <direction>out</direction>
        </argument>
      </argumentList>
    </action>
    <action>
      <name>GetStatus</name>
      <argumentList>
        <argument>
          <name>ResultStatus</name>
          <relatedStateVariable>Status</relatedStateVariable>
          <direction>out</direction>
        </argument>
      </argumentList>
    </action>
  </actionList>
  <serviceStateTable>
    <stateVariable sendEvents="no">
      <name>Target</name>
      <dataType>boolean</dataType>
      <defaultValue>0</defaultValue>
    </stateVariable>
    <stateVariable sendEvents="yes">
      <name>Status</name>
      <dataType>boolean</dataType>
      <defaultValue>0</defaultValue>
    </stateVariable>
  </serviceStateTable>
</scpd>
```

Again, the `<specVersion>` tag defines the UPnP version
that is being used.  The rest of the document consists of an
`<actionList>` defining the actions available and a
`<serviceStateTable>` defining the state variables.

Every `<action>` has a `<name>` and a list
of `<argument>`s.  Arguments also have a name, a direction
(`in` or `out` for input or output
 arguments) and a related state variable.  The state variable is used to
determine the type of the argument, and as such is a required element.
This can lead to the creation of otherwise unused state variables to
define the type for an argument (the `WANIPConnection`
service is a good example of this), thanks to the legacy behind UPnP.

`<stateVariable>`s need to specify their
`<name>` and `<dataType>`.  State variables
by default send notifications when they change, to specify that a variable
doesn't do this set the `<sendEvents>` attribute to
`no`.  Finally there are optional
`<defaultValue>`, `<allowedValueList>` and
`<allowedValueRange>` elements which specify what the
default and valid values for the variable.

For the full specification of the service definition file, including a
complete list of valid `<dataType>`s, see section 2.3 of
the [UPnP Device Architecture](http://upnp.org/specs/arch/UPnP-arch-DeviceArchitecture-v1.0.pdf)

## Implementing the Device

Before starting to implement the device, some boilerplate code is needed
to initialise GUPnP. A GUPnP context can be created using `gupnp_context_new()`.

```c
GUPnPContext *context;
/* Create the GUPnP context with default host and port */
context = gupnp_context_new (NULL, 0, NULL);
```
Next the root device can be created. The name of the device description
file can be passed as an absolute file path or a relative path to the
second parameter of `gupnp_root_device_new()`. The service description
files referenced in the device description are expected to be at the path
given there as well.

```c
GUPnPRootDevice *dev;
/* Create the root device object */
dev = gupnp_root_device_new (context, "BinaryLight1.xml", ".");
/* Activate the root device, so that it announces itself */
gupnp_root_device_set_available (dev, TRUE);
```

GUPnP scans the device description and any service description files it
refers to, so if the main loop was entered now the device and service
would be available on the network, albeit with no functionality.  The
remaining task is to implement the services.

## Implementing a Service

To implement a service we first fetch the #GUPnPService from the root
device using gupnp_device_info_get_service() (#GUPnPRootDevice is a
subclass of #GUPnPDevice, which implements #GUPnPDeviceInfo).  This
returns a #GUPnPServiceInfo which again is an interface, implemented by
#GUPnPService (on the server) and #GUPnPServiceProxy (on the client).
  
```c
GUPnPServiceInfo *service;
service = gupnp_device_info_get_service (GUPNP_DEVICE_INFO (dev), "urn:schemas-upnp-org:service:SwitchPower:1");
```

GUPnPService handles interacting with the network itself, leaving the
implementation of the service itself to signal handlers that we need to
connect.  There are two signals: #GUPnPService::action-invoked and
#GUPnPService::query-variable.  #GUPnPService::action-invoked is emitted
when a client invokes an action: the handler is passed a
#GUPnPServiceAction object that identifies which action was invoked, and
is used to return values using `gupnp_service_action_set()`.
#GUPnPService::query-variable is emitted for evented variables when a
control point subscribes to the service (to announce the initial value),
or whenever a client queries the value of a state variable (note that this
is now deprecated behaviour for UPnP control points): the handler is
passed the variable name and a #GValue which should be set to the current
value of the variable.

Handlers should be targetted at specific actions or variables by using
the signal detail when connecting. For example,
this causes `on_get_status_action` to be called when
the `GetStatus` action is invoked:

```c
static void on_get_status_action (GUPnPService *service, GUPnPServiceAction *action, gpointer user_data);
// ...
g_signal_connect (service, "action-invoked::GetStatus", G_CALLBACK (on_get_status_action), NULL);
```

The implementation of action handlers is quite simple.  The handler is
passed a #GUPnPServiceAction object which represents the in-progress
action.  If required it can be queried using
gupnp_service_action_get_name() to identify the action (this isn't
required if detailed signals were connected).  Any
in arguments can be retrieving using
`gupnp_service_action_get()`, and then return values can be set using
`gupnp_service_action_set()`.  Once the action has been performed, either
`gupnp_service_action_return()` or `gupnp_service_action_return_error()`
should be called to either return successfully or return an error code.
  
If any evented state variables were modified during the action then a
notification should be emitted using `gupnp_service_notify()`.  This is an
example implementation of `GetStatus` and `SetTarget`

```c
static gboolean status;

static void
get_status_cb (GUPnPService *service, GUPnPServiceAction *action, gpointer user_data)
{
    gupnp_service_action_set (action,
                              "ResultStatus", G_TYPE_BOOLEAN, status,
                              NULL);
    gupnp_service_action_return (action);
}

void
set_target_cb (GUPnPService *service, GUPnPServiceAction *action, gpointer user_data)
{
    gupnp_service_action_get (action,
                              "NewTargetValue", G_TYPE_BOOLEAN, &status,
                              NULL);
    gupnp_service_action_return (action);
    gupnp_service_notify (service, "Status", G_TYPE_STRING, status, NULL);
}

//...

g_signal_connect (service, "action-invoked::GetStatus", G_CALLBACK (get_status_cb), NULL);
g_signal_connect (service, "action-invoked::SetTarget", G_CALLBACK (set_target_cb), NULL);
```

State variable query handlers are called with the name of the variable and
a #GValue.  This value should be initialized with the relevant type and
then set to the current value.  Again signal detail can be used to connect
handlers to specific state variable callbacks.

```c
static gboolean status;

static void
query_status_cb (GUPnPService *service, char *variable, GValue *value, gpointer user_data)
{
    g_value_init (value, G_TYPE_BOOLEAN);
    g_value_set_boolean (value, status);
}

// ...

g_signal_connect (service, "query-variable::Status", G_CALLBACK (query_status_cb), NULL);
```

The service is now fully implemented.  To complete it, enter a GLib main
loop and wait for a client to connect.  The complete source code for this
example is available as [examples/light-server.c](https://gitlab.gnome.org/GNOME/gupnp/-/blob/master/examples/light-server.c) in
the GUPnP sources.

For services which have many actions and variables there is a convenience
method [method@GUPnP.Service.signals_autoconnect] which will automatically
connect specially named handlers to signals.  See the documentation for
full details on how it works.

## Generating Service-specific Wrappers

Using service-specific wrappers can simplify the implementation of a service.
Wrappers can be generated with gupnp-binding-tool
using the option `--mode server`. 

In the following examples the wrapper has been created with
  `--mode server --prefix switch`. Please note that the callback handlers
  (`get_status_cb` and `set_target_cb`) are not automatically
  generated by gupnp-binding-tool for you.

```c
static gboolean status;

static void
get_status_cb (GUPnPService *service,
           GUPnPServiceAction *action,
           gpointer user_data)
{
    switch_get_status_action_set (action, status);

    gupnp_service_action_return (action);
}

static void
set_target_cb (GUPnPService *service,
           GUPnPServiceAction *action,
           gpointer user_data)
{
    switch_set_target_action_get (action, &status);
    switch_status_variable_notify (service, status);

    gupnp_service_action_return (action);
}

// ...

switch_get_status_action_connect (service, G_CALLBACK(get_status_cb), NULL);
switch_set_target_action_connect (service, G_CALLBACK(set_target_cb), NULL);
```

Note how many possible problem situations that were run-time errors are 
actually compile-time errors when wrappers are used: Action names, 
argument names and argument types are easier to get correct (and available
in editor autocompletion).

State variable query handlers are implemented in a similar manner, but 
they are even simpler as the return value of the handler is the state 
variable value.

```c
static gboolean
query_status_cb (GUPnPService *service, 
             gpointer user_data)
{
    return status;
}

// ...


switch_status_query_connect (service, query_status_cb, NULL);
```
