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
		_RemoteCredential() : id(ZT_MEMBERSHIP_CRED_ID_UNUSED),lastReceived(0),revocationThreshold(0) {}
		uint64_t id;
		uint64_t lastReceived; // last time we got this credential
		uint64_t revocationThreshold; // credentials before this time are invalid
		T credential;
		inline bool operator<(const _RemoteCredential &c) const { return (id < c.id); }
	};

	template<typename T>
	struct _RemoteCredentialComp
	{
		inline bool operator()(const _RemoteCredential<T> *a,const _RemoteCredential<T> *b) const { return (a->id < b->id); }
		inline bool operator()(const uint64_t a,const _RemoteCredential<T> *b) const { return (a < b->id); }
		inline bool operator()(const _RemoteCredential<T> *a,const uint64_t b) const { return (a->id < b); }
		inline bool operator()(const uint64_t a,const uint64_t b) const { return (a < b); }
	};

	// Used to track push state for network config tags[] and capabilities[] entries
	struct _LocalCredentialPushState
	{
		_LocalCredentialPushState() : lastPushed(0),id(0) {}
		uint64_t lastPushed; // last time we sent our own copy of this credential
		uint64_t id;
	};

public:
	enum AddCredentialResult
	{
		ADD_REJECTED,
		ADD_ACCEPTED_NEW,
		ADD_ACCEPTED_REDUNDANT,
		ADD_DEFERRED_FOR_WHOIS
	};

	/**
	 * Iterator to scan forward through capabilities in ascending order of ID
	 */
	class CapabilityIterator
	{
	public:
		CapabilityIterator(const Membership &m,const NetworkConfig &nconf) :
			_m(&m),
			_c(&nconf),
			_i(&(m._remoteCaps[0])) {}

		inline const Capability *next()
		{
			for(;;) {
				if ((_i != &(_m->_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]))&&((*_i)->id != ZT_MEMBERSHIP_CRED_ID_UNUSED)) {
					const Capability *tmp = &((*_i)->credential);
					if (_m->_isCredentialTimestampValid(*_c,**_i)) {
						++_i;
						return tmp;
					} else ++_i;
				} else {
					return (const Capability *)0;
				}
			}
		}

	private:
		const Membership *_m;
		const NetworkConfig *_c;
		const _RemoteCredential<Capability> *const *_i;
	};
	friend class CapabilityIterator;

	/**
	 * Iterator to scan forward through tags in ascending order of ID
	 */
	class TagIterator
	{
	public:
		TagIterator(const Membership &m,const NetworkConfig &nconf) :
			_m(&m),
			_c(&nconf),
			_i(&(m._remoteTags[0])) {}

		inline const Tag *next()
		{
			for(;;) {
				if ((_i != &(_m->_remoteTags[ZT_MAX_NETWORK_TAGS]))&&((*_i)->id != ZT_MEMBERSHIP_CRED_ID_UNUSED)) {
					const Tag *tmp = &((*_i)->credential);
					if (_m->_isCredentialTimestampValid(*_c,**_i)) {
						++_i;
						return tmp;
					} else ++_i;
				} else {
					return (const Tag *)0;
				}
			}
		}

	private:
		const Membership *_m;
		const NetworkConfig *_c;
		const _RemoteCredential<Tag> *const *_i;
	};
	friend class TagIterator;

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
		if (nconf.isPublic())
			return true;
		if (_com.timestamp().first <= _comRevocationThreshold)
			return false;
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
	inline bool hasCertificateOfOwnershipFor(const NetworkConfig &nconf,const T &r) const
	{
		for(unsigned int i=0;i<ZT_MAX_CERTIFICATES_OF_OWNERSHIP;++i) {
			if (_remoteCoos[i]->id == ZT_MEMBERSHIP_CRED_ID_UNUSED)
				break;
			if ((_isCredentialTimestampValid(nconf,*_remoteCoos[i]))&&(_remoteCoos[i]->credential.owns(r)))
				return true;
		}
		return false;
	}

	/**
	 * @param nconf Network configuration
	 * @param id Tag ID
	 * @return Pointer to tag or NULL if not found
	 */
	const Tag *getTag(const NetworkConfig &nconf,const uint32_t id) const;

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
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const Revocation &rev);

	/**
	 * Validate and add a credential if signature is okay and it's otherwise good
	 */
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,void *tPtr,const NetworkConfig &nconf,const CertificateOfOwnership &coo);

private:
	_RemoteCredential<Tag> *_newTag(const uint64_t id);
	_RemoteCredential<Capability> *_newCapability(const uint64_t id);
	_RemoteCredential<CertificateOfOwnership> *_newCoo(const uint64_t id);
	bool _revokeCom(const Revocation &rev);
	bool _revokeCap(const Revocation &rev,const uint64_t now);
	bool _revokeTag(const Revocation &rev,const uint64_t now);
	bool _revokeCoo(const Revocation &rev,const uint64_t now);

	template<typename C>
	inline bool _isCredentialTimestampValid(const NetworkConfig &nconf,const _RemoteCredential<C> &remoteCredential) const
	{
		if (!remoteCredential.lastReceived)
			return false;
		const uint64_t ts = remoteCredential.credential.timestamp();
		return ( (((ts >= nconf.timestamp) ? (ts - nconf.timestamp) : (nconf.timestamp - ts)) <= nconf.credentialTimeMaxDelta) && (ts > remoteCredential.revocationThreshold) );
	}

	// Last time we pushed MULTICAST_LIKE(s)
	uint64_t _lastUpdatedMulticast;

	// Last time we pushed our COM to this peer
	uint64_t _lastPushedCom;

	// Revocation threshold for COM or 0 if none
	uint64_t _comRevocationThreshold;

	// Remote member's latest network COM
	CertificateOfMembership _com;

	// Sorted (in ascending order of ID) arrays of pointers to remote credentials
	_RemoteCredential<Tag> *_remoteTags[ZT_MAX_NETWORK_TAGS];
	_RemoteCredential<Capability> *_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES];
	_RemoteCredential<CertificateOfOwnership> *_remoteCoos[ZT_MAX_CERTIFICATES_OF_OWNERSHIP];

	// This is the RAM allocated for remote credential cache objects
	_RemoteCredential<Tag> _tagMem[ZT_MAX_NETWORK_TAGS];
	_RemoteCredential<Capability> _capMem[ZT_MAX_NETWORK_CAPABILITIES];
	_RemoteCredential<CertificateOfOwnership> _cooMem[ZT_MAX_CERTIFICATES_OF_OWNERSHIP];

	// Local credential push state tracking
	_LocalCredentialPushState _localTags[ZT_MAX_NETWORK_TAGS];
	_LocalCredentialPushState _localCaps[ZT_MAX_NETWORK_CAPABILITIES];
	_LocalCredentialPushState _localCoos[ZT_MAX_CERTIFICATES_OF_OWNERSHIP];
};

} // namespace ZeroTier

#endif
