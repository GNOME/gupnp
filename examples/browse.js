#!/usr/bin/gjs
//
// Copyright (c) 2016, Jens Georg <mail@jensge.org>
//
// All rights reserved.
//
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
imports.gi.versions.GSSDP = "1.6"

const Mainloop = imports.mainloop;
const GUPnP = imports.gi.GUPnP;
const GSSDP = imports.gi.GSSDP;
const GObject = imports.gi.GObject;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Soup = imports.gi.Soup;

const MEDIA_SERVER = "urn:schemas-upnp-org:device:MediaServer:1";
const CONTENT_DIR = "urn:schemas-upnp-org:service:ContentDirectory";

function _on_browse_ready (cd, res)
{
    var action = cd.call_action_finish(res)
    let foo = [GObject.TYPE_STRING, GObject.TYPE_UINT, GObject.TYPE_UINT, GObject.TYPE_UINT];

    let [a, b] = action.get_result_list(["Result", "NumberReturned", "TotalMatches", "UpdateID"], foo);

    if (a) {
        print ("Number Returned:", b[1]);
        print ("Total Matches:", b[2]);
        print ("UpdateID:", b[3]);
    }
}

function _on_proxy_available (control_point, proxy) {
    print ("-> Proxy available!", proxy.get_friendly_name ());
    var cd = proxy.get_service (CONTENT_DIR);

    print ("-> Start browsing of", proxy.get_friendly_name ());
    var action = GUPnP.ServiceProxyAction.new_from_list ("Search",
                          ["ContainerID", "Filter", "StartingIndex", "RequestedCount", "SortCriteria", "SearchCriteria"],
                          ["0", "*", 0, 10, "+dc:title", "upnp:class derivedFrom \"object.item\""])
    cd.call_action_async (action, null, _on_browse_ready)
}

function _on_proxy_unavailable (control_point, proxy) {
}

function _on_context_available (manager, context) {

    var dms_cp = new GUPnP.ControlPoint ( { 'client': context, 'target': MEDIA_SERVER } );
    print ("Attaching to", context.host_ip);
    dms_cp.connect('device-proxy-available', _on_proxy_available);
    dms_cp.connect('device-proxy-unavailable', _on_proxy_unavailable);
    dms_cp.set_active (true);

    manager.manage_control_point (dms_cp);
    context.unref();
}


var cm = GUPnP.ContextManager.create_full (GSSDP.UDAVersion.VERSION_1_1, Gio.SocketFamily.INVALID, 0);
cm.connect('context-available', _on_context_available)

Mainloop.run ('hello');
