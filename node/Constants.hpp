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

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
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
#include <WinSock2.h>
#include <Windows.h>
#endif

#ifdef __NetBSD__
#ifndef RTF_MULTICAST
#define RTF_MULTICAST   0x20000000
#endif
#endif

// Define ZT_NO_TYPE_PUNNING to disable reckless casts on anything other than x86/x64.
#if (!(defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_AMD64) || defined(_M_X64) || defined(i386) || defined(__i386) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || defined(_M_IX86) || defined(__X86__) || defined(_X86_) || defined(__I86__) || defined(__INTEL__) || defined(__386)))
#ifndef ZT_NO_TYPE_PUNNING
#define ZT_NO_TYPE_PUNNING
#endif
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

#ifdef __WINDOWS__
#define ZT_PATH_SEPARATOR '\\'
#define ZT_PATH_SEPARATOR_S "\\"
#define ZT_EOL_S "\r\n"
#else
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

#ifdef __WINDOWS__
#define ZT_PACKED_STRUCT(D) __pragma(pack(push,1)) D __pragma(pack(pop))
#else
#define ZT_PACKED_STRUCT(D) D __attribute__((packed))
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
 * Default MTU used for Ethernet tap device
 */
#define ZT_DEFAULT_MTU 2800

/**
 * Maximum number of packet fragments we'll support (protocol max: 16)
 */
#define ZT_MAX_PACKET_FRAGMENTS 7

/**
 * Size of RX queue
 */
#define ZT_RX_QUEUE_SIZE 32

/**
 * Size of TX queue
 */
#define ZT_TX_QUEUE_SIZE 32

/**
 * Length of secret key in bytes -- 256-bit -- do not change
 */
#define ZT_PEER_SECRET_KEY_LENGTH 32

/**
 * Minimum delay between timer task checks to prevent thrashing
 */
#define ZT_CORE_TIMER_TASK_GRANULARITY 60

/**
 * How often Topology::clean() and Network::clean() and similar are called, in ms
 */
#define ZT_HOUSEKEEPING_PERIOD 60000

/**
 * Delay between WHOIS retries in ms
 */
#define ZT_WHOIS_RETRY_DELAY 500

/**
 * Transmit queue entry timeout
 */
#define ZT_TRANSMIT_QUEUE_TIMEOUT 5000

/**
 * Receive queue entry timeout
 */
#define ZT_RECEIVE_QUEUE_TIMEOUT 5000

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
 * Period for multicast LIKE announcements
 */
#define ZT_MULTICAST_ANNOUNCE_PERIOD 120000

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
 * Delay between checks of peer pings, etc., and also related housekeeping tasks
 */
#define ZT_PING_CHECK_INVERVAL 5000

/**
 * How often the local.conf file is checked for changes (service, should be moved there)
 */
#define ZT_LOCAL_CONF_FILE_CHECK_INTERVAL 10000

/**
 * How frequently to send heartbeats over in-use paths
 */
#define ZT_PATH_HEARTBEAT_PERIOD 14000

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
 * How often to retry expired paths that we're still remembering
 */
#define ZT_PEER_EXPIRED_PATH_TRIAL_PERIOD (ZT_PEER_PING_PERIOD * 10)

/**
 * Outgoing packets are only used for QoS/ACK statistical sampling if their
 * packet ID is divisible by this integer. This is to provide a mechanism for
 * both peers to agree on which packets need special treatment without having
 * to exchange information. Changing this value would be a breaking change and
 * would necessitate a protocol version upgrade. Since each incoming and
 * outgoing packet ID is checked against this value its evaluation is of the
 * form:
 *
 * (id & (divisor - 1)) == 0, thus the divisor must be a power of 2.
 *
 * This value is set at (16) so that given a normally-distributed RNG output
 * we will sample 1/16th (or ~6.25%) of packets.
 */
#define ZT_QOS_ACK_DIVISOR 0x2

/**
 * Time horizon for VERB_QOS_MEASUREMENT and VERB_ACK packet processing cutoff
 */
#define ZT_QOS_ACK_CUTOFF_TIME 30000

/**
 * Maximum number of VERB_QOS_MEASUREMENT and VERB_ACK packets allowed to be
 * processed within cutoff time. Separate totals are kept for each type but
 * the limit is the same for both.
 *
 * This limits how often this peer will compute statistical estimates
 * of various QoS measures from a VERB_QOS_MEASUREMENT or VERB_ACK packets to
 * CUTOFF_LIMIT times per CUTOFF_TIME milliseconds per peer to prevent
 * this from being useful for DOS amplification attacks.
 */
#define ZT_QOS_ACK_CUTOFF_LIMIT 128

/**
 * Minimum acceptable size for a VERB_QOS_MEASUREMENT packet
 */
#define ZT_QOS_MIN_PACKET_SIZE (8 + 1)

/**
 * Maximum acceptable size for a VERB_QOS_MEASUREMENT packet
 */
#define ZT_QOS_MAX_PACKET_SIZE 1400

/**
 * How many ID:sojourn time pairs are in a single QoS packet
 */
#define ZT_QOS_TABLE_SIZE ((ZT_QOS_MAX_PACKET_SIZE * 8) / (64 + 16))

/**
 * Maximum number of outgoing packets we monitor for QoS information
 */
#define ZT_QOS_MAX_OUTSTANDING_RECORDS (1024*16)

/**
 * Interval used for rate-limiting the computation of path quality estimates.
 */
#define ZT_QOS_COMPUTE_INTERVAL 1000

/**
 * Number of samples to consider when processing real-time path statistics
 */
#define ZT_QOS_SHORTTERM_SAMPLE_WIN_SIZE 32

/**
 * Max allowable time spent in any queue (in ms)
 */
#define ZT_AQM_TARGET 5

/**
 * Time period where the time spent in the queue by a packet should fall below.
 * target at least once. (in ms)
 */
#define ZT_AQM_INTERVAL 100

/**
 * The number of bytes that each queue is allowed to send during each DRR cycle.
 * This approximates a single-byte-based fairness queuing scheme.
 */
#define ZT_AQM_QUANTUM ZT_DEFAULT_MTU

/**
 * The maximum total number of packets that can be queued among all
 * active/inactive, old/new queues.
 */
#define ZT_AQM_MAX_ENQUEUED_PACKETS 1024

/**
 * Number of QoS queues (buckets)
 */
#define ZT_AQM_NUM_BUCKETS 9

/**
 * All unspecified traffic is put in this bucket. Anything in a bucket with a
 * smaller value is deprioritized. Anything in a bucket with a higher value is
 prioritized over other traffic.
 */
#define ZT_AQM_DEFAULT_BUCKET 0

/**
 * How long before we consider a path to be dead in the general sense. This is
 * used while searching for default or alternative paths to try in the absence
 * of direct guidance from the user or a selection policy.
 */
#define ZT_MULTIPATH_DEFAULT_FAILOVER_INTERVAL 10000

/**
 * How often flows are evaluated
 */
#define ZT_MULTIPATH_FLOW_CHECK_INTERVAL 10000

/**
 * How long before we consider a flow to be dead and remove it from the
 * policy's list.
 */
#define ZT_MULTIPATH_FLOW_EXPIRATION_INTERVAL 30000

/**
 * How often a flow's statistical counters are reset
 */
#define ZT_FLOW_STATS_RESET_INTERVAL ZT_MULTIPATH_FLOW_EXPIRATION_INTERVAL

/**
 * Maximum number of flows allowed before we start forcibly forgetting old ones
 */
#define ZT_FLOW_MAX_COUNT (1024*64)

/**
 * How often flows are rebalanced across slave interfaces (if at all)
 */
#define ZT_FLOW_MIN_REBALANCE_INTERVAL 5000

/**
 * How often flows are rebalanced across slave interfaces (if at all)
 */
#define ZT_FLOW_REBALANCE_INTERVAL 5000

/**
 * A defensive timer to prevent path quality metrics from being
 * processed too often.
 */
#define ZT_BOND_BACKGROUND_TASK_MIN_INTERVAL ZT_CORE_TIMER_TASK_GRANULARITY

/**
 * How often a bonding policy's background tasks are processed,
 * some need more frequent attention than others.
 */
#define ZT_MULTIPATH_ACTIVE_BACKUP_CHECK_INTERVAL ZT_CORE_TIMER_TASK_GRANULARITY

/**
 * Minimum amount of time (since a previous transition) before the active-backup bonding
 * policy is allowed to transition to a different slave. Only valid for active-backup.
 */
#define ZT_MULTIPATH_MIN_ACTIVE_BACKUP_AUTOFLOP_INTERVAL 10000

/**
 * How often a peer checks that incoming (and outgoing) traffic on a bonded link is
 * appropriately paired.
 */
#define ZT_PATH_NEGOTIATION_CHECK_INTERVAL 15000

/**
 * Time horizon for path negotiation paths cutoff
 */
#define ZT_PATH_NEGOTIATION_CUTOFF_TIME 60000

/**
 * Maximum number of path negotiations within cutoff time
 *
 * This limits response to PATH_NEGOTIATION to CUTOFF_LIMIT responses
 * per CUTOFF_TIME milliseconds per peer to prevent this from being
 * useful for DOS amplification attacks.
 */
#define ZT_PATH_NEGOTIATION_CUTOFF_LIMIT 8

/**
 * How many times a peer will attempt to petition another peer to synchronize its
 * traffic to the same path before giving up and surrendering to the other peer's preference.
 */
#define ZT_PATH_NEGOTIATION_TRY_COUNT 3

/**
 * How much greater the quality of a path should be before an
 * optimization procedure triggers a switch.
 */
#define ZT_MULTIPATH_ACTIVE_BACKUP_OPTIMIZE_MIN_THRESHOLD 0.10

/**
 * Artificially inflates the failover score for paths which meet
 * certain non-performance-related policy ranking criteria.
 */
#define ZT_MULTIPATH_FAILOVER_HANDICAP_PREFERRED 500
#define ZT_MULTIPATH_FAILOVER_HANDICAP_PRIMARY 1000
#define ZT_MULTIPATH_FAILOVER_HANDICAP_NEGOTIATED 5000

/**
 * An indicator that no flow is to be associated with the given packet
 */
#define ZT_QOS_NO_FLOW -1

/**
 * Timeout for overall peer activity (measured from last receive)
 */
#ifndef ZT_SDK
#define ZT_PEER_ACTIVITY_TIMEOUT 500000
#else
#define ZT_PEER_ACTIVITY_TIMEOUT 30000
#endif

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
 * If there is no known L2 bridging route, spam to up to this many active bridges
 */
#define ZT_MAX_BRIDGE_SPAM 32

/**
 * Interval between direct path pushes in milliseconds
 */
#define ZT_DIRECT_PATH_PUSH_INTERVAL 15000

/**
 * Interval between direct path pushes in milliseconds if we already have a path
 */
#define ZT_DIRECT_PATH_PUSH_INTERVAL_HAVEPATH 120000

/**
 * Time horizon for push direct paths cutoff
 */
#define ZT_PUSH_DIRECT_PATHS_CUTOFF_TIME 30000

/**
 * Drainage constants for VERB_ECHO rate-limiters
 */
#define ZT_ECHO_CUTOFF_LIMIT ((1000 / ZT_CORE_TIMER_TASK_GRANULARITY) * ZT_MAX_PEER_NETWORK_PATHS)
#define ZT_ECHO_DRAINAGE_DIVISOR (1000 / ZT_ECHO_CUTOFF_LIMIT)

/**
 * Drainage constants for VERB_QOS rate-limiters
 */
#define ZT_QOS_CUTOFF_LIMIT ((1000 / ZT_CORE_TIMER_TASK_GRANULARITY) * ZT_MAX_PEER_NETWORK_PATHS)
#define ZT_QOS_DRAINAGE_DIVISOR (1000 / ZT_QOS_CUTOFF_LIMIT)

/**
 * Drainage constants for VERB_ACK rate-limiters
 */
#define ZT_ACK_CUTOFF_LIMIT 128
#define ZT_ACK_DRAINAGE_DIVISOR (1000 / ZT_ACK_CUTOFF_LIMIT)

#define ZT_MULTIPATH_DEFAULT_REFRCTORY_PERIOD 8000
#define ZT_MULTIPATH_MAX_REFRACTORY_PERIOD 600000

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

// Exceptions thrown in core ZT code
#define ZT_EXCEPTION_OUT_OF_BOUNDS 100
#define ZT_EXCEPTION_OUT_OF_MEMORY 101
#define ZT_EXCEPTION_PRIVATE_KEY_REQUIRED 102
#define ZT_EXCEPTION_INVALID_ARGUMENT 103
#define ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_TYPE 200
#define ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW 201
#define ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_CRYPTOGRAPHIC_TOKEN 202
#define ZT_EXCEPTION_INVALID_SERIALIZED_DATA_BAD_ENCODING 203

#endif
