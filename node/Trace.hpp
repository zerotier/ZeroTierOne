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
class CertificateOfRepresentation;
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

	Trace(const RuntimeEnvironment *renv) : RR(renv) {}

	void resettingPathsInScope(void *const tPtr,const Address &reporter,const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,const InetAddress::IpScope scope);
	void txTimedOut(void *const tPtr,const Address &destination);

	void peerConfirmingUnknownPath(void *const tPtr,const uint64_t networkId,Peer &peer,const SharedPtr<Path> &path,const uint64_t packetId,const Packet::Verb verb);
	void peerLearnedNewPath(void *const tPtr,const uint64_t networkId,Peer &peer,const SharedPtr<Path> &oldPath,const SharedPtr<Path> &newPath,const uint64_t packetId);
	void peerRedirected(void *const tPtr,const uint64_t networkId,Peer &peer,const SharedPtr<Path> &oldPath,const SharedPtr<Path> &newPath);

	void incomingPacketTrustedPath(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const uint64_t trustedPathId,bool approved);
	void incomingPacketMessageAuthenticationFailure(void *const tPtr,const SharedPtr<Path> &path,const uint64_t packetId,const Address &source,const unsigned int hops);
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
	void credentialRejected(void *const tPtr,const CertificateOfRepresentation &c,const char *reason);
	void credentialRejected(void *const tPtr,const Capability &c,const char *reason);
	void credentialRejected(void *const tPtr,const Tag &c,const char *reason);
	void credentialRejected(void *const tPtr,const Revocation &c,const char *reason);
	void credentialAccepted(void *const tPtr,const CertificateOfMembership &c);
	void credentialAccepted(void *const tPtr,const CertificateOfOwnership &c);
	void credentialAccepted(void *const tPtr,const CertificateOfRepresentation &c);
	void credentialAccepted(void *const tPtr,const Capability &c);
	void credentialAccepted(void *const tPtr,const Tag &c);
	void credentialAccepted(void *const tPtr,const Revocation &c);

private:
	const RuntimeEnvironment *const RR;

	void _send(void *const tPtr,const Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> &d);
	void _send(void *const tPtr,const Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> &d,const uint64_t networkId);
	void _send(void *const tPtr,const Dictionary<ZT_MAX_REMOTE_TRACE_SIZE> &d,const Network &network);

#ifdef ZT_TRACE
	char _traceMsgBuf[4096];
#endif
};

} // namespace ZeroTier

#endif
