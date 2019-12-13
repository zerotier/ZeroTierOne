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

#ifndef ZT_NETWORKCONFIG_HPP
#define ZT_NETWORKCONFIG_HPP

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <vector>
#include <stdexcept>
#include <algorithm>

#include "../include/ZeroTierOne.h"

#include "Constants.hpp"
#include "Buffer.hpp"
#include "InetAddress.hpp"
#include "MulticastGroup.hpp"
#include "Address.hpp"
#include "CertificateOfMembership.hpp"
#include "CertificateOfOwnership.hpp"
#include "Capability.hpp"
#include "Tag.hpp"
#include "Dictionary.hpp"
#include "Hashtable.hpp"
#include "Identity.hpp"
#include "Utils.hpp"
#include "Trace.hpp"

namespace ZeroTier {

/**
 * Default maximum time delta for COMs, tags, and capabilities
 *
 * The current value is two hours, providing ample time for a controller to
 * experience fail-over, etc.
 */
#define ZT_NETWORKCONFIG_DEFAULT_CREDENTIAL_TIME_MAX_MAX_DELTA 7200000ULL

/**
 * Default minimum credential TTL and maxDelta for COM timestamps
 *
 * This is just slightly over three minutes and provides three retries for
 * all currently online members to refresh.
 */
#define ZT_NETWORKCONFIG_DEFAULT_CREDENTIAL_TIME_MIN_MAX_DELTA 185000ULL

/**
 * Flag: enable broadcast
 */
#define ZT_NETWORKCONFIG_FLAG_ENABLE_BROADCAST 0x0000000000000002ULL

/**
 * Flag: enable IPv6 NDP emulation for certain V6 address patterns
 */
#define ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION 0x0000000000000004ULL

/**
 * Flag: result of unrecognized MATCH entries in a rules table: match if set, no-match if clear
 */
#define ZT_NETWORKCONFIG_FLAG_RULES_RESULT_OF_UNSUPPORTED_MATCH 0x0000000000000008ULL

/**
 * Flag: disable frame compression
 */
#define ZT_NETWORKCONFIG_FLAG_DISABLE_COMPRESSION 0x0000000000000010ULL

/**
 * Device can bridge to other Ethernet networks and gets unknown recipient multicasts
 */
#define ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE 0x0000020000000000ULL

/**
 * Device that replicates multicasts
 */
#define ZT_NETWORKCONFIG_SPECIALIST_TYPE_MULTICAST_REPLICATOR 0x0000040000000000ULL

/**
 * Device that can probe and receive remote trace info about this network
 */
#define ZT_NETWORKCONFIG_SPECIALIST_TYPE_DIAGNOSTICIAN 0x0000080000000000ULL

// Dictionary capacity needed for max size network config
#define ZT_NETWORKCONFIG_DICT_CAPACITY (1024 + (sizeof(ZT_VirtualNetworkRule) * ZT_MAX_NETWORK_RULES) + (sizeof(Capability) * ZT_MAX_NETWORK_CAPABILITIES) + (sizeof(Tag) * ZT_MAX_NETWORK_TAGS) + (sizeof(CertificateOfOwnership) * ZT_MAX_CERTIFICATES_OF_OWNERSHIP))

// Dictionary capacity needed for max size network meta-data
#define ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY 8192

// Fields for meta-data sent with network config requests

// Protocol version (see Packet.hpp)
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_PROTOCOL_VERSION "pv"
// Software vendor
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_VENDOR "vend"
// Software major version
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MAJOR_VERSION "majv"
// Software minor version
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MINOR_VERSION "minv"
// Software revision
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_REVISION "revv"
// Rules engine revision
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_RULES_ENGINE_REV "revr"
// Maximum number of rules per network this node can accept
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_MAX_NETWORK_RULES "mr"
// Maximum number of capabilities this node can accept
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_MAX_NETWORK_CAPABILITIES "mc"
// Maximum number of rules per capability this node can accept
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_MAX_CAPABILITY_RULES "mcr"
// Maximum number of tags this node can accept
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_MAX_NETWORK_TAGS "mt"
// Network join authorization token (if any)
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_AUTH "a"
// Network configuration meta-data flags
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_FLAGS "f"

// These dictionary keys are short so they don't take up much room.
// By convention we use upper case for binary blobs, but it doesn't really matter.

// network config version
#define ZT_NETWORKCONFIG_DICT_KEY_VERSION "v"
// network ID
#define ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID "nwid"
// integer(hex)
#define ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP "ts"
// integer(hex)
#define ZT_NETWORKCONFIG_DICT_KEY_REVISION "r"
// address of member
#define ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO "id"
// flags(hex)
#define ZT_NETWORKCONFIG_DICT_KEY_FLAGS "f"
// integer(hex)
#define ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT "ml"
// integer(hex)
#define ZT_NETWORKCONFIG_DICT_KEY_TOKEN "k"
// network type (hex)
#define ZT_NETWORKCONFIG_DICT_KEY_TYPE "t"
// text
#define ZT_NETWORKCONFIG_DICT_KEY_NAME "n"
// network MTU
#define ZT_NETWORKCONFIG_DICT_KEY_MTU "mtu"
// credential time max delta in ms
#define ZT_NETWORKCONFIG_DICT_KEY_CREDENTIAL_TIME_MAX_DELTA "ctmd"
// binary serialized certificate of membership
#define ZT_NETWORKCONFIG_DICT_KEY_COM "C"
// specialists (binary array of uint64_t)
#define ZT_NETWORKCONFIG_DICT_KEY_SPECIALISTS "S"
// routes (binary blob)
#define ZT_NETWORKCONFIG_DICT_KEY_ROUTES "RT"
// static IPs (binary blob)
#define ZT_NETWORKCONFIG_DICT_KEY_STATIC_IPS "I"
// rules (binary blob)
#define ZT_NETWORKCONFIG_DICT_KEY_RULES "R"
// capabilities (binary blobs)
#define ZT_NETWORKCONFIG_DICT_KEY_CAPABILITIES "CAP"
// tags (binary blobs)
#define ZT_NETWORKCONFIG_DICT_KEY_TAGS "TAG"
// tags (binary blobs)
#define ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATES_OF_OWNERSHIP "COO"

/**
 * Network configuration received from network controller nodes
 *
 * This is a memcpy()'able structure and is safe (in a crash sense) to modify
 * without locks.
 */
struct NetworkConfig
{
	inline NetworkConfig() :
		networkId(0),
		timestamp(0),
		credentialTimeMaxDelta(0),
		revision(0),
		issuedTo(),
		flags(0),
		mtu(0),
		multicastLimit(0),
		specialistCount(0),
		routeCount(0),
		staticIpCount(0),
		ruleCount(0),
		capabilityCount(0),
		tagCount(0),
		certificateOfOwnershipCount(0),
		type(ZT_NETWORK_TYPE_PRIVATE)
	{
		name[0] = 0;
	}

	/**
	 * Write this network config to a dictionary for transport
	 *
	 * @param d Dictionary
	 * @param includeLegacy If true, include legacy fields for old node versions
	 * @return True if dictionary was successfully created, false if e.g. overflow
	 */
	bool toDictionary(Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> &d,bool includeLegacy) const;

	/**
	 * Read this network config from a dictionary
	 *
	 * @param d Dictionary (non-const since it might be modified during parse, should not be used after call)
	 * @return True if dictionary was valid and network config successfully initialized
	 */
	bool fromDictionary(const Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> &d);

	/**
	 * @return True if broadcast (ff:ff:ff:ff:ff:ff) address should work on this network
	 */
	inline bool enableBroadcast() const { return ((this->flags & ZT_NETWORKCONFIG_FLAG_ENABLE_BROADCAST) != 0); }

	/**
	 * @return True if IPv6 NDP emulation should be allowed for certain "magic" IPv6 address patterns
	 */
	inline bool ndpEmulation() const { return ((this->flags & ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION) != 0); }

	/**
	 * @return True if frames should not be compressed
	 */
	inline bool disableCompression() const
	{
#ifndef ZT_DISABLE_COMPRESSION
		return ((this->flags & ZT_NETWORKCONFIG_FLAG_DISABLE_COMPRESSION) != 0);
#else
		/* Compression is disabled for libzt builds since it causes non-obvious chaotic
		interference with lwIP's TCP congestion algorithm. Compression is also disabled
		for some NAS builds due to the usage of low-performance processors in certain
		older and budget models. */
		return true;
#endif
	}

	/**
	 * @return Network type is public (no access control)
	 */
	inline bool isPublic() const { return (this->type == ZT_NETWORK_TYPE_PUBLIC); }

	/**
	 * @return Network type is private (certificate access control)
	 */
	inline bool isPrivate() const { return (this->type == ZT_NETWORK_TYPE_PRIVATE); }

	/**
	 * @param fromPeer Peer attempting to bridge other Ethernet peers onto network
	 * @return True if this network allows bridging
	 */
	inline bool permitsBridging(const Address &fromPeer) const
	{
		for(unsigned int i=0;i<specialistCount;++i) {
			if ((fromPeer == specialists[i])&&((specialists[i] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE) != 0))
				return true;
		}
		return false;
	}

	inline operator bool() const { return (networkId != 0); }
	inline bool operator==(const NetworkConfig &nc) const { return (memcmp(this,&nc,sizeof(NetworkConfig)) == 0); }
	inline bool operator!=(const NetworkConfig &nc) const { return (!(*this == nc)); }

	/**
	 * Add a specialist or mask flags if already present
	 *
	 * This masks the existing flags if the specialist is already here or adds
	 * it otherwise.
	 *
	 * @param a Address of specialist
	 * @param f Flags (OR of specialist role/type flags)
	 * @return True if successfully masked or added
	 */
	inline bool addSpecialist(const Address &a,const uint64_t f)
	{
		const uint64_t aint = a.toInt();
		for(unsigned int i=0;i<specialistCount;++i) {
			if ((specialists[i] & 0xffffffffffULL) == aint) {
				specialists[i] |= f;
				return true;
			}
		}
		if (specialistCount < ZT_MAX_NETWORK_SPECIALISTS) {
			specialists[specialistCount++] = f | aint;
			return true;
		}
		return false;
	}

	inline const Capability *capability(const uint32_t id) const
	{
		for(unsigned int i=0;i<capabilityCount;++i) {
			if (capabilities[i].id() == id)
				return &(capabilities[i]);
		}
		return (Capability *)0;
	}

	inline const Tag *tag(const uint32_t id) const
	{
		for(unsigned int i=0;i<tagCount;++i) {
			if (tags[i].id() == id)
				return &(tags[i]);
		}
		return (Tag *)0;
	}

	/**
	 * Network ID that this configuration applies to
	 */
	uint64_t networkId;

	/**
	 * Controller-side time of config generation/issue
	 */
	int64_t timestamp;

	/**
	 * Max difference between timestamp and tag/capability timestamp
	 */
	int64_t credentialTimeMaxDelta;

	/**
	 * Controller-side revision counter for this configuration
	 */
	uint64_t revision;

	/**
	 * Address of device to which this config is issued
	 */
	Address issuedTo;

	/**
	 * Flags (64-bit)
	 */
	uint64_t flags;

	/**
	 * Token (64-bit token known only to network members)
	 */
	uint64_t token;

	/**
	 * Network MTU
	 */
	unsigned int mtu;

	/**
	 * Maximum number of recipients per multicast (not including active bridges)
	 */
	unsigned int multicastLimit;

	/**
	 * Number of specialists
	 */
	unsigned int specialistCount;

	/**
	 * Number of routes
	 */
	unsigned int routeCount;

	/**
	 * Number of ZT-managed static IP assignments
	 */
	unsigned int staticIpCount;

	/**
	 * Number of rule table entries
	 */
	unsigned int ruleCount;

	/**
	 * Number of capabilities
	 */
	unsigned int capabilityCount;

	/**
	 * Number of tags
	 */
	unsigned int tagCount;

	/**
	 * Number of certificates of ownership
	 */
	unsigned int certificateOfOwnershipCount;

	/**
	 * Specialist devices
	 *
	 * For each entry the least significant 40 bits are the device's ZeroTier
	 * address and the most significant 24 bits are flags indicating its role.
	 */
	uint64_t specialists[ZT_MAX_NETWORK_SPECIALISTS];

	/**
	 * Statically defined "pushed" routes (including default gateways)
	 */
	ZT_VirtualNetworkRoute routes[ZT_MAX_NETWORK_ROUTES];

	/**
	 * Static IP assignments
	 */
	InetAddress staticIps[ZT_MAX_ZT_ASSIGNED_ADDRESSES];

	/**
	 * Base network rules
	 */
	ZT_VirtualNetworkRule rules[ZT_MAX_NETWORK_RULES];

	/**
	 * Capabilities for this node on this network, in ascending order of capability ID
	 */
	Capability capabilities[ZT_MAX_NETWORK_CAPABILITIES];

	/**
	 * Tags for this node on this network, in ascending order of tag ID
	 */
	Tag tags[ZT_MAX_NETWORK_TAGS];

	/**
	 * Certificates of ownership for this network member
	 */
	CertificateOfOwnership certificatesOfOwnership[ZT_MAX_CERTIFICATES_OF_OWNERSHIP];

	/**
	 * Network type (currently just public or private)
	 */
	ZT_VirtualNetworkType type;

	/**
	 * Network short name or empty string if not defined
	 */
	char name[ZT_MAX_NETWORK_SHORT_NAME_LENGTH + 1];

	/**
	 * Certificate of membership (for private networks)
	 */
	CertificateOfMembership com;
};

} // namespace ZeroTier

#endif
