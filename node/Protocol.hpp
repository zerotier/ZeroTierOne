/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_PROTOCOL_HPP
#define ZT_PROTOCOL_HPP

#include "Constants.hpp"
#include "AES.hpp"
#include "Salsa20.hpp"
#include "Poly1305.hpp"
#include "LZ4.hpp"
#include "Buf.hpp"

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
 *    + inline push of CertificateOfMembership deprecated
 * 9  - 1.2.0 ... 1.2.14
 * 10 - 1.4.0 ... 1.4.6
 * 11 - 2.0.0 ... CURRENT
 *    + Peer-to-peer multicast replication
 *    + Old planet/moon stuff is DEAD!
 *    + AES encryption support
 *    + NIST P-384 (type 1) identities
 *    + Ephemeral keys
 */
#define ZT_PROTO_VERSION 11

/**
 * Minimum supported protocol version
 *
 * As of v2 we don't "officially" support anything older than 1.2.14, but this
 * is the hard cutoff before which peers will be flat out rejected.
 */
#define ZT_PROTO_VERSION_MIN 6

/**
 * Packet buffer size (can be changed)
 */
#define ZT_PROTO_MAX_PACKET_LENGTH (ZT_MAX_PACKET_FRAGMENTS * ZT_DEFAULT_PHYSMTU)

/**
 * Minimum viable packet length (outer header + verb)
 */
#define ZT_PROTO_MIN_PACKET_LENGTH 28

/**
 * Index at which the encrypted section of a packet begins
 */
#define ZT_PROTO_PACKET_ENCRYPTED_SECTION_START 27

/**
 * Index at which packet payload begins (after verb)
 */
#define ZT_PROTO_PACKET_PAYLOAD_START 28

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
 * AES-GCM-NRH (AES-GCM with nonce reuse hardening) w/AES-256
 */
#define ZT_PROTO_CIPHER_SUITE__AES_GCM_NRH 3

/**
 * Magic number indicating a fragment
 */
#define ZT_PACKET_FRAGMENT_INDICATOR 0xff

/**
 * Minimum viable length for a fragment
 */
#define ZT_PROTO_MIN_FRAGMENT_LENGTH 16

/**
 * Index at which packet fragment payload starts
 */
#define ZT_PROTO_PACKET_FRAGMENT_PAYLOAD_START_AT 16

/**
 * Header flag indicating that a packet is fragmented and more fragments should be expected
 */
#define ZT_PROTO_FLAG_FRAGMENTED 0x40

/**
 * Verb flag indicating payload is compressed with LZ4
 */
#define ZT_PROTO_VERB_FLAG_COMPRESSED 0x80

/**
 * HELLO exchange meta-data: signed locator for this node
 */
#define ZT_PROTO_HELLO_NODE_META_LOCATOR "l"

/**
 * HELLO exchange meta-data: ephemeral C25519 public key
 */
#define ZT_PROTO_HELLO_NODE_META_EPHEMERAL_KEY_C25519 "e0"

/**
 * HELLO exchange meta-data: ephemeral NIST P-384 public key
 */
#define ZT_PROTO_HELLO_NODE_META_EPHEMERAL_KEY_P384 "e1"

/**
 * HELLO exchange meta-data: address(es) of nodes to whom this node will relay
 */
#define ZT_PROTO_HELLO_NODE_META_WILL_RELAY_TO "wr"

/**
 * HELLO exchange meta-data: X coordinate of your node (sent in OK(HELLO))
 */
#define ZT_PROTO_HELLO_NODE_META_LOCATION_X "gX"

/**
 * HELLO exchange meta-data: Y coordinate of your node (sent in OK(HELLO))
 */
#define ZT_PROTO_HELLO_NODE_META_LOCATION_Y "gY"

/**
 * HELLO exchange meta-data: Z coordinate of your node (sent in OK(HELLO))
 */
#define ZT_PROTO_HELLO_NODE_META_LOCATION_Z "gZ"

/****************************************************************************/

/*
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
 *   <[8] packet ID of packet to which this fragment belongs>
 *   <[5] destination ZT address>
 *   <[1] 0xff here signals that this is a fragment>
 *   <[1] total fragments (most significant 4 bits), fragment no (LS 4 bits)>
 *   <[1] ZT hop count (least significant 3 bits; others are reserved)>
 *   <[...] fragment data>
 *
 * The protocol supports a maximum of 16 fragments. If a fragment is received
 * before its main packet header, it should be cached for a brief period of
 * time to see if its parent arrives. Loss of any fragment constitutes packet
 * loss; there is no retransmission mechanism. The receiver must wait for full
 * receipt to authenticate and decrypt; there is no per-fragment MAC. (But if
 * fragments are corrupt, the MAC will fail for the whole assembled packet.)
 */

namespace ZeroTier {
namespace Protocol {

/**
 * Packet verb (message type)
 */
enum Verb
{
	VERB_NOP = 0x00,

	/**
	 * Announcement of a node's existence and vitals:
	 *   <[1] protocol version>
	 *   <[1] software major version>
	 *   <[1] software minor version>
	 *   <[2] software revision>
	 *   <[8] timestamp for determining latency>
	 *   <[...] binary serialized identity>
	 *   <[...] physical destination address of packet>
	 *   [... begin encrypted region ...]
	 *   <[2] 16-bit reserved (legacy) field, always 0>
	 *   <[2] 16-bit length of meta-data dictionary>
	 *   <[...] meta-data dictionary>
	 *   [... end encrypted region ...]
	 *   <[48] HMAC-SHA384 of all fields to this point (as plaintext)>
	 *
	 * HELLO is sent with authentication but without the usual encryption so
	 * that peers can exchange identities.
	 *
	 * Destination address is the actual wire address to which the packet
	 * was sent. See InetAddress::serialize() for format.
	 *
	 * Starting at "begin encrypted section" the reset of the packet is
	 * encrypted with Salsa20/12. This is not the normal packet encryption
	 * and is technically not necessary as nothing in HELLO is secret. It
	 * exists merely to shield meta-data info from passive listeners to
	 * slightly improve privacy, and for backward compatibility with older
	 * nodes that required it.
	 *
	 * HELLO (and its OK response) ends with a large 384-bit HMAC to allow
	 * identity exchanges to be authenticated with additional strength beyond
	 * ordinary packet authentication.
	 *
	 * OK payload:
	 *   <[8] HELLO timestamp field echo>
	 *   <[1] protocol version>
	 *   <[1] software major version>
	 *   <[1] software minor version>
	 *   <[2] software revision>
	 *   <[...] physical destination address of packet>
	 *   <[2] 16-bit reserved (legacy) field, always 0>
	 *   <[2] 16-bit length of meta-data dictionary>
	 *   <[...] meta-data dictionary>
	 *   <[48] HMAC-SHA384 of all fields to this point (as plaintext)>
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
	 *   <[...] identity>
	 *   <[...] locator>
	 *   [... additional identity/locator pairs]
	 *
	 * If the address is not found, no response is generated. The semantics
	 * of WHOIS is similar to ARP and NDP in that persistent retrying can
	 * be performed.
	 *
	 * It is possible for an identity but a null/empty locator to be returned
	 * if no locator is known for a node. Older versions will also send no
	 * locator field at all.
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
	 * Announce interest in multicast group(s):
	 *   <[8] 64-bit network ID>
	 *   <[6] multicast Ethernet address>
	 *   <[4] multicast additional distinguishing information (ADI)>
	 *   [... additional tuples of network/address/adi ...]
	 *
	 * LIKEs may be sent to any peer, though a good implementation should
	 * restrict them to peers on the same network they're for and to network
	 * controllers and root servers. In the current network, root servers
	 * will provide the service of final multicast cache.
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
	 * providing it. Responses can be sent as OK(NETWORK_CONFIG_REQUEST)
	 * or NETWORK_CONFIG messages. NETWORK_CONFIG can also be sent by
	 * network controllers or other nodes unsolicited.
	 *
	 * OK response payload:
	 *   (same as VERB_NETWORK_CONFIG payload)
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
	 *   <[1] 8-bit reserved field (legacy)>
	 *   <[2] 16-bit length of chunk signature>
	 *   <[...] chunk signature>
	 *
	 * Network configurations can come from network controllers or theoretically
	 * any other node, but each chunk must be signed by the network controller
	 * that generated it originally. The config update ID is arbitrary and is merely
	 * used by the receiver to group chunks. Chunk indexes must be sequential and
	 * the total delivered chunks must yield a total network config equal to the
	 * specified total length.
	 *
	 * Flags:
	 *   0x01 - Use fast propagation -- rumor mill flood this chunk to other members
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
	 * Request endpoints for multicast distribution:
	 *   <[8] 64-bit network ID>
	 *   <[1] flags>
	 *   <[6] MAC address of multicast group being queried>
	 *   <[4] 32-bit ADI for multicast group being queried>
	 *   <[4] 32-bit requested max number of multicast peers>
	 *
	 * This message asks a peer for additional known endpoints that have
	 * LIKEd a given multicast group. It's sent when the sender wishes
	 * to send multicast but does not have the desired number of recipient
	 * peers.
	 *
	 * OK response payload: (multiple OKs can be generated)
	 *   <[8] 64-bit network ID>
	 *   <[6] MAC address of multicast group being queried>
	 *   <[4] 32-bit ADI for multicast group being queried>
	 *   <[4] 32-bit total number of known members in this multicast group>
	 *   <[2] 16-bit number of members enumerated in this packet>
	 *   <[...] series of 5-byte ZeroTier addresses of enumerated members>
	 *
	 * ERROR is not generated; queries that return no response are dropped.
	 */
	VERB_MULTICAST_GATHER = 0x0d,

	/** *** DEPRECATED ***
	 * Multicast frame:
	 *   <[8] 64-bit network ID>
	 *   <[1] flags>
	 *  [<[4] 32-bit implicit gather limit>]
	 *  [<[6] source MAC>]
	 *   <[6] destination MAC (multicast address)>
	 *   <[4] 32-bit multicast ADI (multicast address extension)>
	 *   <[2] 16-bit ethertype>
	 *   <[...] ethernet payload>
	 *
	 * Flags:
	 *   0x01 - Network certificate of membership attached (DEPRECATED)
	 *   0x02 - Implicit gather limit field is present
	 *   0x04 - Source MAC is specified -- otherwise it's computed from sender
	 *   0x08 - Please replicate (sent to multicast replicators)
	 *
	 * OK and ERROR responses are optional. OK may be generated if there are
	 * implicit gather results or if the recipient wants to send its own
	 * updated certificate of network membership to the sender. ERROR may be
	 * generated if a certificate is needed or if multicasts to this group
	 * are no longer wanted (multicast unsubscribe).
	 *
	 * OK response payload:
	 *   <[8] 64-bit network ID>
	 *   <[6] MAC address of multicast group>
	 *   <[4] 32-bit ADI for multicast group>
	 *   <[1] flags>
	 *  [<[...] network certificate of membership (DEPRECATED)>]
	 *  [<[...] implicit gather results if flag 0x01 is set>]
	 *
	 * OK flags (same bits as request flags):
	 *   0x01 - OK includes certificate of network membership (DEPRECATED)
	 *   0x02 - OK includes implicit gather results
	 *
	 * ERROR response payload:
	 *   <[8] 64-bit network ID>
	 *   <[6] multicast group MAC>
	 *   <[4] 32-bit multicast group ADI>
	 */
	VERB_MULTICAST_FRAME_deprecated = 0x0e,

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
	 * Encapsulate a ZeroTier packet for multicast distribution:
	 *   [... begin signed portion ...]
	 *   <[1] 8-bit flags>
	 *   <[5] 40-bit ZeroTier address of sender>
	 *   <[2] 16-bit length of inner payload>
	 *   <[1] inner payload verb>
	 *   <[...] inner payload data>
	 *   [... end signed portion ...]
	 *   <[2] 16-bit length of signature or 0 if un-signed>
	 *  [<[...] optional signature of multicast>]
	 *   <[...] address (min prefix) list>
	 */
	VERB_MULTICAST = 0x16,

	/**
	 * Encapsulate a full ZeroTier packet in another:
	 *   <[...] raw encapsulated packet>
	 *
	 * Encapsulation exists to enable secure relaying as opposed to the usual
	 * "dumb" relaying. The latter is faster but secure relaying has roles
	 * where endpoint privacy is desired. Multiply nested ENCAP packets
	 * could allow ZeroTier to act as an onion router.
	 *
	 * When encapsulated packets are forwarded they do have their hop count
	 * field incremented.
	 */
	VERB_ENCAP = 0x17

	// protocol max: 0x1f
};

/**
 * Error codes used in ERROR packets.
 */
enum ErrorCode
{
	/* Invalid request */
	ERROR_INVALID_REQUEST = 0x01,

	/* Bad/unsupported protocol version */
	ERROR_BAD_PROTOCOL_VERSION = 0x02,

	/* Unknown object queried */
	ERROR_OBJ_NOT_FOUND = 0x03,

	/* Verb or use case not supported/enabled by this node */
	ERROR_UNSUPPORTED_OPERATION = 0x05,

	/* Network access denied; updated credentials needed */
	ERROR_NEED_MEMBERSHIP_CERTIFICATE = 0x06,

	/* Tried to join network, but you're not a member */
	ERROR_NETWORK_ACCESS_DENIED_ = 0x07, /* extra _ at end to avoid Windows name conflict */

	/* Cannot deliver a forwarded ZeroTier packet (for any reason) */
	ERROR_CANNOT_DELIVER = 0x09
};

/**
 * EXT_FRAME subtypes, which are packed into three bits in the flags field.
 *
 * This allows the node to know whether this is a normal frame or one generated
 * by a special tee or redirect type flow rule.
 */
enum ExtFrameSubtype
{
	EXT_FRAME_SUBTYPE_NORMAL = 0x0,
	EXT_FRAME_SUBTYPE_TEE_OUTBOUND = 0x1,
	EXT_FRAME_SUBTYPE_REDIRECT_OUTBOUND = 0x2,
	EXT_FRAME_SUBTYPE_WATCH_OUTBOUND = 0x3,
	EXT_FRAME_SUBTYPE_TEE_INBOUND = 0x4,
	EXT_FRAME_SUBTYPE_REDIRECT_INBOUND = 0x5,
	EXT_FRAME_SUBTYPE_WATCH_INBOUND = 0x6
};

/**
 * EXT_FRAME flags
 */
enum ExtFrameFlag
{
	/**
	 * A certifiate of membership was included (no longer used but still accepted)
	 */
	EXT_FRAME_FLAG_COM_ATTACHED_deprecated = 0x01,

	// bits 0x02, 0x04, and 0x08 are occupied by the 3-bit ExtFrameSubtype value.

	/**
	 * An OK(EXT_FRAME) acknowledgement was requested by the sender.
	 */
	EXT_FRAME_FLAG_ACK_REQUESTED = 0x10
};

/**
 * NETWORK_CONFIG (or OK(NETWORK_CONFIG_REQUEST)) flags
 */
enum NetworkConfigFlag
{
	/**
	 * Indicates that this network config chunk should be fast propagated via rumor mill flooding.
	 */
	NETWORK_CONFIG_FLAG_FAST_PROPAGATE = 0x01
};

/****************************************************************************/

/*
 * These are bit-packed structures for rapid parsing of packets or at least
 * the fixed size headers thereof. Not all packet types have these as some
 * are full of variable length fields are are more easily parsed through
 * incremental decoding.
 *
 * All fields larger than one byte are in big-endian byte order on the wire.
 */

/**
 * Normal packet header
 *
 * @tparam PT Packet payload type (default: uint8_t[])
 */
ZT_PACKED_STRUCT(struct Header
{
	uint64_t packetId;
	uint8_t destination[5];
	uint8_t source[5];
	uint8_t flags;
	uint64_t mac;
	// --- begin encrypted envelope ---
	uint8_t verb;
});

/**
 * Packet fragment header
 */
ZT_PACKED_STRUCT(struct FragmentHeader
{
	uint64_t packetId;
	uint8_t destination[5];
	uint8_t fragmentIndicator; // always 0xff for fragments
	uint8_t counts; // total: most significant four bits, number: least significant four bits
	uint8_t hops; // top 5 bits unused and must be zero
	uint8_t p[];
});

ZT_PACKED_STRUCT(struct HELLO
{
	Header h;
	uint8_t versionProtocol;
	uint8_t versionMajor;
	uint8_t versionMinor;
	uint16_t versionRev;
	uint64_t timestamp;
	uint8_t p[];
});

ZT_PACKED_STRUCT(struct RENDEZVOUS
{
	Header h;
	uint8_t flags;
	uint8_t peerAddress[5];
	uint16_t port;
	uint8_t addressLength;
	uint8_t address[];
});

ZT_PACKED_STRUCT(struct FRAME
{
	Header h;
	uint64_t networkId;
	uint16_t etherType;
	uint8_t data[];
});

ZT_PACKED_STRUCT(struct EXT_FRAME
{
	Header h;
	uint64_t networkId;
	uint8_t flags;
	uint8_t p[];
});

ZT_PACKED_STRUCT(struct MULTICAST_LIKE
{
	ZT_PACKED_STRUCT(struct Entry
	{
		uint64_t networkId;
		uint8_t mac[6];
		uint32_t adi;
	});

	Header h;
	Entry groups[];
});

namespace OK {

/**
 * OK response header
 *
 * @tparam PT OK payload type (default: uint8_t[])
 */
ZT_PACKED_STRUCT(struct Header
{
	uint8_t inReVerb;
	uint64_t inRePacketId;
});

ZT_PACKED_STRUCT(struct WHOIS
{
	Protocol::Header h;
	OK::Header oh;
});

ZT_PACKED_STRUCT(struct ECHO
{
	Protocol::Header h;
	OK::Header oh;
});

ZT_PACKED_STRUCT(struct HELLO
{
	Protocol::Header h;
	OK::Header oh;
	uint64_t timestampEcho;
	uint8_t versionProtocol;
	uint8_t versionMajor;
	uint8_t versionMinor;
	uint16_t versionRev;
});

ZT_PACKED_STRUCT(struct EXT_FRAME
{
	Protocol::Header h;
	OK::Header oh;
	uint64_t networkId;
	uint8_t flags;
	uint8_t destMac[6];
	uint8_t sourceMac[6];
	uint16_t etherType;
});

ZT_PACKED_STRUCT(struct NETWORK_CONFIG
{
	Protocol::Header h;
	OK::Header oh;
	uint64_t networkId;
	uint64_t configUpdateId;
});

} // namespace OK

namespace ERROR {

/**
 * Error header
 *
 * The error header comes after the packet header but before type-specific payloads.
 *
 * @tparam PT Error payload type (default: uint8_t[])
 */
ZT_PACKED_STRUCT(struct Header
{
	int8_t inReVerb;
	uint64_t inRePacketId;
	uint8_t error;
});

ZT_PACKED_STRUCT(struct NEED_MEMBERSHIP_CERTIFICATE
{
	Protocol::Header h;
	ERROR::Header eh;
	uint64_t networkId;
});

ZT_PACKED_STRUCT(struct UNSUPPORTED_OPERATION__NETWORK_CONFIG_REQUEST
{
	Protocol::Header h;
	ERROR::Header eh;
	uint64_t networkId;
});

} // namespace ERROR

/****************************************************************************/

/**
 * Increment the 3-bit hops field embedded in the packet flags field
 */
ZT_ALWAYS_INLINE unsigned int incrementPacketHops(Header &h)
{
	uint8_t flags = h.flags;
	uint8_t hops = flags;
	flags &= 0xf8U;
	++hops;
	h.flags = flags | (hops & 0x07U);
	return (unsigned int)hops;
}

/**
 * @return 3-bit hops field embedded in packet flags field
 */
ZT_ALWAYS_INLINE uint8_t packetHops(const Header &h) { return (h.flags & 0x07U); }

/**
 * @return 3-bit cipher field embedded in packet flags field
 */
ZT_ALWAYS_INLINE uint8_t packetCipher(const Header &h) { return ((h.flags >> 3U) & 0x07U); }

void _armor(Buf< Header > &packet,unsigned int packetSize,const uint8_t key[ZT_PEER_SECRET_KEY_LENGTH],uint8_t cipherSuite);
int _dearmor(Buf< Header > &packet,unsigned int packetSize,const uint8_t key[ZT_PEER_SECRET_KEY_LENGTH]);
unsigned int _compress(Buf< Header > &packet,unsigned int packetSize);
int _uncompress(Buf< Header > &packet,unsigned int packetSize);

/**
 * Armor a packet for transport
 *
 * @param packet Packet to armor
 * @param packetSize Size of data in packet (must be at least the minimum packet size)
 * @param key 256-bit symmetric key
 * @param cipherSuite Cipher suite to apply
 */
template<typename X>
static ZT_ALWAYS_INLINE void armor(Buf< X > &packet,unsigned int packetSize,const uint8_t key[ZT_PEER_SECRET_KEY_LENGTH],uint8_t cipherSuite)
{ _armor(reinterpret_cast< Buf< Header > & >(packet),packetSize,key,cipherSuite); }

/**
 * Dearmor a packet and check message authentication code
 *
 * If the packet is valid and MAC (if indicated) passes, the cipher suite
 * is returned. Otherwise -1 is returned to indicate a MAC failure.
 *
 * @param packet Packet to dearmor
 * @param packetSize Size of data in packet (must be at least the minimum packet size)
 * @param key 256-bit symmetric key
 * @return Cipher suite or -1 if MAC validation failed
 */
template<typename X>
static ZT_ALWAYS_INLINE int dearmor(Buf< X > &packet,unsigned int packetSize,const uint8_t key[ZT_PEER_SECRET_KEY_LENGTH])
{ return _dearmor(reinterpret_cast< Buf < Header > & >(packet),packetSize,key); }

/**
 * Compress packet payload
 *
 * @param packet Packet to compress
 * @param packetSize Original packet size
 * @return New packet size (returns original size of compression didn't help, in which case packet is unmodified)
 */
template<typename X>
static ZT_ALWAYS_INLINE unsigned int compress(Buf< X > &packet,unsigned int packetSize)
{ return _compress(reinterpret_cast< Buf< Header > & >(packet),packetSize); }

/**
 * Uncompress packet payload (if compressed)
 *
 * @param packet Packet to uncompress
 * @param packetSize Original packet size
 * @return New packet size or -1 on decompression error (returns original packet size if packet wasn't compressed)
 */
template<typename X>
static ZT_ALWAYS_INLINE int uncompress(Buf< X > &packet,unsigned int packetSize)
{ return _uncompress(reinterpret_cast< Buf< Header > & >(packet),packetSize); }

/**
 * Get a sequential non-repeating packet ID for the next packet (thread-safe)
 *
 * @return Next packet ID / cryptographic nonce
 */
uint64_t getPacketId();

} // namespace Protocol
} // namespace ZeroTier

#endif
