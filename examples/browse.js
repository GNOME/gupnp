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

const Mainloop = imports.mainloop;
const GUPnP = imports.gi.GUPnP;
const GObject = imports.gi.GObject;
const GLib = imports.gi.GLib;
const Soup = imports.gi.Soup;

const MEDIA_SERVER = "urn:schemas-upnp-org:device:MediaServer:1";
const CONTENT_DIR = "urn:schemas-upnp-org:service:ContentDirectory";

function _on_browse_ready (cd, action)
{
    let foo = [GObject.TYPE_STRING, GObject.TYPE_UINT, GObject.TYPE_UINT, GObject.TYPE_UINT];

    let [a, b] = cd.end_action_list (action, ["Result", "NumberReturned", "TotalMatches", "UpdateID"], foo);
    cd.unref();

    if (a) {
        print ("Number Returned:", b[1]);
        print ("Total Matches:", b[2]);
        print ("UpdateID:", b[3]);
    }
}

function _on_proxy_available (control_point, proxy) {
    print ("-> Proxy available!", proxy.get_friendly_name ());
    var cd = proxy.get_service (CONTENT_DIR);

    // FIXME: This needs probably fixing on C side
    cd.ref();

    print ("-> Start browsing of", proxy.get_friendly_name ());
    cd.begin_action_list ("Search",
                          ["ContainerID", "Filter", "StartingIndex", "RequestedCount", "SortCriteria", "SearchCriteria"],
                          ["0", "*", 0, 10, "+dc:title", "upnp:class derivedFrom \"object.item\""],
                          _on_browse_ready)
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
}


var cm = GUPnP.ContextManager.create (0);
cm.connect('context-available', _on_context_available)

Mainloop.run ('hello');
