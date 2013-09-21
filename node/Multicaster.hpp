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
#include <vector>
#include <set>
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
		std::map< Address,_PeerInfo >::iterator pi(_peers.find(a));
		if (pi == _peers.end()) {
			pi = _peers.insert(std::pair< Address,_PeerInfo >(a,_PeerInfo())).first;
			_proximity.push_front(a);
			pi->second.proximitySlot = _proximity.begin();
		}
		pi->second.groups[mg] = now;
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
		std::map< Address,_PeerInfo >::iterator pi(_peers.find(a));
		if (pi != _peers.end()) {
			if (pi->second.proximitySlot != _proximity.begin())
				_proximity.splice(_proximity.begin(),_proximity,pi->second.proximitySlot);
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
	 * Erase entries for expired LIKEs
	 */
	inline void clean(uint64_t now)
	{
		Mutex::Lock _l(_lock);

		for(std::map< uint64_t,std::pair< uint64_t,std::set<Address> > >::iterator g(_got.begin());g!=_got.end();) {
			if ((now - g->second.first) > ZT_MULTICAST_MAGNET_STATE_EXPIRE)
				_got.erase(g++);
			else ++g;
		}

		for(std::map< Address,_PeerInfo >::iterator pi(_peers.begin());pi!=_peers.end();) {
			for(std::map< MulticastGroup,uint64_t >::iterator g(pi->second.groups.begin());g!=pi->second.groups.end();) {
				if ((now - g->second) > ZT_MULTICAST_LIKE_EXPIRE)
					pi->second.groups.erase(g++);
				else ++g;
			}
			if (pi->second.groups.empty()) {
				_proximity.erase(pi->second.proximitySlot);
				_peers.erase(pi++);
			} else ++pi;
		}
	}

	/**
	 * Pick next hops for a multicast by proximity
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
		for(std::list< Address >::iterator a(_proximity.begin());a!=_proximity.end();++a) {
			if (((g == _got.end())||(!g->second.second.count(*a)))&&(_peers.find(*a)->second.groups.count(mg))) {
				if (!nextHopFunc(*a))
					break;
			}
		}
	}

private:
	// GOTs by multicast GUID: time of last GOT, addresses that GOT
	std::map< uint64_t,std::pair< uint64_t,std::set< Address > > > _got;

	// Peer proximity ordering
	std::list< Address > _proximity;

	struct _PeerInfo
	{
		// Groups and time of last LIKE for each group
		std::map< MulticastGroup,uint64_t > groups;

		// Peer's slot in _proximity
		std::list< Address >::iterator proximitySlot;
	};

	// Time of last LIKE for each address's group subscriptions
	std::map< Address,_PeerInfo > _peers;

	Mutex _lock;
};

} // namespace ZeroTier

#endif
