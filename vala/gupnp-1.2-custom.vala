/*
 * Copyright (C) 2012 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * This file is part of GUPnP.
 *
 * GUPnP is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GUPnP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

public class GUPnP.ServiceProxy : GUPnP.ServiceInfo {
    public bool send_action (string action, ...) throws GLib.Error;
    public bool end_action (GUPnP.ServiceProxyAction action, ...) throws GLib.Error;
    public bool end_action_hash (GUPnP.ServiceProxyAction action, GLib.HashTable<string, weak GLib.Value*> hash) throws GLib.Error;
    public bool end_action_list (GUPnP.ServiceProxyAction action, GLib.List<string> out_names, GLib.List<GLib.Type?> out_types, out GLib.List<weak GLib.Value*> out_values) throws GLib.Error;

    public bool send_action_list (string action, GLib.List<string> in_names, GLib.List<weak GLib.Value?> in_values, GLib.List<string> out_names, GLib.List<GLib.Type?> out_types, out GLib.List<weak GLib.Value*> out_values) throws GLib.Error;
}

public interface GUPnP.Acl : GLib.Object {
		public abstract bool is_allowed (GUPnP.Device? device, GUPnP.Service? service, string path, string address, string? agent);
		public abstract async bool is_allowed_async (GUPnP.Device? device, GUPnP.Service? service, string path, string address, string? agent, GLib.Cancellable? cancellable) throws GLib.Error;
}

