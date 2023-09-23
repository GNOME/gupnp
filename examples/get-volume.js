#!/usr/bin/gjs
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//         SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.

imports.gi.versions.GUPnP = "1.6"

const Mainloop = imports.mainloop;
const GObject = imports.gi.GObject;
const GUPnP = imports.gi.GUPnP;
const GLib = imports.gi.GLib;

const CONTENT_DIR = "urn:schemas-upnp-org:service:RenderingControl:1";

function on_action(proxy, res) {
    let action = proxy.call_action_finish (res);
    let iter = action.iterate();
    while (iter.next()) {
      print("name:", iter.get_name());
      if (iter.get_name() == "CurrentVolume") {
        print(iter.get_value());
      }
    }
}

function on_service_introspection(proxy, res) {
    var result = proxy.introspect_finish(res);
    var action_info = result.get_action("GetVolume")
    var state_variable_name = action_info.arguments[1].related_state_variable;

    var channel = result.get_state_variable (state_variable_name);

    print ("Calling GetVolume for channel ", channel.allowed_values[0]);
    var action = GUPnP.ServiceProxyAction.new_plain("GetVolume");
    action.add_argument("InstanceID", 0).add_argument("Channel", channel.allowed_values[0]);
    proxy.call_action_async (action, null, on_action);
}

function _on_sp_available (cp, proxy) {
    print ("Got ServiceProxy ", proxy.get_id(), proxy.get_location());
    print ("Introspecting service...");
    proxy.introspect_async(null, on_service_introspection)
}

var context = new GUPnP.Context ( {'interface': "wlp3s0"});
context.init(null);
var cp = new GUPnP.ControlPoint ( {'client': context, 'target' : CONTENT_DIR});
cp.connect ("service-proxy-available", _on_sp_available);
cp.active = true;
GLib.timeout_add_seconds (GLib.PRIORITY_LOW, 10, () => { Mainloop.quit(); return false; })
Mainloop.run ();
