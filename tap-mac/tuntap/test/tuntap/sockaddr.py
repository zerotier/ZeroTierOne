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

import socket
import struct

class SockaddrDl(object):

    AF_LINK = 18
    STRUCT = struct.Struct('BBH4B')

    def __init__(self, name, addr, type, index = 0, af = AF_LINK):
        self.af = af
        self.index = index
        self.type = type
        self.name = name
        self.addr = addr

    def __repr__(self):
        return 'SockaddrDl<%d, %d, %d, %s, %s>' % (self.af, self.index, self.type,
                                                   self.name, repr(self.addr))

    def __eq__(self, other):
        return (self.af == other.af and self.index == other.index and self.type == other.type and
                self.name == other.name and self.addr == other.addr)

    def encode(self):
        # It's important to make this size 12 at least to meet sizeof(struct sockaddr_dl), routing
        # setup chokes if it's not.
        datalen = max(len(self.name) + len(self.addr), 12) 
        namelen = datalen - len(self.addr)
        data = SockaddrDl.STRUCT.pack(SockaddrDl.STRUCT.size + datalen,
                                      self.af, self.index, self.type,
                                      namelen, len(self.addr), 0) 
        return data + self.name + '\x00' * (namelen - len(self.name)) + self.addr

    @classmethod
    def decode(self, data):
        fields = SockaddrDl.STRUCT.unpack_from(data)
        pname = SockaddrDl.STRUCT.size
        paddr = pname + fields[4]
        pend = paddr + fields[5]
        return SockaddrDl(af = fields[1], index = fields[2], type = fields[3],
                          name = data[pname:paddr], addr = data[paddr:pend])


class SockaddrIn(object):
    """
    Python wrapper for struct sockaddr_in.
    """

    STRUCT = struct.Struct('BBH4s8x')

    def __init__(self, addr, port = 0, af = socket.AF_INET):
        self.addr = addr or '0.0.0.0'
        self.port = port
        self.af = af

    def __repr__(self):
        return 'SockaddrIn<%d, %d, %s>' % (self.af, self.port, self.addr)

    def __eq__(self, other):
        return self.encode() == other.encode()

    def encode(self):
        return SockaddrIn.STRUCT.pack(16, self.af, self.port, socket.inet_aton(self.addr))

    @classmethod
    def decode(cls, data):
        t = SockaddrIn.STRUCT.unpack(data)
        return SockaddrIn(addr = socket.inet_ntoa(t[3]), port = t[2], af = t[1])


class SockaddrIn6(object):
    """
    Python wrapper for struct sockaddr_in6.
    """

    STRUCT = struct.Struct('BBHI16sI')

    def __init__(self, addr, port = 0, af = socket.AF_INET6, flowinfo = 0, scopeid = 0):
        self.addr = addr or '::0'
        self.port = port
        self.af = af
        self.flowinfo = flowinfo
        self.scopeid = scopeid

    def __repr__(self):
        return 'SockaddrIn6<%d, %d, %s, %d, %d>' % (self.af, self.port, self.addr,
                                                    self.flowinfo, self.scopeid)

    def __eq__(self, other):
        return self.encode() == other.encode()

    def encode(self):
        return SockaddrIn6.STRUCT.pack(28, self.af, self.port, self.flowinfo,
                                       socket.inet_pton(socket.AF_INET6, self.addr), self.scopeid)

    @classmethod
    def decode(cls, data):
        t = SockaddrIn6.STRUCT.unpack(data)
        return SockaddrIn6(addr = socket.inet_ntop(socket.AF_INET6, t[4]), port = t[2], af = t[1],
                           flowinfo = t[3], scopeid = t[5])


