/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
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
	 * Send COM and other credentials to this peer
	 *
	 * @param RR Runtime environment
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param peerAddress Address of member peer (the one that this Membership describes)
	 * @param nconf My network config
	 * @param localCapabilityIndex Index of local capability to include (in nconf.capabilities[]) or -1 if none
	 */
	void pushCredentials(const RuntimeEnvironment *RR,void *tPtr,const int64_t now,const Address &peerAddress,const NetworkConfig &nconf,int localCapabilityIndex);

	/**
	 * Check whether we should push MULTICAST_LIKEs to this peer, and update last sent time if true
	 *
	 * @param now Current time
	 * @return True if we should update multicasts
	 */
	inline bool multicastLikeGate(const int64_t now)
	{
		if ((now - _lastUpdatedMulticast) >= ZT_MULTICAST_ANNOUNCE_PERIOD) {
			_lastUpdatedMulticast = now;
			return true;
		}
		return false;
	}

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

	inline bool recentlyAssociated(const int64_t now) const
	{
		return ((_com)&&((now - _com.timestamp()) < ZT_PEER_ACTIVITY_TIMEOUT));
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
	inline bool hasCertificateOfOwnershipFor(const NetworkConfig &nconf,const T &r) const
	{
		uint32_t *k = (uint32_t *)0;
		CertificateOfOwnership *v = (CertificateOfOwnership *)0;
		Hashtable< uint32_t,CertificateOfOwnership >::Iterator i(*(const_cast< Hashtable< uint32_t,CertificateOfOwnership> *>(&_remoteCoos)));
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
	 * Clean internal databases of stale entries
	 *
	 * @param now Current time
	 * @param nconf Current network configuration
	 */
	void clean(const int64_t now,const NetworkConfig &nconf);

	/**
	 * Generates a key for the internal use in indexing credentials by type and credential ID
	 */
	static uint64_t credentialKey(const Credential::Type &t,const uint32_t i) { return (((uint64_t)t << 32) | (uint64_t)i); }

private:
	template<typename C>
	inline bool _isCredentialTimestampValid(const NetworkConfig &nconf,const C &remoteCredential) const
	{
		const int64_t ts = remoteCredential.timestamp();
		if (((ts >= nconf.timestamp) ? (ts - nconf.timestamp) : (nconf.timestamp - ts)) <= nconf.credentialTimeMaxDelta) {
			const int64_t *threshold = _revocations.get(credentialKey(C::credentialType(),remoteCredential.id()));
			return ((!threshold)||(ts > *threshold));
		}
		return false;
	}

	template<typename C>
	inline void _cleanCredImpl(const NetworkConfig &nconf,Hashtable<uint32_t,C> &remoteCreds)
	{
		uint32_t *k = (uint32_t *)0;
		C *v = (C *)0;
		typename Hashtable<uint32_t,C>::Iterator i(remoteCreds);
		while (i.next(k,v)) {
			if (!_isCredentialTimestampValid(nconf,*v))
				remoteCreds.erase(*k);
		}
	}

	// Last time we pushed MULTICAST_LIKE(s)
	int64_t _lastUpdatedMulticast;

	// Revocation threshold for COM or 0 if none
	int64_t _comRevocationThreshold;

	// Remote member's latest network COM
	CertificateOfMembership _com;

	// Revocations by credentialKey()
	Hashtable< uint64_t,int64_t > _revocations;

	// Remote credentials that we have received from this member (and that are valid)
	Hashtable< uint32_t,Tag > _remoteTags;
	Hashtable< uint32_t,Capability > _remoteCaps;
	Hashtable< uint32_t,CertificateOfOwnership > _remoteCoos;

public:
	class CapabilityIterator
	{
	public:
		CapabilityIterator(Membership &m,const NetworkConfig &nconf) :
			_hti(m._remoteCaps),
			_k((uint32_t *)0),
			_c((Capability *)0),
			_m(m),
			_nconf(nconf)
		{
		}

		inline Capability *next()
		{
			while (_hti.next(_k,_c)) {
				if (_m._isCredentialTimestampValid(_nconf,*_c))
					return _c;
			}
			return (Capability *)0;
		}

	private:
		Hashtable< uint32_t,Capability >::Iterator _hti;
		uint32_t *_k;
		Capability *_c;
		Membership &_m;
		const NetworkConfig &_nconf;
	};
};

} // namespace ZeroTier

#endif
