#!/usr/bin/env python3

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#         SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

import gi
gi.require_version('GUPnP', '1.6')

from gi.repository import GUPnP
from gi.repository import GLib
from gi.repository import GObject

RENDERING_CONTROL = "urn:schemas-upnp-org:service:RenderingControl:1"

def do_quit():
    global l

    l.quit()
    return False

def on_service_introspection(proxy, res):
    result = proxy.introspect_finish(res)
    action_info = result.get_action('GetVolume')
    state_variable_name = action_info.arguments[1].related_state_variable

    channel = result.get_state_variable(state_variable_name)
    print (f'Calling GetVolume for channel {channel.allowed_values[0]}')

    action = GUPnP.ServiceProxyAction.new_plain('GetVolume');
    action.add_argument("InstanceID", 0).add_argument("Channel", channel.allowed_values[0])
    proxy.call_action (action, None)
    it = action.iterate()
    while it.next():
        print(it.get_name(), it.get_value()[1])

def on_sp_available(cp, proxy):
    print(f'Got ServiceProxy {proxy.get_id()} at {proxy.get_location()}')
    print('Introspecting service...')
    proxy.introspect_async(None, on_service_introspection)

if __name__ == '__main__':
    global l
    print("Looking for renderers and querying volumes fo 10s")

    l = GLib.MainLoop()
    context = GUPnP.Context.new ('wlp3s0', 0)
    cp = GUPnP.ControlPoint.new (context, RENDERING_CONTROL)
    cp.set_active (True)

    cp.connect ('service-proxy-available', on_sp_available)
    GLib.timeout_add_seconds (10, do_quit)
    l.run()
