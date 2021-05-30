/*
 * Copyright (C) 2012 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * This file is part of GUPnP.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

public class GUPnP.ServiceProxy : GUPnP.ServiceInfo {
    public bool send_action (string action, ...) throws GLib.Error;
    public bool end_action (GUPnP.ServiceProxyAction action, ...) throws GLib.Error;
    public bool end_action_hash (GUPnP.ServiceProxyAction action, GLib.HashTable<string, weak GLib.Value*> hash) throws GLib.Error;
    public bool end_action_list (GUPnP.ServiceProxyAction action, GLib.List<string> out_names, GLib.List<GLib.Type?> out_types, out GLib.List<weak GLib.Value*> out_values) throws GLib.Error;

    public bool send_action_list (string action, GLib.List<string> in_names, GLib.List<weak GLib.Value?> in_values, GLib.List<string> out_names, GLib.List<GLib.Type?> out_types, out GLib.List<weak GLib.Value*> out_values) throws GLib.Error;
}

[Compact]
public class GUPnP.ServiceProxyAction {
    [CCode (has_construct_function = false)]
    public ServiceProxyAction (string action, ...);
    public bool get_result (...) throws GLib.Error;
}

public interface GUPnP.Acl : GLib.Object {
		public abstract bool is_allowed (GUPnP.Device? device, GUPnP.Service? service, string path, string address, string? agent);
		public abstract async bool is_allowed_async (GUPnP.Device? device, GUPnP.Service? service, string path, string address, string? agent, GLib.Cancellable? cancellable) throws GLib.Error;
}


