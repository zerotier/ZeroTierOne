/*
 * Copyright (c)2019 ZeroTier, Inc.
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
	 */
	void pushCredentials(const RuntimeEnvironment *RR,void *tPtr,const int64_t now,const Address &peerAddress,const NetworkConfig &nconf);

	inline int64_t lastPushedCredentials() { return _lastPushedCredentials; }

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
	 * @param otherNodeIdentity Identity of remote node
	 * @return True if this peer is allowed on this network at all
	 */
	inline bool isAllowedOnNetwork(const NetworkConfig &thisNodeNetworkConfig, const Identity &otherNodeIdentity) const
	{
		return (thisNodeNetworkConfig.isPublic() || (((_com.timestamp() > _comRevocationThreshold) && (thisNodeNetworkConfig.com.agreesWith(_com, otherNodeIdentity)))));
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
		return _isV6NDPEmulated(nconf,r);
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
	inline bool _isV6NDPEmulated(const NetworkConfig &nconf,const MAC &m) const { return false; }
	inline bool _isV6NDPEmulated(const NetworkConfig &nconf,const InetAddress &ip) const
	{
		if ((ip.isV6())&&(nconf.ndpEmulation())) {
			const InetAddress sixpl(InetAddress::makeIpv66plane(nconf.networkId,nconf.issuedTo.toInt()));
			for(unsigned int i=0;i<nconf.staticIpCount;++i) {
				if (nconf.staticIps[i].ipsEqual(sixpl)) {
					bool prefixMatches = true;
					for(unsigned int j=0;j<5;++j) { // check for match on /40
						if ((((const struct sockaddr_in6 *)&ip)->sin6_addr.s6_addr)[j] != (((const struct sockaddr_in6 *)&sixpl)->sin6_addr.s6_addr)[j]) {
							prefixMatches = false;
							break;
						}
					}
					if (prefixMatches)
						return true;
					break;
				}
			}

			const InetAddress rfc4193(InetAddress::makeIpv6rfc4193(nconf.networkId,nconf.issuedTo.toInt()));
			for(unsigned int i=0;i<nconf.staticIpCount;++i) {
				if (nconf.staticIps[i].ipsEqual(rfc4193)) {
					bool prefixMatches = true;
					for(unsigned int j=0;j<11;++j) { // check for match on /88
						if ((((const struct sockaddr_in6 *)&ip)->sin6_addr.s6_addr)[j] != (((const struct sockaddr_in6 *)&rfc4193)->sin6_addr.s6_addr)[j]) {
							prefixMatches = false;
							break;
						}
					}
					if (prefixMatches)
						return true;
					break;
				}
			}
		}
		return false;
	}

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

	// Time we last pushed credentials
	int64_t _lastPushedCredentials;

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
