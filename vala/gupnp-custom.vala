/*
 * Copyright (C) 2012 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * This file is part of GUPnP.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

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

