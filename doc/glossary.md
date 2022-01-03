---
Title: UPnP Glossary
---

# Action

> An Action is a method call on a Service, which encapsulated a single piece of
functionality.  Actions can have multiple input and output arguments, and
can return error codes.  UPnP allows one of the output arguments to be
marked as the return value, but GUPnP doesn't treat return values specially.

> Every action argument has a related State Variable,
which determines the type of the argument.  Note that even if the argument
wouldn't need a state variable it is still required, due to historical
reasons.

# Control Point

> A Control Point is an entity on the network which
communicates with other Devices and
Services.  In the client/server model the control
point is a client and the Service is a server,
although it is common for devices to also be a control point because
whilst a single control point/service connection is client/server, the
UPnP network as whole is peer-to-peer.

# Device
> A Device is an entity on the network which
communicates using the UPnP standards.  This can be a dedicated physical
device such as a router or printer, or a PC which is running software
implementing the UPnP standards.

> A Device can contain sub-devices, for example a combination
printer/scanner could appear as a general device with a printer
sub-device and a scanner sub-device.

> Every device has zero or more Services. UPnP defines many standard
device types, which specify services which are required to be implemented.
Alternatively, a non-standard device type could be used.  Examples of
standard device types are `MediaRenderer` or
`InternetGatewayDevice`.

# DIDL-Lite

> Digital Item Declaration Language - Lite

> An XML schema used to represent digital content metadata. Defined by
the UPnP Forum.

# Service

> A Service is a collection of related methods
(called Actions) and public variables (called
State Variables) which together form a logical interface.
>      UPnP defines standard services that define actions and variables which
must be present and their semantics.  Examples of these are
`AVTransport` and `WANIPConnection`.

See also:

- [Action](#action)
- [Device](#device)
- [State Variable](#state-variable)

# SCDP
> Service Control Protocol Document


> An XML document which defines the set of <glossterm>Actions</glossterm>
and <glossterm>State Variables</glossterm> that a
<glossterm>Service</glossterm> implements.

See also:

- [Action](#action)
- [Device](#device)
- [State Variable](#state-variable)

# SSDP
> <glossterm>Simple Service Discovery Protocol</glossterm>

> UPnP device discovery protocol. Specifies how <glossterm>Devices</glossterm> 
advertise their <glossterm>Services</glossterm> in the network and also how 
<glossterm>Control Points</glossterm> search for
services and devices respond.

See also:

- [Device](#device)
- [Action](#controlpoint)
- [Service](#service)

# State Variable

> A <firstterm>State Variable</firstterm> is a public variable exposing some
aspect of the service's state.  State variables are typed and optionally
are <firstterm>evented</firstterm>, which means that any changes will be
notified.  Control points are said to <firstterm>subscribe</firstterm> to
a state variable to receive change notifications.


# UDN
> Unique Device Name

> An unique identifier which is <emphasis>unique</emphasis> for every
device but <emphasis>never changes</emphasis> for each particular
device.

> A common practise is to generate a unique UDN on first boot from a
random seed, or use some unique and persistent property such as the
device's MAC address to create the UDN.

See also:

- [Device](#device)
