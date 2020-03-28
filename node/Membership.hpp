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
#include "FlatMap.hpp"
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
	~Membership();

	/**
	 * Send COM and other credentials to this peer
	 *
	 * @param RR Runtime environment
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param to Peer identity
	 * @param nconf My network config
	 */
	void pushCredentials(const RuntimeEnvironment *RR,void *tPtr,int64_t now,const SharedPtr<Peer> &to,const NetworkConfig &nconf);

	/**
	 * @return Time we last pushed credentials to this member
	 */
	ZT_INLINE int64_t lastPushedCredentials() const noexcept { return _lastPushedCredentials; }

	/**
	 * Get a remote member's tag (if we have it)
	 *
	 * @param nconf Network configuration
	 * @param id Tag ID
	 * @return Pointer to tag or NULL if not found
	 */
	ZT_INLINE const Tag *getTag(const NetworkConfig &nconf,const uint32_t id) const noexcept
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
	static ZT_INLINE uint64_t credentialKey(const ZT_CredentialType &t,const uint32_t i) noexcept { return (((uint64_t)t << 32U) | (uint64_t)i); }

	/**
	 * Check whether the peer represented by this Membership owns a given address
	 *
	 * @tparam Type of resource: InetAddress or MAC
	 * @param nconf Our network config
	 * @param r Resource to check
	 * @return True if this peer has a certificate of ownership for the given resource
	 */
	template<typename T>
	ZT_INLINE bool peerOwnsAddress(const NetworkConfig &nconf,const T &r) const noexcept
	{
		if (_isUnspoofableAddress(nconf,r))
			return true;
		for(FlatMap< uint32_t,CertificateOfOwnership >::const_iterator i(_remoteCoos.begin());i!=_remoteCoos.end();++i) {
			if (_isCredentialTimestampValid(nconf,i->second)&&(i->second.owns(r)))
				return true;
		}
		return false;
	}

	/**
	 * Check if our local COM agrees with theirs, with possible memo-ization.
	 *
	 * @param localCom
	 */
	ZT_INLINE bool certificateOfMembershipAgress(const CertificateOfMembership &localCom,const Identity &remoteIdentity)
	{
		if ((_comAgreementLocalTimestamp == localCom.timestamp())&&(_comAgreementRemoteTimestamp == _com.timestamp()))
			return true;
		if (_com.agreesWith(localCom)) {
			// SECURITY: newer network controllers embed the full fingerprint into the COM. If we are
			// joined to a network managed by one of these, our COM will contain one. If it's present
			// we compare vs the other and require them to match. If our COM does not contain a full
			// identity fingerprint we compare by address only, which is a legacy mode supported for
			// old network controllers. Note that this is safe because the controller issues us our COM
			// and in so doing indicates if it's new or old. However this will go away after a while
			// once we can be pretty sure there are no ancient controllers around.
			if (localCom.issuedTo().haveHash()) {
				if (localCom.issuedTo() != _com.issuedTo())
					return false;
			} else {
				// LEGACY: support networks run by old controllers.
				if (localCom.issuedTo().address() != _com.issuedTo().address())
					return false;
			}

			// Remember that these two COMs agreed. If any are updated this is invalidated and a full
			// agreement check will be done again.
			_comAgreementLocalTimestamp = localCom.timestamp();
			_comAgreementRemoteTimestamp = _com.timestamp();

			return true;
		}
		return false;
	}

	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const CertificateOfMembership &com);
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const Tag &tag);
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const Capability &cap);
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const CertificateOfOwnership &coo);
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const Identity &sourcePeerIdentity,const NetworkConfig &nconf,const Revocation &rev);

private:
	// This returns true if a resource is an IPv6 NDP-emulated address. These embed the ZT
	// address of the peer and therefore cannot be spoofed, causing peerOwnsAddress() to
	// always return true for them. A certificate is not required for these.
	ZT_INLINE bool _isUnspoofableAddress(const NetworkConfig &nconf,const MAC &m) const noexcept { return false; }
	bool _isUnspoofableAddress(const NetworkConfig &nconf,const InetAddress &ip) const noexcept;

	// This compares the remote credential's timestamp to the timestamp in our network config
	// plus or minus the permitted maximum timestamp delta.
	template<typename C>
	ZT_INLINE bool _isCredentialTimestampValid(const NetworkConfig &nconf,const C &remoteCredential) const noexcept
	{
		const int64_t ts = remoteCredential.timestamp();
		if (((ts >= nconf.timestamp) ? (ts - nconf.timestamp) : (nconf.timestamp - ts)) <= nconf.credentialTimeMaxDelta) {
			const int64_t *threshold = _revocations.get(credentialKey(C::credentialType(),remoteCredential.id()));
			return ((!threshold)||(ts > *threshold));
		}
		return false;
	}

	template<typename C>
	ZT_INLINE void _cleanCredImpl(const NetworkConfig &nconf,FlatMap<uint32_t,C> &remoteCreds)
	{
		for(typename FlatMap<uint32_t,C>::iterator i(remoteCreds.begin());i!=remoteCreds.end();) {
			if (!_isCredentialTimestampValid(nconf,i->second))
				remoteCreds.erase(i++);
			else ++i;
		}
	}

	// Revocation threshold for COM or 0 if none
	int64_t _comRevocationThreshold;

	// Time we last pushed credentials
	int64_t _lastPushedCredentials;

	// COM timestamps at which we last agreed-- used to memo-ize agreement and avoid having to recompute constantly.
	int64_t _comAgreementLocalTimestamp,_comAgreementRemoteTimestamp;

	// Remote member's latest network COM
	CertificateOfMembership _com;

	// Revocations by credentialKey()
	FlatMap< uint64_t,int64_t > _revocations;

	// Remote credentials that we have received from this member (and that are valid)
	FlatMap< uint32_t,Tag > _remoteTags;
	FlatMap< uint32_t,Capability > _remoteCaps;
	FlatMap< uint32_t,CertificateOfOwnership > _remoteCoos;

public:
	class CapabilityIterator
	{
	public:
		ZT_INLINE CapabilityIterator(Membership &m,const NetworkConfig &nconf) noexcept :
			_hti(m._remoteCaps.begin()),
			_m(m),
			_nconf(nconf)
		{
		}

		ZT_INLINE Capability *next() noexcept
		{
			while (_hti != _m._remoteCaps.end()) {
				FlatMap< uint32_t,Capability >::iterator i(_hti++);
				if (_m._isCredentialTimestampValid(_nconf,i->second))
					return &(i->second);
			}
			return nullptr;
		}

	private:
		FlatMap< uint32_t,Capability >::iterator _hti;
		Membership &_m;
		const NetworkConfig &_nconf;
	};
};

} // namespace ZeroTier

#endif
