/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

/*
 * This defines the external C API for the ZeroTier network hypervisor.
 */

#ifndef ZT_ZEROTIER_API_H
#define ZT_ZEROTIER_API_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This symbol may be defined to anything we need to put in front of API function prototypes. */
#ifndef ZT_SDK_API
#define ZT_SDK_API extern
#endif

/* ---------------------------------------------------------------------------------------------------------------- */

/**
 * Default primary UDP port
 */
#define ZT_DEFAULT_PORT 9993

/**
 * Default secondary UDP port (if enabled)
 */
#define ZT_DEFAULT_SECONDARY_PORT 293

/**
 * IP protocol number for naked IP encapsulation (this is not currently used)
 */
#define ZT_DEFAULT_RAW_IP_PROTOCOL 193

/**
 * Ethernet type for naked Ethernet encapsulation (this is not currently used)
 */
#define ZT_DEFAULT_ETHERNET_PROTOCOL 0x9993

/**
 * Size of a standard I/O buffer as returned by getBuffer().
 */
#define ZT_BUF_SIZE 16384

/**
 * Minimum Ethernet MTU allowed on virtual (not physical) networks
 */
#define ZT_MIN_MTU 1280

/**
 * Maximum Ethernet MTU allowed on virtual (not physical) networks
 */
#define ZT_MAX_MTU 10000

/**
 * Minimum allowed physical UDP MTU (smaller values are clipped to this)
 */
#define ZT_MIN_UDP_MTU 1400

/**
 * Default UDP payload size NOT including UDP and IP overhead
 *
 * This is small enough for PPPoE and for Google Cloud's bizarrely tiny MTUs.
 * A payload size corresponding to the default 2800 byte virtual MTU fits
 * into two packets of less than or equal to this size.
 */
#define ZT_DEFAULT_UDP_MTU 1432

/**
 * Maximum physical payload size that can ever be used
 */
#define ZT_MAX_UDP_PHYSPAYLOAD 10100

/**
 * Headroom for max physical MTU
 */
#define ZT_MAX_UDP_HEADROOM 224

/**
 * Maximum payload MTU for UDP packets
 */
#define ZT_MAX_UDP_MTU (ZT_MAX_UDP_PHYSPAYLOAD + ZT_MAX_UDP_HEADROOM)

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

/* ---------------------------------------------------------------------------------------------------------------- */

/**
 * Identity type codes (must be the same as Identity.hpp).
 *
 * Do not change these integer values. They're protocol constants.
 */
enum ZT_IdentityType
{
	ZT_IDENTITY_TYPE_C25519 = 0, /* C25519/Ed25519 */
	ZT_IDENTITY_TYPE_P384 = 1  /* Combined C25519/NIST-P-384 key */
};

/**
 * ZeroTier identity (address plus keys)
 */
typedef void ZT_Identity;

/**
 * Locator is a signed list of endpoints
 */
typedef void ZT_Locator;

#define ZT_SOCKADDR_STORAGE_SIZE 128

/**
 * InetAddress holds a socket address
 *
 * This is a sized placeholder for InetAddress in the C++ code or the
 * sockaddr_storage structure. Its size is checked at compile time in
 * InetAddress.cpp against sizeof(sockaddr_storage) to ensure that it
 * is correct for the platform. If it's not correct, a platform ifdef
 * will be needed.
 */
typedef struct { uint64_t bits[ZT_SOCKADDR_STORAGE_SIZE / 8]; } ZT_InetAddress;

/**
 * IP scope types as identified by InetAddress.
 */
enum ZT_InetAddress_IpScope
{
	ZT_IP_SCOPE_NONE = 0,          // NULL or not an IP address
	ZT_IP_SCOPE_MULTICAST = 1,     // 224.0.0.0 and other V4/V6 multicast IPs
	ZT_IP_SCOPE_LOOPBACK = 2,      // 127.0.0.1, ::1, etc.
	ZT_IP_SCOPE_PSEUDOPRIVATE = 3, // 28.x.x.x, etc. -- unofficially unrouted IPv4 blocks often "bogarted"
	ZT_IP_SCOPE_GLOBAL = 4,        // globally routable IP address (all others)
	ZT_IP_SCOPE_LINK_LOCAL = 5,    // 169.254.x.x, IPv6 LL
	ZT_IP_SCOPE_SHARED = 6,        // currently unused, formerly used for carrier-grade NAT ranges
	ZT_IP_SCOPE_PRIVATE = 7        // 10.x.x.x, 192.168.x.x, etc.
};

/**
 * Full identity fingerprint with address and 384-bit hash of public key(s)
 */
typedef struct
{
	/**
	 * Short address (only least significant 40 bits are used)
	 */
	uint64_t address;

	/**
	 * 384-bit hash of identity public key(s)
	 */
	uint8_t hash[48];
} ZT_Fingerprint;

/**
 * Maximum length of string fields in certificates
 */
#define ZT_CERTIFICATE_MAX_STRING_LENGTH 127

/**
 * Certificate is a root CA (local trust flag)
 */
#define ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA 0x0001U

/**
 * Certificate can affect configuration of the node.
 *
 * An example is a ZeroTier root set certificate which adds root servers.
 */
#define ZT_CERTIFICATE_LOCAL_TRUST_FLAG_CONFIG 0x0002U

/*
 * Certificate usage flags
 *
 * Not all of these are used in ZeroTier. Some are simply reserved because
 * same named usage attributes exist in X509 and we want to be future proof
 * against future desires to translate over X509 functionality.
 */

/**
 * Certificate can sign things (general).
 */
#define ZT_CERTIFICATE_USAGE_DIGITAL_SIGNATURE          0x00000001U

/**
 * Certificate can verify signatures to verify actions.
 *
 * (not used in ZeroTier)
 */
#define ZT_CERTIFICATE_USAGE_NON_REPUDIATION            0x00000002U

/**
 * Certificate's key can encipher other keys.
 *
 * (not used in ZeroTier)
 */
#define ZT_CERTIFICATE_USAGE_KEY_ENCIPHERMENT           0x00000004U

/**
 * Certificate's key can encipher data.
 *
 * (not used in ZeroTier)
 */
#define ZT_CERTIFICATE_USAGE_DATA_ENCIPHERMENT          0x00000008U

/**
 * Certificate's key can be used for Diffie-Hellman style key agreemtn.
 *
 * (not used in ZeroTier)
 */
#define ZT_CERTIFICATE_USAGE_KEY_AGREEMENT              0x00000010U

/**
 * Certificate can sign other certificates.
 */
#define ZT_CERTIFICATE_USAGE_CERTIFICATE_SIGNING        0x00000020U

/**
 * Certificate can revoke signatures.
 */
#define ZT_CERTIFICATE_USAGE_CRL_SIGNING                0x00000040U

/**
 * Certificate can sign executable code.
 *
 * (not used in ZeroTier)
 */
#define ZT_CERTIFICATE_USAGE_EXECUTABLE_SIGNATURE       0x00000080U

/**
 * Certificate's public key can be used for a timestamp service.
 *
 * (not used in ZeroTier)
 */
#define ZT_CERTIFICATE_USAGE_TIMESTAMPING               0x00000100U

/**
 * Certificate can enumerate a set of ZeroTier root nodes.
 */
#define ZT_CERTIFICATE_USAGE_ZEROTIER_ROOT_SET          0x00000200U

/**
 * Errors returned by functions that verify or handle certificates.
 */
enum ZT_CertificateError
{
	/**
	 * No error (certificate is valid or operation was successful)
	 */
	ZT_CERTIFICATE_ERROR_NONE = 0,

	/**
	 * Certificate format is invalid or required fields are missing
	 */
	ZT_CERTIFICATE_ERROR_INVALID_FORMAT = 1,

	/**
	 * One or more identities in the certificate are invalid or fail consistency check
	 */
	ZT_CERTIFICATE_ERROR_INVALID_IDENTITY = 2,

	/**
	 * Certificate primary signature is invalid
	 */
	ZT_CERTIFICATE_ERROR_INVALID_PRIMARY_SIGNATURE = 3,

	/**
	 * Full chain validation of certificate failed
	 */
	ZT_CERTIFICATE_ERROR_INVALID_CHAIN = 4,

	/**
	 * One or more signed components (e.g. a Locator) has an invalid signature.
	 */
	ZT_CERTIFICATE_ERROR_INVALID_COMPONENT_SIGNATURE = 5,

	/**
	 * Unique ID proof signature in subject was not valid.
	 */
	ZT_CERTIFICATE_ERROR_INVALID_UNIQUE_ID_PROOF = 6,

	/**
	 * Certificate is missing a required field
	 */
	ZT_CERTIFICATE_ERROR_MISSING_REQUIRED_FIELDS = 7,

	/**
	 * Certificate is expired or not yet in effect
	 */
	ZT_CERTIFICATE_ERROR_OUT_OF_VALID_TIME_WINDOW = 8,

	/**
	 * Certificate explicitly revoked
	 */
	ZT_CERTIFICATE_ERROR_REVOKED = 9
};

/**
 * Public key signing algorithm for certificates
 */
enum ZT_CertificatePublicKeyAlgorithm
{
	/**
	 * Nil value indicating no signature.
	 */
	ZT_CERTIFICATE_PUBLIC_KEY_ALGORITHM_NONE = 0,

	/**
	 * ECDSA with the NIST P-384 curve.
	 */
	ZT_CERTIFICATE_PUBLIC_KEY_ALGORITHM_ECDSA_NIST_P_384 = 1
};

/**
 * Maximum size of a public key in bytes (can be increased)
 */
#define ZT_CERTIFICATE_MAX_PUBLIC_KEY_SIZE 64

/**
 * Maximum size of a private key in bytes (can be increased)
 */
#define ZT_CERTIFICATE_MAX_PRIVATE_KEY_SIZE 128

/**
 * Maximum size of a signature in bytes (can be increased)
 */
#define ZT_CERTIFICATE_MAX_SIGNATURE_SIZE 128

/**
 * Size of a SHA384 hash
 */
#define ZT_CERTIFICATE_HASH_SIZE 48

/*
 * Maximum number of certificates that can be revoked at once.
 *
 * This shouldn't be changed and is set to be small enough to fit in a packet.
 */
#define ZT_CERTIFICATE_REVOCATION_MAX_CERTIFICATES 24

/**
 * Information about a real world entity.
 *
 * These fields are all optional and are all taken from the
 * most common fields present in X509 certificates.
 */
typedef struct
{
	char serialNo[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];
	char commonName[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];
	char country[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];
	char organization[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];
	char unit[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];
	char locality[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];
	char province[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];
	char streetAddress[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];
	char postalCode[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];
	char email[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];
	char url[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];
	char host[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];
} ZT_Certificate_Name;

/**
 * Identity and optional locator to help find a node on physical networks.
 */
typedef struct
{
	/**
	 * Identity (never NULL)
	 */
	const ZT_Identity *identity;

	/**
	 * Locator (NULL if no locator included)
	 */
	const ZT_Locator *locator;
} ZT_Certificate_Identity;

/**
 * ID and primary controller for a network
 */
typedef struct
{
	/**
	 * Network ID
	 */
	uint64_t id;

	/**
	 * Full fingerprint of primary controller
	 */
	ZT_Fingerprint controller;
} ZT_Certificate_Network;

/**
 * Identification certificate subject
 */
typedef struct
{
	/**
	 * Timestamp of subject, can also be a revision ID for this subject's name.
	 */
	int64_t timestamp;

	/**
	 * Identities and optional locators of nodes
	 */
	ZT_Certificate_Identity *identities;

	/**
	 * Networks owned by this entity
	 */
	ZT_Certificate_Network *networks;

	/**
	 * URLs that can be consulted for updates to this certificate.
	 */
	const char **updateURLs;

	/**
	 * Number of identities
	 */
	unsigned int identityCount;

	/**
	 * Number of networks
	 */
	unsigned int networkCount;

	/**
	 * Number of update URLs
	 */
	unsigned int updateURLCount;

	/**
	 * Information about owner of items.
	 */
	ZT_Certificate_Name name;

	/**
	 * Globally unique ID for this subject
	 *
	 * This is actually a public key and is generated the same way as a normal
	 * certificate public key.
	 */
	uint8_t uniqueId[ZT_CERTIFICATE_MAX_PUBLIC_KEY_SIZE];

	/**
	 * Signature proving ownership of unique ID.
	 */
	uint8_t uniqueIdSignature[ZT_CERTIFICATE_MAX_SIGNATURE_SIZE];

	/**
	 * Size of unique ID in bytes or 0 if none.
	 */
	unsigned int uniqueIdSize;

	/**
	 * Proof signature size or 0 if none.
	 */
	unsigned int uniqueIdSignatureSize;
} ZT_Certificate_Subject;

/**
 * Certificate
 *
 * This is designed to be compatible with x509 certificate interfaces,
 * presenting similar concepts and fields.
 *
 * It's not X509 because we want to keep ZeroTier clean, as simple as
 * possible, small, and secure. X509 is both bloated and a security
 * disaster as it's very hard to implement correctly.
 */
typedef struct
{
	/**
	 * Serial number, a SHA384 hash of this certificate (minus signature).
	 */
	uint8_t serialNo[ZT_CERTIFICATE_HASH_SIZE];

	/**
	 * Certificate usage flags.
	 */
	uint64_t usageFlags;

	/**
	 * Certificate timestamp in milliseconds since epoch.
	 */
	int64_t timestamp;

	/**
	 * Valid time range: not before, not after.
	 */
	int64_t validity[2];

	/**
	 * Subject of certificate
	 */
	ZT_Certificate_Subject subject;

	/**
	 * Issuer certificate serial number.
	 */
	uint8_t issuer[ZT_CERTIFICATE_HASH_SIZE];

	/**
	 * Public key of issuer certificate.
	 */
	uint8_t issuerPublicKey[ZT_CERTIFICATE_MAX_PUBLIC_KEY_SIZE];

	/**
	 * Certificate public key (first byte is ZT_CertificatePublicKeyAlgorithm)
	 */
	uint8_t publicKey[ZT_CERTIFICATE_MAX_PUBLIC_KEY_SIZE];

	/**
	 * Size of issuer public key.
	 */
	unsigned int issuerPublicKeySize;

	/**
	 * Size of public key in bytes
	 */
	unsigned int publicKeySize;

	/**
	 * Extended attributes set by issuer (in Dictionary format, NULL if none)
	 */
	const uint8_t *extendedAttributes;

	/**
	 * Size of extended attributes field in bytes
	 */
	unsigned int extendedAttributesSize;

	/**
	 * Signature by issuer.
	 */
	uint8_t signature[ZT_CERTIFICATE_MAX_SIGNATURE_SIZE];

	/**
	 * Size of signature in bytes.
	 */
	unsigned int signatureSize;

	/**
	 * Maximum path length from this certificate toward further certificates.
	 *
	 * Subjects may sign other certificates whose path lengths are less than
	 * this value. A value of zero indicates that no identification certificates
	 * may be signed (not a CA).
	 */
	unsigned int maxPathLength;
} ZT_Certificate;

/**
 * A revocation for one or more certificates.
 */
typedef struct
{
	/**
	 * Certificate issuing this revocation.
	 */
	uint8_t issuer[ZT_CERTIFICATE_HASH_SIZE];

	/**
	 * Timestamp in milliseconds since epoch.
	 */
	int64_t timestamp;

	/**
	 * Revoked certificate serials.
	 */
	uint8_t serials[ZT_CERTIFICATE_REVOCATION_MAX_CERTIFICATES][ZT_CERTIFICATE_HASH_SIZE];

	/**
	 * Short optional human-readable reason or URL.
	 */
	char reason[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];

	/**
	 * Signature of revocation by revoking issuer.
	 */
	uint8_t signature[ZT_CERTIFICATE_MAX_SIGNATURE_SIZE];

	/**
	 * Number of revoked certificates.
	 */
	unsigned int count;

	/**
	 * Size of signature in bytes.
	 */
	unsigned int signatureSize;
} ZT_CertificateRevocation;

/**
 * A list of certificates
 */
typedef struct
{
	/**
	 * Function that is called to free this list (called by ZT_freeQueryResult)
	 */
	void (*freeFunction)(const void *);

	/**
	 * Array of pointers to certificates
	 */
	const ZT_Certificate *const *certs;

	/**
	 * Array of local trust flags for each certificate
	 */
	const unsigned int *localTrust;

	/**
	 * Number of certificates
	 */
	unsigned long certCount;
} ZT_CertificateList;

/**
 * Credential type IDs
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

/**
 * Endpoint address and protocol types
 *
 * Do not change these. They're used as protocol constants and the
 * fact that IP types start at 5 is exploited in the code.
 *
 * Most of these are not currently implemented and are just reserved
 * for future use.
 */
enum ZT_EndpointType
{
	ZT_ENDPOINT_TYPE_NIL = 0,         /* Nil/empty endpoint */
	ZT_ENDPOINT_TYPE_ZEROTIER = 1,    /* ZeroTier relaying (address+fingerprint) */
	ZT_ENDPOINT_TYPE_ETHERNET = 2,    /* Ethernet with ethertype 0x9993 */
	ZT_ENDPOINT_TYPE_WIFI_DIRECT = 3, /* Ethernet using WiFi direct */
	ZT_ENDPOINT_TYPE_BLUETOOTH = 4,   /* Bluetooth (same address type as Ethernet) */
	ZT_ENDPOINT_TYPE_IP = 5,          /* Naked IP (protocol 193) */
	ZT_ENDPOINT_TYPE_IP_UDP = 6,      /* IP/UDP (the default and original) */
	ZT_ENDPOINT_TYPE_IP_TCP = 7,      /* IP/TCP */
	ZT_ENDPOINT_TYPE_IP_TCP_WS = 8    /* IP/TCP web sockets */
};

/**
 * Maximum numeric value of the ZT_EndpointType enum.
 */
#define ZT_ENDPOINT_TYPE__MAX 8

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
	ZT_TRACE_UNEXPECTED_ERROR = 0,
	ZT_TRACE_VL1_RESETTING_PATHS_IN_SCOPE = 1,
	ZT_TRACE_VL1_TRYING_NEW_PATH = 2,
	ZT_TRACE_VL1_LEARNED_NEW_PATH = 3,
	ZT_TRACE_VL1_INCOMING_PACKET_DROPPED = 4,
	ZT_TRACE_VL2_OUTGOING_FRAME_DROPPED = 100,
	ZT_TRACE_VL2_INCOMING_FRAME_DROPPED = 101,
	ZT_TRACE_VL2_NETWORK_CONFIG_REQUESTED = 102,
	ZT_TRACE_VL2_NETWORK_FILTER = 103,
	ZT_TRACE_VL2_NETWORK_CREDENTIAL_REJECTED = 104,
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
	ZT_TRACE_PACKET_DROP_REASON_UNRECOGNIZED_VERB = 7,
	ZT_TRACE_PACKET_DROP_REASON_REPLY_NOT_EXPECTED = 8
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
 * Reasons for credential rejection
 */
enum ZT_TraceCredentialRejectionReason
{
	ZT_TRACE_CREDENTIAL_REJECTION_REASON_SIGNATURE_VERIFICATION_FAILED = 1,
	ZT_TRACE_CREDENTIAL_REJECTION_REASON_REVOKED = 2,
	ZT_TRACE_CREDENTIAL_REJECTION_REASON_OLDER_THAN_LATEST = 3,
	ZT_TRACE_CREDENTIAL_REJECTION_REASON_INVALID = 4
};

#define ZT_TRACE_FIELD_TYPE                               "t"
#define ZT_TRACE_FIELD_CODE_LOCATION                      "c"
#define ZT_TRACE_FIELD_ENDPOINT                           "e"
#define ZT_TRACE_FIELD_OLD_ENDPOINT                       "oe"
#define ZT_TRACE_FIELD_NEW_ENDPOINT                       "ne"
#define ZT_TRACE_FIELD_TRIGGER_FROM_ENDPOINT              "te"
#define ZT_TRACE_FIELD_TRIGGER_FROM_PACKET_ID             "ti"
#define ZT_TRACE_FIELD_TRIGGER_FROM_PACKET_VERB           "tv"
#define ZT_TRACE_FIELD_TRIGGER_FROM_PEER_FINGERPRINT      "tp"
#define ZT_TRACE_FIELD_MESSAGE                            "m"
#define ZT_TRACE_FIELD_RESET_ADDRESS_SCOPE                "rs"
#define ZT_TRACE_FIELD_IDENTITY_FINGERPRINT               "f"
#define ZT_TRACE_FIELD_PACKET_ID                          "p"
#define ZT_TRACE_FIELD_PACKET_VERB                        "v"
#define ZT_TRACE_FIELD_PACKET_HOPS                        "h"
#define ZT_TRACE_FIELD_NETWORK_ID                         "n"
#define ZT_TRACE_FIELD_REASON                             "r"
#define ZT_TRACE_FIELD_SOURCE_MAC                         "sm"
#define ZT_TRACE_FIELD_DEST_MAC                           "dm"
#define ZT_TRACE_FIELD_ETHERTYPE                          "et"
#define ZT_TRACE_FIELD_VLAN_ID                            "vlid"
#define ZT_TRACE_FIELD_FRAME_LENGTH                       "fl"
#define ZT_TRACE_FIELD_FRAME_DATA                         "fd"
#define ZT_TRACE_FIELD_FLAG_CREDENTIAL_REQUEST_SENT       "crs"
#define ZT_TRACE_FIELD_PRIMARY_RULE_SET_LOG               "rL"
#define ZT_TRACE_FIELD_MATCHING_CAPABILITY_RULE_SET_LOG   "caRL"
#define ZT_TRACE_FIELD_MATCHING_CAPABILITY_ID             "caID"
#define ZT_TRACE_FIELD_MATCHING_CAPABILITY_TIMESTAMP      "caTS"
#define ZT_TRACE_FIELD_SOURCE_ZT_ADDRESS                  "sz"
#define ZT_TRACE_FIELD_DEST_ZT_ADDRESS                    "dz"
#define ZT_TRACE_FIELD_RULE_FLAG_NOTEE                    "rNT"
#define ZT_TRACE_FIELD_RULE_FLAG_INBOUND                  "rIN"
#define ZT_TRACE_FIELD_RULE_FLAG_ACCEPT                   "rACC"
#define ZT_TRACE_FIELD_CREDENTIAL_ID                      "crID"
#define ZT_TRACE_FIELD_CREDENTIAL_TYPE                    "crT"
#define ZT_TRACE_FIELD_CREDENTIAL_TIMESTAMP               "crTS"

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

	/* Fatal errors (>100, <1000) */

	/**
	 * Ran out of memory
	 */
	ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY = 100,

	/**
	 * Data store is not writable or has failed
	 */
	ZT_RESULT_FATAL_ERROR_DATA_STORE_FAILED = 101,

	/**
	 * Internal error fatal to the instance
	 */
	ZT_RESULT_FATAL_ERROR_INTERNAL = 102,

	/* Non-fatal errors (>1000) */

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
	ZT_RESULT_ERROR_BAD_PARAMETER = 1002,

	/**
	 * A credential or other object was supplied that failed cryptographic signature or integrity check
	 */
	ZT_RESULT_ERROR_INVALID_CREDENTIAL = 1003,

	/**
	 * An object collides with another object in some way (meaning is object-specific)
	 */
	ZT_RESULT_ERROR_COLLIDING_OBJECT = 1004,

	/**
	 * An internal error occurred, but one that is not fatal to the whole instance
	 */
	ZT_RESULT_ERROR_INTERNAL = 1005
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

	/* 4 once signaled identity collision but this is no longer an error */

	/**
	 * Trace (debugging) message
	 *
	 * These events are only generated if this is a TRACE-enabled build.
	 * This is for local debug traces, not remote trace diagnostics. The
	 * supplied Dictionary will always be null-terminated.
	 *
	 * Meta-data: null-terminated Dictionary containing trace info
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
	 * Private networks are authorized via membership credentials
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
	/* 0 to 15 reserved for actions */

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
	union
	{
		/**
		 * IPv6 address in big-endian / network byte order and netmask bits
		 */
		struct
		{
			uint8_t ip[16];
			uint8_t mask;
		} ipv6;

		/**
		 * IPv4 address in big-endian / network byte order
		 */
		struct
		{
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
		struct
		{
			uint64_t start; /* integer range start */
			uint32_t end;   /* end of integer range (relative to start, inclusive, 0 for equality w/start) */
			uint16_t idx;   /* index in packet of integer */
			uint8_t format; /* bits in integer (range 1-64, ((format&63)+1)) and endianness (MSB 1 for little, 0 for big) */
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
		struct
		{
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
		struct
		{
			uint8_t type;  /* ICMP type, always matched */
			uint8_t code;  /* ICMP code if matched */
			uint8_t flags; /* flag 0x01 means also match code, otherwise only match type */
		} icmp;

		/**
		 * For tag-related rules
		 */
		struct
		{
			uint32_t id;
			uint32_t value;
		} tag;

		/**
		 * Destinations for TEE and REDIRECT
		 */
		struct
		{
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
	ZT_InetAddress target;

	/**
	 * Gateway IP address (port ignored) or NULL (family == 0) for LAN-local (no gateway)
	 */
	ZT_InetAddress via;

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
	ZT_InetAddress assignedAddresses[ZT_MAX_ZT_ASSIGNED_ADDRESSES];

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
	void (*freeFunction)(const void *);

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
	ZT_InetAddress address;

	/**
	 * If nonzero this address is static and can be incorporated into this node's Locator
	 */
	int permanent;
} ZT_InterfaceAddress;

/**
 * Variant type for storing possible path endpoints or peer contact points.
 */
typedef struct
{
	/**
	 * Endpoint type, which determines what field in the union 'a' applies.
	 */
	enum ZT_EndpointType type;

	union
	{
		/**
		 * ZT_InetAddress, which is identically sized to sockaddr_storage.
		 *
		 * The ZT_InetAddress conversion macros can be used to get this in the
		 * form of a sockaddr, sockaddr_in, etc.
		 */
		ZT_InetAddress ia;

		/* When compiling the ZeroTier core, we want to explicitly define these
		 * in the union. Otherwise we don't because that would require these
		 * structures to be included. */
#ifdef ZT_CORE
		struct sockaddr_storage ss;
		struct sockaddr sa;
		struct sockaddr_in sa_in;
		struct sockaddr_in6 sa_in6;
#endif

		/**
		 * MAC address (least significant 48 bites) for ZT_ENDPOINT_TYPE_ETHERNET and other MAC addressed types
		 */
		uint64_t mac;

		/**
		 * ZeroTier node address and identity fingerprint for ZT_ENDPOINT_TYPE_ZEROTIER
		 */
		ZT_Fingerprint fp;
	} value;
} ZT_Endpoint;

/**
 * Endpoint attributes
 *
 * Right now this is typedef'd to void because there are none. It will become
 * a struct once there's something to specify.
 */
typedef void ZT_EndpointAttributes;

/**
 * Network path to a peer
 */
typedef struct
{
	/**
	 * Path endpoint
	 */
	ZT_Endpoint endpoint;

	/**
	 * Time of last send in milliseconds or 0 for never
	 */
	int64_t lastSend;

	/**
	 * Time of last receive in milliseconds or 0 for never
	 */
	int64_t lastReceive;

	/**
	 * Is path alive?
	 */
	int alive;

	/**
	 * Is path preferred?
	 */
	int preferred;
} ZT_Path;

/**
 * Peer information
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
	 * SHA-384 of identity public key(s)
	 */
	const ZT_Fingerprint *fingerprint;

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
	 * Remote protocol version or -1 if not known
	 */
	int versionProto;

	/**
	 * Last measured latency in milliseconds or -1 if unknown
	 */
	int latency;

	/**
	 * If non-zero this peer is a root
	 */
	int root;

	/**
	 * Network IDs for networks (array size: networkCount)
	 */
	uint64_t *networks;

	/**
	 * Number of networks in which this peer is authenticated
	 */
	unsigned int networkCount;

	/**
	 * Known network paths to peer (array size: pathCount).
	 *
	 * These are direct paths only. Endpoints can also describe indirect paths,
	 * but those would not appear here. Right now those can only be relaying via
	 * a root.
	 */
	ZT_Path *paths;

	/**
	 * Number of paths (size of paths[])
	 */
	unsigned int pathCount;

	/**
	 * Size of locator in bytes or 0 if none
	 */
	unsigned int locatorSize;

	/**
	 * Serialized locator or NULL if none
	 */
	const void *locator;
} ZT_Peer;

/**
 * List of peers
 */
typedef struct
{
	void (*freeFunction)(const void *);

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
	 * Object ID: (none)
	 * Canonical path: <HOME>/identity.public
   * Persistence: required
	 */
	ZT_STATE_OBJECT_IDENTITY_PUBLIC = 1,

	/**
	 * Full identity with secret key
	 *
	 * Object ID: (none)
	 * Canonical path: <HOME>/identity.secret
   * Persistence: required, should be stored with restricted permissions e.g. mode 0600 on *nix
	 */
	ZT_STATE_OBJECT_IDENTITY_SECRET = 2,

	/**
	 * This node's locator
	 *
	 * Object ID: (none)
	 * Canonical path: <HOME>/locator
	 * Persistence: optional
	 */
	ZT_STATE_OBJECT_LOCATOR = 3,

	/**
	 * Peer and related state
	 *
	 * Object ID: [1]address (40 bits, in least significant 64 bits)
	 * Canonical path: <HOME>/peers.d/<ID> (10-digit address)
	 * Persistence: optional, can be cleared at any time
	 */
	ZT_STATE_OBJECT_PEER = 5,

	/**
	 * Network configuration
	 *
	 * Object ID: [1]id (64-bit network ID)
	 * Canonical path: <HOME>/networks.d/<NETWORKID>.conf (16-digit hex ID)
	 * Persistence: required if network memberships should persist
	 */
	ZT_STATE_OBJECT_NETWORK_CONFIG = 6,

	/**
	 * List of certificates, their local trust, and locally added roots
	 *
	 * Object ID: (none)
	 * Canonical path: <HOME>/truststore
	 * Persistence: required if root settings should persist
	 */
	ZT_STATE_OBJECT_TRUST_STORE = 7
};

/**
 * Size of the object ID for peers (in 64-bit uint64_t's)
 */
#define ZT_STATE_OBJECT_PEER_ID_SIZE 1

/**
 * Size of the object ID for network configurations (in 64-bit uint64_t's)
 */
#define ZT_STATE_OBJECT_NETWORK_CONFIG_ID_SIZE 1

/**
 * Size of the object ID for certificates (in 64-bit uint64_t's)
 */
#define ZT_STATE_OBJECT_CERT_ID_SIZE 6

/**
 * An instance of a ZeroTier One node (opaque)
 */
typedef void ZT_Node;

/* ---------------------------------------------------------------------------------------------------------------- */

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
	const void *,                          /* Event payload (if applicable) */
	unsigned int);                         /* Size of event payload */

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
	const uint64_t *,                      /* State object ID (if applicable) */
	unsigned int,                          /* Length of state object ID in quads */
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
	const uint64_t *,                      /* State object ID (if applicable) */
	unsigned int,                          /* Length of object ID in quads */
	void **,                               /* Result parameter: data */
	void (**)(void *));                    /* Result parameter: data free function */

/**
 * Function to send a ZeroTier packet out over the physical wire (L2/L3)
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
	const ZT_InetAddress *,           /* Remote address */
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
	const ZT_InetAddress *);          /* Remote address */

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
	ZT_InetAddress *);                /* Result buffer */

/* ---------------------------------------------------------------------------------------------------------------- */

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

/* ---------------------------------------------------------------------------------------------------------------- */

/**
 * Get a buffer for reading data to be passed back into the core via one of the processX() functions
 *
 * The size of the returned buffer is 16384 bytes (ZT_BUF_SIZE).
 *
 * Buffers retrieved with this method MUST be returned to the core via either one of the processX()
 * functions (with isZtBuffer set to true) or freeBuffer(). Buffers should not be freed directly using free().
 *
 * @return Pointer to I/O buffer
 */
ZT_SDK_API void *ZT_getBuffer();

/**
 * Free an unused buffer obtained via getBuffer
 *
 * @param b Buffer to free
 */
ZT_SDK_API void ZT_freeBuffer(void *b);

/**
 * Free a query result buffer
 *
 * Use this to free the return values of listNetworks(), listPeers(), and
 * other query functions that return allocated structures or buffers.
 *
 * @param qr Query result buffer
 */
ZT_SDK_API void ZT_freeQueryResult(const void *qr);

/* ---------------------------------------------------------------------------------------------------------------- */

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
 * @param callbacks Callback function configuration
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_new(
	ZT_Node **node,
	int64_t clock,
	int64_t ticks,
	void *tptr,
	void *uptr,
	const struct ZT_Node_Callbacks *callbacks);

/**
 * Delete a node and free all resources it consumes
 *
 * If you are using multiple threads, all other threads must be shut down
 * first. This can crash if processXXX() methods are in progress.
 */
ZT_SDK_API void ZT_Node_delete(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr);

/**
 * Process a packet received from the physical wire
 *
 * @param localSocket Local socket (you can use 0 if only one local socket is bound and ignore this)
 * @param remoteAddress Origin of packet
 * @param packetData Packet data
 * @param packetLength Packet length
 * @param isZtBuffer If non-zero then packetData is a buffer obtained with ZT_getBuffer()
 * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to processBackgroundTasks()
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_processWirePacket(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr,
	int64_t localSocket,
	const ZT_InetAddress *remoteAddress,
	const void *packetData,
	unsigned int packetLength,
	int isZtBuffer,
	volatile int64_t *nextBackgroundTaskDeadline);

/**
 * Process a frame from a virtual network port (tap)
 *
 * @param nwid ZeroTier 64-bit virtual network ID
 * @param sourceMac Source MAC address (least significant 48 bits)
 * @param destMac Destination MAC address (least significant 48 bits)
 * @param etherType 16-bit Ethernet frame type
 * @param vlanId 10-bit VLAN ID or 0 if none
 * @param frameData Frame payload data
 * @param frameLength Frame payload length
 * @param isZtBuffer If non-zero then packetData is a buffer obtained with ZT_getBuffer()
 * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to processBackgroundTasks()
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_processVirtualNetworkFrame(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr,
	uint64_t nwid,
	uint64_t sourceMac,
	uint64_t destMac,
	unsigned int etherType,
	unsigned int vlanId,
	const void *frameData,
	unsigned int frameLength,
	int isZtBuffer,
	volatile int64_t *nextBackgroundTaskDeadline);

/**
 * Perform periodic background operations
 *
 * @param now Current clock in milliseconds
 * @param nextBackgroundTaskDeadline Value/result: set to deadline for next call to processBackgroundTasks()
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_processBackgroundTasks(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr,
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
 * @param nwid 64-bit ZeroTier network ID
 * @param fingerprintHash If non-NULL this is the full fingerprint of the controller
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_join(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr,
	void *uptr,
	uint64_t nwid,
	const ZT_Fingerprint *controllerFingerprint);

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
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_leave(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr,
	void **uptr,
	uint64_t nwid);

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
 * @param nwid 64-bit network ID
 * @param multicastGroup Ethernet multicast or broadcast MAC (least significant 48 bits)
 * @param multicastAdi Multicast ADI (least significant 32 bits only, use 0 if not needed)
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_multicastSubscribe(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr,
	uint64_t nwid,
	uint64_t multicastGroup,
	unsigned long multicastAdi);

/**
 * Unsubscribe from an Ethernet multicast group (or all groups)
 *
 * If multicastGroup is zero (0), this will unsubscribe from all groups. If
 * you are not subscribed to a group this has no effect.
 *
 * This does not generate an update call to networkConfigCallback().
 *
 * @param nwid 64-bit network ID
 * @param multicastGroup Ethernet multicast or broadcast MAC (least significant 48 bits)
 * @param multicastAdi Multicast ADI (least significant 32 bits only, use 0 if not needed)
 * @return OK (0) or error code if a fatal error condition has occurred
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_multicastUnsubscribe(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr,
	uint64_t nwid,
	uint64_t multicastGroup,
	unsigned long multicastAdi);

/**
 * Get this node's 40-bit ZeroTier address
 *
 * @return ZeroTier address (least significant 40 bits of 64-bit int)
 */
ZT_SDK_API uint64_t ZT_Node_address(
	ZT_Node *node);

/**
 * Get this node's identity
 *
 * The identity pointer returned by this function need not and should not be
 * freed with ZT_Identity_delete(). It's valid until the node is deleted.
 *
 * @return Identity
 */
ZT_SDK_API const ZT_Identity *ZT_Node_identity(
	ZT_Node *node);

/**
 * Get the status of this node
 *
 * @param status Buffer to fill with current node status
 */
ZT_SDK_API void ZT_Node_status(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr,
	ZT_NodeStatus *status);

/**
 * Get a list of known peer nodes
 *
 * The pointer returned here must be freed with freeQueryResult()
 * when you are done with it.
 *
 * @return List of known peers or NULL on failure
 */
ZT_SDK_API ZT_PeerList *ZT_Node_peers(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr);

/**
 * Get the status of a virtual network
 *
 * The pointer returned here must be freed with freeQueryResult()
 * when you are done with it.
 *
 * @param nwid 64-bit network ID
 * @return Network configuration or NULL if we are not a member of this network
 */
ZT_SDK_API ZT_VirtualNetworkConfig *ZT_Node_networkConfig(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr,
	uint64_t nwid);

/**
 * Enumerate and get status of all networks
 *
 * @param node Node instance
 * @return List of networks or NULL on failure
 */
ZT_SDK_API ZT_VirtualNetworkList *ZT_Node_networks(
	ZT_Node *node);

/**
 * Set the network-associated user-defined pointer for a given network
 *
 * This will have no effect if the network ID is not recognized.
 *
 * @param node Node instance
 * @param nwid Network ID
 * @param ptr New network-associated pointer
 */
ZT_SDK_API void ZT_Node_setNetworkUserPtr(
	ZT_Node *node,
	uint64_t nwid,
	void *ptr);

/**
 * Set external interface addresses where this node could be reached
 *
 * @param node Node instance
 * @param addrs Addresses
 * @param addrCount Number of items in addrs[]
 */
ZT_SDK_API void ZT_Node_setInterfaceAddresses(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr,
	const ZT_InterfaceAddress *addrs,
	unsigned int addrCount);

/**
 * Add a certificate to this node's certificate store
 *
 * This supports adding of certificates as expanded ZT_Certificate structures
 * or as raw data. If 'cert' is NULL then certData/certSize must be set.
 *
 * @param localTrust Local trust flags (ORed together)
 * @param cert Certificate object, or set to NULL if certData and certSize are to be used
 * @param certData Certificate binary data if 'cert' is NULL, NULL otherwise
 * @param certSize Size of certificate binary data, 0 if none
 * @return Certificate error or ZT_CERTIFICATE_ERROR_NONE on success
 */
ZT_SDK_API enum ZT_CertificateError ZT_Node_addCertificate(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr,
	unsigned int localTrust,
	const ZT_Certificate *cert,
	const void *certData,
	unsigned int certSize);

/**
 * Delete a certificate from this node's certificate store
 *
 * Note that deleting CA certificates may also imply deletion of certificates
 * that depend on them for full chain verification.
 *
 * @param node Node instance
 * @param tptr Thread pointer to pass to functions/callbacks resulting from this call
 * @param serialNo 48-byte / 384-bit serial number of certificate to delete
 * @return OK (0) or error code
 */
ZT_SDK_API enum ZT_ResultCode ZT_Node_deleteCertificate(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr,
	const void *serialNo);

/**
 * List certificates installed in this node's trust store
 *
 * @param node Node instance
 * @return List of certificates or NULL on error
 */
ZT_SDK_API ZT_CertificateList *ZT_Node_listCertificates(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr);

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
ZT_SDK_API int ZT_Node_sendUserMessage(
	ZT_Node *node,
	int64_t clock,
	int64_t ticks,
	void *tptr,
	uint64_t dest,
	uint64_t typeId,
	const void *data,
	unsigned int len);

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
ZT_SDK_API void ZT_Node_setController(
	ZT_Node *node,
	void *networkConfigMasterInstance);

/* ---------------------------------------------------------------------------------------------------------------- */

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
ZT_SDK_API ZT_Identity *ZT_Identity_new(enum ZT_IdentityType type);

/**
 * Make a copy of an identity
 *
 * @param id Identity to copy
 * @return Copy, must be freed with ZT_Identity_delete.
 */
ZT_SDK_API ZT_Identity *ZT_Identity_clone(const ZT_Identity *id);

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
ZT_SDK_API unsigned int ZT_Identity_sign(
	const ZT_Identity *id,
	const void *data,
	unsigned int len,
	void *signature,
	unsigned int signatureBufferLength);

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
ZT_SDK_API int ZT_Identity_verify(
	const ZT_Identity *id,
	const void *data,
	unsigned int len,
	const void *signature,
	unsigned int sigLen);

/**
 * Get identity type
 *
 * @param id Identity to query
 * @return Identity type code
 */
ZT_SDK_API enum ZT_IdentityType ZT_Identity_type(const ZT_Identity *id);

/**
 * Convert an identity to its string representation
 *
 * @param id Identity to convert
 * @param buf Buffer to store identity (should be at least about 1024 bytes in length)
 * @param capacity Capacity of buffer
 * @param includePrivate If true include the private key if present
 * @return Pointer to buf or NULL on overflow or other error
 */
ZT_SDK_API char *ZT_Identity_toString(
	const ZT_Identity *id,
	char *buf,
	int capacity,
	int includePrivate);

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
 * Get this identity's full fingerprint
 *
 * @param id Identity to query
 * @return Pointer to fingerprint (remains valid as long as identity itself is valid)
 */
ZT_SDK_API const ZT_Fingerprint *ZT_Identity_fingerprint(const ZT_Identity *id);

/**
 * Compare two identities
 *
 * @param a First identity
 * @param b Second identity
 * @return -1, 0, or 1 if a is less than, equal to, or greater than b
 */
ZT_SDK_API int ZT_Identity_compare(const ZT_Identity *a, const ZT_Identity *b);

/**
 * Delete an identity and free associated memory
 *
 * This should only be used with identities created via Identity_new
 * and Identity_fromString().
 *
 * @param id Identity to delete
 */
ZT_SDK_API void ZT_Identity_delete(const ZT_Identity *id);

/* ---------------------------------------------------------------------------------------------------------------- */

/**
 * Convert an endpoint to a string
 *
 * @param ep Endpoint structure
 * @param buf Buffer to store string (recommended size: 256)
 * @param capacity Capacity of buffer
 * @return String or NULL on error
 */
ZT_SDK_API char *ZT_Endpoint_toString(
	const ZT_Endpoint *ep,
	char *buf,
	int capacity);

/**
 * Parse an endpoint as a string
 *
 * This will automatically detect IP addresses in IP/port format. If one
 * of these is specified rather than a fully specified endpoint it will be
 * parsed as an IP/UDP endpoint.
 *
 * @param ep Endpoint structure to populate
 * @param str String representation of endpoint
 * @return OK (0) or error code
 */
ZT_SDK_API int ZT_Endpoint_fromString(
	ZT_Endpoint *ep,
	const char *str);

/**
 * Decode a binary serialized endpoint
 *
 * @param ep Endpoint structure to populate
 * @param bytes Bytes to decode
 * @param len Length of bytes
 * @return OK (0) or error code
 */
ZT_SDK_API int ZT_Endpoint_fromBytes(
	ZT_Endpoint *ep,
	const void *bytes,
	unsigned int len);

/* ---------------------------------------------------------------------------------------------------------------- */

/**
 * Create and sign a new locator
 *
 * Note that attributes must be either NULL to use defaults for all or there
 * must be an attributes object for each endpoint.
 *
 * @param rev Locator timestamp
 * @param endpoints List of endpoints to store in locator
 * @param endpointAttributes Array of ZT_EndpointAttributes objects or NULL to use defaults
 * @param endpointCount Number of endpoints (maximum: 8)
 * @param signer Identity to sign locator (must include private key)
 * @return Locator or NULL on error (too many endpoints or identity does not have private key)
 */
ZT_SDK_API ZT_Locator *ZT_Locator_create(
	int64_t rev,
	const ZT_Endpoint *endpoints,
	const ZT_EndpointAttributes *endpointAttributes,
	unsigned int endpointCount,
	const ZT_Identity *signer);

/**
 * Decode a serialized locator
 *
 * @param data Data to deserialize
 * @param len Length of data
 * @return Locator or NULL if data is not valid
 */
ZT_SDK_API ZT_Locator *ZT_Locator_unmarshal(
	const void *data,
	unsigned int len);

/**
 * Decode a locator from string format
 *
 * @param str String format locator
 * @return Locator or NULL if string is not valid
 */
ZT_SDK_API ZT_Locator *ZT_Locator_fromString(const char *str);

/**
 * Serialize this locator into a buffer
 *
 * @param loc Locator to serialize
 * @param buf Buffer to store bytes
 * @param bufSize Size of buffer in bytes (needs to be at least 2048 bytes in size)
 * @return Number of bytes stored to buf or -1 on error such as buffer too small
 */
ZT_SDK_API int ZT_Locator_marshal(
	const ZT_Locator *loc,
	void *buf,
	unsigned int bufSize);

/**
 * Get this locator in string format
 *
 * @param loc Locator
 * @param buf Buffer to store string
 * @param capacity Capacity of buffer in bytes (recommended size: 4096)
 * @return Pointer to buffer or NULL if an error occurs
 */
ZT_SDK_API char *ZT_Locator_toString(
	const ZT_Locator *loc,
	char *buf,
	int capacity);

/**
 * Get a locator's revision
 *
 * @param loc Locator to query
 * @return Locator revision
 */
ZT_SDK_API int64_t ZT_Locator_revision(const ZT_Locator *loc);

/**
 * Get a locator's signer
 *
 * @param loc Locator to query
 * @return 40-bit ZeroTier address of signer
 */
ZT_SDK_API uint64_t ZT_Locator_signer(const ZT_Locator *loc);

/**
 * Compare two locators
 *
 * @param a First locator
 * @param b Second locator
 * @return Non-zero if a equals b
 */
ZT_SDK_API int ZT_Locator_equals(const ZT_Locator *a, const ZT_Locator *b);

/**
 * Get the number of endpoints in this locator
 *
 * @param loc Locator to query
 * @return Number of endpoints
 */
ZT_SDK_API unsigned int ZT_Locator_endpointCount(const ZT_Locator *loc);

/**
 * Get a pointer to an endpoint in a locator
 *
 * The returned pointer remains valid as long as the Locator is not deleted.
 *
 * @param ep Endpoint number from 0 to 1 - endpointCount()
 * @return Endpoint or NULL if out of bounds
 */
ZT_SDK_API const ZT_Endpoint *ZT_Locator_endpoint(
	const ZT_Locator *loc,
	unsigned int ep);

/**
 * Verify this locator's signature
 *
 * @param signer Signing identity
 * @return Non-zero if locator is valid
 */
ZT_SDK_API int ZT_Locator_verify(
	const ZT_Locator *loc,
	const ZT_Identity *signer);

/**
 * Delete a locator
 *
 * @param loc Locator to delete
 */
ZT_SDK_API void ZT_Locator_delete(const ZT_Locator *loc);

/* ---------------------------------------------------------------------------------------------------------------- */

/**
 * Get ZeroTier core version
 *
 * @param major Result: major version
 * @param minor Result: minor version
 * @param revision Result: revision
 * @param build Result: build number
 */
ZT_SDK_API void ZT_version(
	int *major,
	int *minor,
	int *revision,
	int *build);

/* ---------------------------------------------------------------------------------------------------------------- */

/**
 * Create a new public/private key pair.
 *
 * This is for use as a certificate public key or as the unique ID for
 * enforcing subject ownership.
 *
 * @param type Type to create
 * @param publicKey Public key buffer
 * @param publicKeySize Result parameter: set to size of public key
 * @param privateKey Private key buffer
 * @param privateKeySize Result parameter: set to size of private key
 * @return OK (0) or error
 */
ZT_SDK_API int ZT_Certificate_newKeyPair(
	enum ZT_CertificatePublicKeyAlgorithm type,
	uint8_t publicKey[ZT_CERTIFICATE_MAX_PUBLIC_KEY_SIZE],
	int *const publicKeySize,
	uint8_t privateKey[ZT_CERTIFICATE_MAX_PRIVATE_KEY_SIZE],
	int *const privateKeySize);

/**
 * Create a new certificate signing request (CSR)
 *
 * A CSR is effectively just an encoded certificate subject.
 * If both uniqueId and uniqueIdPrivate are specified, the subject
 * will be signed with a unique ID. Otherwise these fields are not
 * set. If a unique ID and unique ID signature are present in the
 * supplied subject, these will be ignored.
 *
 * @param subject Subject filled in with fields for CSR
 * @param certificatePublicKey Public key for new certificate
 * @param certificatePublicKeySize Public key size in bytes
 * @param uniqueIdPrivateKey Unique ID private key or NULL if none
 * @param uniqueIdPrivateKeySize Size of unique ID private key
 * @param csr Buffer to hold CSR (recommended size: 16384 bytes)
 * @param csrSize Value/result: size of buffer
 * @return OK (0) or error
 */
ZT_SDK_API int ZT_Certificate_newCSR(
	const ZT_Certificate_Subject *subject,
	const void *certificatePublicKey,
	int certificatePublicKeySize,
	const void *uniqueIdPrivateKey,
	int uniqueIdPrivateKeySize,
	void *csr,
	int *csrSize);

/**
 * Sign a CSR to generate a complete certificate.
 *
 * Note that the signer should fill out timestamp, validity, and other
 * certificate fields before signing. Things outside the subject are
 * filled in (or can be modified) by the signer.
 *
 * The returned certificate must be freed with ZT_Certificate_delete().
 *
 * @param cert Certificate to sign
 * @param issuer Serial number of issuer certificate
 * @param issuerPrivateKey Private key of issuer (also includes public)
 * @param issuerPrivateKeySize Size of private key in bytes
 * @return Signed certificate or NULL on error
 */
ZT_SDK_API ZT_Certificate *ZT_Certificate_sign(
	const ZT_Certificate *cert,
	const uint8_t issuer[ZT_CERTIFICATE_HASH_SIZE],
	const void *issuerPrivateKey,
	int issuerPrivateKeySize);

/**
 * Decode a certificate or CSR
 *
 * A CSR is just the encoded subject part of a certificate. Decoding a CSR
 * results in a certificate whose subject is filled in but nothing else.
 *
 * If no error occurs and the pointer at decodedCert is set to non-NULL,
 * the returned certificate must be freed with ZT_Certificate_delete().
 *
 * @param decodedCert Result parameter: target pointer is set to certificate
 * @param cert Certificate or CSR data
 * @param certSize Size of data
 * @param verify If non-zero, verify signatures and structure
 * @return Certificate error, if any
 */
ZT_SDK_API enum ZT_CertificateError ZT_Certificate_decode(
	const ZT_Certificate **decodedCert,
	const void *cert,
	int certSize,
	int verify);

/**
 * Encode a certificate
 *
 * @param cert Certificate to encode
 * @param encoded Buffer to store certificate (suggested size: 16384)
 * @param encodedSize Value/result: size of certificate encoding buffer
 * @return OK (0) or error
 */
ZT_SDK_API int ZT_Certificate_encode(
	const ZT_Certificate *cert,
	void *encoded,
	int *encodedSize);

/**
 * Verify certificate signatures and internal structure.
 *
 * @param cert Certificate to verify
 * @param clock Clock to check timestamp or -1 to skip this check
 * @return Certificate error or ZT_CERTIFICATE_ERROR_NONE if no errors found.
 */
ZT_SDK_API enum ZT_CertificateError ZT_Certificate_verify(
	const ZT_Certificate *cert,
	int64_t clock);

/**
 * Deep clone a certificate, returning one allocated C-side.
 *
 * The returned certificate must be freed with ZT_Certificate_delete(). This is
 * primarily to make copies of certificates that may contain pointers to objects
 * on the stack, etc., before actually returning them.
 *
 * @param cert Certificate to deep clone
 * @return New certificate with copies of all objects
 */
ZT_SDK_API const ZT_Certificate *ZT_Certificate_clone(const ZT_Certificate *cert);

/**
 * Free a certificate created with ZT_Certificate_decode() or ZT_Certificate_clone()
 *
 * @param cert Certificate to free
 */
ZT_SDK_API void ZT_Certificate_delete(const ZT_Certificate *cert);

/* ---------------------------------------------------------------------------------------------------------------- */

/**
 * Convert a Fingerprint to canonical string form
 *
 * @param fp Fingerprint to convert
 * @param buf Buffer to contain string
 * @param capacity Capacity, must be at least 128 bytes
 * @return Pointer to 'buf' now filled with a string
 */
ZT_SDK_API char *ZT_Fingerprint_toString(const ZT_Fingerprint *fp, char *buf, int capacity);

/**
 * Decode a fingerprint from a string
 *
 * @param fp Buffer to fill with decoded fingerprint
 * @param s String representation of fingerprint
 * @return Non-zero on success
 */
ZT_SDK_API int ZT_Fingerprint_fromString(ZT_Fingerprint *fp, const char *s);

/* ---------------------------------------------------------------------------------------------------------------- */

/*
 * InetAddress casting macros depend on the relevant struct being defined.
 * System headers with sockaddr, sockaddr_in, etc. must have already been
 * included.
 */

#define ZT_InetAddress_ptr_cast_sockaddr_ptr(a) ((struct sockaddr *)(a))
#define ZT_InetAddress_ptr_cast_sockaddr_in_ptr(a) ((struct sockaddr_in *)(a))
#define ZT_InetAddress_ptr_cast_sockaddr_in6_ptr(a) ((struct sockaddr_in6 *)(a))
#define ZT_InetAddress_ptr_cast_sockaddr_storage_ptr(a) ((struct sockaddr_storage *)(a))

#define ZT_InetAddress_ptr_cast_const_sockaddr_ptr(a) ((const struct sockaddr *)(a))
#define ZT_InetAddress_ptr_cast_const_sockaddr_in_ptr(a) ((const struct sockaddr_in *)(a))
#define ZT_InetAddress_ptr_cast_const_sockaddr_in6_ptr(a) ((const struct sockaddr_in6 *)(a))
#define ZT_InetAddress_ptr_cast_const_sockaddr_storage_ptr(a) ((const struct sockaddr_storage *)(a))

#define ZT_InetAddress_cast_sockaddr_ptr(a) ((struct sockaddr *)(&(a)))
#define ZT_InetAddress_cast_sockaddr_in_ptr(a) ((struct sockaddr_in *)(&(a)))
#define ZT_InetAddress_cast_sockaddr_in6_ptr(a) ((struct sockaddr_in6 *)(&(a)))
#define ZT_InetAddress_cast_sockaddr_storage_ptr(a) ((struct sockaddr_storage *)(&(a)))

#define ZT_InetAddress_cast_const_sockaddr_ptr(a) ((const struct sockaddr *)(&(a)))
#define ZT_InetAddress_cast_const_sockaddr_in_ptr(a) ((const struct sockaddr_in *)(&(a)))
#define ZT_InetAddress_cast_const_sockaddr_in6_ptr(a) ((const struct sockaddr_in6 *)(&(a)))
#define ZT_InetAddress_cast_const_sockaddr_storage_ptr(a) ((const struct sockaddr_storage *)(&(a)))

/**
 * Zero the contents of an InetAddress
 */
ZT_SDK_API void ZT_InetAddress_clear(ZT_InetAddress *ia);

/**
 * Convert an IP/port pair to a string
 *
 * @param ia InetAddress to convert
 * @param buf Buffer to store result
 * @param cap Size of buffer, must be at least 64 bytes
 * @return 'buf' is returned
 */
ZT_SDK_API char *ZT_InetAddress_toString(const ZT_InetAddress *ia, char *buf, unsigned int cap);

/**
 * Parse an InetAddress in IP/port format
 *
 * @param ia InetAddress to fill with results
 * @param str String to parse
 * @return Non-zero on success, zero if IP/port is invalid
 */
ZT_SDK_API int ZT_InetAddress_fromString(ZT_InetAddress *ia, const char *str);

/**
 * Set to the value of a sockaddr such as sockaddr_in or sockaddr_in6.
 *
 * This takes a void type pointer to a sockaddr to avoid having to include
 * all the network headers within zerotier.h. It's the responsibility of
 * the caller to ensure that a valid sockaddr is supplied.
 *
 * @param ia InetAddress to fill
 * @param saddr A pointer to a sockaddr
 */
ZT_SDK_API void ZT_InetAddress_set(ZT_InetAddress *ia, const void *saddr);

/**
 * Set raw IP bytes
 *
 * @param ia InetAddress to fill
 * @param ipBytes Raw IP data
 * @param ipLen Length of IP: 4 or 16 for IPv4 or IPv6
 * @param port IP port
 */
ZT_SDK_API void ZT_InetAddress_setIpBytes(ZT_InetAddress *ia, const void *ipBytes, unsigned int ipLen, unsigned int port);

/**
 * Set IP port
 */
ZT_SDK_API void ZT_InetAddress_setPort(ZT_InetAddress *ia, unsigned int port);

/**
 * Get IP port, which can also be used as a CIDR in some use cases.
 */
ZT_SDK_API unsigned int ZT_InetAddress_port(const ZT_InetAddress *ia);

/**
 * Returns non-zero if this InetAddress is nil/zero.
 */
ZT_SDK_API int ZT_InetAddress_isNil(const ZT_InetAddress *ia);

/**
 * Returns non-zero if this is an IPv4 address.
 */
ZT_SDK_API int ZT_InetAddress_isV4(const ZT_InetAddress *ia);

/**
 * Returns non-zero if this is an IPv6 address.
 */
ZT_SDK_API int ZT_InetAddress_isV6(const ZT_InetAddress *ia);

/**
 * Fill buffer with IP address bytes, return length in bytes
 *
 * @param ia InetAddress to access
 * @param buf Buffer with at least 16 bytes of space (to hold IPv6)
 * @return 0 on failure or nil, 4 if buf contains IPv4 IP, 16 if buf contains IPv6 IP
 */
ZT_SDK_API unsigned int ZT_InetAddress_ipBytes(const ZT_InetAddress *ia, void *buf);

/**
 * Classify the network scope of this IP address (local net, global, etc.)
 */
ZT_SDK_API enum ZT_InetAddress_IpScope ZT_InetAddress_ipScope(const ZT_InetAddress *ia);

/**
 * Compare a and b
 *
 * @param a First InetAddress
 * @param b Second InetAddress
 * @return -1, 0, or 1 if a is less than, equal to, or greater than b
 */
ZT_SDK_API int ZT_InetAddress_compare(const ZT_InetAddress *a, const ZT_InetAddress *b);

/* These mirror the values of AF_INET and AF_INET6 for use by Rust and other things that need it. */
ZT_SDK_API const int ZT_AF_INET,ZT_AF_INET6;

/* ---------------------------------------------------------------------------------------------------------------- */

/**
 * Parse a dictionary and invoke 'f' for each key/value pair.
 *
 * This can be used to parse a dictionary such as a network config or trace
 * data supplied with a trace event.
 *
 * Function arguments are: arg, key, length of key (not including terminating null),
 * value, length of value in bytes.
 *
 * @param dict Dictionary in serialized form
 * @param len Maximum length of 'dict' (will also end at first zero)
 * @param f Function to invoke with each key and (binary) value
 * @return Non-zero if dictionary was valid
 */
ZT_SDK_API int ZT_Dictionary_parse(const void *dict, unsigned int len, void *arg, void (*f)(void *, const char *, unsigned int, const void *, unsigned int));

/* ---------------------------------------------------------------------------------------------------------------- */

ZT_SDK_API uint64_t ZT_random();

#ifdef __cplusplus
}
#endif

#endif
