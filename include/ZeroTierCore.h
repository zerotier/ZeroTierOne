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

/*
 * This defines the external C API for ZeroTier's core network virtualization
 * engine.
 */

#ifndef ZT_ZEROTIER_API_H
#define ZT_ZEROTIER_API_H

/* ZT_PACKED_STRUCT encloses structs whose contents should be bit-packed.
 * Nearly all compilers support this. These macros detect the compiler and
 * define it correctly for gcc/icc/clang or MSC. */
#ifndef ZT_PACKED_STRUCT
#if defined(__GCC__) || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1) || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2) || defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4) || defined(__INTEL_COMPILER) || defined(__clang__)
#define ZT_PACKED_STRUCT(D) D __attribute__((packed))
#define ZT_PACKED_STRUCT_START
#define ZT_PACKED_STRUCT_END __attribute__((packed))
#endif
#ifdef _MSC_VER
#define ZT_PACKED_STRUCT(D) __pragma(pack(push,1)) D __pragma(pack(pop))
#define ZT_PACKED_STRUCT_START __pragma(pack(push,1))
#define ZT_PACKED_STRUCT_END __pragma(pack(pop))
#endif
#endif
#ifndef ZT_PACKED_STRUCT
#error Missing a macro to define ZT_PACKED_STRUCT for your compiler.
#endif

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

#ifndef ZT_SDK_API
#define ZT_SDK_API
#endif

/****************************************************************************/

/**
 * Default UDP port for devices running a ZeroTier endpoint
 *
 * NOTE: as of V2 this has changed to 893 since many NATs (even symmetric)
 * treat privileged ports in a special way. The old default was 9993.
 */
#define ZT_DEFAULT_PORT 893

/**
 * Minimum MTU, which is the minimum allowed by IPv6 and several specs
 */
#define ZT_MIN_MTU 1280

/**
 * Maximum MTU for ZeroTier virtual networks
 */
#define ZT_MAX_MTU 10000

/**
 * Minimum UDP payload size allowed
 */
#define ZT_MIN_PHYSMTU 1400

/**
 * Default UDP payload size (physical path MTU) not including UDP and IP overhead
 *
 * This is small enough for PPPoE and for Google Cloud's bizarrely tiny MTUs.
 * A 2800 byte payload still fits into two packets, so this should not impact
 * real world throughput at all vs the previous default of 1444.
 */
#define ZT_DEFAULT_PHYSMTU 1432

/**
 * Maximum physical UDP payload
 */
#define ZT_MAX_PHYSPAYLOAD 10100

/**
 * Headroom for max physical MTU
 */
#define ZT_MAX_HEADROOM 224

/**
 * Maximum payload MTU for UDP packets
 */
#define ZT_MAX_PHYSMTU (ZT_MAX_PHYSPAYLOAD + ZT_MAX_HEADROOM)

/**
 * Maximum length of network short name
 */
#define ZT_MAX_NETWORK_SHORT_NAME_LENGTH 127

/**
 * Maximum number of pushed routes on a network (via ZT in-band mechanisms)
 */
#define ZT_MAX_NETWORK_ROUTES 64

/**
 * Maximum number of statically assigned IP addresses (via ZT in-band mechanisms)
 */
#define ZT_MAX_ZT_ASSIGNED_ADDRESSES 32

/**
 * Maximum number of "specialists" on a network -- bridges, etc.
 *
 * A specialist is a node tagged with some special role like acting as
 * a promiscuous bridge, open relay, administrator, etc.
 */
#define ZT_MAX_NETWORK_SPECIALISTS 256

/**
 * Rules engine revision ID, which specifies rules engine capabilities
 */
#define ZT_RULES_ENGINE_REVISION 1

/**
 * Maximum number of base (non-capability) network rules
 */
#define ZT_MAX_NETWORK_RULES 1024

/**
 * Maximum number of capabilities per network per member
 */
#define ZT_MAX_NETWORK_CAPABILITIES 128

/**
 * Maximum number of tags per network per member
 */
#define ZT_MAX_NETWORK_TAGS 128

/**
 * Maximum number of direct network paths to a given peer
 *
 * Note that dual-stack configs may end up resulting in both IPv6 and IPv4
 * paths existing. This gives enough headroom for multipath configs with
 * dual stacks across the board.
 */
#define ZT_MAX_PEER_NETWORK_PATHS 16

/**
 * Maximum number of path configurations that can be set
 */
#define ZT_MAX_CONFIGURABLE_PATHS 32

/**
 * Maximum number of rules per capability object
 *
 * Capabilities normally contain only a few rules. The rules in a capability
 * should be short and to the point.
 */
#define ZT_MAX_CAPABILITY_RULES 64

/**
 * Maximum number of certificates of ownership to assign to a single network member
 *
 * Network members can have more than four IPs, etc., but right now there
 * is a protocol limit on how many COOs can be assigned. If your config needs
 * more than four authenticated IPs per node you may have personal problems.
 */
#define ZT_MAX_CERTIFICATES_OF_OWNERSHIP 4

/**
 * Global maximum length for capability chain of custody (including initial issue)
 */
#define ZT_MAX_CAPABILITY_CUSTODY_CHAIN_LENGTH 7

/* Rule specification contants **********************************************/

/**
 * Packet characteristics flag: packet direction, 1 if inbound 0 if outbound
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_INBOUND 0x8000000000000000ULL

/**
 * Packet characteristics flag: multicast or broadcast destination MAC
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_MULTICAST 0x4000000000000000ULL

/**
 * Packet characteristics flag: broadcast destination MAC
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_BROADCAST 0x2000000000000000ULL

/**
 * Packet characteristics flag: sending IP address has a certificate of ownership
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_SENDER_IP_AUTHENTICATED 0x1000000000000000ULL

/**
 * Packet characteristics flag: sending MAC address has a certificate of ownership
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_SENDER_MAC_AUTHENTICATED 0x0800000000000000ULL

/**
 * Packet characteristics flag: TCP left-most reserved bit
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_TCP_RESERVED_0 0x0000000000000800ULL

/**
 * Packet characteristics flag: TCP middle reserved bit
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_TCP_RESERVED_1 0x0000000000000400ULL

/**
 * Packet characteristics flag: TCP right-most reserved bit
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_TCP_RESERVED_2 0x0000000000000200ULL

/**
 * Packet characteristics flag: TCP NS flag
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_TCP_NS 0x0000000000000100ULL

/**
 * Packet characteristics flag: TCP CWR flag
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_TCP_CWR 0x0000000000000080ULL

/**
 * Packet characteristics flag: TCP ECE flag
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_TCP_ECE 0x0000000000000040ULL

/**
 * Packet characteristics flag: TCP URG flag
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_TCP_URG 0x0000000000000020ULL

/**
 * Packet characteristics flag: TCP ACK flag
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_TCP_ACK 0x0000000000000010ULL

/**
 * Packet characteristics flag: TCP PSH flag
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_TCP_PSH 0x0000000000000008ULL

/**
 * Packet characteristics flag: TCP RST flag
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_TCP_RST 0x0000000000000004ULL

/**
 * Packet characteristics flag: TCP SYN flag
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_TCP_SYN 0x0000000000000002ULL

/**
 * Packet characteristics flag: TCP FIN flag
 */
#define ZT_RULE_PACKET_CHARACTERISTICS_TCP_FIN 0x0000000000000001ULL

/****************************************************************************/

/**
 * Credential type IDs
 *
 * These are mostly used internally but are declared here so they can be used
 * in trace messages.
 */
enum ZT_CredentialType
{
	ZT_CREDENTIAL_TYPE_NULL = 0,
	ZT_CREDENTIAL_TYPE_COM = 1,
	ZT_CREDENTIAL_TYPE_CAPABILITY = 2,
	ZT_CREDENTIAL_TYPE_TAG = 3,
	ZT_CREDENTIAL_TYPE_COO = 4,
	ZT_CREDENTIAL_TYPE_REVOCATION = 6
};

/* Trace events are sent and received as packed structures of a fixed size.
 * Normally we don't use this form of brittle encoding but in this case the
 * performance benefit is non-trivial as events are generated in critical
 * areas of the code.
 *
 * NOTE: all integer fields larger than one byte are stored in big-endian
 * "network" byte order in these structures. */

/**
 * Flag indicating that VL1 tracing should be generated
 */
#define ZT_TRACE_FLAG_VL1           0x01

/**
 * Flag indicating that VL2 (virtual network) tracing should be generated
 */
#define ZT_TRACE_FLAG_VL2           0x02

/**
 * Flag indicating that VL2 network filter tracing should be generated (separate because this can be very verbose)
 */
#define ZT_TRACE_FLAG_VL2_FILTER    0x04

/**
 * Flag indicating that VL2 multicast propagation should be reported
 */
#define ZT_TRACE_FLAG_VL2_MULTICAST 0x08

/**
 * Trace event types
 *
 * All trace event structures start with a size and type.
 */
enum ZT_TraceEventType
{
	/* VL1 events related to the peer-to-peer layer */
	ZT_TRACE_VL1_RESETTING_PATHS_IN_SCOPE = 1,
	ZT_TRACE_VL1_TRYING_NEW_PATH = 2,
	ZT_TRACE_VL1_LEARNED_NEW_PATH = 3,
	ZT_TRACE_VL1_INCOMING_PACKET_DROPPED = 4,

	/* VL2 events relate to virtual networks, packet filtering, and authentication */
	ZT_TRACE_VL2_OUTGOING_FRAME_DROPPED = 100,
	ZT_TRACE_VL2_INCOMING_FRAME_DROPPED = 101,
	ZT_TRACE_VL2_NETWORK_CONFIG_REQUESTED = 102,
	ZT_TRACE_VL2_NETWORK_FILTER = 103
};

/**
 * Trace VL1 packet drop reasons
 */
enum ZT_TracePacketDropReason
{
	ZT_TRACE_PACKET_DROP_REASON_UNSPECIFIED = 0,
	ZT_TRACE_PACKET_DROP_REASON_PEER_TOO_OLD = 1,
	ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET = 2,
	ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED = 3,
	ZT_TRACE_PACKET_DROP_REASON_RATE_LIMIT_EXCEEDED = 4,
	ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT = 5,
	ZT_TRACE_PACKET_DROP_REASON_INVALID_COMPRESSED_DATA = 6,
	ZT_TRACE_PACKET_DROP_REASON_UNRECOGNIZED_VERB = 7
};

/**
 * Trace VL2 frame drop reasons
 */
enum ZT_TraceFrameDropReason
{
	ZT_TRACE_FRAME_DROP_REASON_UNSPECIFIED = 0,
	ZT_TRACE_FRAME_DROP_REASON_BRIDGING_NOT_ALLOWED_REMOTE = 1,
	ZT_TRACE_FRAME_DROP_REASON_BRIDGING_NOT_ALLOWED_LOCAL = 2,
	ZT_TRACE_FRAME_DROP_REASON_MULTICAST_DISABLED = 3,
	ZT_TRACE_FRAME_DROP_REASON_BROADCAST_DISABLED = 4,
	ZT_TRACE_FRAME_DROP_REASON_FILTER_BLOCKED = 5,
	ZT_TRACE_FRAME_DROP_REASON_FILTER_BLOCKED_AT_BRIDGE_REPLICATION = 6,
	ZT_TRACE_FRAME_DROP_REASON_PERMISSION_DENIED = 7
};

/**
 * Address types for ZT_TraceEventPathAddress
 *
 * These are currently the same as the types in Endpoint.hpp and should remain so
 * if possible for consistency. Not all of these are used (yet?) but they are defined
 * for possible future use and the structure is sized to support them.
 */
enum ZT_TraceEventPathAddressType
{
	ZT_TRACE_EVENT_PATH_TYPE_NIL =          0, /* none/empty */
	ZT_TRACE_EVENT_PATH_TYPE_INETADDR_V4 =  1, /* 4-byte IPv4 */
	ZT_TRACE_EVENT_PATH_TYPE_INETADDR_V6 =  2, /* 16-byte IPv6 */
	ZT_TRACE_EVENT_PATH_TYPE_DNSNAME =      3, /* C string */
	ZT_TRACE_EVENT_PATH_TYPE_ZEROTIER =     4, /* 5-byte ZeroTier + 48-byte identity hash */
	ZT_TRACE_EVENT_PATH_TYPE_URL =          5, /* C string */
	ZT_TRACE_EVENT_PATH_TYPE_ETHERNET =     6  /* 6-byte Ethernet */
};

/**
 * Reasons for trying new paths
 */
enum ZT_TraceTryingNewPathReason
{
	ZT_TRACE_TRYING_NEW_PATH_REASON_PACKET_RECEIVED_FROM_UNKNOWN_PATH = 1,
	ZT_TRACE_TRYING_NEW_PATH_REASON_RECEIVED_PUSH_DIRECT_PATHS = 2,
	ZT_TRACE_TRYING_NEW_PATH_REASON_RENDEZVOUS = 3,
	ZT_TRACE_TRYING_NEW_PATH_REASON_BOOTSTRAP_ADDRESS = 4,
	ZT_TRACE_TRYING_NEW_PATH_REASON_EXPLICITLY_SUGGESTED_ADDRESS = 5
};

/**
 * Reasons for credential rejection
 */
enum ZT_TraceCredentialRejectionReason
{
	ZT_TRACE_CREDENTIAL_REJECTION_REASON_SIGNATURE_VERIFICATION_FAILED = 1,
	ZT_TRACE_CREDENTIAL_REJECTION_REASON_REVOKED = 2,
	ZT_TRACE_CREDENTIAL_REJECTION_REASON_OLDER_THAN_LATEST = 3,
	ZT_TRACE_CREDENTIAL_REJECTION_REASON_INVALID = 4
};

/**
 * Physical path address from a trace event
 *
 * This is a special packed address format that roughly mirrors Endpoint in the core
 * and is designed to support both present and future address types.
 */
ZT_PACKED_STRUCT(struct ZT_TraceEventPathAddress
{
	uint8_t type;        /* ZT_TraceEventPathAddressType */
	uint8_t address[63]; /* Type-dependent address: 4-byte IPv4, 16-byte IPV6, etc. */
	uint16_t port;       /* UDP/TCP port for address types for which this is meaningful */
});

/**
 * Header for all trace events
 *
 * All packet types begin with these fields in this order.
 */
ZT_PACKED_STRUCT(struct ZT_TraceEvent
{
	uint16_t evSize;     /* sizeof(ZT_TraceEvent_XX structure) (inclusive size in bytes) */
	uint16_t evType;     /* ZT_TraceEventType */
});

/* Temporary macros to make it easier to declare all ZT_TraceEvent's sub-types */
#define _ZT_TRACE_EVENT_STRUCT_START(e) ZT_PACKED_STRUCT_START struct ZT_TraceEvent_##e { \
	uint16_t evSize; \
	uint16_t evType;
#define _ZT_TRACE_EVENT_STRUCT_END() } ZT_PACKED_STRUCT_END;

/**
 * Node is resetting all paths in a given address scope
 *
 * This happens when the node detects and external surface IP addressing change
 * via a trusted (usually root) peer. It's used to renegotiate links when nodes
 * move around on the network.
 */
_ZT_TRACE_EVENT_STRUCT_START(VL1_RESETTING_PATHS_IN_SCOPE)
	uint64_t reporter;                               /* ZeroTier address that triggered the reset */
	uint8_t reporterIdentityHash[48];                /* full identity hash of triggering node's identity */
	struct ZT_TraceEventPathAddress from;            /* physical origin of triggering packet */
	struct ZT_TraceEventPathAddress oldExternal;     /* previous detected external address */
	struct ZT_TraceEventPathAddress newExternal;     /* new detected external address */
	uint8_t scope;                                   /* IP scope being reset */
_ZT_TRACE_EVENT_STRUCT_END()

/**
 * Node is trying a new path
 *
 * Paths are tried in response to PUSH_DIRECT_PATHS, RENDEZVOUS, and other places
 * we might hear of them. A node tries a path by sending a trial message to it.
 */
_ZT_TRACE_EVENT_STRUCT_START(VL1_TRYING_NEW_PATH)
	uint64_t address;                                /* short address of node we're trying to reach */
	uint8_t identityHash[48];                        /* identity hash of node we're trying to reach */
	struct ZT_TraceEventPathAddress physicalAddress; /* physical address being tried */
	struct ZT_TraceEventPathAddress triggerAddress;  /* physical origin of triggering packet */
	uint64_t triggeringPacketId;                     /* packet ID of triggering packet */
	uint8_t triggeringPacketVerb;                    /* packet verb of triggering packet */
	uint64_t triggeredByAddress;                     /* short address of node triggering attempt */
	uint8_t triggeredByIdentityHash[48];             /* full identity hash of node triggering attempt */
	uint8_t reason;                                  /* ZT_TraceTryingNewPathReason */
_ZT_TRACE_EVENT_STRUCT_END()

/**
 * Node has learned a new path to another node
 */
_ZT_TRACE_EVENT_STRUCT_START(VL1_LEARNED_NEW_PATH)
	uint64_t packetId;                               /* packet ID of confirming packet */
	uint64_t address;                                /* short address of learned peer */
	uint8_t identityHash[48];                        /* full identity hash of learned peer */
	struct ZT_TraceEventPathAddress physicalAddress; /* physical address learned */
	struct ZT_TraceEventPathAddress replaced;        /* if non-empty, an older address that was replaced */
_ZT_TRACE_EVENT_STRUCT_END()

/**
 * An incoming packet was dropped at the VL1 level
 *
 * This indicates a packet that passed MAC check but was dropped for some other
 * reason such as rate limits, being malformed, etc.
 */
_ZT_TRACE_EVENT_STRUCT_START(VL1_INCOMING_PACKET_DROPPED)
	uint64_t packetId;                               /* packet ID of failed packet */
	uint64_t networkId;                              /* VL2 network ID or 0 if unrelated to a network or unknown */
	uint64_t address;                                /* short address that sent packet */
	uint8_t identityHash[48];                        /* full identity hash of sending node */
	struct ZT_TraceEventPathAddress physicalAddress; /* physical origin address of packet */
	uint8_t hops;                                    /* hop count of packet */
	uint8_t verb;                                    /* packet verb */
	uint8_t reason;                                  /* ZT_TracePacketDropReason */
_ZT_TRACE_EVENT_STRUCT_END()

/**
 * Node declined to send a packet read from a local network port/tap
 */
_ZT_TRACE_EVENT_STRUCT_START(VL2_OUTGOING_FRAME_DROPPED)
	uint64_t networkId;                              /* network ID */
	uint64_t sourceMac;                              /* source MAC address */
	uint64_t destMac;                                /* destination MAC address */
	uint16_t etherType;                              /* Ethernet type of frame */
	uint16_t frameLength;                            /* length of dropped frame */
	uint8_t frameHead[64];                           /* first up to 64 bytes of dropped frame */
	uint8_t reason;                                  /* ZT_TraceFrameDropReason */
_ZT_TRACE_EVENT_STRUCT_END()

/**
 * An incoming frame was dropped
 */
_ZT_TRACE_EVENT_STRUCT_START(VL2_INCOMING_FRAME_DROPPED)
	uint64_t packetId;                               /* VL1 packet ID */
	uint64_t networkId;                              /* VL2 network ID */
	uint64_t sourceMac;                              /* 48-bit source MAC */
	uint64_t destMac;                                /* 48-bit destination MAC */
	uint64_t address;                                /* short address of sending peer */
	struct ZT_TraceEventPathAddress physicalAddress; /* physical source address of packet */
	uint8_t hops;                                    /* hop count of packet */
	uint16_t frameLength;                            /* length of frame in bytes */
	uint8_t frameHead[64];                           /* first up to 64 bytes of dropped frame */
	uint8_t verb;                                    /* packet verb indicating how frame was sent */
	uint8_t credentialRequestSent;                   /* if non-zero a request for credentials was sent */
	uint8_t reason;                                  /* ZT_TraceFrameDropReason */
_ZT_TRACE_EVENT_STRUCT_END()

/**
 * Node is requesting a new network config and certificate from a network controller
 */
_ZT_TRACE_EVENT_STRUCT_START(VL2_NETWORK_CONFIG_REQUESTED)
	uint64_t networkId;                              /* VL2 network ID */
_ZT_TRACE_EVENT_STRUCT_END()

/**
 * Network filter trace results
 *
 * These are generated when filter tracing is enabled to allow filters to be debugged.
 * Format for rule set logs is documented elsewhere.
 */
_ZT_TRACE_EVENT_STRUCT_START(VL2_NETWORK_FILTER)
	uint64_t networkId;                              /* VL2 network ID */
	uint8_t primaryRuleSetLog[512];                  /* primary rule set log */
	uint8_t matchingCapabilityRuleSetLog[512];       /* capability rule set log (if any) */
	uint32_t matchingCapabilityId;                   /* capability ID or 0 if none */
	int64_t matchingCapabilityTimestamp;             /* capability timestamp or 0 if none */
	uint64_t source;                                 /* source ZeroTier address */
	uint64_t dest;                                   /* destination ZeroTier address */
	uint64_t sourceMac;                              /* packet source MAC */
	uint64_t destMac;                                /* packet destination MAC */
	uint16_t frameLength;                            /* length of filtered frame */
	uint8_t frameHead[64];                           /* first up to 64 bytes of filtered frame */
	uint16_t etherType;                              /* frame Ethernet type */
	uint16_t vlanId;                                 /* frame VLAN ID (currently unused, always 0) */
	uint8_t noTee;                                   /* if true noTee flag was set in filter */
	uint8_t inbound;                                 /* direction: 1 for inbound, 0 for outbound */
	int8_t accept;                                   /* 0: drop, 1: accept, 2: "super-accept" */
_ZT_TRACE_EVENT_STRUCT_END()

/**
 * An incoming credential from a peer was rejected
 */
_ZT_TRACE_EVENT_STRUCT_START(VL2_CREDENTIAL_REJECTED)
	uint64_t networkId;                              /* VL2 network ID */
	uint64_t address;                                /* short address of sender */
	uint32_t credentialId;                           /* credential ID */
	int64_t credentialTimestamp;                     /* credential timestamp */
	uint8_t credentialType;                          /* credential type */
	uint8_t reason;                                  /* ZT_TraceCredentialRejectionReason */
_ZT_TRACE_EVENT_STRUCT_END()

#undef _ZT_TRACE_EVENT_STRUCT_START
#undef _ZT_TRACE_EVENT_STRUCT_END

/****************************************************************************/

/**
 * Function return code: OK (0) or error results
 *
 * Use ZT_ResultCode_isFatal() to check for a fatal error. If a fatal error
 * occurs, the node should be considered to not be working correctly. These
 * indicate serious problems like an inaccessible data store or a compile
 * problem.
 */
enum ZT_ResultCode
{
	/**
	 * Operation completed normally
	 */
	ZT_RESULT_OK = 0,

	/**
	 * Call produced no error but no action was taken
	 */
	ZT_RESULT_OK_IGNORED = 1,

	// Fatal errors (>100, <1000)

	/**
	 * Ran out of memory
	 */
	ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY = 100,

	/**
	 * Data store is not writable or has failed
	 */
	ZT_RESULT_FATAL_ERROR_DATA_STORE_FAILED = 101,

	/**
	 * Internal error (e.g. unexpected exception indicating bug or build problem)
	 */
	ZT_RESULT_FATAL_ERROR_INTERNAL = 102,

	// Non-fatal errors (>1000)

	/**
	 * Network ID not valid
	 */
	ZT_RESULT_ERROR_NETWORK_NOT_FOUND = 1000,

	/**
	 * The requested operation is not supported on this version or build
	 */
	ZT_RESULT_ERROR_UNSUPPORTED_OPERATION = 1001,

	/**
	 * The requested operation was given a bad parameter or was called in an invalid state
	 */
	ZT_RESULT_ERROR_BAD_PARAMETER = 1002
};

/**
 * Macro to check for a fatal error result code
 *
 * @param x Result code
 * @return True if result code indicates a fatal error
 */
#define ZT_ResultCode_isFatal(x) ((((int)(x)) >= 100)&&(((int)(x)) < 1000))

/**
 * Status codes sent to status update callback when things happen
 */
enum ZT_Event
{
	/**
	 * Node has been initialized
	 *
	 * This is the first event generated, and is always sent. It may occur
	 * before Node's constructor returns.
	 *
	 * Meta-data: none
	 */
	ZT_EVENT_UP = 0,

	/**
	 * Node appears offline
	 *
	 * This indicates that the node doesn't seem to be able to reach anything,
	 * or hasn't for a while. It's not a hard instantaneous thing.
	 *
	 * Meta-data: none
	 */
	ZT_EVENT_OFFLINE = 1,

	/**
	 * Node appears online
	 *
	 * This indicates that the node was offline but now seems to be able to
	 * reach something. Like OFFLINE it's not a hard instantaneous thing but
	 * more of an indicator for UI reporting purposes.
	 *
	 * Meta-data: none
	 */
	ZT_EVENT_ONLINE = 2,

	/**
	 * Node is shutting down
	 *
	 * This is generated within Node's destructor when it is being shut down.
	 * It's done for convenience in case you want to clean up anything during
	 * node shutdown in your node event handler.
	 *
	 * Meta-data: none
	 */
	ZT_EVENT_DOWN = 3,

	// 4 once signaled identity collision but this is no longer an error

	/**
	 * Trace (debugging) message
	 *
	 * These events are only generated if this is a TRACE-enabled build.
	 * This is for local debug traces, not remote trace diagnostics.
	 *
	 * Meta-data: struct of type ZT_Trace_*
	 */
	ZT_EVENT_TRACE = 5,

	/**
	 * VERB_USER_MESSAGE received
	 *
	 * These are generated when a VERB_USER_MESSAGE packet is received via
	 * ZeroTier VL1. This can be used for below-VL2 in-band application
	 * specific signaling over the ZeroTier protocol.
	 *
	 * It's up to you to ensure that you handle these in a way that does
	 * not introduce a remote security vulnerability into your app! If
	 * your USER_MESSAGE code has a buffer overflow or other vulnerability
	 * then your app will be vulnerable and this is not ZT's fault. :)
	 *
	 * Meta-data: ZT_UserMessage structure
	 */
	ZT_EVENT_USER_MESSAGE = 6
};

/**
 * Identity type codes
 */
enum ZT_Identity_Type
{
	/* These values must be the same as in Identity.hpp in the core. */
	ZT_IDENTITY_TYPE_C25519 = 0,
	ZT_IDENTITY_TYPE_P384 = 1
};

/**
 * A ZeroTier identity (opaque)
 */
typedef void ZT_Identity;

/**
 * User message used with ZT_EVENT_USER_MESSAGE
 *
 * These are direct VL1 P2P messages for application use. Encryption and
 * authentication in the ZeroTier protocol will guarantee the origin
 * address and message content, but you are responsible for any other
 * levels of authentication or access control that are required. Any node
 * in the world can send you a user message! (Unless your network is air
 * gapped.)
 *
 * Pointers to id and data might not remain valid after the event is
 * received.
 */
typedef struct
{
	/**
	 * Identity of sender
	 */
	const ZT_Identity *id;

	/**
	 * User message type ID
	 */
	uint64_t typeId;

	/**
	 * User message data
	 */
	const void *data;

	/**
	 * Length of data in bytes
	 */
	unsigned int length;
} ZT_UserMessage;

/**
 * Current node status
 */
typedef struct
{
	/**
	 * 40-bit ZeroTier address of this node
	 */
	uint64_t address;

	/**
	 * Actual identity object for this node
	 */
	const ZT_Identity *identity;

	/**
	 * Public identity in string-serialized form (safe to send to others)
	 *
	 * This pointer will remain valid as long as the node exists.
	 */
	const char *publicIdentity;

	/**
	 * Full identity including secret key in string-serialized form
	 *
	 * This pointer will remain valid as long as the node exists.
	 */
	const char *secretIdentity;

	/**
	 * True if some kind of connectivity appears available
	 */
	int online;
} ZT_NodeStatus;

/**
 * Virtual network status codes
 */
enum ZT_VirtualNetworkStatus
{
	/**
	 * Waiting for network configuration (also means revision == 0)
	 */
	ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION = 0,

	/**
	 * Configuration received and we are authorized
	 */
	ZT_NETWORK_STATUS_OK = 1,

	/**
	 * Netconf master told us 'nope'
	 */
	ZT_NETWORK_STATUS_ACCESS_DENIED = 2,

	/**
	 * Netconf master exists, but this virtual network does not
	 */
	ZT_NETWORK_STATUS_NOT_FOUND = 3
};

/**
 * Virtual network type codes
 */
enum ZT_VirtualNetworkType
{
	/**
	 * Private networks are authorized via certificates of membership
	 */
	ZT_NETWORK_TYPE_PRIVATE = 0,

	/**
	 * Public networks have no access control -- they'll always be AUTHORIZED
	 */
	ZT_NETWORK_TYPE_PUBLIC = 1
};

/**
 * The type of a virtual network rules table entry
 *
 * These must be from 0 to 63 since the most significant two bits of each
 * rule type are NOT (MSB) and AND/OR.
 *
 * Each rule is composed of zero or more MATCHes followed by an ACTION.
 * An ACTION with no MATCHes is always taken.
 */
enum ZT_VirtualNetworkRuleType
{
	// 0 to 15 reserved for actions

	/**
	 * Drop frame
	 */
	ZT_NETWORK_RULE_ACTION_DROP = 0,

	/**
	 * Accept and pass frame
	 */
	ZT_NETWORK_RULE_ACTION_ACCEPT = 1,

	/**
	 * Forward a copy of this frame to an observer (by ZT address)
	 */
	ZT_NETWORK_RULE_ACTION_TEE = 2,

	/**
	 * Exactly like TEE but mandates ACKs from observer
	 */
	ZT_NETWORK_RULE_ACTION_WATCH = 3,

	/**
	 * Drop and redirect this frame to another node (by ZT address)
	 */
	ZT_NETWORK_RULE_ACTION_REDIRECT = 4,

	/**
	 * Stop evaluating rule set (drops unless there are capabilities, etc.)
	 */
	ZT_NETWORK_RULE_ACTION_BREAK = 5,

	/**
	 * Place a matching frame in the specified QoS bucket
	 */
	ZT_NETWORK_RULE_ACTION_PRIORITY = 6,

	/**
	 * Maximum ID for an ACTION, anything higher is a MATCH
	 */
	ZT_NETWORK_RULE_ACTION__MAX_ID = 15,

	// 16 to 63 reserved for match criteria

	ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS = 24,
	ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS = 25,
	ZT_NETWORK_RULE_MATCH_VLAN_ID = 26,
	ZT_NETWORK_RULE_MATCH_VLAN_PCP = 27,
	ZT_NETWORK_RULE_MATCH_VLAN_DEI = 28,
	ZT_NETWORK_RULE_MATCH_MAC_SOURCE = 29,
	ZT_NETWORK_RULE_MATCH_MAC_DEST = 30,
	ZT_NETWORK_RULE_MATCH_IPV4_SOURCE = 31,
	ZT_NETWORK_RULE_MATCH_IPV4_DEST = 32,
	ZT_NETWORK_RULE_MATCH_IPV6_SOURCE = 33,
	ZT_NETWORK_RULE_MATCH_IPV6_DEST = 34,
	ZT_NETWORK_RULE_MATCH_IP_TOS = 35,
	ZT_NETWORK_RULE_MATCH_IP_PROTOCOL = 36,
	ZT_NETWORK_RULE_MATCH_ETHERTYPE = 37,
	ZT_NETWORK_RULE_MATCH_ICMP = 38,
	ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE = 39,
	ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE = 40,
	ZT_NETWORK_RULE_MATCH_CHARACTERISTICS = 41,
	ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE = 42,
	ZT_NETWORK_RULE_MATCH_RANDOM = 43,
	ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE = 44,
	ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND = 45,
	ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR = 46,
	ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR = 47,
	ZT_NETWORK_RULE_MATCH_TAGS_EQUAL = 48,
	ZT_NETWORK_RULE_MATCH_TAG_SENDER = 49,
	ZT_NETWORK_RULE_MATCH_TAG_RECEIVER = 50,
	ZT_NETWORK_RULE_MATCH_INTEGER_RANGE = 51,

	/**
	 * Maximum ID allowed for a MATCH entry in the rules table
	 */
	ZT_NETWORK_RULE_MATCH__MAX_ID = 63
};

/**
 * Network flow rule
 *
 * Rules are stored in a table in which one or more match entries is followed
 * by an action. If more than one match precedes an action, the rule is
 * the AND of all matches. An action with no match is always taken since it
 * matches anything. If nothing matches, the default action is DROP.
 *
 * This is designed to be a more memory-efficient way of storing rules than
 * a wide table, yet still fast and simple to access in code.
 */
typedef struct
{
	/**
	 * Type and flags
	 *
	 * Bits are: NOTTTTTT
	 *
	 * N - If true, sense of match is inverted (no effect on actions)
	 * O - If true, result is ORed with previous instead of ANDed (no effect on actions)
	 * T - Rule or action type
	 *
	 * AND with 0x3f to get type, 0x80 to get NOT bit, and 0x40 to get OR bit.
	 */
	uint8_t t;

	/**
	 * Union containing the value of this rule -- which field is used depends on 't'
	 */
	union {
		/**
		 * IPv6 address in big-endian / network byte order and netmask bits
		 */
		struct {
			uint8_t ip[16];
			uint8_t mask;
		} ipv6;

		/**
		 * IPv4 address in big-endian / network byte order
		 */
		struct {
			uint32_t ip;
			uint8_t mask;
		} ipv4;

		/**
		 * Integer range match in packet payload
		 *
		 * This allows matching of ranges of integers up to 64 bits wide where
		 * the range is +/- INT32_MAX. It's packed this way so it fits in 16
		 * bytes and doesn't enlarge the overall size of this union.
		 */
		struct {
			uint64_t start; // integer range start
			uint32_t end; // end of integer range (relative to start, inclusive, 0 for equality w/start)
			uint16_t idx; // index in packet of integer
			uint8_t format; // bits in integer (range 1-64, ((format&63)+1)) and endianness (MSB 1 for little, 0 for big)
		} intRange;

		/**
		 * Packet characteristic flags being matched
		 */
		uint64_t characteristics;

		/**
		 * IP port range -- start-end inclusive -- host byte order
		 */
		uint16_t port[2];

		/**
		 * 40-bit ZeroTier address (in least significant bits, host byte order)
		 */
		uint64_t zt;

		/**
		 * 0 = never, UINT32_MAX = always
		 */
		uint32_t randomProbability;

		/**
		 * 48-bit Ethernet MAC address in big-endian order
		 */
		uint8_t mac[6];

		/**
		 * VLAN ID in host byte order
		 */
		uint16_t vlanId;

		/**
		 * VLAN PCP (least significant 3 bits)
		 */
		uint8_t vlanPcp;

		/**
		 * VLAN DEI (single bit / boolean)
		 */
		uint8_t vlanDei;

		/**
		 * Ethernet type in host byte order
		 */
		uint16_t etherType;

		/**
		 * IP protocol
		 */
		uint8_t ipProtocol;

		/**
		 * IP type of service a.k.a. DSCP field
		 */
		struct {
			uint8_t mask;
			uint8_t value[2];
		} ipTos;

		/**
		 * Ethernet packet size in host byte order (start-end, inclusive)
		 */
		uint16_t frameSize[2];

		/**
		 * ICMP type and code
		 */
		struct {
			uint8_t type; // ICMP type, always matched
			uint8_t code; // ICMP code if matched
			uint8_t flags; // flag 0x01 means also match code, otherwise only match type
		} icmp;

		/**
		 * For tag-related rules
		 */
		struct {
			uint32_t id;
			uint32_t value;
		} tag;

		/**
		 * Destinations for TEE and REDIRECT
		 */
		struct {
			uint64_t address;
			uint32_t flags;
			uint16_t length;
		} fwd;

		/**
		 * Quality of Service (QoS) bucket we want a frame to be placed in
		 */
		uint8_t qosBucket;
	} v;
} ZT_VirtualNetworkRule;

/**
 * A route to be pushed on a virtual network
 */
typedef struct
{
	/**
	 * Target network / netmask bits (in port field) or NULL or 0.0.0.0/0 for default
	 */
	struct sockaddr_storage target;

	/**
	 * Gateway IP address (port ignored) or NULL (family == 0) for LAN-local (no gateway)
	 */
	struct sockaddr_storage via;

	/**
	 * Route flags
	 */
	uint16_t flags;

	/**
	 * Route metric
	 */
	uint16_t metric;
} ZT_VirtualNetworkRoute;

/**
 * An Ethernet multicast group
 */
typedef struct
{
	/**
	 * MAC address (least significant 48 bits)
	 */
	uint64_t mac;

	/**
	 * Additional distinguishing information (usually zero)
	 */
	unsigned long adi;
} ZT_MulticastGroup;

/**
 * Virtual network configuration update type
 */
enum ZT_VirtualNetworkConfigOperation
{
	/**
	 * Network is coming up (either for the first time or after service restart)
	 */
	ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP = 1,

	/**
	 * Network configuration has been updated
	 */
	ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE = 2,

	/**
	 * Network is going down (not permanently)
	 */
	ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN = 3,

	/**
	 * Network is going down permanently (leave/delete)
	 */
	ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY = 4
};

/**
 * Virtual network configuration
 */
typedef struct
{
	/**
	 * 64-bit ZeroTier network ID
	 */
	uint64_t nwid;

	/**
	 * Ethernet MAC (48 bits) that should be assigned to port
	 */
	uint64_t mac;

	/**
	 * Network name (from network configuration master)
	 */
	char name[ZT_MAX_NETWORK_SHORT_NAME_LENGTH + 1];

	/**
	 * Network configuration request status
	 */
	enum ZT_VirtualNetworkStatus status;

	/**
	 * Network type
	 */
	enum ZT_VirtualNetworkType type;

	/**
	 * Maximum interface MTU
	 */
	unsigned int mtu;

	/**
	 * If nonzero, this port is allowed to bridge to other networks
	 *
	 * This is informational. If this is false (0), bridged packets will simply
	 * be dropped and bridging won't work.
	 */
	int bridge;

	/**
	 * If nonzero, this network supports and allows broadcast (ff:ff:ff:ff:ff:ff) traffic
	 */
	int broadcastEnabled;

	/**
	 * Revision number as reported by controller or 0 if still waiting for config
	 */
	unsigned long netconfRevision;

	/**
	 * Number of assigned addresses
	 */
	unsigned int assignedAddressCount;

	/**
	 * ZeroTier-assigned addresses (in sockaddr_storage structures)
	 *
	 * For IP, the port number of the sockaddr_XX structure contains the number
	 * of bits in the address netmask. Only the IP address and port are used.
	 * Other fields like interface number can be ignored.
	 *
	 * This is only used for ZeroTier-managed address assignments sent by the
	 * virtual network's configuration master.
	 */
	struct sockaddr_storage assignedAddresses[ZT_MAX_ZT_ASSIGNED_ADDRESSES];

	/**
	 * Number of ZT-pushed routes
	 */
	unsigned int routeCount;

	/**
	 * Routes (excluding those implied by assigned addresses and their masks)
	 */
	ZT_VirtualNetworkRoute routes[ZT_MAX_NETWORK_ROUTES];
} ZT_VirtualNetworkConfig;

/**
 * A list of networks
 */
typedef struct
{
	ZT_VirtualNetworkConfig *networks;
	unsigned long networkCount;
} ZT_VirtualNetworkList;

/**
 * Address where this node could be reached via an external interface
 */
typedef struct
{
	/**
	 * IP and port as would be reachable by external nodes
	 */
	struct sockaddr_storage address;

	/**
	 * If nonzero this address is static and can be incorporated into this node's Locator
	 */
	int permanent;
} ZT_InterfaceAddress;

/**
 * Physical path configuration
 */
typedef struct
{
	/**
	 * If non-zero set this physical network path to be trusted to disable encryption and authentication
	 */
	uint64_t trustedPathId;

	/**
	 * Physical path MTU from ZT_MIN_PHYSMTU and ZT_MAX_PHYSMTU or <= 0 to use default
	 */
	int mtu;
} ZT_PhysicalPathConfiguration;

/**
 * Physical network path to a peer
 */
typedef struct
{
	/**
	 * Address of endpoint
	 */
	struct sockaddr_storage address;

	/**
	 * Time of last send in milliseconds or 0 for never
	 */
	int64_t lastSend;

	/**
	 * Time of last receive in milliseconds or 0 for never
	 */
	int64_t lastReceive;

	/**
	 * Is this a trusted path? If so this will be its nonzero ID.
	 */
	uint64_t trustedPathId;

	/**
	 * Is path alive?
	 */
	int alive;

	/**
	 * Is path preferred?
	 */
	int preferred;
} ZT_PeerPhysicalPath;

/**
 * Peer status result buffer
 */
typedef struct
{
	/**
	 * ZeroTier address (40 bits)
	 */
	uint64_t address;

	/**
	 * Peer identity
	 */
	const ZT_Identity *identity;

	/**
	 * Hash of identity public key(s)
	 */
	uint8_t identityHash[48];

	/**
	 * Remote major version or -1 if not known
	 */
	int versionMajor;

	/**
	 * Remote minor version or -1 if not known
	 */
	int versionMinor;

	/**
	 * Remote revision or -1 if not known
	 */
	int versionRev;

	/**
	 * Last measured latency in milliseconds or -1 if unknown
	 */
	int latency;

	/**
	 * If non-zero this peer is a root
	 */
	int root;

	/**
	 * Bootstrap address
	 *
	 * This is a memo-ized recently valid address that can be saved and used
	 * to attempt rapid reconnection with this peer. If the ss_family field
	 * is 0 this field is considered null/empty.
	 */
	struct sockaddr_storage bootstrap;

	/**
	 * Number of paths (size of paths[])
	 */
	unsigned int pathCount;

	/**
	 * Known network paths to peer
	 */
	ZT_PeerPhysicalPath paths[ZT_MAX_PEER_NETWORK_PATHS];
} ZT_Peer;

/**
 * List of peers
 */
typedef struct
{
	ZT_Peer *peers;
	unsigned long peerCount;
} ZT_PeerList;

/**
 * ZeroTier core state objects
 */
enum ZT_StateObjectType
{
	/**
	 * Null object -- ignored
	 */
	ZT_STATE_OBJECT_NULL = 0,

	/**
	 * Public address and public key
	 *
	 * Object ID: (unused)
	 * Canonical path: <HOME>/identity.public
   * Persistence: required
	 */
	ZT_STATE_OBJECT_IDENTITY_PUBLIC = 1,

	/**
	 * Full identity with secret key
	 *
	 * Object ID: (unused)
	 * Canonical path: <HOME>/identity.secret
   * Persistence: required, should be stored with restricted permissions e.g. mode 0600 on *nix
	 */
	ZT_STATE_OBJECT_IDENTITY_SECRET = 2,

	/**
	 * This node's locator
	 *
	 * Object ID: (unused)
	 * Canonical path: <HOME>/locator
	 * Persistence: optional
	 */
	ZT_STATE_OBJECT_LOCATOR = 3,

	/**
	 * Peer and related state
	 *
	 * Object ID: peer address
	 * Canonical path: <HOME>/peers.d/<ID> (10-digit address
	 * Persistence: optional, can be cleared at any time
	 */
	ZT_STATE_OBJECT_PEER = 5,

	/**
	 * Network configuration
	 *
	 * Object ID: network ID
	 * Canonical path: <HOME>/networks.d/<NETWORKID>.conf (16-digit hex ID)
	 * Persistence: required if network memberships should persist
	 */
	ZT_STATE_OBJECT_NETWORK_CONFIG = 6,

	/**
	 * Root list
	 *
	 * Object ID: (unused)
	 * Canonical path: <HOME>/roots
	 * Persistence: required if root settings should persist
	 */
	ZT_STATE_OBJECT_ROOTS = 7
};

/**
 * An instance of a ZeroTier One node (opaque)
 */
typedef void ZT_Node;

/****************************************************************************/

/**
 * Callback called to update virtual network port configuration
 *
 * This can be called at any time to update the configuration of a virtual
 * network port. The parameter after the network ID specifies whether this
 * port is being brought up, updated, brought down, or permanently deleted.
 *
 * This in turn should be used by the underlying implementation to create
 * and configure tap devices at the OS (or virtual network stack) layer.
 *
 * The supplied config pointer is not guaranteed to remain valid, so make
 * a copy if you want one.
 *
 * This should not call multicastSubscribe() or other network-modifying
 * methods, as this could cause a deadlock in multithreaded or interrupt
 * driven environments.
 */
typedef void (*ZT_VirtualNetworkConfigFunction)(
	ZT_Node *,                             /* Node */
	void *,                                /* User ptr */
	void *,                                /* Thread ptr */
	uint64_t,                              /* Network ID */
	void **,                               /* Modifiable network user PTR */
	enum ZT_VirtualNetworkConfigOperation, /* Config operation */
	const ZT_VirtualNetworkConfig *);      /* Network configuration */

/**
 * Function to send a frame out to a virtual network port
 *
 * Parameters: (1) node, (2) user ptr, (3) network ID, (4) source MAC,
 * (5) destination MAC, (6) ethertype, (7) VLAN ID, (8) frame data,
 * (9) frame length.
 */
typedef void (*ZT_VirtualNetworkFrameFunction)(
	ZT_Node *,                             /* Node */
	void *,                                /* User ptr */
	void *,                                /* Thread ptr */
	uint64_t,                              /* Network ID */
	void **,                               /* Modifiable network user PTR */
	uint64_t,                              /* Source MAC */
	uint64_t,                              /* Destination MAC */
	unsigned int,                          /* Ethernet type */
	unsigned int,                          /* VLAN ID (0 for none) */
	const void *,                          /* Frame data */
	unsigned int);                         /* Frame length */

/**
 * Callback for events
 *
 * Events are generated when the node's status changes in a significant way
 * and on certain non-fatal errors and events of interest. The final void
 * parameter points to event meta-data. The type of event meta-data (and
 * whether it is present at all) is event type dependent. See the comments
 * in the definition of ZT_Event.
 */
typedef void (*ZT_EventCallback)(
	ZT_Node *,                             /* Node */
	void *,                                /* User ptr */
	void *,                                /* Thread ptr */
	enum ZT_Event,                         /* Event type */
	const void *);                         /* Event payload (if applicable) */

/**
 * Callback for storing and/or publishing state information
 *
 * See ZT_StateObjectType docs for information about each state object type
 * and when and if it needs to be persisted.
 *
 * An object of length -1 is sent to indicate that an object should be
 * deleted.
 */
typedef void (*ZT_StatePutFunction)(
	ZT_Node *,                             /* Node */
	void *,                                /* User ptr */
	void *,                                /* Thread ptr */
	enum ZT_StateObjectType,               /* State object type */
	const uint64_t [2],                    /* State object ID (if applicable) */
	const void *,                          /* State object data */
	int);                                  /* Length of data or -1 to delete */

/**
 * Callback for retrieving stored state information
 *
 * This function should return the number of bytes actually stored to the
 * buffer or -1 if the state object was not found. The buffer itself should
 * be set to point to the data, and the last result parameter must point to
 * a function that will be used to free the buffer when the core is done
 * with it. This is very often just a pointer to free().
 */
typedef int (*ZT_StateGetFunction)(
	ZT_Node *,                             /* Node */
	void *,                                /* User ptr */
	void *,                                /* Thread ptr */
	enum ZT_StateObjectType,               /* State object type */
	const uint64_t [2],                    /* State object ID (if applicable) */
	void **,                               /* Result parameter: data */
	void (**)(void *));                    /* Result parameter: data free function */

/**
 * Function to send a ZeroTier packet out over the physical wire (L2/L3)
 *
 * Parameters:
 *  (1) Node
 *  (2) User pointer
 *  (3) Local socket or -1 for "all" or "any"
 *  (4) Remote address
 *  (5) Packet data
 *  (6) Packet length
 *  (7) Desired IP TTL or 0 to use default
 *
 * If there is only one local socket, the local socket can be ignored.
 * If the local socket is -1, the packet should be sent out from all
 * bound local sockets or a random bound local socket.
 *
 * If TTL is nonzero, packets should have their IP TTL value set to this
 * value if possible. If this is not possible it is acceptable to ignore
 * this value and send anyway with normal or default TTL.
 *
 * The function must return zero on success and may return any error code
 * on failure. Note that success does not (of course) guarantee packet
 * delivery. It only means that the packet appears to have been sent.
 */
typedef int (*ZT_WirePacketSendFunction)(
	ZT_Node *,                        /* Node */
	void *,                           /* User ptr */
	void *,                           /* Thread ptr */
	int64_t,                          /* Local socket */
	const struct sockaddr_storage *,  /* Remote address */
	const void *,                     /* Packet data */
	unsigned int,                     /* Packet length */
	unsigned int);                    /* TTL or 0 to use default */

/**
 * Function to check whether a path should be used for ZeroTier traffic
 *
 * Parameters:
 *  (1) Node
 *  (2) User pointer
 *  (3) ZeroTier address or 0 for none/any
 *  (4) Full identity or NULL for none/any
 *  (5) Local socket or -1 if unknown
 *  (6) Remote address
 *
 * This function must return nonzero (true) if the path should be used.
 *
 * If no path check function is specified, ZeroTier will still exclude paths
 * that overlap with ZeroTier-assigned and managed IP address blocks. But the
 * use of a path check function is recommended to ensure that recursion does
 * not occur in cases where addresses are assigned by the OS or managed by
 * an out of band mechanism like DHCP. The path check function should examine
 * all configured ZeroTier interfaces and check to ensure that the supplied
 * addresses will not result in ZeroTier traffic being sent over a ZeroTier
 * interface (recursion).
 */
typedef int (*ZT_PathCheckFunction)(
	ZT_Node *,                        /* Node */
	void *,                           /* User ptr */
	void *,                           /* Thread ptr */
	uint64_t,                         /* ZeroTier address */
	const ZT_Identity *,              /* Full identity of node */
	int64_t,                          /* Local socket or -1 if unknown */
	const struct sockaddr_storage *); /* Remote address */

/**
 * Function to get physical addresses for ZeroTier peers
 *
 * Parameters:
 *  (1) Node
 *  (2) User pointer
 *  (3) ZeroTier address (least significant 40 bits)
 *  (4) Identity in string form
 *  (5) Desired address family or -1 for any
 *  (6) Buffer to fill with result
 *
 * If provided this function will be occasionally called to get physical
 * addresses that might be tried to reach a ZeroTier address. It must
 * return a nonzero (true) value if the result buffer has been filled
 * with an address.
 */
typedef int (*ZT_PathLookupFunction)(
	ZT_Node *,                        /* Node */
	void *,                           /* User ptr */
	void *,                           /* Thread ptr */
	uint64_t,                         /* ZeroTier address (40 bits) */
	const ZT_Identity *,              /* Full identity of node */
	int,                              /* Desired ss_family or -1 for any */
	struct sockaddr_storage *);       /* Result buffer */

/****************************************************************************/

/**
 * Structure for configuring ZeroTier core callback functions
 */
struct ZT_Node_Callbacks
{
	/**
	 * REQUIRED: Function to store and/or replicate state objects
	 */
	ZT_StatePutFunction statePutFunction;

	/**
	 * REQUIRED: Function to retrieve state objects from an object store
	 */
	ZT_StateGetFunction stateGetFunction;

	/**
	 * REQUIRED: Function to send packets over the physical wire
	 */
	ZT_WirePacketSendFunction wirePacketSendFunction;

	/**
	 * REQUIRED: Function to inject frames into a virtual network's TAP
	 */
	ZT_VirtualNetworkFrameFunction virtualNetworkFrameFunction;

	/**
	 * REQUIRED: Function to be called when virtual networks are configured or changed
	 */
	ZT_VirtualNetworkConfigFunction virtualNetworkConfigFunction;

	/**
	 * REQUIRED: Function to be called to notify external code of important events
	 */
	ZT_EventCallback eventCallback;

	/**
	 * OPTIONAL: Function to check whether a given physical path should be used for ZeroTier traffic
	 */
	ZT_PathCheckFunction pathCheckFunction;

	/**
	 * RECOMMENDED: Function to look up paths to ZeroTier nodes
	 */
	ZT_PathLookupFunction pathLookupFunction;
};

/**
 * Create a new ZeroTier node
 *
 * This will attempt to load its identity via the state get function in the
 * callback struct. If that fails it will generate a new identity and store
 * it. Identity generation can take anywhere from a few hundred milliseconds
 * to a few seconds depending on your CPU speed.
 *
 * @param node Result: pointer is set to new node instance on success
 * @param uptr User pointer to pass to functions/callbacks
 * @param tptr Thread pointer to pass to functions/callbacks resulting from this call
 * @param callbacks Callback function configuration
 * @param now Current clock in milliseconds
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_new(ZT_Node **node,void *uptr,void *tptr,const struct ZT_Node_Callbacks *callbacks,int64_t now);

/**
 * Delete a node and free all resources it consumes
 *
 * If you are using multiple threads, all other threads must be shut down
 * first. This can crash if processXXX() methods are in progress.
 *
 * @param node Node to delete
 * @param tptr Thread pointer to pass to functions/callbacks resulting from this call
 */
ZT_SDK_API void ZT_Node_delete(ZT_Node *node,void *tptr);

/**
 * Process a packet received from the physical wire
 *
 * @param node Node instance
 * @param tptr Thread pointer to pass to functions/callbacks resulting from this call
 * @param now Current clock in milliseconds
 * @param localSocket Local socket (you can use 0 if only one local socket is bound and ignore this)
 * @param remoteAddress Origin of packet
 * @param packetData Packet data
 * @param packetLength Packet length
 * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to processBackgroundTasks()
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_processWirePacket(
	ZT_Node *node,
	void *tptr,
	int64_t now,
	int64_t localSocket,
	const struct sockaddr_storage *remoteAddress,
	const void *packetData,
	unsigned int packetLength,
	volatile int64_t *nextBackgroundTaskDeadline);

/**
 * Process a frame from a virtual network port (tap)
 *
 * @param node Node instance
 * @param tptr Thread pointer to pass to functions/callbacks resulting from this call
 * @param now Current clock in milliseconds
 * @param nwid ZeroTier 64-bit virtual network ID
 * @param sourceMac Source MAC address (least significant 48 bits)
 * @param destMac Destination MAC address (least significant 48 bits)
 * @param etherType 16-bit Ethernet frame type
 * @param vlanId 10-bit VLAN ID or 0 if none
 * @param frameData Frame payload data
 * @param frameLength Frame payload length
 * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to processBackgroundTasks()
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_processVirtualNetworkFrame(
	ZT_Node *node,
	void *tptr,
	int64_t now,
	uint64_t nwid,
	uint64_t sourceMac,
	uint64_t destMac,
	unsigned int etherType,
	unsigned int vlanId,
	const void *frameData,
	unsigned int frameLength,
	volatile int64_t *nextBackgroundTaskDeadline);

/**
 * Perform periodic background operations
 *
 * @param node Node instance
 * @param tptr Thread pointer to pass to functions/callbacks resulting from this call
 * @param now Current clock in milliseconds
 * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to processBackgroundTasks()
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_processBackgroundTasks(
	ZT_Node *node,
	void *tptr,
	int64_t now,
	volatile int64_t *nextBackgroundTaskDeadline);

/**
 * Join a network
 *
 * This may generate calls to the port config callback before it returns,
 * or these may be differed if a netconf is not available yet.
 *
 * If we are already a member of the network, nothing is done and OK is
 * returned.
 *
 * @param node Node instance
 * @param nwid 64-bit ZeroTier network ID
 * @param uptr An arbitrary pointer to associate with this network (default: NULL)
 * @param tptr Thread pointer to pass to functions/callbacks resulting from this call
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_join(ZT_Node *node,uint64_t nwid,void *uptr,void *tptr);

/**
 * Leave a network
 *
 * If a port has been configured for this network this will generate a call
 * to the port config callback with a NULL second parameter to indicate that
 * the port is now deleted.
 *
 * The uptr parameter is optional and is NULL by default. If it is not NULL,
 * the pointer it points to is set to this network's uptr on success.
 *
 * @param node Node instance
 * @param nwid 64-bit network ID
 * @param uptr Target pointer is set to uptr (if not NULL)
 * @param tptr Thread pointer to pass to functions/callbacks resulting from this call
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_leave(ZT_Node *node,uint64_t nwid,void **uptr,void *tptr);

/**
 * Subscribe to an Ethernet multicast group
 *
 * ADI stands for additional distinguishing information. This defaults to zero
 * and is rarely used. Right now its only use is to enable IPv4 ARP to scale,
 * and this must be done.
 *
 * For IPv4 ARP, the implementation must subscribe to 0xffffffffffff (the
 * broadcast address) but with an ADI equal to each IPv4 address in host
 * byte order. This converts ARP from a non-scalable broadcast protocol to
 * a scalable multicast protocol with perfect address specificity.
 *
 * If this is not done, ARP will not work reliably.
 *
 * Multiple calls to subscribe to the same multicast address will have no
 * effect. It is perfectly safe to do this.
 *
 * This does not generate an update call to networkConfigCallback().
 *
 * @param node Node instance
 * @param tptr Thread pointer to pass to functions/callbacks resulting from this call
 * @param nwid 64-bit network ID
 * @param multicastGroup Ethernet multicast or broadcast MAC (least significant 48 bits)
 * @param multicastAdi Multicast ADI (least significant 32 bits only, use 0 if not needed)
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_multicastSubscribe(ZT_Node *node,void *tptr,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);

/**
 * Unsubscribe from an Ethernet multicast group (or all groups)
 *
 * If multicastGroup is zero (0), this will unsubscribe from all groups. If
 * you are not subscribed to a group this has no effect.
 *
 * This does not generate an update call to networkConfigCallback().
 *
 * @param node Node instance
 * @param nwid 64-bit network ID
 * @param multicastGroup Ethernet multicast or broadcast MAC (least significant 48 bits)
 * @param multicastAdi Multicast ADI (least significant 32 bits only, use 0 if not needed)
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_multicastUnsubscribe(ZT_Node *node,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);

/**
 * Add a root server (has no effect if already added)
 *
 * @param node Node instance
 * @param tptr Thread pointer to pass to functions/callbacks resulting from this call
 * @param identity Identity of this root server
 * @param bootstrap Optional bootstrap address for initial contact
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_addRoot(ZT_Node *node,void *tptr,const ZT_Identity *identity,const struct sockaddr_storage *bootstrap);

/**
 * Remove a root server
 *
 * This removes this node's root designation but does not prevent this node
 * from communicating with it or close active paths to it.
 *
 * @param node Node instance
 * @param tptr Thread pointer to pass to functions/callbacks resulting from this call
 * @param identity Identity to remove
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_removeRoot(ZT_Node *node,void *tptr,const ZT_Identity *identity);

/**
 * Get this node's 40-bit ZeroTier address
 *
 * @param node Node instance
 * @return ZeroTier address (least significant 40 bits of 64-bit int)
 */
ZT_SDK_API uint64_t ZT_Node_address(ZT_Node *node);

/**
 * Get this node's identity
 *
 * The identity pointer returned by this function need not and should not be
 * freed with ZT_Identity_delete(). It's valid until the node is deleted.
 *
 * @param node Node instance
 * @return Identity
 */
ZT_SDK_API const ZT_Identity *ZT_Node_identity(ZT_Node *node);

/**
 * Get the status of this node
 *
 * @param node Node instance
 * @param status Buffer to fill with current node status
 */
ZT_SDK_API void ZT_Node_status(ZT_Node *node,ZT_NodeStatus *status);

/**
 * Get a list of known peer nodes
 *
 * The pointer returned here must be freed with freeQueryResult()
 * when you are done with it.
 *
 * @param node Node instance
 * @return List of known peers or NULL on failure
 */
ZT_SDK_API ZT_PeerList *ZT_Node_peers(ZT_Node *node);

/**
 * Get the status of a virtual network
 *
 * The pointer returned here must be freed with freeQueryResult()
 * when you are done with it.
 *
 * @param node Node instance
 * @param nwid 64-bit network ID
 * @return Network configuration or NULL if we are not a member of this network
 */
ZT_SDK_API ZT_VirtualNetworkConfig *ZT_Node_networkConfig(ZT_Node *node,uint64_t nwid);

/**
 * Enumerate and get status of all networks
 *
 * @param node Node instance
 * @return List of networks or NULL on failure
 */
ZT_SDK_API ZT_VirtualNetworkList *ZT_Node_networks(ZT_Node *node);

/**
 * Set the network-associated user-defined pointer for a given network
 *
 * This will have no effect if the network ID is not recognized.
 *
 * @param node Node instance
 * @param nwid Network ID
 * @param ptr New network-associated pointer
 */
ZT_SDK_API void ZT_Node_setNetworkUserPtr(ZT_Node *node,uint64_t nwid,void *ptr);

/**
 * Free a query result buffer
 *
 * Use this to free the return values of listNetworks(), listPeers(), etc.
 *
 * @param node Node instance
 * @param qr Query result buffer
 */
ZT_SDK_API void ZT_Node_freeQueryResult(ZT_Node *node,void *qr);

/**
 * Set external interface addresses where this node could be reached
 *
 * @param node Node instance
 * @param addrs Addresses
 * @param addrCount Number of items in addrs[]
 */
ZT_SDK_API void ZT_Node_setInterfaceAddresses(ZT_Node *node,const ZT_InterfaceAddress *addrs,unsigned int addrCount);

/**
 * Send a VERB_USER_MESSAGE to another ZeroTier node
 *
 * There is no delivery guarantee here. Failure can occur if the message is
 * too large or if dest is not a valid ZeroTier address.
 *
 * @param node Node instance
 * @param tptr Thread pointer to pass to functions/callbacks resulting from this call
 * @param dest Destination ZeroTier address
 * @param typeId VERB_USER_MESSAGE type ID
 * @param data Payload data to attach to user message
 * @param len Length of data in bytes
 * @return Boolean: non-zero on success, zero on failure
 */
ZT_SDK_API int ZT_Node_sendUserMessage(ZT_Node *node,void *tptr,uint64_t dest,uint64_t typeId,const void *data,unsigned int len);

/**
 * Set a network controller instance for this node
 *
 * Normal nodes should not need to use this. This is for nodes with
 * special compiled-in support for acting as network configuration
 * masters / controllers.
 *
 * The supplied instance must be a C++ object that inherits from the
 * NetworkConfigMaster base class in node/. No type checking is performed,
 * so a pointer to anything else will result in a crash.
 *
 * @param node ZertTier One node
 * @param networkConfigMasterInstance Instance of NetworkConfigMaster C++ class or NULL to disable
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API void ZT_Node_setController(ZT_Node *node,void *networkConfigMasterInstance);

/**
 * Set configuration for a given physical path
 *
 * @param node Node instance
 * @param pathNetwork Network/CIDR of path or NULL to clear the cache and reset all paths to default
 * @param pathConfig Path configuration or NULL to erase this entry and therefore reset it to NULL
 * @return OK or error code
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_setPhysicalPathConfiguration(ZT_Node *node,const struct sockaddr_storage *pathNetwork,const ZT_PhysicalPathConfiguration *pathConfig);

/****************************************************************************/

/**
 * Generate a new identity
 *
 * Due to a small amount of proof of work this can be a time consuming and CPU
 * intensive operation. It takes less than a second on most desktop-class systems
 * but can take longer on e.g. phones.
 *
 * @param type Type of identity to generate
 * @return New identity or NULL on error
 */
ZT_SDK_API ZT_Identity *ZT_Identity_new(enum ZT_Identity_Type type);

/**
 * Create a new identity object from a string-serialized identity
 *
 * @param idStr Identity in string format
 * @return Identity object or NULL if the supplied identity string was not valid
 */
ZT_SDK_API ZT_Identity *ZT_Identity_fromString(const char *idStr);

/**
 * Validate this identity
 *
 * This can be slightly time consuming due to address derivation (work) checking.
 *
 * @return Non-zero if identity is valid
 */
ZT_SDK_API int ZT_Identity_validate(const ZT_Identity *id);

/**
 * Sign a data object with this identity
 *
 * The identity must have a private key or this will fail.
 *
 * @param id Identity to use to sign
 * @param data Data to sign
 * @param len Length of data
 * @param signature Buffer to store signature
 * @param signatureBufferLength Length of buffer (must be at least 96 bytes)
 * @return Length of signature in bytes or 0 on failure.
 */
ZT_SDK_API unsigned int ZT_Identity_sign(const ZT_Identity *id,const void *data,unsigned int len,void *signature,unsigned int signatureBufferLength);

/**
 * Verify a signature
 *
 * @param id Identity to use to verify
 * @param data Data to verify
 * @param len Length of data
 * @param signature Signature to check
 * @param sigLen Length of signature in bytes
 * @return Non-zero if signature is valid
 */
ZT_SDK_API int ZT_Identity_verify(const ZT_Identity *id,const void *data,unsigned int len,const void *signature,unsigned int sigLen);

/**
 * Get identity type
 *
 * @param id Identity to query
 * @return Identity type code
 */
ZT_SDK_API enum ZT_Identity_Type ZT_Identity_type(const ZT_Identity *id);

/**
 * Convert an identity to its string representation
 *
 * @param id Identity to convert
 * @param buf Buffer to store identity (should be at least about 1024 bytes in length)
 * @param capacity Capacity of buffer
 * @param includePrivate If true include the private key if present
 * @return Pointer to buf or NULL on overflow or other error
 */
ZT_SDK_API char *ZT_Identity_toString(const ZT_Identity *id,char *buf,int capacity,int includePrivate);

/**
 * Check whether this identity object also holds a private key
 *
 * @param id Identity to query
 * @return Non-zero if a private key is held
 */
ZT_SDK_API int ZT_Identity_hasPrivate(const ZT_Identity *id);

/**
 * Get the ZeroTier address associated with this identity
 *
 * @param id Identity to query
 * @return ZeroTier address (only least significant 40 bits are meaningful, rest will be 0)
 */
ZT_SDK_API uint64_t ZT_Identity_address(const ZT_Identity *id);

/**
 * Compute a hash of this identity's public keys (or both public and private if includePrivate is true)
 *
 * @param id Identity to query
 * @param h Buffer for 384-bit hash
 * @param includePrivate If true include private keys if any
 */
ZT_SDK_API void ZT_Identity_hash(const ZT_Identity *id,uint8_t h[48],int includePrivate);

/**
 * Delete an identity and free associated memory
 *
 * This should only be used with identities created via Identity_new
 * and Identity_fromString().
 *
 * @param id Identity to delete
 */
ZT_SDK_API void ZT_Identity_delete(ZT_Identity *id);

/****************************************************************************/

/**
 * Get ZeroTier One version
 *
 * @param major Result: major version
 * @param minor Result: minor version
 * @param revision Result: revision
 */
ZT_SDK_API void ZT_version(int *major,int *minor,int *revision);

#ifdef __cplusplus
}
#endif

#endif
