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

imports.gi.versions.GUPnP = "1.2"

const Mainloop = imports.mainloop;
const GObject = imports.gi.GObject;
const GUPnP = imports.gi.GUPnP;

const CONTENT_DIR = "urn:schemas-upnp-org:service:RenderingControl:1";

function _on_ready () {
}

function _on_sp_available (cp, proxy) {
    print ("Got Proxy ", proxy.get_location());
    for (let i = 0; i < 1000; ++i) {
	let action = GUPnP.ServiceProxyAction.new_from_list("GetVolume", ["InstanceId", "Channel"], [0, 0]);
	proxy.call_action(action, null);
	let [success, result] = action.get_result_list(["CurrentVolume"], [GObject.TYPE_FLOAT]);
	print(result);
    }
}

var context = new GUPnP.Context ( {'interface': "wlp3s0"});
context.init(null);
var cp = new GUPnP.ControlPoint ( {'client': context, 'target' : CONTENT_DIR});
cp.connect ("service-proxy-available", _on_sp_available);
cp.active = true;
Mainloop.run ("");
