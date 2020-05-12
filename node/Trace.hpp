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

#ifndef ZT_TRACE_HPP
#define ZT_TRACE_HPP

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

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
class NetworkConfig;
class MAC;
class CertificateOfMembership;
class CertificateOfOwnership;
class Revocation;
class Tag;
class Capability;

/**
 * Remote tracing and trace logging handler
 */
class Trace
{
public:
	/**
	 * Trace verbosity level
	 */
	enum Level
	{
		LEVEL_NORMAL = 0,
		LEVEL_VERBOSE = 10,
		LEVEL_RULES = 15,
		LEVEL_DEBUG = 20,
		LEVEL_INSANE = 30
	};

	/**
	 * Filter rule evaluation result log
	 *
	 * Each rule in a rule set gets a four-bit log entry. A log entry
	 * of zero means not evaluated. Otherwise each four-bit log entry
	 * contains two two-bit values of 01 for 'false' and 10 for 'true'.
	 * As with four-bit rules an 00 value here means this was not
	 * evaluated or was not relevant.
	 */
	class RuleResultLog
	{
	public:
		RuleResultLog() {}

		inline void log(const unsigned int rn,const uint8_t thisRuleMatches,const uint8_t thisSetMatches)
		{
			_l[rn >> 1] |= ( ((thisRuleMatches + 1) << 2) | (thisSetMatches + 1) ) << ((rn & 1) << 2);
		}
		inline void logSkipped(const unsigned int rn,const uint8_t thisSetMatches)
		{
			_l[rn >> 1] |= (thisSetMatches + 1) << ((rn & 1) << 2);
		}

		inline void clear()
		{
			memset(_l,0,sizeof(_l));
		}

		inline const uint8_t *data() const { return _l; }
		inline unsigned int sizeBytes() const { return (ZT_MAX_NETWORK_RULES / 2); }

	private:
		uint8_t _l[ZT_MAX_NETWORK_RULES / 2];
	};

	Trace(const RuntimeEnvironment *renv) :
		RR(renv),
		_byNet(8)
	{
	}

	void resettingPathsInScope(void *const tPtr,const Address &reporter,const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,const InetAddress::IpScope scope);

	void peerConfirmingUnknownPath(void *const tPtr,const uint64_t networkId,Peer &peer,const SharedPtr<Path> &path,const uint64_t packetId,const Packet::Verb verb);

	void peerLinkNowRedundant(void *const tPtr,Peer &peer);
	void peerLinkNoLongerRedundant(void *const tPtr,Peer &peer);

	void peerLinkAggregateStatistics(void *const tPtr,Peer &peer);

	void peerLearnedNewPath(void *const tPtr,const uint64_t networkId,Peer &peer,const SharedPtr<Path> &newPath,const uint64_t packetId);
	void peerRedirected(void *const tPtr,const uint64_t networkId,Peer &peer,const SharedPtr<Path> &newPath);

	void incomingPacketMessageAuthenticationFailure(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const unsigned int hops,const char *reason);
	void incomingPacketInvalid(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const unsigned int hops,const Packet::Verb verb,const char *reason);
	void incomingPacketDroppedHELLO(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const char *reason);

	void outgoingNetworkFrameDropped(void *const tPtr,const SharedPtr<Network> &network,const MAC &sourceMac,const MAC &destMac,const unsigned int etherType,const unsigned int vlanId,const unsigned int frameLen,const char *reason);
	void incomingNetworkAccessDenied(void *const tPtr,const SharedPtr<Network> &network,const SharedPtr<Path> &path,const uint64_t packetId,const unsigned int packetLength,const Address &source,const Packet::Verb verb,bool credentialsRequested);
	void incomingNetworkFrameDropped(void *const tPtr,const SharedPtr<Network> &network,const SharedPtr<Path> &path,const uint64_t packetId,const unsigned int packetLength,const Address &source,const Packet::Verb verb,const MAC &sourceMac,const MAC &destMac,const char *reason);

	void networkConfigRequestSent(void *const tPtr,const Network &network,const Address &controller);
	void networkFilter(
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
		const int accept);

	void credentialRejected(void *const tPtr,const CertificateOfMembership &c,const char *reason);
	void credentialRejected(void *const tPtr,const CertificateOfOwnership &c,const char *reason);
	void credentialRejected(void *const tPtr,const Capability &c,const char *reason);
	void credentialRejected(void *const tPtr,const Tag &c,const char *reason);
	void credentialRejected(void *const tPtr,const Revocation &c,const char *reason);

	void updateMemoizedSettings();

private:
	const RuntimeEnvironment *const RR;

	void _send(void *const tPtr,const Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> &d,const Address &dest);
	void _spamToAllNetworks(void *const tPtr,const Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> &d,const Level level);

	Address _globalTarget;
	Trace::Level _globalLevel;
	Hashtable< uint64_t,std::pair< Address,Trace::Level > > _byNet;
	Mutex _byNet_m;
};

} // namespace ZeroTier

#endif
