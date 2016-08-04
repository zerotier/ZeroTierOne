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

#include <utility>
#include <algorithm>

#include "Constants.hpp"
#include "../include/ZeroTierOne.h"
#include "CertificateOfMembership.hpp"
#include "Capability.hpp"
#include "Tag.hpp"
#include "Hashtable.hpp"
#include "NetworkConfig.hpp"

namespace ZeroTier {

class Peer;

/**
 * Information related to a peer's participation on a network
 *
 * This structure is not thread-safe and must be locked during use.
 */
class Membership
{
private:
	struct TState
	{
		TState() : lastPushed(0),lastReceived(0) {}
		// Last time we pushed this tag to this peer
		uint64_t lastPushed;
		// Last time we received this tag from this peer
		uint64_t lastReceived;
		// Tag from peer
		Tag tag;
	};

	struct CState
	{
		CState() : lastPushed(0),lastReceived(0) {}
		// Last time we pushed this capability to this peer
		uint64_t lastPushed;
		// Last time we received this capability from this peer
		uint64_t lastReceived;
		// Capability from peer
		Capability cap;
	};

public:
	Membership() :
		_lastPushedCom(0),
		_com(),
		_caps(8),
		_tags(8)
	{
	}

	/**
	 * Send COM and other credentials to this peer if needed
	 *
	 * This checks last pushed times for our COM and for other credentials and
	 * sends VERB_NETWORK_CREDENTIALS if the recipient might need them.
	 *
	 * @param peer Peer that "owns" this membership
	 * @param nconf Network configuration
	 * @param now Current time
	 * @param capIds Capability IDs that this peer might need
	 * @param capCount Number of capability IDs
	 * @param tagIds Tag IDs that this peer might need
	 * @param tagCount Number of tag IDs
	 */
	void sendCredentialsIfNeeded(const Peer &peer,const NetworkConfig &nconf,const uint64_t now,const uint32_t *capIds,const unsigned int capCount,const uint32_t *tagIds,const unsigned int tagCount) const;

	/**
	 * @param nconf Network configuration
	 * @param id Tag ID
	 * @return Pointer to tag or NULL if not found
	 */
	inline const Tag *getTag(const NetworkConfig &nconf,const uint32_t id) const
	{
		const TState *t = _tags.get(id);
		return ((t) ? (((t->lastReceived != 0)&&(t->tag.expiration() < nconf.timestamp)) ? &(t->tag) : (const Tag *)0) : (const Tag *)0);
	}

	/**
	 * @param nconf Network configuration
	 * @param id Capablity ID
	 * @return Pointer to capability or NULL if not found
	 */
	inline const Capability *getCapability(const NetworkConfig &nconf,const uint32_t id) const
	{
		const CState *c = _caps.get(id);
		return ((c) ? (((c->lastReceived != 0)&&(c->cap.expiration() < nconf.timestamp)) ? &(c->cap) : (const Capability *)0) : (const Capability *)0);
	}

	/**
	 * Clean up old or stale entries
	 */
	inline void clean(const uint64_t now)
	{
		uint32_t *i = (uint32_t *)0;
		CState *cs = (CState *)0;
		Hashtable<uint32_t,CState>::Iterator csi(_caps);
		while (csi.next(i,cs)) {
			if ((now - std::max(cs->lastPushed,cs->lastReceived)) > (ZT_NETWORK_COM_DEFAULT_REVISION_MAX_DELTA * 3))
				_caps.erase(*i);
		}

		i = (uint32_t *)0;
		TState *ts = (TState *)0;
		Hashtable<uint32_t,TState>::Iterator tsi(_tags);
		while (tsi.next(i,ts)) {
			if ((now - std::max(ts->lastPushed,ts->lastReceived)) > (ZT_NETWORK_COM_DEFAULT_REVISION_MAX_DELTA * 3))
				_tags.erase(*i);
		}
	}

private:
	// Last time we pushed our COM to this peer
	uint64_t _lastPushedCom;

	// COM from this peer
	CertificateOfMembership _com;

	// Capability-related state
	Hashtable<uint32_t,CState> _caps;

	// Tag-related state
	Hashtable<uint32_t,TState> _tags;
};

} // namespace ZeroTier

#endif
