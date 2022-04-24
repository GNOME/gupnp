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


def do_action_invoked(self, action):
    name = action.get_name()
    try:
        candidate = self.__getattribute__(f'do_{name}')
        if callable(candidate):
            candidate(action)
        else:
            action.return_error(401, "Invalid Action")

    except IndexError:
        action.return_error(401, "Invalid Action")


class SwitchPower(GUPnP.Service):
    __gtype_name__ = "SwitchPower"

    def __init__(self):
        super(GUPnP.Service, self).__init__()
        self.target = False
        self.connect("query-variable::Target", self.query_Target)
        self.connect("query-variable::Status", self.query_Status)

    def do_action_invoked(self, action):
        name = action.get_name()
        try:
            candidate = self.__getattribute__(f'do_{name}')
            if callable(candidate):
                candidate(action)
            else:
                print("Not callable action")
                action.return_error(401, "Invalid Action")

        except IndexError:
            print("No handler...")
            action.return_error(401, "Invalid Action")

    def do_query_variable(self, variable):
        try:
            candidate = self.__getattribute__(f'query_{variable}')
            if callable(candidate):
                return candidate()
        except IndexError:
            pass

        return None

    def do_SetTarget(self, action):
        value = action.get_value("newTargetValue", bool)
        if self.target != value:
            self.target = value
            self.notify_value("Status", value)

        action.return_success()

    def do_GetTarget(self, action):
        action.set_value("RetTargetValue", self.target)
        action.return_success()

    def do_GetStatus(self, action):
        action.set_value("ResultStatus", self.target)
        # action.return_success()

    def query_Status(self):
        print("query_Status")
        return None, self.target

    def query_Target(self, value):
        print("query_Target")
        return None, self.target


GObject.type_register(SwitchPower)


class LightServer(GUPnP.RootDevice):
    __gtype_name__ = "LightServer"

    def __init__(self, context: GUPnP.Context):
        resource_factory = GUPnP.ResourceFactory.get_default()
        resource_factory.register_resource_type ("urn:schemas-upnp-org:service:SwitchPower:1", SwitchPower)
        super().__init__(context=context, resource_factory=resource_factory, description_path="BinaryLight1.xml", description_dir=".")
        self.loop = GLib.MainLoop()
        self.init()
        self.service = self.get_service("urn:schemas-upnp-org:service:SwitchPower:1")

    def run(self):
        self.set_available(True)
        self.loop.run()


if __name__ == '__main__':
    c = GUPnP.Context.new('wlp3s0', 0)
    LightServer(c).run()
