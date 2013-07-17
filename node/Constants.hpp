/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#ifndef _ZT_CONSTANTS_HPP
#define _ZT_CONSTANTS_HPP

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
#undef __UNIX_LIKE__
#define ZT_PATH_SEPARATOR '\\'
#define ZT_PATH_SEPARATOR_S "\\"
#define ZT_EOL_S "\r\n"
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
#ifndef ZT_OSNAME
error_no_ZT_OSNAME_defined;
#endif
#ifndef ZT_ARCH
error_no_ZT_ARCH_defined;
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
 * Default local UDP port
 */
#define ZT_DEFAULT_UDP_PORT 8993

/**
 * Local control port, also used for multiple invocation check
 */
#define ZT_CONTROL_UDP_PORT 39393

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
#define ZT_FRAGMENTED_PACKET_RECEIVE_TIMEOUT 1500

/**
 * First byte of MAC addresses derived from ZeroTier addresses
 * 
 * This has the 0x02 bit set, which indicates a locally administrered
 * MAC address rather than one with a known HW ID.
 */
#define ZT_MAC_FIRST_OCTET 0x32

/**
 * How often Topology::clean() is called in ms
 */
#define ZT_TOPOLOGY_CLEAN_PERIOD 300000

/**
 * Delay between WHOIS retries in ms
 */
#define ZT_WHOIS_RETRY_DELAY 500

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
 * Breadth of tree for rumor mill multicast propagation
 */
#define ZT_MULTICAST_PROPAGATION_BREADTH 4

/**
 * Depth of tree for rumor mill multicast propagation
 *
 * The maximum number of peers who can receive a multicast is equal to
 * the sum of BREADTH^i where I is from 1 to DEPTH. This ignores the effect
 * of the rate limiting algorithm or bloom filter collisions.
 *
 * 7 results in a max of 21844 recipients for a given multicast.
 */
#define ZT_MULTICAST_PROPAGATION_DEPTH 7

/**
 * Length of ring buffer history of recent multicast packets
 */
#define ZT_MULTICAST_DEDUP_HISTORY_LENGTH 1024

/**
 * Expiration time in ms for multicast deduplication history items
 */
#define ZT_MULTICAST_DEDUP_HISTORY_EXPIRE 4000

/**
 * Period between announcements of all multicast 'likes' in ms
 *
 * Announcement occurs when a multicast group is locally joined, but all
 * memberships are periodically re-broadcast. If they're not they will
 * expire.
 */
#define ZT_MULTICAST_LIKE_ANNOUNCE_ALL_PERIOD 120000

/**
 * Expire time for multicast 'likes' in ms
 */
#define ZT_MULTICAST_LIKE_EXPIRE ((ZT_MULTICAST_LIKE_ANNOUNCE_ALL_PERIOD * 2) + 1000)

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
#define ZT_PEER_LINK_ACTIVITY_TIMEOUT ((ZT_PEER_DIRECT_PING_DELAY * 2) + 1000)

/**
 * Delay in ms between firewall opener packets to direct links
 *
 * This should be lower than the UDP conversation entry timeout in most
 * stateful firewalls.
 */
#define ZT_FIREWALL_OPENER_DELAY 50000

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

#endif
