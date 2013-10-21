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

#include <algorithm>

#include "Topology.hpp"
#include "NodeConfig.hpp"
#include "CMWC4096.hpp"

namespace ZeroTier {

Topology::Topology(const RuntimeEnvironment *renv) :
	_r(renv),
	_amSupernode(false)
{
}

Topology::~Topology()
{
}

void Topology::setSupernodes(const std::map< Identity,std::vector<InetAddress> > &sn)
{
	Mutex::Lock _l(_supernodes_m);

	_supernodes = sn;
	_supernodeAddresses.clear();
	_supernodePeers.clear();
	uint64_t now = Utils::now();

	for(std::map< Identity,std::vector<InetAddress> >::const_iterator i(sn.begin());i!=sn.end();++i) {
		if (i->first != _r->identity) {
			SharedPtr<Peer> p(getPeer(i->first.address()));
			if (!p)
				p = addPeer(SharedPtr<Peer>(new Peer(_r->identity,i->first)));
			for(std::vector<InetAddress>::const_iterator j(i->second.begin());j!=i->second.end();++j)
				p->setPathAddress(*j,true);
			p->setLastUsed(now);
			_supernodePeers.push_back(p);
		}
		_supernodeAddresses.insert(i->first.address());
	}

	_amSupernode = (_supernodes.find(_r->identity) != _supernodes.end());
}

SharedPtr<Peer> Topology::addPeer(const SharedPtr<Peer> &peer)
{
	if (peer->address() == _r->identity.address()) {
		TRACE("BUG: addNewPeer() caught and ignored attempt to add peer for self");
		throw std::logic_error("cannot add peer for self");
	}
	uint64_t now = Utils::now();
	Mutex::Lock _l(_activePeers_m);
	SharedPtr<Peer> p(_activePeers.insert(std::pair< Address,SharedPtr<Peer> >(peer->address(),peer)).first->second);
	p->setLastUsed(now);
	return p;
}

SharedPtr<Peer> Topology::getPeer(const Address &zta)
{
	if (zta == _r->identity.address()) {
		TRACE("BUG: ignored attempt to getPeer() for self, returned NULL");
		return SharedPtr<Peer>();
	}
	uint64_t now = Utils::now();
	Mutex::Lock _l(_activePeers_m);
	std::map< Address,SharedPtr<Peer> >::const_iterator ap(_activePeers.find(zta));
	if ((ap != _activePeers.end())&&(ap->second)) {
		ap->second->setLastUsed(now);
		return ap->second;
	}
	return SharedPtr<Peer>();
}

SharedPtr<Peer> Topology::getBestSupernode(const Address *avoid,unsigned int avoidCount,bool strictAvoid) const
{
	SharedPtr<Peer> bestSupernode;
	unsigned int bestSupernodeLatency = 0xffff;
	uint64_t now = Utils::now();

	Mutex::Lock _l(_supernodes_m);

	if (_supernodePeers.empty())
		return bestSupernode;

	for(std::vector< SharedPtr<Peer> >::const_iterator sn=_supernodePeers.begin();sn!=_supernodePeers.end();) {
		for(unsigned int i=0;i<avoidCount;++i) {
			if (avoid[i] == (*sn)->address())
				goto skip_and_try_next_supernode;
		}
		if ((*sn)->hasActiveDirectPath(now)) {
			unsigned int l = (*sn)->latency();
			if (bestSupernode) {
				if ((l)&&(l < bestSupernodeLatency)) {
					bestSupernodeLatency = l;
					bestSupernode = *sn;
				}
			} else {
				if (l)
					bestSupernodeLatency = l;
				bestSupernode = *sn;
			}
		}
skip_and_try_next_supernode:
		++sn;
	}

	if (bestSupernode) {
		bestSupernode->setLastUsed(now);
		return bestSupernode;
	} else if (strictAvoid)
		return SharedPtr<Peer>();

	for(std::vector< SharedPtr<Peer> >::const_iterator sn=_supernodePeers.begin();sn!=_supernodePeers.end();++sn) {
		if ((*sn)->hasActiveDirectPath(now)) {
			unsigned int l = (*sn)->latency();
			if (bestSupernode) {
				if ((l)&&(l < bestSupernodeLatency)) {
					bestSupernodeLatency = l;
					bestSupernode = *sn;
				}
			} else {
				if (l)
					bestSupernodeLatency = l;
				bestSupernode = *sn;
			}
		}
	}

	if (bestSupernode)
		bestSupernode->setLastUsed(now);
	return bestSupernode;
}

void Topology::clean()
{
}

} // namespace ZeroTier
