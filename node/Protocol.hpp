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

// TODO: mlock

/*
 * Core ZeroTier protocol packet formats ------------------------------------------------------------------------------
 *
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
 *
 * --------------------------------------------------------------------------------------------------------------------
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
 *    + HELLO and OK(HELLO) can carry structured meta-data
 *    + Ephemeral keys for forward secrecy and limited key lifetime
 *    + Old planet/moon stuff is DEAD! Independent roots are easier.
 *    + AES encryption is now the default
 *    + New combined Curve25519/NIST P-384 identity type (type 1)
 *    + Short probe packets to reduce probe bandwidth
 *    + Aggressive NAT traversal techniques for IPv4 symmetric NATs
 *    + Remote diagnostics including rewrite of remote tracing
 */
#define ZT_PROTO_VERSION 11

/**
 * Minimum supported protocol version
 */
#define ZT_PROTO_VERSION_MIN 8

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
 * Minimum viable length for a fragment
 */
#define ZT_PROTO_MIN_FRAGMENT_LENGTH 16

/**
 * Magic number indicating a fragment if present at index 13
 */
#define ZT_PROTO_PACKET_FRAGMENT_INDICATOR 0xff

/**
 * Index at which fragment indicator is found in fragments
 */
#define ZT_PROTO_PACKET_FRAGMENT_INDICATOR_INDEX 13

/**
 * Index of flags field in regular packet headers
 */
#define ZT_PROTO_PACKET_FLAGS_INDEX 18

/**
 * Length of a probe packet
 */
#define ZT_PROTO_PROBE_LENGTH 8

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
 * Mask to extract just the verb from the verb field, which also includes flags
 */
#define ZT_PROTO_VERB_MASK 0x1fU

/**
 * Key derivation function label for the keys used with HMAC-384 in HELLO
 *
 * With the KDF the 'iter' parameter is 0 for the key used for
 * HMAC in HELLO and 1 for the one used in OK(HELLO).
 */
#define ZT_PROTO_KDF_KEY_LABEL_HELLO_HMAC 'H'

/**
 * HELLO exchange meta-data: random 128-bit identifier for each running instance
 */
#define ZT_PROTO_HELLO_NODE_META_INSTANCE_ID "i"

/**
 * HELLO exchange meta-data: signed locator for this node
 */
#define ZT_PROTO_HELLO_NODE_META_LOCATOR "l"

/**
 * HELLO exchange meta-data: ephemeral C25519 public key
 */
#define ZT_PROTO_HELLO_NODE_META_EPHEMERAL_C25519 "e0"

/**
 * HELLO exchange meta-data: ephemeral NIST P-384 public key
 */
#define ZT_PROTO_HELLO_NODE_META_EPHEMERAL_P384 "e1"

/**
 * HELLO exchange meta-data: address(es) of nodes to whom this node will relay
 */
#define ZT_PROTO_HELLO_NODE_META_NEIGHBORS "wr"

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

/**
 * HELLO exchange meta-data: preferred cipher suite (may be ignored)
 */
#define ZT_PROTO_HELLO_NODE_META_PREFERRED_CIPHER_SUITE "c"

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
	 *   <[1] software major version (LEGACY)>
	 *   <[1] software minor version (LEGACY)>
	 *   <[2] software revision (LEGACY)>
	 *   <[8] timestamp for determining latency (LEGACY)>
	 *   <[...] binary serialized identity>
	 *   <[...] physical destination address of packet (LEGACY)>
	 *   <[2] 16-bit reserved "encrypted zero" field (LEGACY)>
	 *   <[...] encrypted dictionary>
	 *   <[2] 16-bit length of preceding encrypted dictionary>
	 *   <[48] HMAC-SHA384 of plaintext packet (with hops masked to 0)>
	 *
	 * HELLO is sent to initiate a new pairing between two nodes.
	 *
	 * HELLO is the only packet ever sent without normal payload encryption,
	 * though an inner encrypted envelope exists to obscure all fields that
	 * do not need to be sent in the clear. HELLO's MAC field contains a
	 * Poly1305 MAC for backward compatibility, and v2.x adds an additional
	 * HMAC-SHA384 at the end for stronger authentication of sessions. HELLO
	 * authentication is performed using the long-lived identity key only,
	 * and the encryption of the inner dictionary field is done using a key
	 * derived from this identity key explicitly for this purpose.
	 *
	 * The main payload of HELLO is the protocol version and the full identity
	 * of the sender, which includes the sender's public key(s). An encrypted
	 * dictionary (key/value store) is also included for additional information.
	 * This is encrypted using AES-CTR with a derived key and using the final
	 * 96 bits of the packet's HMAC-SHA384 as the CTR IV. (The HMAC authenticates
	 * the packet prior to this field being encrypted, making this a SIV
	 * construction much like AES-GMAC-SIV.)
	 *
	 * The length of the dictionary field is included immediately after it so
	 * that it can be decrypted and the HMAC validated without performing any
	 * parsing of anything else, since it's a good idea to authenticate any
	 * message as early as possible in any secure protocol.
	 *
	 * V1.x will ignore the HMAC and dictionary fields as it doesn't understand
	 * them, but the packet is constructed so that 1.x nodes will parse what
	 * they need to communicate with 2.x nodes (without forward secrecy) as long
	 * as we wish to support this.
	 *
	 * Several legacy fields are present as well for the benefit of 1.x nodes.
	 * These will go away and become simple reserved space once 1.x is no longer
	 * supported. Some are self-explanatory. The "encrypted zero" is rather
	 * strange. It's a 16-bit zero value encrypted using Salsa20/12 and the
	 * long-lived identity key shared by the two peers. It tells 1.x that an
	 * old encrypted field is no longer there and that it should stop parsing
	 * the packet at that point.
	 *
	 * The following fields are nearly always present and must exist to support
	 * forward secrecy (in the case of the instance ID, keys, and key revision)
	 * or federated root membership (in the case of the locator).
	 *
	 *   TIMESTAMP - node's timestamp in milliseconds (supersedes legacy field)
	 *   INSTANCE_ID - a 64-bit unique value generated on each node start
	 *   EPHEMERAL_C25519 - an ephemeral Curve25519 public key
	 *   EPHEMERAL_P384 - an ephemeral NIST P-384 public key
	 *   EPHEMERAL_REVISION - 64-bit monotonically increasing per-instance counter
	 *   LOCATOR - signed record enumerating this node's trusted contact points
	 *
	 * The following optional fields may also be present:
	 *
	 *   NAME - abitrary short user-defined name for this node
	 *   CONTACT - arbitrary short contact information string for this node
	 *   NEIGHBORS - addresses of node(s) to whom we'll relay (mesh-like routing)
	 *   LOC_X, LOC_Y, LOC_Z - location relative to the nearest large center of mass
	 *   PEER_LOC_X, PEER_LOC_Y, PEER_LOC_Z - where sender thinks peer is located
	 *   SOFTWARE_VENDOR - short name or description of vendor, such as a URL
	 *   SOFTWARE_VERSION - major, minor, revision, and build, and 16-bit integers
	 *   PHYSICAL_DEST - serialized Endpoint to which this message was sent
	 *   VIRTUAL_DEST - ZeroTier address of first hop (if first hop wasn't destination)
	 *   COMPLIANCE - bit mask containing bits for e.g. a FIPS-compliant node
	 *
	 * A valid and successfully authenticated HELLO will generate the following
	 * OK response. It contains an echo of the timestamp supplied by the
	 * initiating peer, the protocol version, and a dictionary containing
	 * the same information about the responding peer as the originating peer
	 * sent.
	 *
	 * OK payload:
	 *   <[8] timestamp echoed from original HELLO>
	 *   <[1] protocol version>
	 *   <[1] software major version (LEGACY)>
	 *   <[1] software minor version (LEGACY)>
	 *   <[2] software revision (LEGACY)>
	 *   <[...] physical destination address of packet (LEGACY)>
	 *   <[2] 16-bit reserved zero field (LEGACY)>
	 *   <[...] dictionary>
	 *   <[48] HMAC-SHA384 of plaintext packet (with hops masked to 0)>
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
	 * if no locator is known for a node. Older versions may omit the locator.
	 */
	VERB_WHOIS = 0x04,

	/**
	 * Relay-mediated NAT traversal or firewall punching initiation:
	 *   <[1] flags (unused, currently 0)>
	 *   <[5] ZeroTier address of peer that might be found at this address>
	 *   <[2] 16-bit protocol address port>
	 *   <[1] protocol address length / type>
	 *   <[...] protocol address (network byte order)>
	 *
	 * This is sent by a third party node to inform a node of where another
	 * may be located. These are currently only allowed from roots.
	 *
	 * The protocol address format differs from the standard InetAddress
	 * encoding for legacy reasons, but it's not hard to decode. The following
	 * values are valid for the protocol address length (type) field:
	 *
	 *   4 - IPv4 IP address
	 *   16 - IPv6 IP address
	 *   255 - Endpoint object, unmarshaled in place (port ignored)
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
	 *   <[1] 8-bit path flags>
	 *   <[2] length of extended path characteristics or 0 for none>
	 *   <[...] extended path characteristics>
	 *   <[1] address type>
	 *   <[1] address record length in bytes>
	 *   <[...] address>
	 *
	 * Path flags:
	 *   0x01 - Sender is likely behind a symmetric NAT
	 *   0x02 - Use BFG1024 algorithm for symmetric NAT-t if conditions met
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
});

ZT_PACKED_STRUCT(struct HELLO
{
	Header h;
	uint8_t versionProtocol;
	uint8_t versionMajor;
	uint8_t versionMinor;
	uint16_t versionRev;
	uint64_t timestamp;
});

ZT_PACKED_STRUCT(struct RENDEZVOUS
{
	Header h;
	uint8_t flags;
	uint8_t peerAddress[5];
	uint16_t port;
	uint8_t addressLength;
});

ZT_PACKED_STRUCT(struct FRAME
{
	Header h;
	uint64_t networkId;
	uint16_t etherType;
});

ZT_PACKED_STRUCT(struct EXT_FRAME
{
	Header h;
	uint64_t networkId;
	uint8_t flags;
});

ZT_PACKED_STRUCT(struct PUSH_DIRECT_PATHS
{
	Header h;
	uint16_t numPaths;
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
});

namespace OK {

/**
 * OK response header
 *
 * @tparam PT OK payload type (default: uint8_t[])
 */
ZT_PACKED_STRUCT(struct Header
{
	Protocol::Header h;
	uint8_t inReVerb;
	uint64_t inRePacketId;
});

ZT_PACKED_STRUCT(struct WHOIS
{
	OK::Header h;
});

ZT_PACKED_STRUCT(struct ECHO
{
	OK::Header h;
});

ZT_PACKED_STRUCT(struct HELLO
{
	OK::Header h;
	uint64_t timestampEcho;
	uint8_t versionProtocol;
	uint8_t versionMajor;
	uint8_t versionMinor;
	uint16_t versionRev;
});

ZT_PACKED_STRUCT(struct EXT_FRAME
{
	OK::Header h;
	uint64_t networkId;
	uint8_t flags;
	uint8_t destMac[6];
	uint8_t sourceMac[6];
	uint16_t etherType;
});

ZT_PACKED_STRUCT(struct NETWORK_CONFIG
{
	OK::Header h;
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
	Protocol::Header h;
	int8_t inReVerb;
	uint64_t inRePacketId;
	uint8_t error;
});

ZT_PACKED_STRUCT(struct NEED_MEMBERSHIP_CERTIFICATE
{
	ERROR::Header h;
	uint64_t networkId;
});

ZT_PACKED_STRUCT(struct UNSUPPORTED_OPERATION__NETWORK_CONFIG_REQUEST
{
	ERROR::Header h;
	uint64_t networkId;
});

} // namespace ERROR

/****************************************************************************/

/**
 * Convenience function to pull packet ID from a raw buffer
 *
 * @param pkt Packet to read first 8 bytes from
 * @param packetSize Packet's actual size in bytes
 * @return Packet ID or 0 if packet size is less than 8
 */
static ZT_INLINE uint64_t packetId(const Buf &pkt,const unsigned int packetSize) noexcept { return (packetSize >= 8) ? Utils::loadBigEndian<uint64_t>(pkt.unsafeData) : 0ULL; }

/**
 * @param Packet to extract hops from
 * @param packetSize Packet's actual size in bytes
 * @return 3-bit hops field embedded in packet flags field
 */
static ZT_INLINE uint8_t packetHops(const Buf &pkt,const unsigned int packetSize) noexcept { return (packetSize >= ZT_PROTO_PACKET_FLAGS_INDEX) ? (pkt.unsafeData[ZT_PROTO_PACKET_FLAGS_INDEX] & ZT_PROTO_FLAG_FIELD_HOPS_MASK) : 0; }

/**
 * @param Packet to extract cipher ID from
 * @param packetSize Packet's actual size in bytes
 * @return 3-bit cipher field embedded in packet flags field
 */
static ZT_INLINE uint8_t packetCipher(const Buf &pkt,const unsigned int packetSize) noexcept { return (packetSize >= ZT_PROTO_PACKET_FLAGS_INDEX) ? ((pkt.unsafeData[ZT_PROTO_PACKET_FLAGS_INDEX] >> 3U) & 0x07U) : 0; }

/**
 * @return 3-bit hops field embedded in packet flags field
 */
static ZT_INLINE uint8_t packetHops(const Header &ph) noexcept { return (ph.flags & 0x07U); }

/**
 * @return 3-bit cipher field embedded in packet flags field
 */
static ZT_INLINE uint8_t packetCipher(const Header &ph) noexcept { return ((ph.flags >> 3U) & 0x07U); }

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
 * Create a short probe packet for probing a recipient for e.g. NAT traversal and path setup
 *
 * @param sender Sender identity
 * @param recipient Recipient identity
 * @param key Long-term shared secret key resulting from sender and recipient agreement
 * @return Probe packed into 64-bit integer (in big-endian byte order)
 */
uint64_t createProbe(const Identity &sender,const Identity &recipient,const uint8_t key[ZT_PEER_SECRET_KEY_LENGTH]) noexcept;

// Do not use directly
extern std::atomic<uint64_t> _s_packetIdCtr;

/**
 * Get a packet ID (and nonce) for a new packet
 *
 * @return Next packet ID
 */
static ZT_INLINE uint64_t getPacketId() noexcept { return ++_s_packetIdCtr; }

/**
 * Encrypt and compute packet MAC
 *
 * @param pkt Packet data to encrypt (in place)
 * @param packetSize Packet size, must be at least ZT_PROTO_MIN_PACKET_LENGTH or crash will occur
 * @param key Key to use for encryption (not per-packet key)
 * @param cipherSuite Cipher suite to use for AEAD encryption or just MAC
 */
void armor(Buf &pkt,int packetSize,const uint8_t key[ZT_PEER_SECRET_KEY_LENGTH],uint8_t cipherSuite) noexcept;

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
int compress(SharedPtr<Buf> &pkt,int packetSize) noexcept;

} // namespace Protocol
} // namespace ZeroTier

#endif
