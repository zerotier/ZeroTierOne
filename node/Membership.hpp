/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#ifndef ZT_MEMBERSHIP_HPP
#define ZT_MEMBERSHIP_HPP

#include <stdint.h>

#include "Constants.hpp"
#include "../include/ZeroTierOne.h"
#include "Credential.hpp"
#include "Hashtable.hpp"
#include "CertificateOfMembership.hpp"
#include "Capability.hpp"
#include "Tag.hpp"
#include "Revocation.hpp"
#include "NetworkConfig.hpp"

#define ZT_MEMBERSHIP_CRED_ID_UNUSED 0xffffffffffffffffULL

namespace ZeroTier {

class RuntimeEnvironment;
class Network;

/**
 * A container for certificates of membership and other network credentials
 *
 * This is essentially a relational join between Peer and Network.
 *
 * This class is not thread safe. It must be locked externally.
 */
class Membership
{
private:
	template<typename T>
	struct _RemoteCredential
	{
		_RemoteCredential() : lastReceived(0),revocationThreshold(0),credential() {}
		uint64_t lastReceived; // last time we got this credential
		uint64_t revocationThreshold; // credentials before this time are invalid
		T credential;
	};

	struct _LocalCredentialPushState
	{
		_LocalCredentialPushState() : lastPushed(0),id(0) {}
		uint64_t lastPushed; // last time we sent our own copy of this credential
		uint32_t id;
	};

public:
	enum AddCredentialResult
	{
		ADD_REJECTED,
		ADD_ACCEPTED_NEW,
		ADD_ACCEPTED_REDUNDANT,
		ADD_DEFERRED_FOR_WHOIS
	};

	Membership();

	/**
	 * Send COM and other credentials to this peer if needed
	 *
	 * This checks last pushed times for our COM and for other credentials and
	 * sends VERB_NETWORK_CREDENTIALS if the recipient might need them.
	 *
	 * @param RR Runtime environment
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param peerAddress Address of member peer (the one that this Membership describes)
	 * @param nconf My network config
	 * @param localCapabilityIndex Index of local capability to include (in nconf.capabilities[]) or -1 if none
	 * @param force If true, send objects regardless of last push time
	 */
	void pushCredentials(const RuntimeEnvironment *RR,void *tPtr,const uint64_t now,const Address &peerAddress,const NetworkConfig &nconf,int localCapabilityIndex,const bool force);

	/**
	 * Check whether we should push MULTICAST_LIKEs to this peer
	 *
	 * @param now Current time
	 * @return True if we should update multicasts
	 */
	inline bool shouldLikeMulticasts(const uint64_t now) const { return ((now - _lastUpdatedMulticast) >= ZT_MULTICAST_ANNOUNCE_PERIOD); }

	/**
	 * Set time we last updated multicasts for this peer
	 *
	 * @param now Current time
	 */
	inline void likingMulticasts(const uint64_t now) { _lastUpdatedMulticast = now; }

	/**
	 * Check whether the peer represented by this Membership should be allowed on this network at all
	 *
	 * @param nconf Our network config
	 * @return True if this peer is allowed on this network at all
	 */
	inline bool isAllowedOnNetwork(const NetworkConfig &nconf) const
	{
		if (nconf.isPublic()) return true;
		if (_com.timestamp() <= _comRevocationThreshold) return false;
		return nconf.com.agreesWith(_com);
	}

	/**
	 * Check whether the peer represented by this Membership owns a given resource
	 *
	 * @tparam Type of resource: InetAddress or MAC
	 * @param nconf Our network config
	 * @param r Resource to check
	 * @return True if this peer has a certificate of ownership for the given resource
	 */
	template<typename T>
	inline bool hasCertificateOfOwnershipFor(const NetworkConfig &nconf,const T &r)
	{
		uint32_t *k = (uint32_t *)0;
		CertificateOfOwnership *v = (CertificateOfOwnership *)0;
		Hashtable< uint32_t,CertificateOfOwnership >::Iterator i(_remoteCoos);
		while (i.next(k,v)) {
			if (_isCredentialTimestampValid(nconf,*v)&&(v->owns(r)))
				return true;
		}
		return false;
	}

	/**
	 * Get a remote member's tag (if we have it)
	 *
	 * @param nconf Network configuration
	 * @param id Tag ID
	 * @return Pointer to tag or NULL if not found
	 */
	inline const Tag *getTag(const NetworkConfig &nconf,const uint32_t id) const
	{
		const Tag *const t = _remoteTags.get(id);
		return (((t)&&(_isCredentialTimestampValid(nconf,*t))) ? t : (Tag *)0);
	}

	/**
	 * Validate and add a credential if signature is okay and it's otherwise good
	 */
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const CertificateOfMembership &com);

	/**
	 * Validate and add a credential if signature is okay and it's otherwise good
	 */
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const Tag &tag);

	/**
	 * Validate and add a credential if signature is okay and it's otherwise good
	 */
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const Capability &cap);

	/**
	 * Validate and add a credential if signature is okay and it's otherwise good
	 */
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const CertificateOfOwnership &coo);

	/**
	 * Validate and add a credential if signature is okay and it's otherwise good
	 */
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const Revocation &rev);

	/**
	 * Generates a key for the internal revocation tracking hash table
	 *
	 * @param t Credential type
	 * @param i Credential ID
	 * @return Key for tracking revocations of this credential
	 */
	static uint64_t revocationKey(const Credential::Type &t,const uint32_t i) { return (((uint64_t)t << 32) | (uint64_t)i); }

private:
	template<typename C>
	inline bool _isCredentialTimestampValid(const NetworkConfig &nconf,const C &remoteCredential) const
	{
		const uint64_t ts = remoteCredential.timestamp();
		if (((ts >= nconf.timestamp) ? (ts - nconf.timestamp) : (nconf.timestamp - ts)) <= nconf.credentialTimeMaxDelta) {
			const uint64_t *threshold = _revocations.get(revocationKey(C::credentialType(),remoteCredential.id()));
			return ((!threshold)||(ts > *threshold));
		}
		return false;
	}

	// Last time we pushed MULTICAST_LIKE(s)
	uint64_t _lastUpdatedMulticast;

	// Last time we pushed our COM to this peer
	uint64_t _lastPushedCom;

	// Revocation threshold for COM or 0 if none
	uint64_t _comRevocationThreshold;

	// Remote member's latest network COM
	CertificateOfMembership _com;

	// Revocations
	Hashtable< uint64_t,uint64_t > _revocations;

	// Remote credentials and credential state
	Hashtable< uint32_t,Tag > _remoteTags;
	Hashtable< uint32_t,Capability > _remoteCaps;
	Hashtable< uint32_t,CertificateOfOwnership > _remoteCoos;

	// Local credential push state tracking
	_LocalCredentialPushState _localTags[ZT_MAX_NETWORK_TAGS];
	_LocalCredentialPushState _localCaps[ZT_MAX_NETWORK_CAPABILITIES];
	_LocalCredentialPushState _localCoos[ZT_MAX_CERTIFICATES_OF_OWNERSHIP];
};

} // namespace ZeroTier

#endif
