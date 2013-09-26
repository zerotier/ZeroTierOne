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

#include "Constants.hpp"
#include "Multicaster.hpp"
#include "Utils.hpp"

namespace ZeroTier {

Multicaster::Multicaster()
{
}

Multicaster::~Multicaster()
{
}

void Multicaster::likesGroup(uint64_t nwid,const Address &a,const MulticastGroup &mg,uint64_t now)
{
	Mutex::Lock _l(_lock);
	_NetInfo &n = _nets[nwid];
	_SubInfo &si = n.subscriptions[_Subscription(a,mg)];
	if (!si.lastLike) { // on first LIKE, we must add to _proximity[mg]
		std::list< Address > &p = n.proximity[mg];
		p.push_front(a);
		si.proximitySlot = p.begin(); // list's iterators remain valid until erase()
	}
	si.lastLike = now;
}

void Multicaster::bringCloser(uint64_t nwid,const Address &a)
{
	Mutex::Lock _l(_lock);

	std::map< uint64_t,_NetInfo >::iterator n(_nets.find(nwid));
	if (n == _nets.end())
		return;

	/* _subscriptions contains pairs of <Address,MulticastGroup>, so we can
	 * easily iterate through all subscriptions for a given address by
	 * starting with the default all-zero MulticastGroup() as lower bound
	 * and stopping when we're not looking at the right address anymore.
	 * Then we can look up _proximity and rapidly splice() the list using
	 * the saved iterator in _SubInfo. */

	std::map< _Subscription,_SubInfo >::iterator s(n->second.subscriptions.lower_bound(_Subscription(a,MulticastGroup())));
	while ((s != n->second.subscriptions.end())&&(s->first.first == a)) {
		std::map< MulticastGroup,std::list< Address > >::iterator p(n->second.proximity.find(s->first.second));
		if (s->second.proximitySlot != p->second.begin())
			p->second.splice(p->second.begin(),p->second,s->second.proximitySlot);
		++s;
	}
}

void Multicaster::clean()
{
	Mutex::Lock _l(_lock);

	uint64_t now = Utils::now();

	for(std::map< uint64_t,_NetInfo >::iterator n(_nets.begin());n!=_nets.end();) {
		for(std::map< _Subscription,_SubInfo >::iterator s(n->second.subscriptions.begin());s!=n->second.subscriptions.end();) {
			if ((now - s->second.lastLike) >= ZT_MULTICAST_LIKE_EXPIRE) {
				std::map< MulticastGroup,std::list< Address > >::iterator p(n->second.proximity.find(s->first.second));
				p->second.erase(s->second.proximitySlot);
				if (p->second.empty())
					n->second.proximity.erase(p);
				n->second.subscriptions.erase(s++);
			} else ++s;
		}

		if (n->second.got.empty()&&n->second.proximity.empty()&&n->second.subscriptions.empty())
			_nets.erase(n++);
		else ++n;
	}
}

} // namespace ZeroTier

