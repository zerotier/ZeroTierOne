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

#ifndef ZT_TRACE_HPP
#define ZT_TRACE_HPP

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include "../include/ZeroTierOne.h"

#include "Constants.hpp"
#include "SharedPtr.hpp"
#include "Packet.hpp"
#include "Credential.hpp"
#include "InetAddress.hpp"
#include "Dictionary.hpp"
#include "Mutex.hpp"
#include "Hashtable.hpp"

namespace ZeroTier {

class RuntimeEnvironment;
class Address;
class Identity;
class Peer;
class Path;
class Network;
class MAC;
class CertificateOfMembership;
class CertificateOfOwnership;
class Revocation;
class Tag;
class Capability;
struct NetworkConfig;

/**
 * Remote tracing and trace logging handler
 */
class Trace
{
public:
	class RuleResultLog
	{
	public:
		ZT_ALWAYS_INLINE RuleResultLog() { this->clear(); }

		ZT_ALWAYS_INLINE void log(const unsigned int rn,const uint8_t thisRuleMatches,const uint8_t thisSetMatches) { _l[rn >> 1U] |= ( ((thisRuleMatches + 1U) << 2U) | (thisSetMatches + 1U) ) << ((rn & 1U) << 2U); }
		ZT_ALWAYS_INLINE void logSkipped(const unsigned int rn,const uint8_t thisSetMatches) { _l[rn >> 1U] |= (thisSetMatches + 1U) << ((rn & 1U) << 2U); }
		ZT_ALWAYS_INLINE void clear() { memset(_l,0,sizeof(_l)); }

		ZT_ALWAYS_INLINE const uint8_t *data() const { return _l; }
		ZT_ALWAYS_INLINE unsigned int sizeBytes() const { return (ZT_MAX_NETWORK_RULES / 2); }

	private:
		uint8_t _l[ZT_MAX_NETWORK_RULES / 2];
	};

	inline Trace(const RuntimeEnvironment *renv)
	{
	}

	inline void resettingPathsInScope(void *const tPtr,const Address &reporter,const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,const InetAddress::IpScope scope)
	{
	}

	inline void peerConfirmingUnknownPath(void *const tPtr,const uint64_t networkId,Peer &peer,const SharedPtr<Path> &path,const uint64_t packetId,const Packet::Verb verb)
	{
	}

	inline void peerLinkNowRedundant(void *const tPtr,Peer &peer)
	{
	}

	inline void peerLinkNoLongerRedundant(void *const tPtr,Peer &peer)
	{
	}

	inline void peerLinkAggregateStatistics(void *const tPtr,Peer &peer)
	{
	}

	inline void peerLearnedNewPath(void *const tPtr,const uint64_t networkId,Peer &peer,const SharedPtr<Path> &newPath,const uint64_t packetId)
	{
	}

	inline void incomingPacketMessageAuthenticationFailure(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const unsigned int hops,const char *reason)
	{
	}

	inline void incomingPacketInvalid(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const unsigned int hops,const Packet::Verb verb,const char *reason)
	{
	}

	inline void incomingPacketDroppedHELLO(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const char *reason)
	{
	}

	inline void outgoingNetworkFrameDropped(void *const tPtr,const SharedPtr<Network> &network,const MAC &sourceMac,const MAC &destMac,const unsigned int etherType,const unsigned int vlanId,const unsigned int frameLen,const char *reason)
	{
	}

	inline void incomingNetworkAccessDenied(void *const tPtr,const SharedPtr<Network> &network,const SharedPtr<Path> &path,const uint64_t packetId,const unsigned int packetLength,const Address &source,const Packet::Verb verb,bool credentialsRequested)
	{
	}

	inline void incomingNetworkFrameDropped(void *const tPtr,const SharedPtr<Network> &network,const SharedPtr<Path> &path,const uint64_t packetId,const unsigned int packetLength,const Address &source,const Packet::Verb verb,const MAC &sourceMac,const MAC &destMac,const char *reason)
	{
	}

	inline void networkConfigRequestSent(void *const tPtr,const Network &network,const Address &controller)
	{
	}

	inline void networkFilter(
		void *const tPtr,
		const Network &network,
		const RuleResultLog &primaryRuleSetLog,
		const RuleResultLog *const matchingCapabilityRuleSetLog,
		const Capability *const matchingCapability,
		const Address &ztSource,
		const Address &ztDest,
		const MAC &macSource,
		const MAC &macDest,
		const uint8_t *const frameData,
		const unsigned int frameLen,
		const unsigned int etherType,
		const unsigned int vlanId,
		const bool noTee,
		const bool inbound,
		const int accept)
	{
	}

	inline void credentialRejected(void *const tPtr,const CertificateOfMembership &c,const char *reason)
	{
	}

	inline void credentialRejected(void *const tPtr,const CertificateOfOwnership &c,const char *reason)
	{
	}

	inline void credentialRejected(void *const tPtr,const Capability &c,const char *reason)
	{
	}

	inline void credentialRejected(void *const tPtr,const Tag &c,const char *reason)
	{
	}

	inline void credentialRejected(void *const tPtr,const Revocation &c,const char *reason)
	{
	}
};

} // namespace ZeroTier

#endif
