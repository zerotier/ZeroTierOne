/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_N_PACKET_HPP
#define ZT_N_PACKET_HPP

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <string>
#include <iostream>

#include "Constants.hpp"

#include "Address.hpp"
#include "Poly1305.hpp"
#include "Salsa20.hpp"
#include "Utils.hpp"
#include "Buffer.hpp"

/**
 * Protocol version -- incremented only for major changes
 *
 * 1  - 0.2.0 ... 0.2.5
 * 2  - 0.3.0 ... 0.4.5
 *    + Added signature and originating peer to multicast frame
 *    + Double size of multicast frame bloom filter
 * 3  - 0.5.0 ... 0.6.0
 *    + Yet another multicast redesign
 *    + New crypto completely changes key agreement cipher
 * 4  - 0.6.0 ... 1.0.6
 *    + BREAKING CHANGE: New identity format based on hashcash design
 * 5  - 1.1.0 ... 1.1.5
 *    + Supports echo
 *    + Supports in-band world (root server definition) updates
 *    + Clustering! (Though this will work with protocol v4 clients.)
 *    + Otherwise backward compatible with protocol v4
 * 6  - 1.1.5 ... 1.1.10
 *    + Network configuration format revisions including binary values
 * 7  - 1.1.10 ... 1.1.17
 *    + Introduce trusted paths for local SDN use
 * 8  - 1.1.17 ... 1.2.0
 *    + Multipart network configurations for large network configs
 *    + Tags and Capabilities
 *    + ZT_ALWAYS_INLINE push of CertificateOfMembership deprecated
 * 9  - 1.2.0 ... 1.2.14
 * 10 - 1.4.0 ... 1.6.0
 *    + Multipath capability and load balancing
 * 11 - 2.0.0 ... CURRENT
 *    + Peer-to-peer multicast replication (optional)
 *    + Old planet/moon stuff is DEAD!
 *    + AES-256-GMAC-CTR encryption is now the default
 *    + NIST P-384 (type 1) identities now supported
 *    + WILL_RELAY allows mesh-like operation
 *    + Ephemeral keys are now negotiated opportunistically
 */
#define ZT_PROTO_VERSION 11

/**
 * Minimum supported protocol version
 */
#define ZT_PROTO_VERSION_MIN 6

/**
 * Maximum hop count allowed by packet structure (3 bits, 0-7)
 *
 * This is a protocol constant. It's the maximum allowed by the length
 * of the hop counter -- three bits. See node/Constants.hpp for the
 * pragmatic forwarding limit, which is typically lower.
 */
#define ZT_PROTO_MAX_HOPS 7

/**
 * NONE/Poly1305 (using Salsa20/12 to generate poly1305 key)
 */
#define ZT_PROTO_CIPHER_SUITE__POLY1305_NONE 0

/**
 * Salsa2012/Poly1305
 */
#define ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012 1

/**
 * No encryption or authentication at all
 *
 * For trusted paths the MAC field is the trusted path ID.
 */
#define ZT_PROTO_CIPHER_SUITE__NONE 2

/**
 * AES-GMAC_SIV with AES-256
 */
#define ZT_PROTO_CIPHER_SUITE__AES256_GMAC_SIV 3

/**
 * Header flag indicating that a packet is fragmented
 *
 * If this flag is set, the receiver knows to expect more than one fragment.
 * See Packet::Fragment for details.
 */
#define ZT_PROTO_FLAG_FRAGMENTED 0x40

/**
 * Verb flag indicating payload is compressed with LZ4
 */
#define ZT_PROTO_VERB_FLAG_COMPRESSED 0x80

// Field indexes in packet header
#define ZT_PACKET_IDX_IV 0
#define ZT_PACKET_IDX_DEST 8
#define ZT_PACKET_IDX_SOURCE 13
#define ZT_PACKET_IDX_FLAGS 18
#define ZT_PACKET_IDX_MAC 19
#define ZT_PACKET_IDX_VERB 27
#define ZT_PACKET_IDX_PAYLOAD 28

/**
 * Packet buffer size (can be changed)
 */
#define ZT_PROTO_MAX_PACKET_LENGTH (ZT_MAX_PACKET_FRAGMENTS * ZT_DEFAULT_PHYSMTU)

/**
 * Minimum viable packet length (a.k.a. header length)
 */
#define ZT_PROTO_MIN_PACKET_LENGTH ZT_PACKET_IDX_PAYLOAD

// Indexes of fields in fragment header
#define ZT_PACKET_FRAGMENT_IDX_PACKET_ID 0
#define ZT_PACKET_FRAGMENT_IDX_DEST 8
#define ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR 13
#define ZT_PACKET_FRAGMENT_IDX_FRAGMENT_NO 14
#define ZT_PACKET_FRAGMENT_IDX_HOPS 15
#define ZT_PACKET_FRAGMENT_IDX_PAYLOAD 16

/**
 * Magic number found at ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR
 */
#define ZT_PACKET_FRAGMENT_INDICATOR ZT_ADDRESS_RESERVED_PREFIX

/**
 * Minimum viable fragment length
 */
#define ZT_PROTO_MIN_FRAGMENT_LENGTH ZT_PACKET_FRAGMENT_IDX_PAYLOAD

// Field indices for parsing verbs -------------------------------------------

// Some verbs have variable-length fields. Those aren't fully defined here
// yet-- instead they are parsed using relative indexes in IncomingPacket.
// See their respective handler functions.

#define ZT_PROTO_VERB_HELLO_IDX_PROTOCOL_VERSION (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_HELLO_IDX_MAJOR_VERSION (ZT_PROTO_VERB_HELLO_IDX_PROTOCOL_VERSION + 1)
#define ZT_PROTO_VERB_HELLO_IDX_MINOR_VERSION (ZT_PROTO_VERB_HELLO_IDX_MAJOR_VERSION + 1)
#define ZT_PROTO_VERB_HELLO_IDX_REVISION (ZT_PROTO_VERB_HELLO_IDX_MINOR_VERSION + 1)
#define ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP (ZT_PROTO_VERB_HELLO_IDX_REVISION + 2)
#define ZT_PROTO_VERB_HELLO_IDX_IDENTITY (ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP + 8)

#define ZT_PROTO_VERB_ERROR_IDX_IN_RE_VERB (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_ERROR_IDX_IN_RE_PACKET_ID (ZT_PROTO_VERB_ERROR_IDX_IN_RE_VERB + 1)
#define ZT_PROTO_VERB_ERROR_IDX_ERROR_CODE (ZT_PROTO_VERB_ERROR_IDX_IN_RE_PACKET_ID + 8)
#define ZT_PROTO_VERB_ERROR_IDX_PAYLOAD (ZT_PROTO_VERB_ERROR_IDX_ERROR_CODE + 1)

#define ZT_PROTO_VERB_OK_IDX_IN_RE_VERB (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_OK_IDX_IN_RE_PACKET_ID (ZT_PROTO_VERB_OK_IDX_IN_RE_VERB + 1)
#define ZT_PROTO_VERB_OK_IDX_PAYLOAD (ZT_PROTO_VERB_OK_IDX_IN_RE_PACKET_ID + 8)

#define ZT_PROTO_VERB_WHOIS_IDX_ZTADDRESS (ZT_PACKET_IDX_PAYLOAD)

#define ZT_PROTO_VERB_RENDEZVOUS_IDX_FLAGS (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_RENDEZVOUS_IDX_ZTADDRESS (ZT_PROTO_VERB_RENDEZVOUS_IDX_FLAGS + 1)
#define ZT_PROTO_VERB_RENDEZVOUS_IDX_PORT (ZT_PROTO_VERB_RENDEZVOUS_IDX_ZTADDRESS + 5)
#define ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRLEN (ZT_PROTO_VERB_RENDEZVOUS_IDX_PORT + 2)
#define ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRESS (ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRLEN + 1)

#define ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_FRAME_IDX_ETHERTYPE (ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID + 8)
#define ZT_PROTO_VERB_FRAME_IDX_PAYLOAD (ZT_PROTO_VERB_FRAME_IDX_ETHERTYPE + 2)

#define ZT_PROTO_VERB_EXT_FRAME_IDX_NETWORK_ID (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_EXT_FRAME_LEN_NETWORK_ID 8
#define ZT_PROTO_VERB_EXT_FRAME_IDX_FLAGS (ZT_PROTO_VERB_EXT_FRAME_IDX_NETWORK_ID + ZT_PROTO_VERB_EXT_FRAME_LEN_NETWORK_ID)
#define ZT_PROTO_VERB_EXT_FRAME_LEN_FLAGS 1
#define ZT_PROTO_VERB_EXT_FRAME_IDX_COM (ZT_PROTO_VERB_EXT_FRAME_IDX_FLAGS + ZT_PROTO_VERB_EXT_FRAME_LEN_FLAGS)
#define ZT_PROTO_VERB_EXT_FRAME_IDX_TO (ZT_PROTO_VERB_EXT_FRAME_IDX_FLAGS + ZT_PROTO_VERB_EXT_FRAME_LEN_FLAGS)
#define ZT_PROTO_VERB_EXT_FRAME_LEN_TO 6
#define ZT_PROTO_VERB_EXT_FRAME_IDX_FROM (ZT_PROTO_VERB_EXT_FRAME_IDX_TO + ZT_PROTO_VERB_EXT_FRAME_LEN_TO)
#define ZT_PROTO_VERB_EXT_FRAME_LEN_FROM 6
#define ZT_PROTO_VERB_EXT_FRAME_IDX_ETHERTYPE (ZT_PROTO_VERB_EXT_FRAME_IDX_FROM + ZT_PROTO_VERB_EXT_FRAME_LEN_FROM)
#define ZT_PROTO_VERB_EXT_FRAME_LEN_ETHERTYPE 2
#define ZT_PROTO_VERB_EXT_FRAME_IDX_PAYLOAD (ZT_PROTO_VERB_EXT_FRAME_IDX_ETHERTYPE + ZT_PROTO_VERB_EXT_FRAME_LEN_ETHERTYPE)

#define ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_NETWORK_ID (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT_LEN (ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_NETWORK_ID + 8)
#define ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT (ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT_LEN + 2)

#define ZT_PROTO_VERB_MULTICAST_GATHER_IDX_NETWORK_ID (ZT_PACKET_IDX_PAYLOAD)
#define ZT_PROTO_VERB_MULTICAST_GATHER_IDX_FLAGS (ZT_PROTO_VERB_MULTICAST_GATHER_IDX_NETWORK_ID + 8)
#define ZT_PROTO_VERB_MULTICAST_GATHER_IDX_MAC (ZT_PROTO_VERB_MULTICAST_GATHER_IDX_FLAGS + 1)
#define ZT_PROTO_VERB_MULTICAST_GATHER_IDX_ADI (ZT_PROTO_VERB_MULTICAST_GATHER_IDX_MAC + 6)
#define ZT_PROTO_VERB_MULTICAST_GATHER_IDX_GATHER_LIMIT (ZT_PROTO_VERB_MULTICAST_GATHER_IDX_ADI + 4)
#define ZT_PROTO_VERB_MULTICAST_GATHER_IDX_COM (ZT_PROTO_VERB_MULTICAST_GATHER_IDX_GATHER_LIMIT + 4)

#define ZT_PROTO_VERB_HELLO__OK__IDX_TIMESTAMP (ZT_PROTO_VERB_OK_IDX_PAYLOAD)
#define ZT_PROTO_VERB_HELLO__OK__IDX_PROTOCOL_VERSION (ZT_PROTO_VERB_HELLO__OK__IDX_TIMESTAMP + 8)
#define ZT_PROTO_VERB_HELLO__OK__IDX_MAJOR_VERSION (ZT_PROTO_VERB_HELLO__OK__IDX_PROTOCOL_VERSION + 1)
#define ZT_PROTO_VERB_HELLO__OK__IDX_MINOR_VERSION (ZT_PROTO_VERB_HELLO__OK__IDX_MAJOR_VERSION + 1)
#define ZT_PROTO_VERB_HELLO__OK__IDX_REVISION (ZT_PROTO_VERB_HELLO__OK__IDX_MINOR_VERSION + 1)

#define ZT_PROTO_VERB_WHOIS__OK__IDX_IDENTITY (ZT_PROTO_VERB_OK_IDX_PAYLOAD)

#define ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_NETWORK_ID (ZT_PROTO_VERB_OK_IDX_PAYLOAD)
#define ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_DICT_LEN (ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_NETWORK_ID + 8)
#define ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_DICT (ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_DICT_LEN + 2)

#define ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_NETWORK_ID (ZT_PROTO_VERB_OK_IDX_PAYLOAD)
#define ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_MAC (ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_NETWORK_ID + 8)
#define ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_ADI (ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_MAC + 6)
#define ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_GATHER_RESULTS (ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_ADI + 4)

#define ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_NETWORK_ID (ZT_PROTO_VERB_OK_IDX_PAYLOAD)
#define ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_MAC (ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_NETWORK_ID + 8)
#define ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_ADI (ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_MAC + 6)
#define ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_FLAGS (ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_ADI + 4)
#define ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_COM_AND_GATHER_RESULTS (ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_FLAGS + 1)

// ---------------------------------------------------------------------------

namespace ZeroTier {

/**
 * ZeroTier packet
 *
 * Packet format:
 *   <[8] 64-bit packet ID / crypto IV>
 *   <[5] destination ZT address>
 *   <[5] source ZT address>
 *   <[1] flags/cipher/hops>
 *   <[8] 64-bit MAC (or trusted path ID in trusted path mode)>
 *   [... -- begin encryption envelope -- ...]
 *   <[1] encrypted flags (MS 3 bits) and verb (LS 5 bits)>
 *   [... verb-specific payload ...]
 *
 * Packets smaller than 28 bytes are invalid and silently discarded.
 *
 * The 64-bit packet ID is a strongly random value used as a crypto IV.
 * Its least significant 3 bits are also used as a monotonically increasing
 * (and looping) counter for sending packets to a particular recipient. This
 * can be used for link quality monitoring and reporting and has no crypto
 * impact as it does not increase the likelihood of an IV collision. (The
 * crypto we use is not sensitive to the nature of the IV, only that it does
 * not repeat.)
 *
 * The flags/cipher/hops bit field is: FFCCCHHH where C is a 3-bit cipher
 * selection allowing up to 7 cipher suites, F is outside-envelope flags,
 * and H is hop count.
 *
 * The three-bit hop count is the only part of a packet that is mutable in
 * transit without invalidating the MAC. All other bits in the packet are
 * immutable. This is because intermediate nodes can increment the hop
 * count up to 7 (protocol max).
 *
 * For unencrypted packets, MAC is computed on plaintext. Only HELLO is ever
 * sent in the clear, as it's the "here is my public key" message.
 */
class Packet : public Buffer<ZT_PROTO_MAX_PACKET_LENGTH>
{
public:
	/**
	 * A packet fragment
	 *
	 * Fragments are sent if a packet is larger than UDP MTU. The first fragment
	 * is sent with its normal header with the fragmented flag set. Remaining
	 * fragments are sent this way.
	 *
	 * The fragmented bit indicates that there is at least one fragment. Fragments
	 * themselves contain the total, so the receiver must "learn" this from the
	 * first fragment it receives.
	 *
	 * Fragments are sent with the following format:
	 *   <[8] packet ID of packet whose fragment this belongs to>
	 *   <[5] destination ZT address>
	 *   <[1] 0xff, a reserved address, signals that this isn't a normal packet>
	 *   <[1] total fragments (most significant 4 bits), fragment no (LS 4 bits)>
	 *   <[1] ZT hop count (top 5 bits unused and must be zero)>
	 *   <[...] fragment data>
	 *
	 * The protocol supports a maximum of 16 fragments. If a fragment is received
	 * before its main packet header, it should be cached for a brief period of
	 * time to see if its parent arrives. Loss of any fragment constitutes packet
	 * loss; there is no retransmission mechanism. The receiver must wait for full
	 * receipt to authenticate and decrypt; there is no per-fragment MAC. (But if
	 * fragments are corrupt, the MAC will fail for the whole assembled packet.)
	 */
	class Fragment : public Buffer<ZT_PROTO_MAX_PACKET_LENGTH>
	{
	public:
		ZT_ALWAYS_INLINE Fragment() :
			Buffer<ZT_PROTO_MAX_PACKET_LENGTH>() {}

		template<unsigned int C2>
		ZT_ALWAYS_INLINE Fragment(const Buffer<C2> &b) :
			Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(b) {}

		ZT_ALWAYS_INLINE Fragment(const void *data,unsigned int len) :
			Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(data,len) {}

		/**
		 * Initialize from a packet
		 *
		 * @param p Original assembled packet
		 * @param fragStart Start of fragment (raw index in packet data)
		 * @param fragLen Length of fragment in bytes
		 * @param fragNo Which fragment (>= 1, since 0 is Packet with end chopped off)
		 * @param fragTotal Total number of fragments (including 0)
		 */
		ZT_ALWAYS_INLINE Fragment(const Packet &p,unsigned int fragStart,unsigned int fragLen,unsigned int fragNo,unsigned int fragTotal)
		{
			init(p,fragStart,fragLen,fragNo,fragTotal);
		}

		/**
		 * Initialize from a packet
		 *
		 * @param p Original assembled packet
		 * @param fragStart Start of fragment (raw index in packet data)
		 * @param fragLen Length of fragment in bytes
		 * @param fragNo Which fragment (>= 1, since 0 is Packet with end chopped off)
		 * @param fragTotal Total number of fragments (including 0)
		 */
		ZT_ALWAYS_INLINE void init(const Packet &p,unsigned int fragStart,unsigned int fragLen,unsigned int fragNo,unsigned int fragTotal)
		{
			if ((fragStart + fragLen) > p.size())
				throw ZT_EXCEPTION_OUT_OF_BOUNDS;
			setSize(fragLen + ZT_PROTO_MIN_FRAGMENT_LENGTH);

			// NOTE: this copies both the IV/packet ID and the destination address.
			memcpy(field(ZT_PACKET_FRAGMENT_IDX_PACKET_ID,13),p.field(ZT_PACKET_IDX_IV,13),13);

			(*this)[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] = ZT_PACKET_FRAGMENT_INDICATOR;
			(*this)[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_NO] = (char)(((fragTotal & 0xf) << 4) | (fragNo & 0xf));
			(*this)[ZT_PACKET_FRAGMENT_IDX_HOPS] = 0;

			memcpy(field(ZT_PACKET_FRAGMENT_IDX_PAYLOAD,fragLen),p.field(fragStart,fragLen),fragLen);
		}

		/**
		 * Get this fragment's destination
		 *
		 * @return Destination ZT address
		 */
		ZT_ALWAYS_INLINE Address destination() const { return Address(field(ZT_PACKET_FRAGMENT_IDX_DEST,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); }

		/**
		 * @return True if fragment is of a valid length
		 */
		ZT_ALWAYS_INLINE bool lengthValid() const { return (size() >= ZT_PACKET_FRAGMENT_IDX_PAYLOAD); }

		/**
		 * @return ID of packet this is a fragment of
		 */
		ZT_ALWAYS_INLINE uint64_t packetId() const { return at<uint64_t>(ZT_PACKET_FRAGMENT_IDX_PACKET_ID); }

		/**
		 * @return Total number of fragments in packet
		 */
		ZT_ALWAYS_INLINE unsigned int totalFragments() const { return (((unsigned int)((*this)[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_NO]) >> 4) & 0xf); }

		/**
		 * @return Fragment number of this fragment
		 */
		ZT_ALWAYS_INLINE unsigned int fragmentNumber() const { return ((unsigned int)((*this)[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_NO]) & 0xf); }

		/**
		 * @return Fragment ZT hop count
		 */
		ZT_ALWAYS_INLINE unsigned int hops() const { return (unsigned int)((*this)[ZT_PACKET_FRAGMENT_IDX_HOPS]); }

		/**
		 * Increment this packet's hop count
		 */
		ZT_ALWAYS_INLINE unsigned int incrementHops()
		{
			return (unsigned int)((*this)[ZT_PACKET_FRAGMENT_IDX_HOPS] = (((*this)[ZT_PACKET_FRAGMENT_IDX_HOPS]) + 1));
		}

		/**
		 * @return Length of payload in bytes
		 */
		ZT_ALWAYS_INLINE unsigned int payloadLength() const { return ((size() > ZT_PACKET_FRAGMENT_IDX_PAYLOAD) ? (size() - ZT_PACKET_FRAGMENT_IDX_PAYLOAD) : 0); }

		/**
		 * @return Raw packet payload
		 */
		ZT_ALWAYS_INLINE const unsigned char *payload() const { return field(ZT_PACKET_FRAGMENT_IDX_PAYLOAD,size() - ZT_PACKET_FRAGMENT_IDX_PAYLOAD); }
	};

	/**
	 * ZeroTier protocol verbs
	 */
	enum Verb /* Max value: 32 (5 bits) */
	{
		/**
		 * No operation (ignored, no reply)
		 */
		VERB_NOP = 0x00,

		/**
		 * Announcement of a node's existence and vitals:
		 *   <[1] protocol version>
		 *   <[1] software major version>
		 *   <[1] software minor version>
		 *   <[2] software revision>
		 *   <[8] timestamp for determining latency>
		 *   <[...] binary serialized identity (see Identity)>
		 *   <[...] physical destination address of packet>
		 *
		 * HELLO is sent in the clear as it is how peers share their identity
		 * public keys.
		 *
		 * Destination address is the actual wire address to which the packet
		 * was sent. See InetAddress::serialize() for format.
		 *
		 * OK payload:
		 *   <[8] HELLO timestamp field echo>
		 *   <[1] protocol version>
		 *   <[1] software major version>
		 *   <[1] software minor version>
		 *   <[2] software revision>
		 *   <[...] physical destination address of packet>
		 *
		 * With the exception of the timestamp, the other fields pertain to the
		 * respondent who is sending OK and are not echoes.
		 *
		 * ERROR has no payload.
		 */
		VERB_HELLO = 0x01,

		/**
		 * Error response:
		 *   <[1] in-re verb>
		 *   <[8] in-re packet ID>
		 *   <[1] error code>
		 *   <[...] error-dependent payload>
		 *
		 * If this is not in response to a single packet then verb can be
		 * NOP and packet ID can be zero.
		 */
		VERB_ERROR = 0x02,

		/**
		 * Success response:
		 *   <[1] in-re verb>
		 *   <[8] in-re packet ID>
		 *   <[...] request-specific payload>
		 */
		VERB_OK = 0x03,

		/**
		 * Query an identity by address:
		 *   <[5] address to look up>
		 *   [<[...] additional addresses to look up>
		 *
		 * OK response payload:
		 *   <[...] binary serialized identity>
		 *  [<[...] additional binary serialized identities>]
		 *
		 * If querying a cluster, duplicate OK responses may occasionally occur.
		 * These must be tolerated, which is easy since they'll have info you
		 * already have.
		 *
		 * If the address is not found, no response is generated. The semantics
		 * of WHOIS is similar to ARP and NDP in that persistent retrying can
		 * be performed.
		 */
		VERB_WHOIS = 0x04,

		/**
		 * Relay-mediated NAT traversal or firewall punching initiation:
		 *   <[1] flags (unused, currently 0)>
		 *   <[5] ZeroTier address of peer that might be found at this address>
		 *   <[2] 16-bit protocol address port>
		 *   <[1] protocol address length (4 for IPv4, 16 for IPv6)>
		 *   <[...] protocol address (network byte order)>
		 *
		 * An upstream node can send this to inform both sides of a relay of
		 * information they might use to establish a direct connection.
		 *
		 * Upon receipt a peer sends HELLO to establish a direct link.
		 *
		 * No OK or ERROR is generated.
		 */
		VERB_RENDEZVOUS = 0x05,

		/**
		 * ZT-to-ZT unicast ethernet frame (shortened EXT_FRAME):
		 *   <[8] 64-bit network ID>
		 *   <[2] 16-bit ethertype>
		 *   <[...] ethernet payload>
		 *
		 * MAC addresses are derived from the packet's source and destination
		 * ZeroTier addresses. This is a shortened EXT_FRAME that elides full
		 * Ethernet framing and other optional flags and features when they
		 * are not necessary.
		 *
		 * ERROR may be generated if a membership certificate is needed for a
		 * closed network. Payload will be network ID.
		 */
		VERB_FRAME = 0x06,

		/**
		 * Full Ethernet frame with MAC addressing and optional fields:
		 *   <[8] 64-bit network ID>
		 *   <[1] flags>
		 *   <[6] destination MAC or all zero for destination node>
		 *   <[6] source MAC or all zero for node of origin>
		 *   <[2] 16-bit ethertype>
		 *   <[...] ethernet payload>
		 *
		 * Flags:
		 *   0x01 - Certificate of network membership attached (DEPRECATED)
		 *   0x02 - Most significant bit of subtype (see below)
		 *   0x04 - Middle bit of subtype (see below)
		 *   0x08 - Least significant bit of subtype (see below)
		 *   0x10 - ACK requested in the form of OK(EXT_FRAME)
		 *
		 * Subtypes (0..7):
		 *   0x0 - Normal frame (bridging can be determined by checking MAC)
		 *   0x1 - TEEd outbound frame
		 *   0x2 - REDIRECTed outbound frame
		 *   0x3 - WATCHed outbound frame (TEE with ACK, ACK bit also set)
		 *   0x4 - TEEd inbound frame
		 *   0x5 - REDIRECTed inbound frame
		 *   0x6 - WATCHed inbound frame
		 *   0x7 - (reserved for future use)
		 *
		 * An extended frame carries full MAC addressing, making it a
		 * superset of VERB_FRAME. If 0x20 is set then p2p or hub and
		 * spoke multicast propagation is requested.
		 *
		 * OK payload (if ACK flag is set):
		 *   <[8] 64-bit network ID>
		 *   <[1] flags>
		 *   <[6] destination MAC or all zero for destination node>
		 *   <[6] source MAC or all zero for node of origin>
		 *   <[2] 16-bit ethertype>
		 */
		VERB_EXT_FRAME = 0x07,

		/**
		 * ECHO request (a.k.a. ping):
		 *   <[...] arbitrary payload>
		 *
		 * This generates OK with a copy of the transmitted payload. No ERROR
		 * is generated. Response to ECHO requests is optional and ECHO may be
		 * ignored if a node detects a possible flood.
		 */
		VERB_ECHO = 0x08,

		/**
		 * Announce interest in multicast group(s) (DEPRECATED):
		 *   <[8] 64-bit network ID>
		 *   <[6] multicast Ethernet address>
		 *   <[4] multicast additional distinguishing information (ADI)>
		 *   [... additional tuples of network/address/adi ...]
		 *
		 * OK/ERROR are not generated.
		 */
		VERB_MULTICAST_LIKE = 0x09,

		/**
		 * Network credentials push:
		 *   [<[...] one or more certificates of membership>]
		 *   <[1] 0x00, null byte marking end of COM array>
		 *   <[2] 16-bit number of capabilities>
		 *   <[...] one or more serialized Capability>
		 *   <[2] 16-bit number of tags>
		 *   <[...] one or more serialized Tags>
		 *   <[2] 16-bit number of revocations>
		 *   <[...] one or more serialized Revocations>
		 *   <[2] 16-bit number of certificates of ownership>
		 *   <[...] one or more serialized CertificateOfOwnership>
		 *
		 * This can be sent by anyone at any time to push network credentials.
		 * These will of course only be accepted if they are properly signed.
		 * Credentials can be for any number of networks.
		 *
		 * The use of a zero byte to terminate the COM section is for legacy
		 * backward compatibility. Newer fields are prefixed with a length.
		 *
		 * OK/ERROR are not generated.
		 */
		VERB_NETWORK_CREDENTIALS = 0x0a,

		/**
		 * Network configuration request:
		 *   <[8] 64-bit network ID>
		 *   <[2] 16-bit length of request meta-data dictionary>
		 *   <[...] string-serialized request meta-data>
		 *   <[8] 64-bit revision of netconf we currently have>
		 *   <[8] 64-bit timestamp of netconf we currently have>
		 *
		 * This message requests network configuration from a node capable of
		 * providing it.
		 *
		 * Responses to this are always whole configs intended for the recipient.
		 * For patches and other updates a NETWORK_CONFIG is sent instead.
		 *
		 * It would be valid and correct as of 1.2.0 to use NETWORK_CONFIG always,
		 * but OK(NETWORK_CONFIG_REQUEST) should be sent for compatibility.
		 *
		 * OK response payload:
		 *   <[8] 64-bit network ID>
		 *   <[2] 16-bit length of network configuration dictionary chunk>
		 *   <[...] network configuration dictionary (may be incomplete)>
		 *   [ ... end of legacy single chunk response ... ]
		 *   <[1] 8-bit flags>
		 *   <[8] 64-bit config update ID (should never be 0)>
		 *   <[4] 32-bit total length of assembled dictionary>
		 *   <[4] 32-bit index of chunk>
		 *   [ ... end signed portion ... ]
		 *   <[1] 8-bit chunk signature type>
		 *   <[2] 16-bit length of chunk signature>
		 *   <[...] chunk signature>
		 *
		 * The chunk signature signs the entire payload of the OK response.
		 * Currently only one signature type is supported: ed25519 (1).
		 *
		 * Each config chunk is signed to prevent memory exhaustion or
		 * traffic crowding DOS attacks against config fragment assembly.
		 *
		 * If the packet is from the network controller it is permitted to end
		 * before the config update ID or other chunking related or signature
		 * fields. This is to support older controllers that don't include
		 * these fields and may be removed in the future.
		 *
		 * ERROR response payload:
		 *   <[8] 64-bit network ID>
		 */
		VERB_NETWORK_CONFIG_REQUEST = 0x0b,

		/**
		 * Network configuration data push:
		 *   <[8] 64-bit network ID>
		 *   <[2] 16-bit length of network configuration dictionary chunk>
		 *   <[...] network configuration dictionary (may be incomplete)>
		 *   <[1] 8-bit flags>
		 *   <[8] 64-bit config update ID (should never be 0)>
		 *   <[4] 32-bit total length of assembled dictionary>
		 *   <[4] 32-bit index of chunk>
		 *   [ ... end signed portion ... ]
		 *   <[1] 8-bit chunk signature type>
		 *   <[2] 16-bit length of chunk signature>
		 *   <[...] chunk signature>
		 *
		 * This is a direct push variant for network config updates. It otherwise
		 * carries the same payload as OK(NETWORK_CONFIG_REQUEST) and has the same
		 * semantics.
		 *
		 * The legacy mode missing the additional chunking fields is not supported
		 * here.
		 *
		 * Flags:
		 *   0x01 - Use fast propagation
		 *
		 * An OK should be sent if the config is successfully received and
		 * accepted.
		 *
		 * OK payload:
		 *   <[8] 64-bit network ID>
		 *   <[8] 64-bit config update ID>
		 */
		VERB_NETWORK_CONFIG = 0x0c,

		/**
		 * Push of potential endpoints for direct communication:
		 *   <[2] 16-bit number of paths>
		 *   <[...] paths>
		 *
		 * Path record format:
		 *   <[1] 8-bit path flags (always 0, currently unused)>
		 *   <[2] length of extended path characteristics or 0 for none>
		 *   <[...] extended path characteristics>
		 *   <[1] address type>
		 *   <[1] address length in bytes>
		 *   <[...] address>
		 *
		 * The receiver may, upon receiving a push, attempt to establish a
		 * direct link to one or more of the indicated addresses. It is the
		 * responsibility of the sender to limit which peers it pushes direct
		 * paths to to those with whom it has a trust relationship. The receiver
		 * must obey any restrictions provided such as exclusivity or blacklists.
		 * OK responses to this message are optional.
		 *
		 * Note that a direct path push does not imply that learned paths can't
		 * be used unless they are blacklisted explicitly or unless flag 0x01
		 * is set.
		 *
		 * OK and ERROR are not generated.
		 */
		VERB_PUSH_DIRECT_PATHS = 0x10,

		/**
		 * An acknowledgment of receipt of a series of recent packets from another
		 * peer. This is used to calculate relative throughput values and to detect
		 * packet loss. Only VERB_FRAME and VERB_EXT_FRAME packets are counted.
		 *
		 * ACK response format:
		 *  <[4] 32-bit number of bytes received since last ACK>
		 *
		 * Upon receipt of this packet, the local peer will verify that the correct
		 * number of bytes were received by the remote peer. If these values do
		 * not agree that could be an indicator of packet loss.
		 *
		 * Additionally, the local peer knows the interval of time that has
		 * elapsed since the last received ACK. With this information it can compute
		 * a rough estimate of the current throughput.
		 *
		 * This is sent at a maximum rate of once per every ZT_PATH_ACK_INTERVAL
		 */
		VERB_ACK = 0x12,

		/**
		 * A packet containing timing measurements useful for estimating path quality.
		 * Composed of a list of <packet ID:internal sojourn time> pairs for an
		 * arbitrary set of recent packets. This is used to sample for latency and
		 * packet delay variance (PDV, "jitter").
		 *
		 * QoS record format:
		 *
		 *  <[8] 64-bit packet ID of previously-received packet>
		 *  <[1] 8-bit packet sojourn time>
		 *  <...repeat until end of max 1400 byte packet...>
		 *
		 * The number of possible records per QoS packet is: (1400 * 8) / 72 = 155
		 * This packet should be sent very rarely (every few seconds) as it can be
		 * somewhat large if the connection is saturated. Future versions might use
		 * a bloom table to probabilistically determine these values in a vastly
		 * more space-efficient manner.
		 *
		 * Note: The 'internal packet sojourn time' is a slight misnomer as it is a
		 * measure of the amount of time between when a packet was received and the
		 * egress time of its tracking QoS packet.
		 *
		 * This is sent at a maximum rate of once per every ZT_PATH_QOS_INTERVAL
		 */
		VERB_QOS_MEASUREMENT = 0x13,

		/**
		 * A message with arbitrary user-definable content:
		 *   <[8] 64-bit arbitrary message type ID>
		 *  [<[...] message payload>]
		 *
		 * This can be used to send arbitrary messages over VL1. It generates no
		 * OK or ERROR and has no special semantics outside of whatever the user
		 * (via the ZeroTier core API) chooses to give it.
		 *
		 * Message type IDs less than or equal to 65535 are reserved for use by
		 * ZeroTier, Inc. itself. We recommend making up random ones for your own
		 * implementations.
		 */
		VERB_USER_MESSAGE = 0x14,

		/**
		 * A trace for remote debugging or diagnostics:
		 *   <[...] null-terminated dictionary containing trace information>
		 *  [<[...] additional null-terminated dictionaries>]
		 *
		 * This message contains a remote trace event. Remote trace events can
		 * be sent to observers configured at the network level for those that
		 * pertain directly to activity on a network, or to global observers if
		 * locally configured.
		 */
		VERB_REMOTE_TRACE = 0x15,

		/**
		 * A signed locator for this node:
		 *   <[8] 64-bit flags>
		 *   <[2] 16-bit length of locator>
		 *   <[...] serialized locator>
		 *
		 * This message is sent in response to OK(HELLO) and can be pushed
		 * opportunitistically. Its payload is a signed Locator object that
		 * attests to where and how this Node may be reached. A locator can
		 * contain static IPs/ports or other ZeroTier nodes that can be used
		 * to reach this one.
		 *
		 * These Locator objects can be stored e.g. by roots in LF to publish
		 * node reachability. Since they're signed any node can verify that
		 * the originating node approves of their content.
		 */
		VERB_SET_LOCATOR = 0x16,

		/**
		 * A list of peers this node will relay traffic to/from:
		 *   <[2] 16-bit number of peers>
		 *   <[16] 128-bit hash of node public key>
		 *   <[2] 16-bit latency to node or 0 if unspecified>
		 *   <[4] 32-bit max bandwidth in megabits or 0 if unspecified>
		 *  [<[...] additional hash,latency,bandwidth tuples>]
		 *
		 * This messages can be pushed to indicate that this peer is willing
		 * to relay traffic to other peers. It contains a list of 128-bit
		 * hashes (the first 128 bits of a SHA512) of identity public keys
		 * of currently reachable and willing-to-relay-for nodes.
		 *
		 * This can be used to initiate mesh-like behavior in ZeroTier. The
		 * peers for which this node is willing to relay are reported as
		 * hashes of their identity public keys. This prevents this message
		 * from revealing explicit information about linked peers. The
		 * receiving peer can only "see" a will-relay entry if it knows the
		 * identity of the peer it is trying to reach.
		 */
  	VERB_WILL_RELAY = 0x17,

		/**
		 * Multipurpose VL2 network multicast:
		 *   <[5] start of range of addresses for propagation>
		 *   <[5] end of range of addresses for propagation>
		 *   <[1] 8-bit propagation depth / hops>
		 *   <[1] 8-bit length of bloom filter in 256-byte/2048-bit chunks>
		 *   <[...] propagation bloom filter>
		 *   [... start of signed portion ...]
		 *   <[8] 64-bit timestamp>
		 *   <[8] 64-bit network ID>
		 *   <[5] 40-bit address of sender>
		 *   <[2] 16-bit length of multicast payload>
		 *   [... start multicast payload ...]
		 *   <[1] 8-bit payload type>
		 *   [... end multicast payload and signed portion ...]
		 *   <[2] 16-bit length of signature or 0 if not present>
		 *   <[...] signature of signed portion>
		 *
		 * Payload type 0x00: multicast frame:
		 *   <[6] MAC address of multicast group>
		 *   <[4] 32-bit ADI of multicast group>
		 *   <[6] 48-bit source MAC of packet or all 0 if from sender>
		 *   <[2] 16-bit ethertype>
		 *   <[...] ethernet payload>
		 *
		 * Payload type 0x01: multicast subscribe:
		 *   <[2] 16-bit number of multicast group IDs to subscribe>
		 *   <[...] series of 32-bit multicast group IDs>
		 *
		 * Payload type 0x02: multicast unsubscribe:
		 *   <[2] 16-bit number of multicast group IDs to unsubscribe>
		 *   <[...] series of 32-bit multicast group IDs>
		 *
		 * This is the common packet structure for VL2 network-level multicasts
		 * and is used for multicast frames, multicast group subscribe and
		 * unsubscribe, and could be used in the future for other purposes such
		 * as credential propagation or diagnostics.
		 *
		 * The header contains an address range, bloom filter, and depth/hop
		 * counter. The bloom filter tracks which nodes have seen this multicast,
		 * with bits being set prior to send. The range allows the total set of
		 * subscribers to be partitioned in the case of huge networks that would
		 * saturate the bloom filter or have collisions. The propagation depth
		 * allows propagation to stop at some maximum value, and the value 0xff
		 * can be used to indicate that further propagation is not desired.
		 *
		 * Logic connected to the parsing of the multicast payload will determine
		 * whether or not and to whom this multicast is propagated. Subscribe and
		 * unsubscribe messages are propagated to online nodes up to a maximum
		 * depth, while frames have the added constraint of being propagated only
		 * to nodes that subscribe to the target multicast group.
		 */
		VERB_VL2_MULTICAST = 0x18,

		// protocol max: 0x1f
	};

	/**
	 * Error codes for VERB_ERROR
	 */
	enum ErrorCode
	{
		/* No error, not actually used in transit */
		ERROR_NONE = 0x00,

		/* Invalid request */
		ERROR_INVALID_REQUEST = 0x01,

		/* Bad/unsupported protocol version */
		ERROR_BAD_PROTOCOL_VERSION = 0x02,

		/* Unknown object queried */
		ERROR_OBJ_NOT_FOUND = 0x03,

		/* HELLO pushed an identity whose address is already claimed */
		ERROR_IDENTITY_COLLISION = 0x04,

		/* Verb or use case not supported/enabled by this node */
		ERROR_UNSUPPORTED_OPERATION = 0x05,

		/* Network membership certificate update needed */
		ERROR_NEED_MEMBERSHIP_CERTIFICATE = 0x06,

		/* Tried to join network, but you're not a member */
		ERROR_NETWORK_ACCESS_DENIED_ = 0x07, /* extra _ at end to avoid Windows name conflict */

		/* Cannot deliver a forwarded ZeroTier packet (e.g. hops exceeded, no routes) */
		/* Payload: <packet ID>, <destination>, <... additional packet ID / destinations> */
		ERROR_CANNOT_DELIVER = 0x09
	};

	template<unsigned int C2>
	ZT_ALWAYS_INLINE Packet(const Buffer<C2> &b) :
		Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(b)
	{
	}

	ZT_ALWAYS_INLINE Packet(const void *data,unsigned int len) :
		Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(data,len)
	{
	}

	/**
	 * Construct a new empty packet with a unique random packet ID
	 *
	 * Flags and hops will be zero. Other fields and data region are undefined.
	 * Use the header access methods (setDestination() and friends) to fill out
	 * the header. Payload should be appended; initial size is header size.
	 */
	ZT_ALWAYS_INLINE Packet() :
		Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(ZT_PROTO_MIN_PACKET_LENGTH)
	{
		setAt<uint64_t>(ZT_PACKET_IDX_IV,Packet::nextPacketId());
		(*this)[ZT_PACKET_IDX_FLAGS] = 0; // zero flags, cipher ID, and hops
	}

	/**
	 * Make a copy of a packet with a new initialization vector and destination address
	 *
	 * This can be used to take one draft prototype packet and quickly make copies to
	 * encrypt for different destinations.
	 *
	 * @param prototype Prototype packet
	 * @param dest Destination ZeroTier address for new packet
	 */
	ZT_ALWAYS_INLINE Packet(const Packet &prototype,const Address &dest) :
		Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(prototype)
	{
		setAt<uint64_t>(ZT_PACKET_IDX_IV,Packet::nextPacketId());
		setDestination(dest);
	}

	/**
	 * Construct a new empty packet with a unique random packet ID
	 *
	 * @param dest Destination ZT address
	 * @param source Source ZT address
	 * @param v Verb
	 */
	ZT_ALWAYS_INLINE Packet(const Address &dest,const Address &source,const Verb v) :
		Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(ZT_PROTO_MIN_PACKET_LENGTH)
	{
		setAt<uint64_t>(ZT_PACKET_IDX_IV,Packet::nextPacketId());
		setDestination(dest);
		setSource(source);
		(*this)[ZT_PACKET_IDX_FLAGS] = 0; // zero flags and hops
		setVerb(v);
	}

	/**
	 * Reset this packet structure for reuse in place
	 *
	 * @param dest Destination ZT address
	 * @param source Source ZT address
	 * @param v Verb
	 */
	ZT_ALWAYS_INLINE void reset(const Address &dest,const Address &source,const Verb v)
	{
		setSize(ZT_PROTO_MIN_PACKET_LENGTH);
		setAt<uint64_t>(ZT_PACKET_IDX_IV,Packet::nextPacketId());
		setDestination(dest);
		setSource(source);
		(*this)[ZT_PACKET_IDX_FLAGS] = 0; // zero flags, cipher ID, and hops
		setVerb(v);
	}

	/**
	 * Generate a new IV / packet ID in place
	 *
	 * This can be used to re-use a packet buffer multiple times to send
	 * technically different but otherwise identical copies of the same
	 * packet.
	 */
	ZT_ALWAYS_INLINE void newInitializationVector() { setAt<uint64_t>(ZT_PACKET_IDX_IV,Packet::nextPacketId()); }

	/**
	 * Set this packet's destination
	 *
	 * @param dest ZeroTier address of destination
	 */
	ZT_ALWAYS_INLINE void setDestination(const Address &dest) { dest.copyTo(field(ZT_PACKET_IDX_DEST,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); }

	/**
	 * Set this packet's source
	 *
	 * @param source ZeroTier address of source
	 */
	ZT_ALWAYS_INLINE void setSource(const Address &source) { source.copyTo(field(ZT_PACKET_IDX_SOURCE,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); }

	/**
	 * Get this packet's destination
	 *
	 * @return Destination ZT address
	 */
	ZT_ALWAYS_INLINE Address destination() const { return Address(field(ZT_PACKET_IDX_DEST,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); }

	/**
	 * Get this packet's source
	 *
	 * @return Source ZT address
	 */
	ZT_ALWAYS_INLINE Address source() const { return Address(field(ZT_PACKET_IDX_SOURCE,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); }

	/**
	 * @return True if packet is of valid length
	 */
	ZT_ALWAYS_INLINE bool lengthValid() const { return (size() >= ZT_PROTO_MIN_PACKET_LENGTH); }

	/**
	 * @return True if packet is fragmented (expect fragments)
	 */
	ZT_ALWAYS_INLINE bool fragmented() const { return (((unsigned char)(*this)[ZT_PACKET_IDX_FLAGS] & ZT_PROTO_FLAG_FRAGMENTED) != 0); }

	/**
	 * Set this packet's fragmented flag
	 *
	 * @param f Fragmented flag value
	 */
	ZT_ALWAYS_INLINE void setFragmented(bool f)
	{
		if (f)
			(*this)[ZT_PACKET_IDX_FLAGS] |= (char)ZT_PROTO_FLAG_FRAGMENTED;
		else (*this)[ZT_PACKET_IDX_FLAGS] &= (char)(~ZT_PROTO_FLAG_FRAGMENTED);
	}

	/**
	 * @return True if compressed (result only valid if unencrypted)
	 */
	ZT_ALWAYS_INLINE bool compressed() const { return (((unsigned char)(*this)[ZT_PACKET_IDX_VERB] & ZT_PROTO_VERB_FLAG_COMPRESSED) != 0); }

	/**
	 * @return ZeroTier forwarding hops (0 to 7)
	 */
	ZT_ALWAYS_INLINE unsigned int hops() const { return ((unsigned int)(*this)[ZT_PACKET_IDX_FLAGS] & 0x07); }

	/**
	 * Increment this packet's hop count
	 */
	ZT_ALWAYS_INLINE unsigned char incrementHops()
	{
		unsigned char &b = (*this)[ZT_PACKET_IDX_FLAGS];
		const unsigned char h = (b + 1) & 0x07;
		b = (b & 0xf8) | h;
		return (unsigned int)h;
	}

	/**
	 * @return Cipher suite selector: 0 - 7 (see #defines)
	 */
	ZT_ALWAYS_INLINE unsigned int cipher() const
	{
		return (((unsigned int)(*this)[ZT_PACKET_IDX_FLAGS] & 0x38) >> 3);
	}

	/**
	 * Set this packet's cipher suite
	 */
	ZT_ALWAYS_INLINE void setCipher(unsigned int c)
	{
		unsigned char &b = (*this)[ZT_PACKET_IDX_FLAGS];
		b = (b & 0xc7) | (unsigned char)((c << 3) & 0x38); // bits: FFCCCHHH
	}

	/**
	 * Get the trusted path ID for this packet (only meaningful if cipher is trusted path)
	 *
	 * @return Trusted path ID (from MAC field)
	 */
	ZT_ALWAYS_INLINE uint64_t trustedPathId() const { return at<uint64_t>(ZT_PACKET_IDX_MAC); }

	/**
	 * Set this packet's trusted path ID and set the cipher spec to trusted path
	 *
	 * @param tpid Trusted path ID
	 */
	ZT_ALWAYS_INLINE void setTrusted(const uint64_t tpid)
	{
		setCipher(ZT_PROTO_CIPHER_SUITE__NONE);
		setAt(ZT_PACKET_IDX_MAC,tpid);
	}

	/**
	 * Get this packet's unique ID (the IV field interpreted as uint64_t)
	 *
	 * Note that the least significant 3 bits of this ID will change when armor()
	 * is called to armor the packet for transport. This is because armor() will
	 * mask the last 3 bits against the send counter for QoS monitoring use prior
	 * to actually using the IV to encrypt and MAC the packet. Be aware of this
	 * when grabbing the packetId of a new packet prior to armor/send.
	 *
	 * @return Packet ID
	 */
	ZT_ALWAYS_INLINE uint64_t packetId() const { return at<uint64_t>(ZT_PACKET_IDX_IV); }

	/**
	 * Set packet verb
	 *
	 * This also has the side-effect of clearing any verb flags, such as
	 * compressed, and so must only be done during packet composition.
	 *
	 * @param v New packet verb
	 */
	ZT_ALWAYS_INLINE void setVerb(Verb v) { (*this)[ZT_PACKET_IDX_VERB] = (char)v; }

	/**
	 * @return Packet verb (not including flag bits)
	 */
	ZT_ALWAYS_INLINE Verb verb() const { return (Verb)((*this)[ZT_PACKET_IDX_VERB] & 0x1f); }

	/**
	 * @return Length of packet payload
	 */
	ZT_ALWAYS_INLINE unsigned int payloadLength() const { return ((size() < ZT_PROTO_MIN_PACKET_LENGTH) ? 0 : (size() - ZT_PROTO_MIN_PACKET_LENGTH)); }

	/**
	 * @return Raw packet payload
	 */
	ZT_ALWAYS_INLINE const unsigned char *payload() const { return field(ZT_PACKET_IDX_PAYLOAD,size() - ZT_PACKET_IDX_PAYLOAD); }

	/**
	 * Armor packet for transport
	 *
	 * @param key 32-byte key
	 * @param encryptPayload If true, encrypt packet payload, else just MAC
	 */
	void armor(const void *key,bool encryptPayload);

	/**
	 * Verify and (if encrypted) decrypt packet
	 *
	 * This does not handle trusted path mode packets and will return false
	 * for these. These are handled in IncomingPacket if the sending physical
	 * address and MAC field match a trusted path.
	 *
	 * @param key 32-byte key
	 * @return False if packet is invalid or failed MAC authenticity check
	 */
	bool dearmor(const void *key);

	/**
	 * Attempt to compress payload if not already (must be unencrypted)
	 *
	 * This requires that the payload at least contain the verb byte already
	 * set. The compressed flag in the verb is set if compression successfully
	 * results in a size reduction. If no size reduction occurs, compression
	 * is not done and the flag is left cleared.
	 *
	 * @return True if compression occurred
	 */
	bool compress();

	/**
	 * Attempt to decompress payload if it is compressed (must be unencrypted)
	 *
	 * If payload is compressed, it is decompressed and the compressed verb
	 * flag is cleared. Otherwise nothing is done and true is returned.
	 *
	 * @return True if data is now decompressed and valid, false on error
	 */
	bool uncompress();

private:
	static const unsigned char ZERO_KEY[32];

	static uint64_t nextPacketId();

	/**
	 * Deterministically mangle a 256-bit crypto key based on packet
	 *
	 * This uses extra data from the packet to mangle the secret, giving us an
	 * effective IV that is somewhat more than 64 bits. This is "free" for
	 * Salsa20 since it has negligible key setup time so using a different
	 * key each time is fine.
	 *
	 * @param in Input key (32 bytes)
	 * @param out Output buffer (32 bytes)
	 */
	ZT_ALWAYS_INLINE void _salsa20MangleKey(const unsigned char *in,unsigned char *out) const
	{
		const unsigned char *d = (const unsigned char *)data();

		// IV and source/destination addresses. Using the addresses divides the
		// key space into two halves-- A->B and B->A (since order will change).
		for(unsigned int i=0;i<18;++i) // 8 + (ZT_ADDRESS_LENGTH * 2) == 18
			out[i] = in[i] ^ d[i];

		// Flags, but with hop count masked off. Hop count is altered by forwarding
		// nodes. It's one of the only parts of a packet modifiable by people
		// without the key.
		out[18] = in[18] ^ (d[ZT_PACKET_IDX_FLAGS] & 0xf8);

		// Raw packet size in bytes -- thus each packet size defines a new
		// key space.
		out[19] = in[19] ^ (unsigned char)(size() & 0xff);
		out[20] = in[20] ^ (unsigned char)((size() >> 8) & 0xff); // little endian

		// Rest of raw key is used unchanged
		for(unsigned int i=21;i<32;++i)
			out[i] = in[i];
	}
};

} // namespace ZeroTier

#endif
