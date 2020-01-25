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
 * Maximum DNS or URL name size for an Endpoint (set so that max marshaled endpoint size is 64 bytes)
 */
#define ZT_ENDPOINT_MAX_NAME_SIZE 61

/**
 * Size of an identity hash (SHA384)
 */
#define ZT_IDENTITY_HASH_SIZE 48

/**
 * Default virtual network MTU (not physical)
 */
#define ZT_DEFAULT_MTU 2800

/**
 * Maximum number of packet fragments we'll support (11 is the maximum that will fit in a Buf)
 */
#define ZT_MAX_PACKET_FRAGMENTS 11

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
#define ZT_MAX_TIMER_TASK_INTERVAL 5000

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
#define ZT_NETWORK_HOUSEKEEPING_PERIOD 15000

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
 * Period between keepalives sent to paths if no other traffic has been sent
 */
#define ZT_PATH_KEEPALIVE_PERIOD 20000

/**
 * Timeout for path alive-ness (measured from last receive)
 */
#define ZT_PATH_ALIVE_TIMEOUT ((ZT_PATH_KEEPALIVE_PERIOD * 2) + 5000)

/**
 * Timeout for path active-ness (measured from last receive)
 */
#define ZT_PATH_ACTIVITY_TIMEOUT (ZT_PATH_KEEPALIVE_PERIOD + 5000)

/**
 * Delay between full HELLO messages between peers
 */
#define ZT_PEER_PING_PERIOD 60000

/**
 * Timeout for peer alive-ness (measured from last receive)
 */
#define ZT_PEER_ALIVE_TIMEOUT ((ZT_PEER_PING_PERIOD * 2) + 5000)

/**
 * Timeout for peer active-ness (measured from last receive)
 */
#define ZT_PEER_ACTIVITY_TIMEOUT (ZT_PEER_PING_PERIOD + 5000)

/**
 * Maximum interval between sort/prioritize of paths for a peer
 */
#define ZT_PEER_PRIORITIZE_PATHS_INTERVAL 5000

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
 * If there is no known L2 bridging route, spam to up to this many active bridges
 */
#define ZT_MAX_BRIDGE_SPAM 32

/**
 * Interval between direct path pushes in milliseconds if we don't have a path
 */
#define ZT_DIRECT_PATH_PUSH_INTERVAL 30000

/**
 * Interval between direct path pushes in milliseconds if we already have a path
 */
#define ZT_DIRECT_PATH_PUSH_INTERVAL_HAVEPATH 120000

/**
 * Maximum number of paths per IP scope (e.g. global, link-local) and family (e.g. v4/v6)
 */
#define ZT_PUSH_DIRECT_PATHS_MAX_PER_SCOPE_AND_FAMILY 4

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

// Internal cryptographic algorithm IDs (these match relevant identity types)
#define ZT_CRYPTO_ALG_C25519 0
#define ZT_CRYPTO_ALG_P384 1

// Exceptions thrown in core ZT code
#define ZT_EXCEPTION_OUT_OF_BOUNDS 100
#define ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_TYPE 200
#define ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW 201
#define ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_CRYPTOGRAPHIC_TOKEN 202
#define ZT_EXCEPTION_INVALID_SERIALIZED_DATA_BAD_ENCODING 203

/* Ethernet frame types that might be relevant to us */
#define ZT_ETHERTYPE_IPV4 0x0800
#define ZT_ETHERTYPE_ARP 0x0806
#define ZT_ETHERTYPE_IPV6 0x86dd

#endif
