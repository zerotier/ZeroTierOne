# Copyright (c) 2011 Mattias Nissler <mattias.nissler@gmx.de>
#
# Redistribution and use in source and binary forms, with or without modification, are permitted
# provided that the following conditions are met:
#
#   1. Redistributions of source code must retain the above copyright notice, this list of
#      conditions and the following disclaimer.
#   2. Redistributions in binary form must reproduce the above copyright notice, this list of
#      conditions and the following disclaimer in the documentation and/or other materials provided
#      with the distribution.
#   3. The name of the author may not be used to endorse or promote products derived from this
#      software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import errno
import socket

from tuntap.char_dev_harness import TunCharDevHarness, TapCharDevHarness
from tuntap.interface_harness import Address, InterfaceHarness
import tuntap.route

class TunTapHarness(object):

    def __init__(self, name, newCharDevHarness, addr, addr6):
        self._newCharDevHarness = newCharDevHarness
        self.name = name
        self.addr = addr
        self.addr6 = addr6

    def start(self):
        self.char_dev = self._newCharDevHarness()
        self.char_dev.open()
        self.interface = InterfaceHarness(self.name, self.char_dev.unit)

    def up(self):
        self.interface.addIfAddr(local = self.addr.sa_local,
                                 dst = self.addr.sa_dst,
                                 mask = self.addr.sa_mask)
        self.interface.addIfAddr6(local = self.addr6.sa_local,
                                  dst = self.addr6.sa_dst,
                                  mask = self.addr6.sa_mask)

        # Lion automatically creates routes for IPv6 addresses, earlier versions don't.
        try:
            tuntap.route.addNet(dst = self.addr6.sa_remote,
                                netmask = self.addr6.sa_mask,
                                interface = self.interface.lladdr)
        except IOError as e:
            if e.errno != errno.EEXIST:
                raise e

        self.interface.flags |= InterfaceHarness.IFF_UP

    def stop(self):
        self.interface.flags &= ~InterfaceHarness.IFF_UP
        self.char_dev.close()


class TunHarness(TunTapHarness):

    def __init__(self, 
                 addr = Address(af = socket.AF_INET,
                                local = '10.0.0.1',
                                remote = '10.0.0.2',
                                dst = '10.0.0.2',
                                mask = '255.255.255.255'),
                 addr6 = Address(af = socket.AF_INET6,
                                 local = 'fd00::1',
                                 remote = 'fd00::2',
                                 dst = 'fd00::2',
                                 mask = 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff')):
        super(TunHarness, self).__init__('tun', TunCharDevHarness, addr, addr6)


class TapHarness(TunTapHarness):

    def __init__(self, 
                 addr = Address(af = socket.AF_INET,
                                local = '10.0.0.1',
                                remote = '10.0.0.2',
                                dst = '10.255.255.255',
                                mask = '255.0.0.0'),
                 addr6 = Address(af = socket.AF_INET6,
                                 local = 'fd00::1',
                                 remote = 'fd00::2',
                                 dst = None,
                                 mask = 'ffff:ffff:ffff:ffff::0')):
        super(TapHarness, self).__init__('tap', TapCharDevHarness, addr, addr6)
