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

class BinStruct(object):
    """
    Handles packing and unpacking of binary data. It is vaguely inspired by the struct module but
    taylored for bit-granular fields. Also, it's probably not very fast :)
    """

    @staticmethod
    def str2num(data, width):
        if not data:
            return 0
        len, rem = divmod(width, 8)
        val = 0
        for i in range(len):
            val = (val << 8) | ord(data[i])
        if rem:
            val = (val << rem) | (ord(data[len]) & ((1 << rem) - 1))
        return val

    @staticmethod
    def num2str(val, width):
        result = bytearray((width + 7) / 8)
        p, rem = divmod(width, 8)
        if rem:
            result[p] = chr(val & ((1 << rem) - 1))
            val >>= rem
        while p > 0:
            p -= 1
            result[p] = chr(val & 0xff)
            val >>= 8
        return str(result)

    def __init__(self, format):
        """
        Initializes a BinStruct object that can encode and decode the binary structure specified in
        the format parameters.

        Args:
            format: Specifies the format of the binary data. The syntax is

                    (<width><type>)*

                where:
                    width is the width of a component in number of bits
                    type is indicates the type of the component and may be one of:
                        s: binary data
                        n: number
        """
        self._format = []
        id = lambda x, width : x or 0
        typemap = {
            'n': (id, id),
            's': (BinStruct.num2str, BinStruct.str2num),
        }
        pos = 0
        self._width = 0
        while pos < len(format):
            start = pos
            while str.isdigit(format[pos]):
                pos += 1
            width = int(format[start:pos])
            self._width += width
            codec = typemap[format[pos]]
            self._format.insert(0, (width, codec[0], codec[1]))
            pos += 1

    @property
    def size(self):
        return (self._width + 7) / 8

    def pack(self, *values):
        """
        Encodes the passed values according to this BinStruct's format definition.

        Args:
            values: The values to encode.
        Returns:
            The encoded struct as a binary string.
        """
        assert len(values) == len(self._format)
        val = 0
        pos = len(self._format)
        for value in values:
            pos -= 1
            (width, decode, encode) = self._format[pos]
            val = (val << width) | (encode(value, width) & ((1 << width) - 1))
        return BinStruct.num2str(val, self._width)
            
    def unpack(self, data):
        """
        Decodes a binary string according to the format definition.

        Args:
            data: The binary string to decode.
        Returns:
            A value tuple.
        """
        assert len(data) >= self.size
        val = BinStruct.str2num(data, self._width)
        pos = len(self._format)
        result = [ None for i in range(pos) ]
        for (width, decode, encode) in self._format:
            pos -= 1
            result[pos] = decode(val & ((1 << width) - 1), width)
            val >>= width
        return tuple(result)


class Packet(object):
    """
    Base class for packet encoding and decoding.
    """
 
    def __init__(self, format, names, data = None, **initializer):
        """
        Initializes the packet.

        Args:
            format: Binary format description.
            names: Names for the packet fields.
            data: Optional binary packet to decode.
            initializer: Optional initialization values for the packet fields.
        """
        self._struct = BinStruct(format)
        self._names = names
        self.__dict__.update(dict.fromkeys(self._names, None))
        self.payload = None

        if isinstance(data, str):
            self.decode(data)
        elif isinstance(data, Packet):
            self.update(data)

        self.__dict__.update(initializer)

    def __repr__(self):
        return repr(dict(map(lambda x : (x, getattr(self, x)), self._names + ('payload',))))

    def _payloadPos(self):
        """
        Returns: The payload position in the data buffer.
        """
        return self._struct.size

    def _decodePayload(self, data):
        """
        Decodes the payload data.

        Args:
            data: Payload data buffer.
        Returns:
            The payload object.
        """
        return data

    def _encodePayload(self):
        """
        Encodes the payload data.

        Args:
            payload: Payload object.
        Returns:
            Encoded payload byte string.
        """
        if issubclass(self.payload.__class__, Packet):
            return self.payload.encode()
        return str(self.payload)
        
    def _encodeFields(self, *fields):
        """
        Takes a fields tuple and returns encoded field data.

        Args:
            fields: Field values.
        Returns:
            Tuple of encoded fields.
        """
        return self._struct.pack(*fields)

    def decode(self, data):
        """
        Decode a binary packet.

        Args:
            data: Binary packet data to decode.
        """
        fields = self._struct.unpack(data)
        assert len(fields) == len(self._names)
        self.__dict__.update(dict(zip(self._names, fields)))
        self.payload = self._decodePayload(data[self._payloadPos():])

    def update(self, data):
        """
        Update the packet from a dictionary.

        Args:
            data: The dictionary to update from.
        """
        self.__dict__.update(map(lambda x : (x, getattr(data, x)), self._names + ('payload',)))
        if isinstance(self.payload, str):
            self.payload = self._decodePayload(self.payload)
        

    def encode(self):
        """
        Encodes the packet into binary format.

        Returns:
            The packet data.
        """
        fields = map(lambda x : getattr(self, x), self._names)
        return self._encodeFields(*fields) + self._encodePayload()

    @property
    def headerLen(self):
        """
        The size of the header according to the format.

        Returns:
            The header length.
        """
        return self._struct.size


class TunAFFrame(Packet):

    def __init__(self, data = None, **initializer):
        super(TunAFFrame, self).__init__('32n', ('af',), data, **initializer)

    def _decodePayload(self, data):
        if self.af == socket.AF_INET:
            return IPv4Packet(data)
        elif self.af == socket.AF_INET6:
            return IPv6Packet(data)
        return data


class EthernetFrame(Packet):

    TYPE_IPV4 = 0x0800
    TYPE_ARP  = 0x0806
    TYPE_IPV6 = 0x86dd

    def __init__(self, data = None, **initializer):
        super(EthernetFrame, self).__init__('48s48s16n', ('dst', 'src', 'type'),
                                             data, **initializer)

    def _decodePayload(self, data):
        if self.type == EthernetFrame.TYPE_IPV4:
            return IPv4Packet(data)
        elif self.type == EthernetFrame.TYPE_ARP:
            return ARPPacket(data)
        elif self.type == EthernetFrame.TYPE_IPV6:
            return IPv6Packet(data)
        return data


class ARPPacket(Packet):

    HTYPE_ETHERNET = 0x01
    HLEN_ETHERNET  = 6
    PTYPE_IPV4     = 0x0800
    PLEN_IPV4      = 4
    OPER_REQUEST   = 1
    OPER_REPLY     = 2

    def __init__(self, data = None, **initializer):
        super(ARPPacket, self).__init__('16n16n8n8n16n48s32s48s32s',
                                        ('htype', 'ptype', 'hlen', 'plen', 'oper',
                                         'sha', 'spa', 'tha', 'tpa'),
                                        data, **initializer)


class IPv4Packet(Packet):

    PROTO_ICMP = 0x01
    PROTO_TCP = 0x06
    PROTO_UDP = 0x11

    class UDPPseudoHeader(Packet):

        def __init__(self, data = None, **initializer):
            super(IPv4Packet.UDPPseudoHeader, self).__init__('32s32s8s8n16n',
                                                             ('src', 'dst',
                                                              'padding', 'proto', 'length'),
                                                             data, **initializer)


    def __init__(self, data = None, **initializer):
        super(IPv4Packet, self).__init__('4n4n6n2n16n16n2n14n8n8n16n32s32s',
                                         ('version', 'hdrlen', 'dscp', 'ecn',
                                          'len', 'id', 'flags', 'fragoffset',
                                          'ttl', 'proto', 'checksum', 'src', 'dst'),
                                         data, **initializer)

    def _payloadPos(self):
        return self.hdrlen * 4

    def _decodePayload(self, data):
        if self.proto == IPv4Packet.PROTO_UDP:
            return UDPPacket(data)
        return data

    @staticmethod
    def computeChecksum(data):
        """
        Computes the IPv4 header checksum.

        Args:
            Header in binary.
        Returns:
            The header checksum.
        """
        sum = 0
        for i in range(0, len(data) - 1, 2):
            sum += ord(data[i]) << 8 | ord(data[i + 1])
        if len(data) % 2 == 1:
            sum += ord(data[-1]) << 8 | 0
        return ~((sum & 0xffff) + (sum >> 16))

    def encode(self):
        payload = self._encodePayload()
        hdrlen = self.hdrlen or 5
        payloadlen = self.len or len(payload)
        fields = [self.version or 4, hdrlen, self.dscp or 0, self.ecn or 0,
                  payloadlen + hdrlen * 4, self.id or 0, self.flags or 0,
                  self.fragoffset or 0, self.ttl or 255, self.proto, self.checksum or 0,
                  self.src, self.dst]

        # Need to compute UDP checksum here since it includes the IPv4 pseudo header.
        if (self.proto == IPv4Packet.PROTO_UDP and
            issubclass(self.payload.__class__, UDPPacket) and
            self.payload.checksum == None):
            
            header = IPv4Packet.UDPPseudoHeader(src = self.src, dst = self.dst,
                                                proto = IPv4Packet.PROTO_UDP, length = payloadlen,
                                                payload = payload)
            payload = UDPPacket(data = self.payload,
                                checksum = IPv4Packet.computeChecksum(header.encode())).encode()

        header = self._encodeFields(*tuple(fields))
        if self.checksum == None:
            fields[10] = IPv4Packet.computeChecksum(header)
            header = self._encodeFields(*tuple(fields))
        return header + payload

class IPv6Packet(Packet):

    PROTO_ICMP = 1
    PROTO_TCP = 6
    PROTO_UDP = 17
    PROTO_ICMPV6 = 58

    class UDPPseudoHeader(Packet):

        def __init__(self, data = None, **initializer):
            super(IPv6Packet.UDPPseudoHeader, self).__init__('128s128s32n24s8n',
                                                             ('src', 'dst',
                                                              'length', 'padding', 'proto'),
                                                             data, **initializer)


    def __init__(self, data = None, **initializer):
        super(IPv6Packet, self).__init__('4n8n20n16n8n8n128s128s',
                                         ('version', 'traffic_class', 'flow_label',
                                          'len', 'proto', 'hop_limit',
                                          'src', 'dst'),
                                         data, **initializer)

    def _decodePayload(self, data):
        if self.proto == IPv6Packet.PROTO_UDP:
            return UDPPacket(data)
        elif self.proto == IPv6Packet.PROTO_ICMPV6:
            return ICMPV6Packet(data)
        return data

    def encode(self):
        payload = self._encodePayload()
        fields = [self.version or 6, self.traffic_class or 0, self.flow_label or 0,
                  self.len or len(payload), self.proto, self.hop_limit or 255,
                  self.src, self.dst]

        # Need to compute checksum for UDP, ICMPV6 here since it includes the IPv6 pseudo header.
        checksummedProtos = { IPv6Packet.PROTO_UDP: UDPPacket,
                              IPv6Packet.PROTO_ICMPV6: ICMPV6Packet }
        payloadClass = checksummedProtos.get(self.proto)
        if (payloadClass != None and
            issubclass(self.payload.__class__, payloadClass) and
            self.payload.checksum == None):
            
            header = IPv6Packet.UDPPseudoHeader(src = self.src, dst = self.dst, length = fields[3],
                                                proto = self.proto, payload = payload)
            payload = payloadClass(data = self.payload,
                                   checksum = IPv4Packet.computeChecksum(header.encode())).encode()

        return self._encodeFields(*tuple(fields)) + payload


class ICMPV6Packet(Packet):

    TYPE_NEIGHBOR_SOLICITATION = 135
    TYPE_NEIGHBOR_ADVERTISMENT = 136

    def __init__(self, data = None, **initializer):
        super(ICMPV6Packet, self).__init__('8n8n16n',
                                           ('type', 'code', 'checksum'),
                                           data, **initializer)
                                        
    def _decodePayload(self, data):
        if self.type == ICMPV6Packet.TYPE_NEIGHBOR_SOLICITATION:
            return ICMPV6NeighborSolicitation(data)
        elif self.type == ICMPV6Packet.TYPE_NEIGHBOR_ADVERTISMENT:
            return ICMPV6NeighborAdvertisement(data)
        return data


class ICMPV6NeighborDiscoveryOption(Packet):

    TYPE_SOURCE_LINK_LAYER_ADDRESS = 1
    TYPE_TARGET_LINK_LAYER_ADDRESS = 2

    def __init__(self, data = None, **initializer):
        super(ICMPV6NeighborDiscoveryOption, self).__init__('8n8n',
                                                            ('type', 'length'),
                                                            data, **initializer)

    def encode(self):
        payload = self._encodePayload()
        length = self.length
        if length == None:
            length = (len(payload) + 2 + 7) / 8
            payload += '\x00' * (length * 8 - len(payload) - 2)
        fields = [self.type, length]
        header = self._encodeFields(*tuple(fields))
        return header + payload

    @staticmethod
    def decodeOptions(data):
        options = []
        while len(data) > 2:
            type = ord(data[0])
            length = ord(data[1])
            if len(data) < length * 8:
                break
            options.append(ICMPV6NeighborDiscoveryOption(type = type, length = length,
                                                         payload = data[0:length * 8]))
            data = data[length * 8:]
        return options


class ICMPV6NeighborSolicitation(Packet):

    def __init__(self, data = None, **initializer):
        super(ICMPV6NeighborSolicitation, self).__init__('32s128s',
                                                         ('reserved', 'target'),
                                                         data, **initializer)
        self.target_lladdr = initializer.get('src_lladdr')

    def _decodePayload(self, data):
        for option in ICMPV6NeighborDiscoveryOption.decodeOptions(data):
            if option.type ==  ICMPV6NeighborDiscoveryOption.TYPE_SOURCE_LINK_LAYER_ADDRESS:
                self.src_lladdr = option.payload
        return None

    def _encodePayload(self):
        if self.src_lladdr:
            return ICMPV6NeighborDiscoveryOption(
                type = ICMPV6NeighborDiscoveryOption.TYPE_SOURCE_LINK_LAYER_ADDRESS,
                payload = self.src_lladdr).encode()
        return ''


class ICMPV6NeighborAdvertisement(Packet):

    def __init__(self, data = None, **initializer):
        super(ICMPV6NeighborAdvertisement, self).__init__('1n1n1n29s128s',
                                                          ('router', 'solicited', 'override',
                                                           'reserved', 'target'),
                                                          data, **initializer)
        self.target_lladdr = initializer.get('target_lladdr')

    def _decodePayload(self, data):
        for option in ICMPV6NeighborDiscoveryOptions.decodeOptions(data):
            if option.type ==  ICMPV6NeighborDiscoveryOption.TYPE_TARGET_LINK_LAYER_ADDRESS:
                self.target_lladdr = option.payload
        return None

    def _encodePayload(self):
        if self.target_lladdr:
            return ICMPV6NeighborDiscoveryOption(
                type = ICMPV6NeighborDiscoveryOption.TYPE_TARGET_LINK_LAYER_ADDRESS,
                payload = self.target_lladdr).encode()
        return ''


class UDPPacket(Packet):

    def __init__(self, data = None, **initializer):
        super(UDPPacket, self).__init__('16n16n16n16n',
                                        ('src', 'dst', 'len', 'checksum'),
                                        data, **initializer)

    def encode(self):
        payload = self._encodePayload()
        packetlen = self.len or (len(payload) + self.headerLen)
        fields = [self.src, self.dst, packetlen, self.checksum or 0]
        header = self._encodeFields(*tuple(fields))
        return header + payload
