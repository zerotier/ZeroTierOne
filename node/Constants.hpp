/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
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

#ifdef __APPLE__
#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)
#include <TargetConditionals.h>
#ifndef __UNIX_LIKE__
#define __UNIX_LIKE__
#endif
#ifndef __BSD__
#define __BSD__
#endif
#include <machine/endian.h>
#endif

// Defined this macro to disable "type punning" on a number of targets that
// have issues with unaligned memory access.
#if defined(__arm__) || defined(__ARMEL__) || (defined(__APPLE__) && ( (defined(TARGET_OS_IPHONE) && (TARGET_OS_IPHONE != 0)) || (defined(TARGET_OS_WATCH) && (TARGET_OS_WATCH != 0)) || (defined(TARGET_IPHONE_SIMULATOR) && (TARGET_IPHONE_SIMULATOR != 0)) ) )
#ifndef ZT_NO_TYPE_PUNNING
#define ZT_NO_TYPE_PUNNING
#endif
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__)
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

#if defined(_WIN32) || defined(_WIN64)
#ifndef __WINDOWS__
#define __WINDOWS__
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
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

// Assume little endian if not defined
#if (defined(__APPLE__) || defined(__WINDOWS__)) && (!defined(__BYTE_ORDER))
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

#if (defined(__GNUC__) && (__GNUC__ >= 3)) || (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 800)) || defined(__clang__)
#ifndef likely
#define likely(x) __builtin_expect((x),1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect((x),0)
#endif
#else
#ifndef likely
#define likely(x) (x)
#endif
#ifndef unlikely
#define unlikely(x) (x)
#endif
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
#define ZT_DEFAULT_MTU 2800

/**
 * Maximum number of packet fragments we'll support (protocol max: 16)
 */
#define ZT_MAX_PACKET_FRAGMENTS 7

/**
 * Size of RX queue
 *
 * This is about 2mb, and can be decreased for small devices. A queue smaller
 * than about 4 is probably going to cause a lot of lost packets.
 */
#define ZT_RX_QUEUE_SIZE 64

/**
 * RX queue entries older than this do not "exist"
 */
#define ZT_RX_QUEUE_EXPIRE 4000

/**
 * Length of secret key in bytes -- 256-bit -- do not change
 */
#define ZT_PEER_SECRET_KEY_LENGTH 32

/**
 * Minimum delay between timer task checks to prevent thrashing
 */
#define ZT_CORE_TIMER_TASK_GRANULARITY 500

/**
 * How often Topology::clean() and Network::clean() and similar are called, in ms
 */
#define ZT_HOUSEKEEPING_PERIOD 120000

/**
 * How long to remember peer records in RAM if they haven't been used
 */
#define ZT_PEER_IN_MEMORY_EXPIRATION 600000

/**
 * Delay between WHOIS retries in ms
 */
#define ZT_WHOIS_RETRY_DELAY 1000

/**
 * Maximum identity WHOIS retries (each attempt tries consulting a different peer)
 */
#define ZT_MAX_WHOIS_RETRIES 4

/**
 * Transmit queue entry timeout
 */
#define ZT_TRANSMIT_QUEUE_TIMEOUT (ZT_WHOIS_RETRY_DELAY * (ZT_MAX_WHOIS_RETRIES + 1))

/**
 * Receive queue entry timeout
 */
#define ZT_RECEIVE_QUEUE_TIMEOUT (ZT_WHOIS_RETRY_DELAY * (ZT_MAX_WHOIS_RETRIES + 1))

/**
 * Maximum latency to allow for OK(HELLO) before packet is discarded
 */
#define ZT_HELLO_MAX_ALLOWABLE_LATENCY 60000

/**
 * Maximum number of ZT hops allowed (this is not IP hops/TTL)
 *
 * The protocol allows up to 7, but we limit it to something smaller.
 */
#define ZT_RELAY_MAX_HOPS 3

/**
 * Maximum number of upstreams to use (far more than we should ever need)
 */
#define ZT_MAX_UPSTREAMS 64

/**
 * Expire time for multicast 'likes' and indirect multicast memberships in ms
 */
#define ZT_MULTICAST_LIKE_EXPIRE 600000

/**
 * Period for multicast LIKE announcements
 */
#define ZT_MULTICAST_ANNOUNCE_PERIOD 120000

/**
 * Delay between explicit MULTICAST_GATHER requests for a given multicast channel
 */
#define ZT_MULTICAST_EXPLICIT_GATHER_DELAY (ZT_MULTICAST_LIKE_EXPIRE / 10)

/**
 * Expiration for credentials presented for MULTICAST_LIKE or MULTICAST_GATHER (for non-network-members)
 */
#define ZT_MULTICAST_CREDENTIAL_EXPIRATON ZT_MULTICAST_LIKE_EXPIRE

/**
 * Timeout for outgoing multicasts
 *
 * This is how long we wait for explicit or implicit gather results.
 */
#define ZT_MULTICAST_TRANSMIT_TIMEOUT 5000

/**
 * Delay between checks of peer pings, etc., and also related housekeeping tasks
 */
#define ZT_PING_CHECK_INVERVAL 5000

/**
 * How frequently to send heartbeats over in-use paths
 */
#define ZT_PATH_HEARTBEAT_PERIOD 14000

/**
 * Paths are considered inactive if they have not received traffic in this long
 */
#define ZT_PATH_ALIVE_TIMEOUT 45000

/**
 * Minimum time between attempts to check dead paths to see if they can be re-awakened
 */
#define ZT_PATH_MIN_REACTIVATE_INTERVAL 2500

/**
 * Do not accept HELLOs over a given path more often than this
 */
#define ZT_PATH_HELLO_RATE_LIMIT 1000

/**
 * Delay between full-fledge pings of directly connected peers
 */
#define ZT_PEER_PING_PERIOD 60000

/**
 * Paths are considered expired if they have not sent us a real packet in this long
 */
#define ZT_PEER_PATH_EXPIRATION ((ZT_PEER_PING_PERIOD * 4) + 3000)

/**
 * Send a full HELLO every this often (ms)
 */
#define ZT_PEER_SEND_FULL_HELLO_EVERY (ZT_PEER_PING_PERIOD * 2)

/**
 * How often to retry expired paths that we're still remembering
 */
#define ZT_PEER_EXPIRED_PATH_TRIAL_PERIOD (ZT_PEER_PING_PERIOD * 10)

/**
 * Timeout for overall peer activity (measured from last receive)
 */
#define ZT_PEER_ACTIVITY_TIMEOUT 500000

/**
 * General rate limit timeout for multiple packet types (HELLO, etc.)
 */
#define ZT_PEER_GENERAL_INBOUND_RATE_LIMIT 500

/**
 * General limit for max RTT for requests over the network
 */
#define ZT_GENERAL_RTT_LIMIT 5000

/**
 * Delay between requests for updated network autoconf information
 *
 * Don't lengthen this as it affects things like QoS / uptime monitoring
 * via ZeroTier Central. This is the heartbeat, basically.
 */
#define ZT_NETWORK_AUTOCONF_DELAY 60000

/**
 * Minimum interval between attempts by relays to unite peers
 *
 * When a relay gets a packet destined for another peer, it sends both peers
 * a RENDEZVOUS message no more than this often. This instructs the peers
 * to attempt NAT-t and gives each the other's corresponding IP:port pair.
 */
#define ZT_MIN_UNITE_INTERVAL 30000

/**
 * How often should peers try memorized or statically defined paths?
 */
#define ZT_TRY_MEMORIZED_PATH_INTERVAL 30000

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
#define ZT_MAX_BRIDGE_SPAM 32

/**
 * Interval between direct path pushes in milliseconds
 */
#define ZT_DIRECT_PATH_PUSH_INTERVAL 120000

/**
 * Time horizon for push direct paths cutoff
 */
#define ZT_PUSH_DIRECT_PATHS_CUTOFF_TIME 30000

/**
 * Maximum number of direct path pushes within cutoff time
 *
 * This limits response to PUSH_DIRECT_PATHS to CUTOFF_LIMIT responses
 * per CUTOFF_TIME milliseconds per peer to prevent this from being
 * useful for DOS amplification attacks.
 */
#define ZT_PUSH_DIRECT_PATHS_CUTOFF_LIMIT 8

/**
 * Maximum number of paths per IP scope (e.g. global, link-local) and family (e.g. v4/v6)
 */
#define ZT_PUSH_DIRECT_PATHS_MAX_PER_SCOPE_AND_FAMILY 8

/**
 * Time horizon for VERB_NETWORK_CREDENTIALS cutoff
 */
#define ZT_PEER_CREDENTIALS_CUTOFF_TIME 60000

/**
 * Maximum number of VERB_NETWORK_CREDENTIALS within cutoff time
 */
#define ZT_PEER_CREDEITIALS_CUTOFF_LIMIT 15

/**
 * WHOIS rate limit (we allow these to be pretty fast)
 */
#define ZT_PEER_WHOIS_RATE_LIMIT 100

/**
 * General rate limit for other kinds of rate-limited packets (HELLO, credential request, etc.) both inbound and outbound
 */
#define ZT_PEER_GENERAL_RATE_LIMIT 1000

/**
 * Don't do expensive identity validation more often than this
 *
 * IPv4 and IPv6 address prefixes are hashed down to 14-bit (0-16383) integers
 * using the first 24 bits for IPv4 or the first 48 bits for IPv6. These are
 * then rate limited to one identity validation per this often milliseconds.
 */
#if (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__) || defined(_M_X64) || defined(_M_AMD64))
// AMD64 machines can do anywhere from one every 50ms to one every 10ms. This provides plenty of margin.
#define ZT_IDENTITY_VALIDATION_SOURCE_RATE_LIMIT 2000
#else
#if (defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || defined(_M_IX86) || defined(_X86_) || defined(__I86__))
// 32-bit Intel machines usually average about one every 100ms
#define ZT_IDENTITY_VALIDATION_SOURCE_RATE_LIMIT 5000
#else
// This provides a safe margin for ARM, MIPS, etc. that usually average one every 250-400ms
#define ZT_IDENTITY_VALIDATION_SOURCE_RATE_LIMIT 10000
#endif
#endif

/**
 * How long is a path or peer considered to have a trust relationship with us (for e.g. relay policy) since last trusted established packet?
 */
#define ZT_TRUST_EXPIRATION 600000

/**
 * Enable support for older network configurations from older (pre-1.1.6) controllers
 */
#define ZT_SUPPORT_OLD_STYLE_NETCONF 1

/**
 * Desired buffer size for UDP sockets (used in service and osdep but defined here)
 */
#if (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__))
#define ZT_UDP_DESIRED_BUF_SIZE 1048576
#else
#define ZT_UDP_DESIRED_BUF_SIZE 131072
#endif

/**
 * Desired / recommended min stack size for threads (used on some platforms to reset thread stack size)
 */
#define ZT_THREAD_MIN_STACK_SIZE 1048576

/* Ethernet frame types that might be relevant to us */
#define ZT_ETHERTYPE_IPV4 0x0800
#define ZT_ETHERTYPE_ARP 0x0806
#define ZT_ETHERTYPE_RARP 0x8035
#define ZT_ETHERTYPE_ATALK 0x809b
#define ZT_ETHERTYPE_AARP 0x80f3
#define ZT_ETHERTYPE_IPX_A 0x8137
#define ZT_ETHERTYPE_IPX_B 0x8138
#define ZT_ETHERTYPE_IPV6 0x86dd

#endif
