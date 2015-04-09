/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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

#include "../include/ZeroTierOne.h"

//
// This include file also auto-detects and canonicalizes some environment
// information defines:
//
// __LINUX__
// __APPLE__
// __BSD__ (OSX also defines this)
// __UNIX_LIKE__ (Linux, BSD, etc.)
// __WINDOWS__
//
// Also makes sure __BYTE_ORDER is defined reasonably.
//

// Hack: make sure __GCC__ is defined on old GCC compilers
#ifndef __GCC__
#if defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1) || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2) || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)
#define __GCC__
#endif
#endif

#if defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)
#ifndef __LINUX__
#define __LINUX__
#endif
#ifndef __UNIX_LIKE__
#define __UNIX_LIKE__
#endif
#include <endian.h>
#endif

#ifdef __FreeBSD__
#ifndef __UNIX_LIKE__
#define __UNIX_LIKE__
#endif
#ifndef __BSD__
#define __BSD__
#endif
#include <machine/endian.h>
#ifndef __BYTE_ORDER
#define __BYTE_ORDER _BYTE_ORDER
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#define __BIG_ENDIAN _BIG_ENDIAN
#endif
#endif

// TODO: Android is what? Linux technically, but does it define it?

#ifdef __APPLE__
#include <TargetConditionals.h>
#ifndef __UNIX_LIKE__
#define __UNIX_LIKE__
#endif
#ifndef __BSD__
#define __BSD__
#endif
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
#undef __BSD__
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

#ifndef __BYTE_ORDER
#include <endian.h>
#endif

/**
 * Length of a ZeroTier address in bytes
 */
#define ZT_ADDRESS_LENGTH 5

/**
 * Length of a hexadecimal ZeroTier address
 */
#define ZT_ADDRESS_LENGTH_HEX 10

/**
 * Addresses beginning with this byte are reserved for the joy of in-band signaling
 */
#define ZT_ADDRESS_RESERVED_PREFIX 0xff

/**
 * Default payload MTU for UDP packets
 *
 * In the future we might support UDP path MTU discovery, but for now we
 * set a maximum that is equal to 1500 minus 8 (for PPPoE overhead, common
 * in some markets) minus 48 (IPv6 UDP overhead).
 */
#define ZT_UDP_DEFAULT_PAYLOAD_MTU 1444

/**
 * Default MTU used for Ethernet tap device
 */
#define ZT_IF_MTU ZT1_MAX_MTU

/**
 * Default interface metric for ZeroTier taps -- should be higher than physical ports
 */
#define ZT_DEFAULT_IF_METRIC 32768

/**
 * Maximum number of packet fragments we'll support
 * 
 * The actual spec allows 16, but this is the most we'll support right
 * now. Packets with more than this many fragments are dropped.
 */
#define ZT_MAX_PACKET_FRAGMENTS 4

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
 * Length of secret key in bytes -- 256-bit for Salsa20
 */
#define ZT_PEER_SECRET_KEY_LENGTH 32

/**
 * How often Topology::clean() and Network::clean() and similar are called, in ms
 */
#define ZT_HOUSEKEEPING_PERIOD 120000

/**
 * Overriding granularity for timer tasks to prevent CPU-intensive thrashing on every packet
 */
#define ZT_CORE_TIMER_TASK_GRANULARITY 1000

/**
 * How long to remember peer records in RAM if they haven't been used
 */
#define ZT_PEER_IN_MEMORY_EXPIRATION 600000

/**
 * Delay between WHOIS retries in ms
 */
#define ZT_WHOIS_RETRY_DELAY 500

/**
 * Maximum identity WHOIS retries (each attempt tries consulting a different peer)
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
 * Maximum number of ZT hops allowed (this is not IP hops/TTL)
 * 
 * The protocol allows up to 7, but we limit it to something smaller.
 */
#define ZT_RELAY_MAX_HOPS 3

/**
 * Expire time for multicast 'likes' and indirect multicast memberships in ms
 */
#define ZT_MULTICAST_LIKE_EXPIRE 600000

/**
 * Delay between explicit MULTICAST_GATHER requests for a given multicast channel
 */
#define ZT_MULTICAST_EXPLICIT_GATHER_DELAY (ZT_MULTICAST_LIKE_EXPIRE / 10)

/**
 * Timeout for outgoing multicasts
 *
 * This is how long we wait for explicit or implicit gather results.
 */
#define ZT_MULTICAST_TRANSMIT_TIMEOUT 5000

/**
 * Default maximum number of peers to address with a single multicast (if unspecified in network config)
 */
#define ZT_MULTICAST_DEFAULT_LIMIT 32

/**
 * How frequently to send a zero-byte UDP keepalive packet
 *
 * There are NATs with timeouts as short as 30 seconds, so this turns out
 * to be needed.
 */
#define ZT_NAT_KEEPALIVE_DELAY 25000

/**
 * Delay between scans of the topology active peer DB for peers that need ping
 *
 * This is also how often pings will be retried to upstream peers (supernodes)
 * constantly until something is heard.
 */
#define ZT_PING_CHECK_INVERVAL 6250

/**
 * Delay between ordinary case pings of direct links
 */
#define ZT_PEER_DIRECT_PING_DELAY 120000

/**
 * Delay between requests for updated network autoconf information
 */
#define ZT_NETWORK_AUTOCONF_DELAY 60000

/**
 * Increment core desperation after this multiple of ping checks without responses from upstream peers
 */
#define ZT_CORE_DESPERATION_INCREMENT 2

/**
 * Timeout for overall peer activity (measured from last receive)
 */
#define ZT_PEER_ACTIVITY_TIMEOUT (ZT_PEER_DIRECT_PING_DELAY + (ZT_PING_CHECK_INVERVAL * 3))

/**
 * Stop relaying via peers that have not responded to direct sends
 *
 * When we send something (including frames), we generally expect a response.
 * Switching relays if no response in a short period of time causes more
 * rapid failover if a supernode goes down or becomes unreachable. In the
 * mistaken case, little harm is done as it'll pick the next-fastest
 * supernode and will switch back eventually.
 */
#define ZT_PEER_RELAY_CONVERSATION_LATENCY_THRESHOLD 10000

/**
 * Minimum interval between attempts by relays to unite peers
 *
 * When a relay gets a packet destined for another peer, it sends both peers
 * a RENDEZVOUS message no more than this often. This instructs the peers
 * to attempt NAT-t and gives each the other's corresponding IP:port pair.
 */
#define ZT_MIN_UNITE_INTERVAL 60000

/**
 * Delay between initial direct NAT-t packet and more aggressive techniques
 */
#define ZT_NAT_T_TACTICAL_ESCALATION_DELAY 2000

/**
 * Size of anti-recursion history (see AntiRecursion.hpp)
 */
#define ZT_ANTIRECURSION_HISTORY_SIZE 16

/**
 * How often to broadcast beacons over physical local LANs
 */
#define ZT_BEACON_INTERVAL 30000

/**
 * Do not respond to any beacon more often than this
 */
#define ZT_MIN_BEACON_RESPONSE_INTERVAL (ZT_BEACON_INTERVAL / 32)

/**
 * Sanity limit on maximum bridge routes
 *
 * If the number of bridge routes exceeds this, we cull routes from the
 * bridges with the most MACs behind them until it doesn't. This is a
 * sanity limit to prevent memory-filling DOS attacks, nothing more. No
 * physical LAN has anywhere even close to this many nodes. Note that this
 * does not limit the size of ZT virtual LANs, only bridge routing.
 */
#define ZT_MAX_BRIDGE_ROUTES 67108864

/**
 * If there is no known route, spam to up to this many active bridges
 */
#define ZT_MAX_BRIDGE_SPAM 16

/**
 * A test pseudo-network-ID that can be joined
 *
 * Joining this network ID will result in a network with no IP addressing
 * and default parameters. No network configuration master will be consulted
 * and instead a static config will be used. This is used in built-in testnet
 * scenarios and can also be used for external testing.
 *
 * This is an impossible real network ID since 0xff is a reserved address
 * prefix.
 */
#define ZT_TEST_NETWORK_ID 0xffffffffffffffffULL

#endif
