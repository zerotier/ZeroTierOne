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

import functools
import socket

from tuntap.tun_tap_harness import TunHarness, TapHarness
from tuntap.packet import (
    ARPPacket,
    EthernetFrame,
    ICMPV6Packet,
    ICMPV6NeighborAdvertisement,
    ICMPV6NeighborSolicitation,
    IPv4Packet,
    IPv6Packet,
    TunAFFrame,
    UDPPacket
)
from tuntap.packet_reader import PacketReader, SelectPacketSource

class PacketCodec(object):
    """
    Helper for tests that wish to send and receive packets. This provides the interface to send and
    receive packets at the IP/IPv6 level on both the network interface and char dev sides.
    """

    def __init__(self, af, listenAddress, newHarness, newPacketSource):
        self._af = af
        self._listenAddress = listenAddress
        self._newHarness = newHarness
        self._newPacketSource = newPacketSource

    def __str__(self):
        af_map = { socket.AF_INET: 'IN', socket.AF_INET6: 'IN6' }
        return '<%s<%s, %s>>' % (self.__class__.__name__,
                                 af_map[self._af],
                                 self._newPacketSource.__name__)

    def _decodePacket(self, packet):
        return packet

    def _framePacket(self, payload):
        return payload

    def _frameExpectation(self, expectation):
        return expectation

    @property
    def af(self):
        return self._af

    @property
    def addr(self):
        if self._af == socket.AF_INET:
            return self._harness.addr
        elif self._af == socket.AF_INET6:
            return self._harness.addr6
        assert False

    @property
    def UDPPort(self):
        return self._recvSock.getsockname()[1]

    def start(self):
        self._harness = self._newHarness()
        self._harness.start()
        self._harness.up()

        self._sendSock = socket.socket(self.addr.af, socket.SOCK_DGRAM)
        self._recvSock = socket.socket(self.addr.af, socket.SOCK_DGRAM)
        self._recvSock.bind((self._listenAddress or self.addr.local, 0))

        self._reader = PacketReader(source = self._newPacketSource(self._harness.char_dev.fileno()),
                                    skip = True,
                                    decode = lambda packet : self._decodePacket(packet))
        self._sockReader = PacketReader(source = SelectPacketSource(self._recvSock.fileno()))

        self._reader.start()
        self._sockReader.start()

    def stop(self):
        self._sockReader.stop()
        self._reader.stop()
        self._harness.stop()
        self._sendSock.close()
        self._recvSock.close()

    def sendUDP(self, payload, addr):
        self._sendSock.sendto(payload, addr)

    def expectUDP(self, expectation):
        self._sockReader.expect(expectation)

    def runUDP(self):
        return self._sockReader.run()

    def sendPacket(self, payload):
        self._harness.char_dev.send(self._framePacket(payload))

    def expectPacket(self, expectation):
        self._reader.expect(self._frameExpectation(expectation))

    def runPacket(self):
        return self._reader.run()


class TunPacketCodec(PacketCodec):
    
    def __init__(self, af, listenAddress, newPacketSource):
        super(TunPacketCodec, self).__init__(af, listenAddress, TunHarness, newPacketSource)

    def _decodePacket(self, packet):
        # Look at the first byte to figure out whether it's IPv4 or IPv6.
        version = (ord(packet[0]) & 0xf0) >> 4
        if version == 4:
            return IPv4Packet(packet)
        elif version == 6:
            return IPv6Packet(packet)
        else:
            return packet


class TunAFPacketCodec(PacketCodec):
    
    def __init__(self, af, listenAddress, newPacketSource):
        super(TunAFPacketCodec, self).__init__(af, listenAddress, TunHarness, newPacketSource)

    def _decodePacket(self, packet):
        return TunAFFrame(packet)

    def _framePacket(self, payload):
        return TunAFFrame(af = self.addr.af, payload = payload).encode()

    def _frameExpectation(self, expectation):
        return { 'af': self.addr.af,
                 'payload': expectation }

    def start(self):
        super(TunAFPacketCodec, self).start()
        self._harness.char_dev.prependAF = 1


class TapPacketCodec(PacketCodec):

    TYPE_MAP = { socket.AF_INET: EthernetFrame.TYPE_IPV4,
                 socket.AF_INET6: EthernetFrame.TYPE_IPV6 }

    ETHER_ADDR_ANY = '\xff\xff\xff\xff\xff\xff'
    ETHER_ADDR_REMOTE = '\x11\x22\x33\x44\x55\x66'
    
    def __init__(self, af, listenAddress, newPacketSource):
        super(TapPacketCodec, self).__init__(af, listenAddress, TapHarness, newPacketSource)

    def _decodePacket(self, packet):
        return EthernetFrame(packet)

    def _framePacket(self, payload):
        return EthernetFrame(src = TapPacketCodec.ETHER_ADDR_REMOTE,
                             dst = self._harness.interface.lladdr.addr,
                             type = TapPacketCodec.TYPE_MAP[self.addr.af],
                             payload = payload).encode()

    def _frameExpectation(self, expectation):
        return { 'type': TapPacketCodec.TYPE_MAP[self.addr.af],
                 'src': self._harness.interface.lladdr.addr,
                 'payload': expectation }

    def _sendArpReply(self, packet):
        reply = EthernetFrame(dst = packet.src,
                              src = TapPacketCodec.ETHER_ADDR_ANY,
                              type = EthernetFrame.TYPE_ARP,
                              payload = ARPPacket(htype = ARPPacket.HTYPE_ETHERNET,
                                                  ptype = ARPPacket.PTYPE_IPV4,
                                                  hlen = ARPPacket.HLEN_ETHERNET,
                                                  plen = ARPPacket.PLEN_IPV4,
                                                  oper = ARPPacket.OPER_REPLY,
                                                  sha = TapPacketCodec.ETHER_ADDR_REMOTE,
                                                  spa = packet.payload.tpa,
                                                  tha = packet.payload.sha,
                                                  tpa = packet.payload.spa))
        self._harness.char_dev.send(reply.encode())

    def _sendNeighborAdvertisement(self, packet):
        reply = EthernetFrame(
                    dst = packet.payload.payload.payload.src_lladdr,
                    src = TapPacketCodec.ETHER_ADDR_ANY,
                    type = EthernetFrame.TYPE_IPV6,
                    payload = IPv6Packet(
                        src = socket.inet_pton(self.addr.af, self.addr.remote),
                        dst = packet.payload.src,
                        proto = IPv6Packet.PROTO_ICMPV6,
                        payload = ICMPV6Packet(
                            type = ICMPV6Packet.TYPE_NEIGHBOR_ADVERTISMENT,
                            payload = ICMPV6NeighborAdvertisement(
                                solicited = 1,
                                override = 1,
                                target = socket.inet_pton(self.addr.af, self.addr.remote),
                                target_lladdr = TapPacketCodec.ETHER_ADDR_REMOTE))))
        self._harness.char_dev.send(reply.encode())

    def start(self):
        super(TapPacketCodec, self).start()
        # Answer ARP resolution requests for the destination address.
        self._reader.expect(
            expectation = { 'type': EthernetFrame.TYPE_ARP,
                            'payload': { 'htype': ARPPacket.HTYPE_ETHERNET,
                                         'ptype': ARPPacket.PTYPE_IPV4,
                                         'hlen': ARPPacket.HLEN_ETHERNET,
                                         'plen': ARPPacket.PLEN_IPV4,
                                         'oper': ARPPacket.OPER_REQUEST,
                                         'tpa': socket.inet_pton(self.addr.af, self.addr.remote) }},
            times = None,
            action = functools.partial(TapPacketCodec._sendArpReply, self))
        # Answer Neighbor Solicitation requests for IPv6.
        self._reader.expect(
            expectation = {
                'type': EthernetFrame.TYPE_IPV6,
                'payload': {
                    'proto': IPv6Packet.PROTO_ICMPV6,
                    'payload': {
                        'type': ICMPV6Packet.TYPE_NEIGHBOR_SOLICITATION,
                        'payload': {
                            'target': socket.inet_pton(self.addr.af, self.addr.remote) }}}},
            times = None,
            action = functools.partial(TapPacketCodec._sendNeighborAdvertisement, self))
