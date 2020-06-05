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
#include "Address.hpp"
#include "Identity.hpp"
#include "SymmetricKey.hpp"

/*
 * Packet format:
 *   <[8] 64-bit packet ID / crypto IV>
 *   <[5] destination ZT address>
 *   <[5] source ZT address>
 *   <[1] outer visible flags, cipher, and hop count (bits: FFCCHHH)>
 *   <[8] 64-bit MAC (or trusted path ID in trusted path mode)>
 *   [... -- begin encryption envelope -- ...]
 *   <[1] inner envelope flags (MS 3 bits) and verb (LS 5 bits)>
 *   [... verb-specific payload ...]
 *
 * Packets smaller than 28 bytes are invalid and silently discarded.
 *
 * The hop count field is masked during message authentication computation
 * and is thus the only field that is mutable in transit. It's incremented
 * when roots or other nodes forward packets and exists to prevent infinite
 * forwarding loops and to detect direct paths.
 *
 * HELLO is normally sent in the clear with the POLY1305_NONE cipher suite
 * and with Poly1305 computed on plain text (Salsa20/12 is still used to
 * generate a one time use Poly1305 key). As of protocol version 11 HELLO
 * also includes a terminating HMAC (last 48 bytes) that significantly
 * hardens HELLO authentication beyond what a 64-bit MAC can guarantee.
 *
 * Fragmented packets begin with a packet header whose fragment bit (bit
 * 0x40 in the flags field) is set. This constitutes fragment zero. The
 * total number of expected fragments is contained in each subsequent
 * fragment packet. Unfragmented packets must not have the fragment bit
 * set or the receiver will expect at least one additional fragment.
 *
 * --
 *
 * Packet fragment format (fragments beyond 0):
 *   <[8] packet ID of packet to which this fragment belongs>
 *   <[5] destination ZT address>
 *   <[1] 0xff here signals that this is a fragment>
 *   <[1] total fragments (most significant 4 bits), fragment no (LS 4 bits)>
 *   <[1] ZT hop count (least significant 3 bits; others are reserved)>
 *   <[...] fragment data>
 *
 * The protocol supports a maximum of 16 fragments including fragment 0
 * which contains the full packet header (with fragment bit set). Fragments
 * thus always carry fragment numbers between 1 and 15. All fragments
 * belonging to the same packet must carry the same total fragment count in
 * the most significant 4 bits of the fragment numbering field.
 *
 * All fragments have the same packet ID and destination. The packet ID
 * doubles as the grouping identifier for fragment reassembly.
 *
 * Fragments do not carry their own packet MAC. The entire packet is
 * authenticated once it is assembled by the receiver. Incomplete packets
 * are discarded after a receiver configured period of time.
 */

/*
 * Protocol versions
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
 *    + Contained early pre-alpha versions of multipath, which are deprecated
 * 11 - 2.0.0 ... CURRENT
 *    + New more WAN-efficient P2P-assisted multicast algorithm
 *    + HELLO and OK(HELLO) include an extra HMAC to harden authentication
 *    + HELLO and OK(HELLO) carry meta-data in a dictionary that's encrypted
 *    + Forward secrecy, key lifetime management
 *    + Old planet/moon stuff is DEAD! Independent roots are easier.
 *    + AES encryption with the SIV construction AES-GMAC-SIV
 *    + New combined Curve25519/NIST P-384 identity type (type 1)
 *    + Short probe packets to reduce probe bandwidth
 *    + More aggressive NAT traversal techniques for IPv4 symmetric NATs
 */
#define ZT_PROTO_VERSION 11

/**
 * Minimum supported protocol version
 */
#define ZT_PROTO_VERSION_MIN 9

/**
 * Maximum allowed packet size (can technically be increased up to 16384)
 */
#define ZT_PROTO_MAX_PACKET_LENGTH (ZT_MAX_PACKET_FRAGMENTS * ZT_MIN_UDP_MTU)

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
 * of the hop counter -- three bits. A lower limit is specified as
 * the actual maximum hop count.
 */
#define ZT_PROTO_MAX_HOPS 7

/**
 * NONE/Poly1305 (used for HELLO for backward compatibility)
 */
#define ZT_PROTO_CIPHER_SUITE__POLY1305_NONE 0

/**
 * Salsa2012/Poly1305 (legacy)
 */
#define ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012 1

/**
 * Deprecated, not currently used.
 */
#define ZT_PROTO_CIPHER_SUITE__NONE 2

/**
 * AES-GMAC-SIV
 */
#define ZT_PROTO_CIPHER_SUITE__AES_GMAC_SIV 3

/**
 * Minimum viable length for a fragment
 */
#define ZT_PROTO_MIN_FRAGMENT_LENGTH 16

/**
 * Magic number indicating a fragment if present at index 13
 */
#define ZT_PROTO_PACKET_FRAGMENT_INDICATOR 0xff

/**
 * Length of a probe packet
 */
#define ZT_PROTO_PROBE_LENGTH 4

/**
 * Index at which packet fragment payload starts
 */
#define ZT_PROTO_PACKET_FRAGMENT_PAYLOAD_START_AT ZT_PROTO_MIN_FRAGMENT_LENGTH

/**
 * Header flag indicating that a packet is fragmented and more fragments should be expected
 */
#define ZT_PROTO_FLAG_FRAGMENTED 0x40U

/**
 * Mask for obtaining hops from the combined flags, cipher, and hops field
 */
#define ZT_PROTO_FLAG_FIELD_HOPS_MASK 0x07U

/**
 * Verb flag indicating payload is compressed with LZ4
 */
#define ZT_PROTO_VERB_FLAG_COMPRESSED 0x80U

/**
 * Mask to extract just the verb from the verb / verb flags field
 */
#define ZT_PROTO_VERB_MASK 0x1fU

/**
 * AES-GMAC-SIV first of two keys
 */
#define ZT_KBKDF_LABEL_AES_GMAC_SIV_K0 '0'

/**
 * AES-GMAC-SIV second of two keys
 */
#define ZT_KBKDF_LABEL_AES_GMAC_SIV_K1 '1'

/**
 * Key used to encrypt dictionary in HELLO with AES-CTR.
 */
#define ZT_KBKDF_LABEL_HELLO_DICTIONARY_ENCRYPT 'H'

/**
 * Key used for extra HMAC-SHA384 authentication on some packets.
 */
#define ZT_KBKDF_LABEL_PACKET_HMAC 'M'

#define ZT_PROTO_PACKET_FRAGMENT_INDICATOR_INDEX 13
#define ZT_PROTO_PACKET_FRAGMENT_COUNTS          14

#define ZT_PROTO_PACKET_ID_INDEX          0
#define ZT_PROTO_PACKET_DESTINATION_INDEX 8
#define ZT_PROTO_PACKET_SOURCE_INDEX      13
#define ZT_PROTO_PACKET_FLAGS_INDEX       18
#define ZT_PROTO_PACKET_MAC_INDEX         19
#define ZT_PROTO_PACKET_VERB_INDEX        27

#define ZT_PROTO_HELLO_NODE_META_INSTANCE_ID                "i"
#define ZT_PROTO_HELLO_NODE_META_LOCATOR                    "l"
#define ZT_PROTO_HELLO_NODE_META_SOFTWARE_VENDOR            "s"
#define ZT_PROTO_HELLO_NODE_META_COMPLIANCE                 "c"
#define ZT_PROTO_HELLO_NODE_META_EPHEMERAL_PUBLIC           "e"
#define ZT_PROTO_HELLO_NODE_META_EPHEMERAL_ACK              "E"

static_assert(ZT_PROTO_MAX_PACKET_LENGTH < ZT_BUF_MEM_SIZE,"maximum packet length won't fit in Buf");
static_assert(ZT_PROTO_PACKET_ENCRYPTED_SECTION_START == (ZT_PROTO_MIN_PACKET_LENGTH-1),"encrypted packet section must start right before protocol verb at one less than minimum packet size");

namespace ZeroTier {
namespace Protocol {

/**
 * Packet verb (message type)
 */
enum Verb
{
	/**
	 * No operation
	 *
	 * This packet does nothing, but it is sometimes sent as a probe to
	 * trigger a HELLO exchange as the code will attempt HELLO when it
	 * receives a packet from an unidentified source.
	 */
	VERB_NOP = 0x00,

	/**
	 * Announcement of a node's existence and vitals:
	 *   <[1] protocol version>
	 *   <[1] software major version (optional, 0 if unspecified)>
	 *   <[1] software minor version (optional, 0 if unspecified)>
	 *   <[2] software revision (optional, 0 if unspecified)>
	 *   <[8] timestamp>
	 *   <[...] binary serialized full sender identity>
	 *   <[...] physical destination of packet>
	 *   <[12] 96-bit CTR IV>
	 *   <[6] reserved bytes, currently used for legacy compatibility>
	 *   [... start of encrypted section ...]
	 *   <[2] 16-bit length of encrypted dictionary>
	 *   <[...] encrypted dictionary>
	 *   [... end of encrypted section ...]
	 *   <[48] HMAC-SHA384 of packet>
	 *
	 * HELLO is sent to initiate a new pairing between two nodes and
	 * periodically to refresh information.
	 *
	 * HELLO is the only packet ever sent without whole payload encryption,
	 * though an inner encrypted envelope exists to obscure all fields that
	 * do not need to be sent in the clear. There is nothing in this
	 * encrypted section that would be catastrophic if it leaked, but it's
	 * good to proactively limit exposed information.
	 *
	 * Inner encryption is AES-CTR with a key derived using KBKDF and a
	 * label indicating this specific usage. A 96-bit CTR IV precedes this
	 * encrypted section.
	 *
	 * Authentication and encryption in HELLO and OK(HELLO) are always done
	 * with the long-lived identity key, not ephemeral shared keys. This
	 * is so ephemeral key negotiation can always occur on the first try
	 * even if things get out of sync e.g. by one side restarting. Nothing
	 * in HELLO is likely to be dangerous if decrypted later.
	 *
	 * HELLO and OK(HELLO) include an extra HMAC at the end of the packet.
	 * This authenticates them to a level of certainty beyond that afforded
	 * by regular AEAD. HMAC is computed over the whole packet prior to
	 * packet MAC and with the 3-bit hop count field masked as it is
	 * with regular packet AEAD, and it is then included in the regular
	 * packet MAC.
	 *
	 * LEGACY: for legacy reasons the MAC field of HELLO is a poly1305
	 * MAC initialized in the same manner as 1.x. Since HMAC provides
	 * additional full 384-bit strength authentication this should not be
	 * a problem for FIPS.
	 *
	 * Several legacy fields are present as well for the benefit of 1.x nodes.
	 * These will go away and become simple reserved space once 1.x is no longer
	 * supported. Some are self-explanatory. The "encrypted zero" is rather
	 * strange. It's a 16-bit zero value encrypted using Salsa20/12 and the
	 * long-lived identity key shared by the two peers. It tells 1.x that an
	 * old encrypted field is no longer there and that it should stop parsing
	 * the packet at that point.
	 *
	 * 1.x does not understand the dictionary and HMAC fields, but it will
	 * ignore them due to the "encrypted zero" field indicating that the
	 * packet contains no more information.
	 *
	 * Dictionary fields (defines start with ZT_PROTO_HELLO_NODE_META_):
	 * 
	 *   INSTANCE_ID - a 64-bit unique value generated on each node start
	 *   PREFERRED_CIPHER_MODE - preferred symmetric encryption mode
	 *   LOCATOR - signed record enumerating this node's trusted contact points
	 *   EPHEMERAL_PUBLIC - Ephemeral public key(s)
	 * 
	 * OK will contain EPHEMERAL_PUBLIC (of the sender) and:
	 * 
	 *   EPHEMERAL_ACK - SHA384 of EPHEMERAL_PUBLIC received
	 *
	 * The following optional fields may also be present:
	 *
	 *   HOSTNAME - arbitrary short host name for this node
	 *   CONTACT - arbitrary short contact information string for this node
	 *   SOFTWARE_VENDOR - short name or description of vendor, such as a URL
	 *   COMPLIANCE - bit mask containing bits for e.g. a FIPS-compliant node
	 *
	 * The timestamp field in OK is echoed but the others represent the sender
	 * of the OK and are not echoes from HELLO. The dictionary in OK typically
	 * only contains the EPHEMERAL fields, allowing the receiver of the OK to
	 * confirm that both sides know the correct keys and thus begin using the
	 * ephemeral shared secret to send packets.
	 * 
	 * OK is sent encrypted with the usual AEAD, but still includes a full HMAC
	 * as well (inside the cryptographic envelope).
	 *
	 * OK payload:
	 *   <[8] timestamp echoed from original HELLO>
	 *   <[1] protocol version of responding node>
	 *   <[1] software major version (optional)>
	 *   <[1] software minor version (optional)>
	 *   <[2] software revision (optional)>
	 *   <[...] physical destination address of packet>
	 *   <[2] 16-bit reserved field (zero for legacy compatibility)>
	 *   <[2] 16-bit length of dictionary>
	 *   <[...] dictionary>
	 *   <[48] HMAC-SHA384 of plaintext packet>
	 */
	VERB_HELLO = 0x01,

	/**
	 * Error response:
	 *   <[1] in-re verb>
	 *   <[8] in-re packet ID>
	 *   <[1] error code>
	 *   <[...] error-dependent payload, may be empty>
	 *
	 * An ERROR that does not pertain to a specific packet will have its verb
	 * set to VERB_NOP and its packet ID set to zero.
	 */
	VERB_ERROR = 0x02,

	/**
	 * Success response:
	 *   <[1] in-re verb>
	 *   <[8] in-re packet ID>
	 *   <[...] response-specific payload>
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
	 * if no locator is known for a node. Older versions may omit the locator.
	 */
	VERB_WHOIS = 0x04,

	/**
	 * Relay-mediated NAT traversal or firewall punching initiation:
	 *   <[1] flags>
	 *   <[5] ZeroTier address of other peer>
	 *   <[2] 16-bit number of endpoints where peer might be reached>
	 *   [<[...] endpoints to attempt>]
	 *
	 * Legacy packet format for pre-2.x peers:
	 *   <[1] flags (unused, currently 0)>
	 *   <[5] ZeroTier address of other peer>
	 *   <[2] 16-bit protocol address port>
	 *   <[1] protocol address length / type>
	 *   <[...] protocol address (network byte order)>
	 *
	 * When a root or other peer is relaying messages, it can periodically send
	 * RENDEZVOUS to assist peers in establishing direct communication.
	 *
	 * Peers also directly exchange information via HELLO, so this serves as
	 * a second way for peers to learn about their possible locations.
	 *
	 * It also serves another function: temporal coordination of NAT traversal
	 * attempts. Some NATs traverse better if both sides first send "firewall
	 * opener" packets and then send real packets and if this exchange is
	 * coordinated in time so that the packets effectively pass each other in
	 * flight.
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

	// Deprecated multicast frame message type.
	VERB_MULTICAST_FRAME_deprecated = 0x0e,

	/**
	 * Push of potential endpoints for direct communication:
	 *   <[2] 16-bit number of endpoints>
	 *   <[...] endpoints>
	 *
	 * If the target node is pre-2.0 path records of the following format
	 * are sent instead of post-2.x endpoints:
	 *   <[1] 8-bit path flags (zero)>
	 *   <[2] length of extended path characteristics (0)>
	 *   [<[...] extended path characteristics>]
	 *   <[1] address type>
	 *   <[1] address length in bytes>
	 *   <[...] address>
	 *
	 * Recipients will add these endpoints to a queue of possible endpoints
	 * to try for a given peer.
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

	VERB_MULTICAST = 0x16,

	/**
	 * Encapsulate a full ZeroTier packet in another:
	 *   <[...] raw encapsulated packet>
	 *
	 * Encapsulation exists to enable secure relaying as opposed to the usual
	 * "dumb" relaying. The latter is faster but secure relaying has roles
	 * where endpoint privacy is desired.
	 *
	 * Packet hop count is incremented as normal.
	 */
	VERB_ENCAP = 0x17

	// protocol max: 0x1f
};

#ifdef ZT_DEBUG_SPEW
static ZT_INLINE const char *verbName(const Verb v) noexcept
{
	switch(v) {
		case VERB_NOP:                        return "NOP";
		case VERB_HELLO:                      return "HELLO";
		case VERB_ERROR:                      return "ERROR";
		case VERB_OK:                         return "OK";
		case VERB_WHOIS:                      return "WHOIS";
		case VERB_RENDEZVOUS:                 return "RENDEZVOUS";
		case VERB_FRAME:                      return "FRAME";
		case VERB_EXT_FRAME:                  return "EXT_FRAME";
		case VERB_ECHO:                       return "ECHO";
		case VERB_MULTICAST_LIKE:             return "MULTICAST_LIKE";
		case VERB_NETWORK_CREDENTIALS:        return "NETWORK_CREDENTIALS";
		case VERB_NETWORK_CONFIG_REQUEST:     return "NETWORK_CONFIG_REQUEST";
		case VERB_NETWORK_CONFIG:             return "NETWORK_CONFIG";
		case VERB_MULTICAST_GATHER:           return "MULTICAST_GATHER";
		case VERB_MULTICAST_FRAME_deprecated: return "MULTICAST_FRAME_deprecated";
		case VERB_PUSH_DIRECT_PATHS:          return "PUSH_DIRECT_PATHS";
		case VERB_USER_MESSAGE:               return "USER_MESSAGE";
		case VERB_MULTICAST:                  return "MULTICAST";
		case VERB_ENCAP:                      return "ENCAP";
		default:                              return "(unknown)";
	}
}
#endif

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

/**
 * Deterministically mangle a 256-bit crypto key based on packet characteristics
 *
 * This uses extra data from the packet to mangle the secret, yielding when
 * combined with Salsa20's conventional 64-bit nonce an effective nonce that's
 * more like 68 bits.
 *
 * @param in Input key (32 bytes)
 * @param out Output buffer (32 bytes)
 */
static ZT_INLINE void salsa2012DeriveKey(const uint8_t *const in,uint8_t *const out,const Buf &packet,const unsigned int packetSize) noexcept
{
	// IV and source/destination addresses. Using the addresses divides the
	// key space into two halves-- A->B and B->A (since order will change).
#ifdef ZT_NO_UNALIGNED_ACCESS
	for(int i=0;i<18;++i)
		out[i] = in[i] ^ packet.unsafeData[i];
#else
	*reinterpret_cast<uint64_t *>(out) = *reinterpret_cast<const uint64_t *>(in) ^ *reinterpret_cast<const uint64_t *>(packet.unsafeData);
	*reinterpret_cast<uint64_t *>(out + 8) = *reinterpret_cast<const uint64_t *>(in + 8) ^ *reinterpret_cast<const uint64_t *>(packet.unsafeData + 8);
	*reinterpret_cast<uint16_t *>(out + 16) = *reinterpret_cast<const uint16_t *>(in + 16) ^ *reinterpret_cast<const uint16_t *>(packet.unsafeData + 16);
#endif

	// Flags, but with hop count masked off. Hop count is altered by forwarding
	// nodes and is the only field that is mutable by unauthenticated third parties.
	out[18] = in[18] ^ (packet.unsafeData[18] & 0xf8U);

	// Raw packet size in bytes -- thus each packet size defines a new key space.
	out[19] = in[19] ^ (uint8_t)packetSize;
	out[20] = in[20] ^ (uint8_t)(packetSize >> 8U); // little endian

	// Rest of raw key is used unchanged
#ifdef ZT_NO_UNALIGNED_ACCESS
	for(int i=21;i<32;++i)
		out[i] = in[i];
#else
	out[21] = in[21];
	out[22] = in[22];
	out[23] = in[23];
	*reinterpret_cast<uint64_t *>(out + 24) = *reinterpret_cast<const uint64_t *>(in + 24);
#endif
}

/**
 * Fill out packet header fields (except for mac, which is filled out by armor())
 * 
 * @param pkt Start of packet buffer
 * @param packetId Packet IV / cryptographic MAC
 * @param destination Destination ZT address
 * @param source Source (sending) ZT address
 * @param verb Protocol verb
 * @return Index of packet start
 */
static ZT_INLINE int newPacket(uint8_t pkt[28],const uint64_t packetId,const Address destination,const Address source,const Verb verb) noexcept
{
	Utils::storeAsIsEndian<uint64_t>(pkt + ZT_PROTO_PACKET_ID_INDEX,packetId);
	destination.copyTo(pkt + ZT_PROTO_PACKET_DESTINATION_INDEX);
	source.copyTo(pkt + ZT_PROTO_PACKET_SOURCE_INDEX);
	pkt[ZT_PROTO_PACKET_FLAGS_INDEX] = 0;
	Utils::storeAsIsEndian<uint64_t>(pkt + ZT_PROTO_PACKET_MAC_INDEX,0);
	pkt[ZT_PROTO_PACKET_VERB_INDEX] = (uint8_t)verb;
	return ZT_PROTO_PACKET_VERB_INDEX + 1;
}
static ZT_INLINE int newPacket(Buf &pkt,const uint64_t packetId,const Address destination,const Address source,const Verb verb) noexcept { return newPacket(pkt.unsafeData,packetId,destination,source,verb); }

/**
 * Encrypt and compute packet MAC
 *
 * @param pkt Packet data to encrypt (in place)
 * @param packetSize Packet size, must be at least ZT_PROTO_MIN_PACKET_LENGTH or crash will occur
 * @param key Key to use for encryption
 * @param cipherSuite Cipher suite to use for AEAD encryption or just MAC
 */
static ZT_INLINE void armor(uint8_t *const pkt,const int packetSize,const SharedPtr<SymmetricKey> &key,const uint8_t cipherSuite) noexcept
{
#if 0
	Protocol::Header &ph = pkt.as<Protocol::Header>(); // NOLINT(hicpp-use-auto,modernize-use-auto)
	ph.flags = (ph.flags & 0xc7U) | ((cipherSuite << 3U) & 0x38U); // flags: FFCCCHHH where CCC is cipher

	switch(cipherSuite) {
		case ZT_PROTO_CIPHER_SUITE__POLY1305_NONE: {
			uint8_t perPacketKey[ZT_SYMMETRIC_KEY_SIZE];
			salsa2012DeriveKey(key,perPacketKey,pkt,packetSize);
			Salsa20 s20(perPacketKey,&ph.packetId);

			uint8_t macKey[ZT_POLY1305_KEY_SIZE];
			s20.crypt12(Utils::ZERO256,macKey,ZT_POLY1305_KEY_SIZE);

			// only difference here is that we don't encrypt the payload

			uint64_t mac[2];
			poly1305(mac,pkt.unsafeData + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,packetSize - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,macKey);
			ph.mac = mac[0];
		} break;

		case ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012: {
			uint8_t perPacketKey[ZT_SYMMETRIC_KEY_SIZE];
			salsa2012DeriveKey(key,perPacketKey,pkt,packetSize);
			Salsa20 s20(perPacketKey,&ph.packetId);

			uint8_t macKey[ZT_POLY1305_KEY_SIZE];
			s20.crypt12(Utils::ZERO256,macKey,ZT_POLY1305_KEY_SIZE);

			const unsigned int encLen = packetSize - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START;
			s20.crypt12(pkt.unsafeData + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,pkt.unsafeData + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,encLen);

			uint64_t mac[2];
			poly1305(mac,pkt.unsafeData + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,encLen,macKey);
			ph.mac = mac[0];
		} break;

		case ZT_PROTO_CIPHER_SUITE__AES_GMAC_SIV: {
		} break;
	}
#endif
}

/**
 * Attempt to compress packet payload
 *
 * This attempts compression and swaps the pointer in 'pkt' for a buffer holding
 * compressed data on success. If compression did not shrink the packet, the original
 * packet size is returned and 'pkt' remains unchanged. If compression is successful
 * the compressed verb flag is also set.
 *
 * @param pkt Packet buffer value/result parameter: pointer may be swapped if compression is successful
 * @param packetSize Total size of packet in bytes (including headers)
 * @return New size of packet after compression or original size of compression wasn't helpful
 */
static ZT_INLINE int compress(SharedPtr<Buf> &pkt,int packetSize) noexcept
{
	// TODO
	return packetSize;
}

} // namespace Protocol
} // namespace ZeroTier

#endif
