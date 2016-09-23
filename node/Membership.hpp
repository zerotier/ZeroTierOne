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

namespace ZeroTier {

class RuntimeEnvironment;
class Network;

/**
 * A container for certificates of membership and other network credentials
 *
 * This is kind of analogous to a join table between Peer and Network. It is
 * held by the Network object for each participating Peer.
 *
 * This class is not thread safe. It must be locked externally.
 */
class Membership
{
private:
	// Tags and related state
	struct _RemoteTag
	{
		_RemoteTag() : id(0xffffffffffffffffULL),lastReceived(0),revocationThreshold(0) {}
		// Tag ID (last 32 bits, first 32 bits are set in unused entries to sort them to end)
		uint64_t id;
		// Last time we received THEIR tag (with this ID)
		uint64_t lastReceived;
		// Revocation blacklist threshold or 0 if none
		uint64_t revocationThreshold;
		// THEIR tag
		Tag tag;
	};

	// Credentials and related state
	struct _RemoteCapability
	{
		_RemoteCapability() : id(0xffffffffffffffffULL),lastReceived(0),revocationThreshold(0) {}
		// Capability ID (last 32 bits, first 32 bits are set in unused entries to sort them to end)
		uint64_t id;
		// Last time we received THEIR capability (with this ID)
		uint64_t lastReceived;
		// Revocation blacklist threshold or 0 if none
		uint64_t revocationThreshold;
		// THEIR capability
		Capability cap;
	};

	// Comparison operator for remote credential entries
	template<typename T>
	struct _RemoteCredentialSorter
	{
		inline bool operator()(const T *a,const T *b) const { return (a->id < b->id); }
		inline bool operator()(const uint64_t a,const T *b) const { return (a < b->id); }
		inline bool operator()(const T *a,const uint64_t b) const { return (a->id < b); }
		inline bool operator()(const uint64_t a,const uint64_t b) const { return (a < b); }
	};

	// Used to track push state for network config tags[] and capabilities[] entries
	struct _LocalCredentialPushState
	{
		_LocalCredentialPushState() : lastPushed(0),id(0) {}
		uint64_t lastPushed;
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
				if ((_i != &(_m->_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES]))&&((*_i)->id != 0xffffffffffffffffULL)) {
					const Capability *tmp = &((*_i)->cap);
					if (_m->_isCredentialTimestampValid(*_c,*tmp,**_i)) {
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
		const _RemoteCapability *const *_i;
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
				if ((_i != &(_m->_remoteTags[ZT_MAX_NETWORK_TAGS]))&&((*_i)->id != 0xffffffffffffffffULL)) {
					const Tag *tmp = &((*_i)->tag);
					if (_m->_isCredentialTimestampValid(*_c,*tmp,**_i)) {
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
		const _RemoteTag *const *_i;
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
	 * @param now Current time
	 * @param peerAddress Address of member peer (the one that this Membership describes)
	 * @param nconf My network config
	 * @param localCapabilityIndex Index of local capability to include (in nconf.capabilities[]) or -1 if none
	 * @param force If true, send objects regardless of last push time
	 */
	void pushCredentials(const RuntimeEnvironment *RR,const uint64_t now,const Address &peerAddress,const NetworkConfig &nconf,int localCapabilityIndex,const bool force);

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
		if ((_comRevocationThreshold)&&(_com.timestamp().first <= _comRevocationThreshold))
			return false;
		return nconf.com.agreesWith(_com);
	}

	/**
	 * @param nconf Network configuration
	 * @param id Capablity ID
	 * @return Pointer to capability or NULL if not found
	 */
	const Capability *getCapability(const NetworkConfig &nconf,const uint32_t id) const;

	/**
	 * @param nconf Network configuration
	 * @param id Tag ID
	 * @return Pointer to tag or NULL if not found
	 */
	const Tag *getTag(const NetworkConfig &nconf,const uint32_t id) const;

	/**
	 * Validate and add a credential if signature is okay and it's otherwise good
	 */
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,const NetworkConfig &nconf,const CertificateOfMembership &com);

	/**
	 * Validate and add a credential if signature is okay and it's otherwise good
	 */
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,const NetworkConfig &nconf,const Tag &tag);

	/**
	 * Validate and add a credential if signature is okay and it's otherwise good
	 */
	AddCredentialResult addCredential(const RuntimeEnvironment *RR,const NetworkConfig &nconf,const Capability &cap);

private:
	template<typename C,typename CS>
	inline bool _isCredentialTimestampValid(const NetworkConfig &nconf,const C &cred,const CS &state) const
	{
		const uint64_t ts = cred.timestamp();
		return ( (((ts >= nconf.timestamp) ? (ts - nconf.timestamp) : (nconf.timestamp - ts)) <= nconf.credentialTimeMaxDelta) && (ts > state.revocationThreshold) );
	}

	// Last time we pushed MULTICAST_LIKE(s)
	uint64_t _lastUpdatedMulticast;

	// Last time we checked if credential push was needed
	uint64_t _lastPushAttempt;

	// Last time we pushed our COM to this peer
	uint64_t _lastPushedCom;

	// Revocation threshold for COM or 0 if none
	uint64_t _comRevocationThreshold;

	// Remote member's latest network COM
	CertificateOfMembership _com;

	// Sorted (in ascending order of ID) arrays of pointers to remote tags and capabilities
	_RemoteTag *_remoteTags[ZT_MAX_NETWORK_TAGS];
	_RemoteCapability *_remoteCaps[ZT_MAX_NETWORK_CAPABILITIES];

	// This is the RAM allocated for remote tags and capabilities from which the sorted arrays are populated
	_RemoteTag _tagMem[ZT_MAX_NETWORK_TAGS];
	_RemoteCapability _capMem[ZT_MAX_NETWORK_CAPABILITIES];

	// Local credential push state tracking
	_LocalCredentialPushState _localTags[ZT_MAX_NETWORK_TAGS];
	_LocalCredentialPushState _localCaps[ZT_MAX_NETWORK_CAPABILITIES];
};

} // namespace ZeroTier

#endif
