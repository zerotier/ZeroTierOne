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

#ifndef ZT_MEMBERSHIP_HPP
#define ZT_MEMBERSHIP_HPP

#include <cstdint>

#include "Constants.hpp"
#include "Credential.hpp"
#include "Hashtable.hpp"
#include "CertificateOfMembership.hpp"
#include "Capability.hpp"
#include "Tag.hpp"
#include "Revocation.hpp"
#include "NetworkConfig.hpp"

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
	void pushCredentials(const RuntimeEnvironment *RR,void *tPtr,int64_t now,const Address &peerAddress,const NetworkConfig &nconf);

	/**
	 * @return Time we last pushed credentials to this member
	 */
	ZT_ALWAYS_INLINE int64_t lastPushedCredentials() const { return _lastPushedCredentials; }

	/**
	 * Check whether the peer represented by this Membership should be allowed on this network at all
	 *
	 * @param nconf Our network config
	 * @return True if this peer is allowed on this network at all
	 */
	ZT_ALWAYS_INLINE bool isAllowedOnNetwork(const NetworkConfig &nconf) const
	{
		if (nconf.isPublic()) return true; // public network
		if (_com.timestamp() <= _comRevocationThreshold) return false; // COM has been revoked
		return nconf.com.agreesWith(_com); // check timestamp agreement window
	}

	/**
	 * Check whether the peer represented by this Membership owns a given address
	 *
	 * @tparam Type of resource: InetAddress or MAC
	 * @param nconf Our network config
	 * @param r Resource to check
	 * @return True if this peer has a certificate of ownership for the given resource
	 */
	template<typename T>
	ZT_ALWAYS_INLINE bool peerOwnsAddress(const NetworkConfig &nconf,const T &r) const
	{
		if (_isUnspoofableAddress(nconf,r))
			return true;
		uint32_t *k = nullptr;
		CertificateOfOwnership *v = nullptr;
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
	ZT_ALWAYS_INLINE const Tag *getTag(const NetworkConfig &nconf,const uint32_t id) const
	{
		const Tag *const t = _remoteTags.get(id);
		return (((t)&&(_isCredentialTimestampValid(nconf,*t))) ? t : (Tag *)0);
	}

	/**
	 * Clean internal databases of stale entries
	 *
	 * @param now Current time
	 * @param nconf Current network configuration
	 */
	void clean(int64_t now,const NetworkConfig &nconf);

	/**
	 * Generates a key for internal use in indexing credentials by type and credential ID
	 */
	static ZT_ALWAYS_INLINE uint64_t credentialKey(const ZT_CredentialType &t,const uint32_t i) noexcept { return (((uint64_t)t << 32U) | (uint64_t)i); }

	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const CertificateOfMembership &com);
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const Tag &tag);
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const Capability &cap);
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const CertificateOfOwnership &coo);
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const Revocation &rev);

private:
	// This returns true if a resource is an IPv6 NDP-emulated address. These embed the ZT
	// address of the peer and therefore cannot be spoofed, causing peerOwnsAddress() to
	// always return true for them. A certificate is not required for these.
	ZT_ALWAYS_INLINE bool _isUnspoofableAddress(const NetworkConfig &nconf,const MAC &m) const { return false; }
	bool _isUnspoofableAddress(const NetworkConfig &nconf,const InetAddress &ip) const;

	// This compares the remote credential's timestamp to the timestamp in our network config
	// plus or minus the permitted maximum timestamp delta.
	template<typename C>
	ZT_ALWAYS_INLINE bool _isCredentialTimestampValid(const NetworkConfig &nconf,const C &remoteCredential) const
	{
		const int64_t ts = remoteCredential.timestamp();
		if (((ts >= nconf.timestamp) ? (ts - nconf.timestamp) : (nconf.timestamp - ts)) <= nconf.credentialTimeMaxDelta) {
			const int64_t *threshold = _revocations.get(credentialKey(C::credentialType(),remoteCredential.id()));
			return ((!threshold)||(ts > *threshold));
		}
		return false;
	}

	template<typename C>
	ZT_ALWAYS_INLINE void _cleanCredImpl(const NetworkConfig &nconf,Hashtable<uint32_t,C> &remoteCreds)
	{
		uint32_t *k = nullptr;
		C *v = nullptr;
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
		ZT_ALWAYS_INLINE CapabilityIterator(Membership &m,const NetworkConfig &nconf) :
			_hti(m._remoteCaps),
			_k(nullptr),
			_c(nullptr),
			_m(m),
			_nconf(nconf)
		{
		}

		ZT_ALWAYS_INLINE Capability *next()
		{
			while (_hti.next(_k,_c)) {
				if (_m._isCredentialTimestampValid(_nconf,*_c))
					return _c;
			}
			return nullptr;
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
