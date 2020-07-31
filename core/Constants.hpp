/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_CONSTANTS_HPP
#define ZT_CONSTANTS_HPP

#include "zerotier.h"
#include "OS.hpp"

#include "version.h"

/**
 * Version bit packed into four 16-bit fields in a 64-bit unsigned integer.
 */
#define ZT_VERSION_PACKED (                      \
	((uint64_t)ZEROTIER_VERSION_MAJOR    << 48U) | \
	((uint64_t)ZEROTIER_VERSION_MINOR    << 32U) | \
	((uint64_t)ZEROTIER_VERSION_REVISION << 16U) | \
	((uint64_t)ZEROTIER_VERSION_BUILD)         )

/**
 * Length of a ZeroTier address in bytes
 */
#define ZT_ADDRESS_LENGTH 5

/**
 * Length of a ZeroTier address in digits
 */
#define ZT_ADDRESS_LENGTH_HEX 10

/**
 * Addresses beginning with this byte are reserved for the joy of in-band signaling
 */
#define ZT_ADDRESS_RESERVED_PREFIX 0xff

/**
 * Bit mask for addresses against a uint64_t
 */
#define ZT_ADDRESS_MASK 0xffffffffffULL

/**
 * Size of an identity fingerprint hash (SHA384) in bytes
 */
#define ZT_FINGERPRINT_HASH_SIZE 48

/**
 * Default virtual network MTU (not physical)
 */
#define ZT_DEFAULT_MTU 2800

/**
 * Maximum number of packet fragments we'll support (11 is the maximum that will fit in a Buf)
 */
#define ZT_MAX_PACKET_FRAGMENTS 11

/**
 * Anti-DOS limit on the maximum incoming fragments per path
 */
#define ZT_MAX_INCOMING_FRAGMENTS_PER_PATH 16

/**
 * Sanity limit on the maximum size of a network config object
 */
#define ZT_MAX_NETWORK_CONFIG_BYTES 131072

/**
 * Length of symmetric keys
 */
#define ZT_SYMMETRIC_KEY_SIZE 48

/**
 * Time limit for ephemeral keys: 30 minutes.
 */
#define ZT_SYMMETRIC_KEY_TTL 1800000

/**
 * Maximum number of messages per symmetric key.
 */
#define ZT_SYMMETRIC_KEY_TTL_MESSAGES 2147483648

/**
 * Normal delay between processBackgroundTasks calls.
 */
#define ZT_TIMER_TASK_INTERVAL 2000

/**
 * How often most internal cleanup and housekeeping tasks are performed
 */
#define ZT_HOUSEKEEPING_PERIOD 300000

/**
 * How often network housekeeping is performed
 *
 * Note that this affects how frequently we re-request network configurations
 * from network controllers if we haven't received one yet.
 */
#define ZT_NETWORK_HOUSEKEEPING_PERIOD 30000

/**
 * Delay between WHOIS retries in ms
 */
#define ZT_WHOIS_RETRY_DELAY 500

/**
 * Maximum number of ZT hops allowed (this is not IP hops/TTL)
 *
 * The protocol allows up to 7, but we limit it to something smaller.
 */
#define ZT_RELAY_MAX_HOPS 4

/**
 * Period between keepalives sent to paths if no other traffic has been sent.
 *
 * The average NAT timeout is 60-120s, but there exist NATs in the wild with timeouts
 * as short as 30s. Come in just under 30s and we should be fine.
 */
#define ZT_PATH_KEEPALIVE_PERIOD 28000

/**
 * Timeout for path alive-ness (measured from last receive)
 */
#define ZT_PATH_ALIVE_TIMEOUT ((ZT_PATH_KEEPALIVE_PERIOD * 2) + 5000)

/**
 * Maximum number of queued endpoints to try per "pulse."
 */
#define ZT_NAT_T_PORT_SCAN_MAX 16

/**
 * Minimum interval between attempts to reach a given physical endpoint
 */
#define ZT_PATH_MIN_TRY_INTERVAL ZT_PATH_KEEPALIVE_PERIOD

/**
 * Delay between calls to the pulse() method in Peer for each peer
 */
#define ZT_PEER_PULSE_INTERVAL 8000

/**
 * Interval between HELLOs to peers.
 */
#define ZT_PEER_HELLO_INTERVAL 120000

/**
 * Timeout for peers being alive
 */
#define ZT_PEER_ALIVE_TIMEOUT ((ZT_PEER_HELLO_INTERVAL * 2) + 5000)

/**
 * Global timeout for peers in milliseconds
 *
 * This is global as in "entire world," and this value is 30 days. In this
 * code the global timeout is used to determine when to ignore cached
 * peers and their identity<>address mappings.
 */
#define ZT_PEER_GLOBAL_TIMEOUT 2592000000LL

/**
 * Interval between sort/prioritize of paths for a peer
 */
#define ZT_PEER_PRIORITIZE_PATHS_INTERVAL 5000

/**
 * Number of previous endpoints to cache in peer records.
 */
#define ZT_PEER_ENDPOINT_CACHE_SIZE 8

/**
 * Delay between requests for updated network autoconf information
 *
 * Don't lengthen this as it affects things like QoS / uptime monitoring
 * via ZeroTier Central. This is the heartbeat, basically.
 */
#define ZT_NETWORK_AUTOCONF_DELAY 60000

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
 * WHOIS rate limit (we allow these to be pretty fast)
 */
#define ZT_PEER_WHOIS_RATE_LIMIT 100

/**
 * General rate limit for other kinds of rate-limited packets (HELLO, credential request, etc.) both inbound and outbound
 */
#define ZT_PEER_GENERAL_RATE_LIMIT 500

/**
 * Rate limit for responses to short probes to prevent amplification attacks
 */
#define ZT_PEER_PROBE_RESPONSE_RATE_LIMIT 5000

/**
 * Size of a buffer to store either a C25519 or an ECC P-384 signature
 *
 * This must be large enough to hold all signature types, which right now is
 * Curve25519 EDDSA and NIST P-384 ECDSA.
 */
#define ZT_SIGNATURE_BUFFER_SIZE 96

/* Ethernet frame types that might be relevant to us */
#define ZT_ETHERTYPE_IPV4 0x0800
#define ZT_ETHERTYPE_ARP 0x0806
#define ZT_ETHERTYPE_IPV6 0x86dd

#endif
