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

#ifndef ZT_CONSTANTS_HPP
#define ZT_CONSTANTS_HPP

#include "../include/ZeroTierCore.h"
#include "OS.hpp"

#if __has_include("version.h")
#include "version.h"
#else /* dummy values for use inside IDEs, etc. */
#define ZEROTIER_ONE_VERSION_MAJOR 255
#define ZEROTIER_ONE_VERSION_MINOR 255
#define ZEROTIER_ONE_VERSION_REVISION 255
#define ZEROTIER_ONE_VERSION_BUILD 255
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
 * Maximum DNS or URL name size for an Endpoint (set so that max marshaled endpoint size is 128 bytes)
 */
#define ZT_ENDPOINT_MAX_NAME_SIZE 124

/**
 * Size of an identity hash (SHA384)
 */
#define ZT_IDENTITY_HASH_SIZE 48

/**
 * Default virtual network MTU (not physical)
 */
#define ZT_DEFAULT_MTU 2800

/**
 * Maximum number of packet fragments we'll support (protocol limit: 16)
 */
#define ZT_MAX_PACKET_FRAGMENTS 12

/**
 * Size of RX queue in packets
 */
#define ZT_RX_QUEUE_SIZE 32

/**
 * Size of TX queue in packets
 */
#define ZT_TX_QUEUE_SIZE 32

/**
 * Length of peer shared secrets (256-bit, do not change)
 */
#define ZT_PEER_SECRET_KEY_LENGTH 32

/**
 * Minimum delay between timer task checks to prevent thrashing
 */
#define ZT_MIN_TIMER_TASK_INTERVAL 500

/**
 * Maximum delay between timer task checks (should be a fraction of smallest housekeeping interval)
 */
#define ZT_MAX_TIMER_TASK_INTERVAL 3000

/**
 * How often most internal cleanup and housekeeping tasks are performed
 */
#define ZT_HOUSEKEEPING_PERIOD 120000

/**
 * How often network housekeeping is performed
 *
 * Note that this affects how frequently we re-request network configurations
 * from network controllers if we haven't received one yet.
 */
#define ZT_NETWORK_HOUSEKEEPING_PERIOD 12000

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
#define ZT_RELAY_MAX_HOPS 4

/**
 * Expire time for multicast 'likes' and indirect multicast memberships in ms
 */
#define ZT_MULTICAST_LIKE_EXPIRE 600000

/**
 * Period for multicast LIKE re-announcements to connected nodes
 */
#define ZT_MULTICAST_ANNOUNCE_PERIOD 60000

/**
 * Packets are only used for QoS/ACK statistical sampling if their packet ID is divisible by
 * this integer. This is to provide a mechanism for both peers to agree on which packets need
 * special treatment without having to exchange information. Changing this value would be
 * a breaking change and would necessitate a protocol version upgrade. Since each incoming and
 * outgoing packet ID is checked against this value its evaluation is of the form:
 * (id & (divisor - 1)) == 0, thus the divisor must be a power of 2.
 *
 * This value is set at (16) so that given a normally-distributed RNG output we will sample
 * 1/16th (or ~6.25%) of packets.
 */
#define ZT_PATH_QOS_ACK_PROTOCOL_DIVISOR 0x10

/**
 * Time horizon for VERB_QOS_MEASUREMENT and VERB_ACK packet processing cutoff
 */
#define ZT_PATH_QOS_ACK_CUTOFF_TIME 30000

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
#define ZT_PATH_QOS_ACK_CUTOFF_LIMIT 128

/**
 * Path choice history window size. This is used to keep track of which paths were
 * previously selected so that we can maintain a target allocation over time.
 */
#define ZT_MULTIPATH_PROPORTION_WIN_SZ 128

/**
 * Interval used for rate-limiting the computation of path quality estimates.
 */
#define ZT_PATH_QUALITY_COMPUTE_INTERVAL 1000

/**
 * Number of samples to consider when computing real-time path statistics
 */
#define ZT_PATH_QUALITY_METRIC_REALTIME_CONSIDERATION_WIN_SZ 128

/**
 * Number of samples to consider when computing performing long-term path quality analysis.
 * By default this value is set to ZT_PATH_QUALITY_METRIC_REALTIME_CONSIDERATION_WIN_SZ but can
 * be set to any value greater than that to observe longer-term path quality behavior.
 */
#define ZT_PATH_QUALITY_METRIC_WIN_SZ ZT_PATH_QUALITY_METRIC_REALTIME_CONSIDERATION_WIN_SZ

/**
 * Maximum acceptable Packet Delay Variance (PDV) over a path
 */
#define ZT_PATH_MAX_PDV 1000

/**
 * Maximum acceptable time interval between expectation and receipt of at least one ACK over a path
 */
#define ZT_PATH_MAX_AGE 30000

/**
 * Maximum acceptable mean latency over a path
 */
#define ZT_PATH_MAX_MEAN_LATENCY 1000

/**
 * How much each factor contributes to the "stability" score of a path
 */
#define ZT_PATH_CONTRIB_PDV                    (1.0 / 3.0)
#define ZT_PATH_CONTRIB_LATENCY                (1.0 / 3.0)
#define ZT_PATH_CONTRIB_THROUGHPUT_DISTURBANCE (1.0 / 3.0)

/**
 * How much each factor contributes to the "quality" score of a path
 */
#define ZT_PATH_CONTRIB_STABILITY  (0.75 / 3.0)
#define ZT_PATH_CONTRIB_THROUGHPUT (1.50 / 3.0)
#define ZT_PATH_CONTRIB_SCOPE      (0.75 / 3.0)

/**
 * How often a QoS packet is sent
 */
#define ZT_PATH_QOS_INTERVAL 3000

/**
 * Min and max acceptable sizes for a VERB_QOS_MEASUREMENT packet
 */
#define ZT_PATH_MIN_QOS_PACKET_SZ 8 + 1
#define ZT_PATH_MAX_QOS_PACKET_SZ 1400

/**
 * How many ID:sojourn time pairs in a single QoS packet
 */
#define ZT_PATH_QOS_TABLE_SIZE ((ZT_PATH_MAX_QOS_PACKET_SZ * 8) / (64 + 16))

/**
 * Maximum number of outgoing packets we monitor for QoS information
 */
#define ZT_PATH_MAX_OUTSTANDING_QOS_RECORDS 128

/**
 * Timeout for QoS records
 */
#define ZT_PATH_QOS_TIMEOUT (ZT_PATH_QOS_INTERVAL * 2)

/**
 * How often the service tests the path throughput
 */
#define ZT_PATH_THROUGHPUT_MEASUREMENT_INTERVAL (ZT_PATH_ACK_INTERVAL * 8)

/**
 * Minimum amount of time between each ACK packet
 */
#define ZT_PATH_ACK_INTERVAL 1000

/**
 * How often an aggregate link statistics report is emitted into this tracing system
 */
#define ZT_PATH_AGGREGATE_STATS_REPORT_INTERVAL 60000

/**
 * How much an aggregate link's component paths can vary from their target allocation
 * before the link is considered to be in a state of imbalance.
 */
#define ZT_PATH_IMBALANCE_THRESHOLD 0.20

/**
 * Max allowable time spent in any queue
 */
#define ZT_QOS_TARGET 5 // ms

/**
 * Time period where the time spent in the queue by a packet should fall below
 * target at least once
 */
#define ZT_QOS_INTERVAL 100 // ms

/**
 * The number of bytes that each queue is allowed to send during each DRR cycle.
 * This approximates a single-byte-based fairness queuing scheme
 */
#define ZT_QOS_QUANTUM ZT_DEFAULT_MTU

/**
 * The maximum total number of packets that can be queued among all
 * active/inactive, old/new queues
 */
#define ZT_QOS_MAX_ENQUEUED_PACKETS 1024

/**
 * Number of QoS queues (buckets)
 */
#define ZT_QOS_NUM_BUCKETS 9

/**
 * All unspecified traffic is put in this bucket. Anything in a bucket with a smaller
 * value is de-prioritized. Anything in a bucket with a higher value is prioritized over
 * other traffic.
 */
#define ZT_QOS_DEFAULT_BUCKET 0

/**
 * Delay between full-fledge pings of directly connected peers
 *
 * See https://conferences.sigcomm.org/imc/2010/papers/p260.pdf for
 * some real world data on NAT UDP timeouts. From the paper: "the
 * lowest measured timeout when a binding has seen bidirectional
 * traffic is 54 sec." 30 seconds is faster than really necessary.
 */
#define ZT_PEER_PING_PERIOD 30000

/**
 * Delay between refreshes of locators via DNS or other methods
 */
#define ZT_DYNAMIC_ROOT_UPDATE_PERIOD 120000

/**
 * Timeout for overall peer activity (measured from last receive)
 */
#ifndef ZT_SDK
#define ZT_PEER_ACTIVITY_TIMEOUT 500000
#else
#define ZT_PEER_ACTIVITY_TIMEOUT 30000
#endif

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
 * Sanity limit on maximum bridge routes
 *
 * If the number of bridge routes exceeds this, we cull routes from the
 * bridges with the most MACs behind them until it doesn't. This is a
 * sanity limit to prevent memory-filling DOS attacks, nothing more. No
 * physical LAN has anywhere even close to this many nodes. Note that this
 * does not limit the size of ZT virtual LANs, only bridge routing.
 */
#define ZT_MAX_BRIDGE_ROUTES 16777216

/**
 * If there is no known L2 bridging route, spam to up to this many active bridges
 */
#define ZT_MAX_BRIDGE_SPAM 32

/**
 * Interval between direct path pushes in milliseconds if we don't have a path
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
#define ZT_PEER_GENERAL_RATE_LIMIT 500

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
 * Size of a buffer to store either a C25519 or an ECC P-384 signature
 *
 * This must be large enough to hold all signature types.
 */
#define ZT_SIGNATURE_BUFFER_SIZE 96

/**
 * Desired / recommended min stack size for threads (used on some platforms to reset thread stack size)
 */
#define ZT_THREAD_MIN_STACK_SIZE 1048576

// Internal cryptographic algorithm IDs (these match relevant identity types)
#define ZT_CRYPTO_ALG_C25519 0
#define ZT_CRYPTO_ALG_P384 1

// Exceptions thrown in core ZT code
#define ZT_EXCEPTION_OUT_OF_BOUNDS 100
#define ZT_EXCEPTION_OUT_OF_MEMORY 101
#define ZT_EXCEPTION_PRIVATE_KEY_REQUIRED 102
#define ZT_EXCEPTION_INVALID_ARGUMENT 103
#define ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_TYPE 200
#define ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW 201
#define ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_CRYPTOGRAPHIC_TOKEN 202
#define ZT_EXCEPTION_INVALID_SERIALIZED_DATA_BAD_ENCODING 203

/* Ethernet frame types that might be relevant to us */
#define ZT_ETHERTYPE_IPV4 0x0800
#define ZT_ETHERTYPE_ARP 0x0806
#define ZT_ETHERTYPE_IPV6 0x86dd

#endif
