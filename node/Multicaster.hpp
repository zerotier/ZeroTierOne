/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef _ZT_MULTICASTER_HPP
#define _ZT_MULTICASTER_HPP

#include <stdint.h>
#include <string.h>

#include <stdexcept>
#include <map>
#include <list>
#include <algorithm>

#include "Constants.hpp"
#include "Mutex.hpp"
#include "MulticastGroup.hpp"
#include "Utils.hpp"
#include "Address.hpp"

namespace ZeroTier {

/**
 * Multicast propagation algorithm
 */
class Multicaster
{
public:
	Multicaster() {}

	/**
	 * Add or renew a peer's subscription to a multicast group
	 *
	 * @param a Address that LIKEd
	 * @param mg Multicast group
	 * @param now Current time
	 */
	inline void likesGroup(const Address &a,const MulticastGroup &mg,uint64_t now)
	{
		Mutex::Lock _l(_lock);
		_SubInfo &si = _subscriptions[_Subscription(a,mg)];
		if (!si.lastLike) { // on first LIKE, we must add to _proximity[mg]
			std::list< Address > &p = _proximity[mg];
			p.push_front(a);
			si.proximitySlot = p.begin(); // list's iterators remain valid until erase()
		}
		si.lastLike = now;
	}

	/**
	 * Bring a peer closer in terms of propagation priority
	 *
	 * @param a Address to bring closer (e.g. due to unicast message)
	 * @param now Current time
	 */
	inline void bringCloser(const Address &a)
	{
		Mutex::Lock _l(_lock);

		// _subscriptions contains pairs of <Address,MulticastGroup>, so we can
		// easily iterate through all subscriptions for a given address by
		// starting with the default all-zero MulticastGroup() as lower bound
		// and stopping when we're not looking at the right address anymore.
		// Then we can look up _proximity and rapidly splice() the list using
		// the saved iterator in _SubInfo.
		std::map< _Subscription,_SubInfo >::iterator s(_subscriptions.lower_bound(_Subscription(a,MulticastGroup())));
		while ((s != _subscriptions.end())&&(s->first.first == a)) {
			std::map< MulticastGroup,std::list< Address > >::iterator p(_proximity.find(s->first.second));
			if (s->second.proximitySlot != p->second.begin())
				p->second.splice(p->second.begin(),p->second,s->second.proximitySlot);
			++s;
		}
	}

	/**
	 * Indicate that a peer reported that it GOT a multicast
	 *
	 * This only happens on magnet nodes for a propagation.
	 *
	 * @param mcGuid Multicast GUID
	 * @param peer Peer that GOT multicast
	 * @param now Current time
	 */
	inlien void got(const Address &peer,uint64_t mcGuid,uint64_t now)
	{
		Mutex::Lock _l(_lock);
		std::pair< uint64_t,std::set<Address> > &g = _got[mcGuid];
		g.first = now;
		g.second.insert(peer);
	}

	/**
	 * Erase entries for expired LIKEs and GOT records
	 */
	inline void clean(uint64_t now)
	{
		Mutex::Lock _l(_lock);

		for(std::map< uint64_t,std::pair< uint64_t,std::set<Address> > >::iterator g(_got.begin());g!=_got.end();) {
			if ((now - g->second.first) > ZT_MULTICAST_MAGNET_STATE_EXPIRE)
				_got.erase(g++);
			else ++g;
		}

		for(std::map< _Subscription,_SubInfo >::iterator s(_subscriptions.begin());s!=_subscriptions.end();) {
			if ((now - s->second.lastLike) > ZT_MULTICAST_LIKE_EXPIRE) {
				std::map< MulticastGroup,std::list< Address > > p(_proximity.find(s->first.second));
				p->second.erase(s->second.proximitySlot);
				if (p->second.empty())
					_proximity.erase(p);
				_subscriptions.erase(s++);
			} else ++s;
		}
	}

	/**
	 * Pick next hops for a multicast by proximity
	 *
	 * The function or function object must return true if more hops are desired
	 * or false to stop finding new hops and return.
	 *
	 * @param mg Multicast group
	 * @param mcGuid Multicast message GUID (signer and signer unique ID)
	 * @param nextHopFunc Function to call for each address, search stops if it returns false
	 */
	template<typename F>
	inline void getNextHops(const MulticastGroup &mg,uint64_t mcGuid,F nextHopFunc)
	{
		Mutex::Lock _l(_lock);
		std::map< uint64_t,std::pair< uint64_t,std::set< Address > > > g(_got.find(mcGuid));
		std::map< MulticastGroup,std::list< Address > > p(_proximity.find(mg));
		if (p != _proximity.end()) {
			for(std::list< Address >::iterator a(p->second.begin());a!=p->second.end();++a) {
				if ((g == _got.end())||(!g->second.second.count(*a))) {
					if (!nextHopFunc(*a))
						break;
				}
			}
		}
	}

private:
	// GOTs by multicast GUID: time of last GOT, addresses that GOT
	std::map< uint64_t,std::pair< uint64_t,std::set< Address > > > _got;

	// Peer proximity ordering for peers subscribed to each group
	std::map< MulticastGroup,std::list< Address > > _proximity;

	// An address and multicast group tuple
	typedef std::pair<Address,MulticastGroup> _Subscription;

	// Information about a subscription
	struct _SubInfo
	{
		_SubInfo() :
			lastLike(0),
			proximitySlot() {}

		// Time of last MULTICAST_LIKE for this group
		uint64_t lastLike;

		// Slot in corresponding list in _proximity
		std::list< Address >::iterator proximitySlot;
	};

	// Peer subscriptions to multicast groups
	std::map< _Subscription,_SubInfo > _subscriptions;

	Mutex _lock;
};

} // namespace ZeroTier

#endif
