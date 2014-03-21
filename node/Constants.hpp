/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef ZT_CONSTANTS_HPP
#define ZT_CONSTANTS_HPP

//
// This include file also auto-detects and canonicalizes some environment
// information defines:
//
// __LINUX__
// __APPLE__
// __UNIX_LIKE__ - any "unix like" OS (BSD, posix, etc.)
// __WINDOWS__
//
// Also makes sure __BYTE_ORDER is defined reasonably.
//

// Canonicalize Linux... is this necessary? Do it anyway to be defensive.
#if defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)
#ifndef __LINUX__
#define __LINUX__
#ifndef __UNIX_LIKE__
#define __UNIX_LIKE__
#endif
#endif
#endif

// TODO: Android is what? Linux technically, but does it define it?

// OSX and iOS are unix-like OSes far as we're concerned
#ifdef __APPLE__
#include <TargetConditionals.h>
#ifndef __UNIX_LIKE__
#define __UNIX_LIKE__
#endif
#endif

// Linux has endian.h
#ifdef __LINUX__
#include <endian.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#ifndef __WINDOWS__
#define __WINDOWS__
#endif
#define NOMINMAX
#pragma warning(disable : 4290)
#pragma warning(disable : 4996)
#pragma warning(disable : 4101)
#undef __UNIX_LIKE__
#define ZT_PATH_SEPARATOR '\\'
#define ZT_PATH_SEPARATOR_S "\\"
#define ZT_EOL_S "\r\n"
#include <WinSock2.h>
#include <Windows.h>
#endif

// Assume these are little-endian. PPC is not supported for OSX, and ARM
// runs in little-endian mode for these OS families.
#if defined(__APPLE__) || defined(__WINDOWS__)
#undef __BYTE_ORDER
#undef __LITTLE_ENDIAN
#undef __BIG_ENDIAN
#define __BIG_ENDIAN 4321
#define __LITTLE_ENDIAN 1234
#define __BYTE_ORDER 1234
#endif

#ifdef __UNIX_LIKE__
#define ZT_PATH_SEPARATOR '/'
#define ZT_PATH_SEPARATOR_S "/"
#define ZT_EOL_S "\n"
#endif

// Error out if required symbols are missing
#ifndef __BYTE_ORDER
error_no_byte_order_defined;
#endif

/**
 * Length of a ZeroTier address in bytes
 */
#define ZT_ADDRESS_LENGTH 5

/**
 * Addresses beginning with this byte are reserved for the joy of in-band signaling
 */
#define ZT_ADDRESS_RESERVED_PREFIX 0xff

/**
 * Default local port for ZeroTier traffic
 */
#define ZT_DEFAULT_PORT 9993

/**
 * Default payload MTU for UDP packets
 *
 * In the future we might support UDP path MTU discovery, but for now we
 * set a maximum that is equal to 1500 minus 8 (for PPPoE overhead, common
 * in some markets) minus 48 (IPv6 UDP overhead).
 */
#define ZT_UDP_DEFAULT_PAYLOAD_MTU 1444

/**
 * MTU used for Ethernet tap device
 *
 * This is pretty much an unchangeable global constant. To make it change
 * across nodes would require logic to send ICMP packet too big messages,
 * which would complicate things. 1500 has been good enough on most LANs
 * for ages, so a larger MTU should be fine for the forseeable future. This
 * typically results in two UDP packets per single large frame. Experimental
 * results seem to show that this is good. Larger MTUs resulting in more
 * fragments seemed too brittle on slow/crummy links for no benefit.
 *
 * If this does change, also change it in tap.h in the tuntaposx code under
 * mac-tap.
 * 
 * Overhead for a normal frame split into two packets:
 *
 * 1414 = 1444 (typical UDP MTU) - 28 (packet header) - 2 (ethertype)
 * 1428 = 1444 (typical UDP MTU) - 16 (fragment header)
 * SUM: 2842
 *
 * We use 2800, which leaves some room for other payload in other types of
 * messages such as multicast propagation or future support for bridging.
 */
#define ZT_IF_MTU 2800

/**
 * Maximum number of packet fragments we'll support
 * 
 * The actual spec allows 16, but this is the most we'll support right
 * now. Packets with more than this many fragments are dropped.
 */
#define ZT_MAX_PACKET_FRAGMENTS 3

/**
 * Timeout for receipt of fragmented packets in ms
 *
 * Since there's no retransmits, this is just a really bad case scenario for
 * transit time. It's short enough that a DOS attack from exhausing buffers is
 * very unlikely, as the transfer rate would have to be fast enough to fill
 * system memory in this time.
 */
#define ZT_FRAGMENTED_PACKET_RECEIVE_TIMEOUT 1000

/**
 * First byte of MAC addresses derived from ZeroTier addresses
 * 
 * This has the 0x02 bit set, which indicates a locally administrered
 * MAC address rather than one with a known HW ID.
 */
#define ZT_MAC_FIRST_OCTET 0x32

/**
 * Length of secret key in bytes
 */
#define ZT_PEER_SECRET_KEY_LENGTH 32

/**
 * How often Topology::clean() and Network::clean() are called in ms
 */
#define ZT_DB_CLEAN_PERIOD 300000

/**
 * How long to remember peers in RAM if they haven't been used
 */
#define ZT_PEER_IN_MEMORY_EXPIRATION 600000

/**
 * Delay between WHOIS retries in ms
 */
#define ZT_WHOIS_RETRY_DELAY 350

/**
 * Maximum identity WHOIS retries
 */
#define ZT_MAX_WHOIS_RETRIES 3

/**
 * Transmit queue entry timeout
 */
#define ZT_TRANSMIT_QUEUE_TIMEOUT (ZT_WHOIS_RETRY_DELAY * (ZT_MAX_WHOIS_RETRIES + 1))

/**
 * Receive queue entry timeout
 */
#define ZT_RECEIVE_QUEUE_TIMEOUT (ZT_WHOIS_RETRY_DELAY * (ZT_MAX_WHOIS_RETRIES + 1))

/**
 * Maximum number of ZT hops allowed
 * 
 * The protocol allows up to 7, but we limit it to something smaller.
 */
#define ZT_RELAY_MAX_HOPS 3

/**
 * Size of multicast deduplication ring buffer in 64-bit ints
 */
#define ZT_MULTICAST_DEDUP_HISTORY_LENGTH 512

/**
 * Default number of bits in multicast propagation prefix
 */
#define ZT_DEFAULT_MULTICAST_PREFIX_BITS 1

/**
 * Default max depth (TTL) for multicast propagation
 */
#define ZT_DEFAULT_MULTICAST_DEPTH 32

/**
 * Global maximum for multicast propagation depth
 *
 * This is kind of an insane value, meant as a sanity check.
 */
#define ZT_MULTICAST_GLOBAL_MAX_DEPTH 500

/**
 * Expire time for multicast 'likes' in ms
 */
#define ZT_MULTICAST_LIKE_EXPIRE 120000

/**
 * Time between polls of local taps for multicast membership changes
 */
#define ZT_MULTICAST_LOCAL_POLL_PERIOD 10000

/**
 * Delay between scans of the topology active peer DB for peers that need ping
 */
#define ZT_PING_CHECK_DELAY 7000

/**
 * Delay between checks of network configuration fingerprint
 */
#define ZT_NETWORK_FINGERPRINT_CHECK_DELAY 5000

/**
 * Delay between pings (actually HELLOs) to direct links
 */
#define ZT_PEER_DIRECT_PING_DELAY 120000

/**
 * Delay in ms between firewall opener packets to direct links
 *
 * This should be lower than the UDP conversation entry timeout in most
 * stateful firewalls.
 */
#define ZT_FIREWALL_OPENER_DELAY 50000

/**
 * Delay between requests for updated network autoconf information
 */
#define ZT_NETWORK_AUTOCONF_DELAY 60000

/**
 * Delay in core loop between checks of network autoconf newness
 */
#define ZT_NETWORK_AUTOCONF_CHECK_DELAY 7000

/**
 * Minimum delay in Node service loop
 * 
 * This is the shortest of the check delays/periods.
 */
#define ZT_MIN_SERVICE_LOOP_INTERVAL ZT_NETWORK_FINGERPRINT_CHECK_DELAY

/**
 * Activity timeout for links
 * 
 * A link that hasn't spoken in this long is simply considered inactive.
 */
#define ZT_PEER_PATH_ACTIVITY_TIMEOUT ((ZT_PEER_DIRECT_PING_DELAY * 2) + 1000)

/**
 * Close TCP tunnels if unused for this long
 */
#define ZT_TCP_TUNNEL_ACTIVITY_TIMEOUT ZT_PEER_PATH_ACTIVITY_TIMEOUT

/**
 * Try TCP tunnels if no response to UDP PINGs in this many milliseconds
 */
#define ZT_PING_UNANSWERED_AFTER 5000

/**
 * Stop relaying via peers that have not responded to direct sends in this long
 */
#define ZT_PEER_RELAY_CONVERSATION_LATENCY_THRESHOLD 10000

/**
 * Number of outgoing verb/packetId pairs to keep for sends expecting responses
 */
#define ZT_PEER_REQUEST_HISTORY_LENGTH 8

/**
 * IP hops (a.k.a. TTL) to set for firewall opener packets
 *
 * 2 should permit traversal of double-NAT configurations, such as from inside
 * a VM running behind local NAT on a host that is itself behind NAT.
 */
#define ZT_FIREWALL_OPENER_HOPS 2

/**
 * Delay sleep overshoot for detection of a probable sleep/wake event
 */
#define ZT_SLEEP_WAKE_DETECTION_THRESHOLD 2000

/**
 * Time to pause main service loop after sleep/wake detect
 */
#define ZT_SLEEP_WAKE_SETTLE_TIME 5000

/**
 * Minimum interval between attempts by relays to unite peers
 */
#define ZT_MIN_UNITE_INTERVAL 30000

/**
 * Delay in milliseconds between firewall opener and real packet for NAT-t
 */
#define ZT_RENDEZVOUS_NAT_T_DELAY 500

/**
 * Minimum interval between attempts to do a software update
 */
#define ZT_UPDATE_MIN_INTERVAL 120000

/**
 * Maximum interval between checks for new versions (2 hours)
 */
#define ZT_UPDATE_MAX_INTERVAL 7200000

/**
 * Update HTTP timeout in seconds
 */
#define ZT_UPDATE_HTTP_TIMEOUT 30

#endif
