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
    public bool end_action_hash (GUPnP.ServiceProxyAction action, [CCode (pos=-0.9)] GLib.HashTable<string, weak GLib.Value*> hash) throws GLib.Error;
    public bool end_action_list (GUPnP.ServiceProxyAction action, [CCode (pos=-0.9)] GLib.List<string> out_names, [CCode (pos=-0.8)] GLib.List<GLib.Type?> out_types, [CCode (pos=-0.7)] out GLib.List<weak GLib.Value*> out_values) throws GLib.Error;

    public bool send_action_hash (string action, [CCode (pos=-0.9)] GLib.HashTable<string, GLib.Value?> in_hash, [CCode (pos=-0.8)] GLib.HashTable<string, weak GLib.Value*> out_hash) throws GLib.Error;
    public bool send_action_list (string action, [CCode (pos=-0.9)] GLib.List<string> in_names, [CCode (pos=-0.8)] GLib.List<weak GLib.Value?> in_values, [CCode (pos=-0.7)] GLib.List<string> out_names, [CCode (pos=-0.6)] GLib.List<GLib.Type?> out_types, [CCode (pos=-0.5)] out GLib.List<weak GLib.Value*> out_values) throws GLib.Error;
}

namespace GUPnP {
	[CCode (cheader_filename = "libgupnp/gupnp.h", cprefix = "GUPNP_CONTROL_ERROR_")]
	public errordomain ControlError {
		INVALID_ACTION,
		INVALID_ARGS,
		OUT_OF_SYNC,
		ACTION_FAILED;
		public static GLib.Quark quark ();
	}
	[CCode (cheader_filename = "libgupnp/gupnp.h", cprefix = "GUPNP_EVENTING_ERROR_")]
	public errordomain EventingError {
		SUBSCRIPTION_FAILED,
		SUBSCRIPTION_LOST,
		NOTIFY_FAILED;
		public static GLib.Quark quark ();
	}
	[CCode (cheader_filename = "libgupnp/gupnp.h", cprefix = "GUPNP_SERVER_ERROR_")]
	public errordomain ServerError {
		INTERNAL_SERVER_ERROR,
		NOT_FOUND,
		NOT_IMPLEMENTED,
		INVALID_RESPONSE,
		INVALID_URL,
		OTHER;
		public static GLib.Quark quark ();
	}
	[CCode (cheader_filename = "libgupnp/gupnp.h", cprefix = "GUPNP_XML_ERROR_")]
	public errordomain XMLError {
		PARSE,
		NO_NODE,
		EMPTY_NODE,
		INVALID_ATTRIBUTE,
		OTHER;
		public static GLib.Quark quark ();
	}
}
