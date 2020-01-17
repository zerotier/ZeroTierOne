/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "Topology.hpp"

namespace ZeroTier {

struct _RootSortComparisonOperator
{
	ZT_ALWAYS_INLINE _RootSortComparisonOperator(const int64_t now) : _now(now) {}
	ZT_ALWAYS_INLINE bool operator()(const SharedPtr<Peer> &a,const SharedPtr<Peer> &b)
	{
		const int64_t now = _now;
		if (a->alive(now)) {
			if (b->alive(now))
				return (a->latency() < b->latency());
			return true;
		}
		return false;
	}
	const int64_t _now;
};

Topology::Topology(const RuntimeEnvironment *renv,const Identity &myId) :
	RR(renv),
	_myIdentity(myId),
	_numConfiguredPhysicalPaths(0),
	_peers(128),
	_paths(256)
{
}

Topology::~Topology()
{
}

SharedPtr<Peer> Topology::add(void *tPtr,const SharedPtr<Peer> &peer)
{
	RWMutex::Lock _l(_peers_l);
	SharedPtr<Peer> &hp = _peers[peer->address()];
	if (hp)
		return hp;
	_loadCached(tPtr,peer->address(),hp);
	if (hp)
		return hp;
	hp = peer;
	return peer;
}

void Topology::getAllPeers(std::vector< SharedPtr<Peer> > &allPeers) const
{
	RWMutex::RLock l(_peers_l);
	allPeers.clear();
	allPeers.reserve(_peers.size());
	Hashtable< Address,SharedPtr<Peer> >::Iterator i(*(const_cast<Hashtable< Address,SharedPtr<Peer> > *>(&_peers)));
	Address *a = (Address *)0;
	SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
	while (i.next(a,p))
		allPeers.push_back(*p);
}

void Topology::setPhysicalPathConfiguration(const struct sockaddr_storage *pathNetwork,const ZT_PhysicalPathConfiguration *pathConfig)
{
	if (!pathNetwork) {
		_numConfiguredPhysicalPaths = 0;
	} else {
		std::map<InetAddress,ZT_PhysicalPathConfiguration> cpaths;
		for(unsigned int i=0,j=_numConfiguredPhysicalPaths;i<j;++i)
			cpaths[_physicalPathConfig[i].first] = _physicalPathConfig[i].second;

		if (pathConfig) {
			ZT_PhysicalPathConfiguration pc(*pathConfig);

			if (pc.mtu <= 0)
				pc.mtu = ZT_DEFAULT_PHYSMTU;
			else if (pc.mtu < ZT_MIN_PHYSMTU)
				pc.mtu = ZT_MIN_PHYSMTU;
			else if (pc.mtu > ZT_MAX_PHYSMTU)
				pc.mtu = ZT_MAX_PHYSMTU;

			cpaths[*(reinterpret_cast<const InetAddress *>(pathNetwork))] = pc;
		} else {
			cpaths.erase(*(reinterpret_cast<const InetAddress *>(pathNetwork)));
		}

		unsigned int cnt = 0;
		for(std::map<InetAddress,ZT_PhysicalPathConfiguration>::const_iterator i(cpaths.begin());((i!=cpaths.end())&&(cnt<ZT_MAX_CONFIGURABLE_PATHS));++i) {
			_physicalPathConfig[cnt].first = i->first;
			_physicalPathConfig[cnt].second = i->second;
			++cnt;
		}
		_numConfiguredPhysicalPaths = cnt;
	}
}

void Topology::addRoot(const Identity &id)
{
	if (id == _myIdentity) return; // sanity check
	RWMutex::Lock l1(_peers_l);
	std::pair< std::set<Identity>::iterator,bool > ir(_roots.insert(id));
	if (ir.second) {
		SharedPtr<Peer> &p = _peers[id.address()];
		if (!p) {
			p.set(new Peer(RR));
			p->init(_myIdentity,id);
		}
		_rootPeers.push_back(p);
	}
}

bool Topology::removeRoot(const Identity &id)
{
	RWMutex::Lock l1(_peers_l);
	std::set<Identity>::iterator r(_roots.find(id));
	if (r != _roots.end()) {
		for(std::vector< SharedPtr<Peer> >::iterator p(_rootPeers.begin());p!=_rootPeers.end();++p) {
			if ((*p)->identity() == id) {
				_rootPeers.erase(p);
				break;
			}
		}
		_roots.erase(r);
		return true;
	}
	return false;
}

void Topology::rankRoots(const int64_t now)
{
	RWMutex::Lock l1(_peers_l);
	std::sort(_rootPeers.begin(),_rootPeers.end(),_RootSortComparisonOperator(now));
}

void Topology::doPeriodicTasks(void *tPtr,const int64_t now)
{
	{
		RWMutex::Lock l1(_peers_l);
		Hashtable< Address,SharedPtr<Peer> >::Iterator i(_peers);
		Address *a = (Address *)0;
		SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
		while (i.next(a,p)) {
			if ( (!(*p)->alive(now)) && (_roots.count((*p)->identity()) == 0) ) {
				(*p)->save(tPtr);
				_peers.erase(*a);
			}
		}
	}
	{
		RWMutex::Lock l1(_paths_l);
		Hashtable< Path::HashKey,SharedPtr<Path> >::Iterator i(_paths);
		Path::HashKey *k = (Path::HashKey *)0;
		SharedPtr<Path> *p = (SharedPtr<Path> *)0;
		while (i.next(k,p)) {
			if (p->references() <= 1)
				_paths.erase(*k);
		}
	}
}

void Topology::saveAll(void *tPtr)
{
	RWMutex::RLock l(_peers_l);
	Hashtable< Address,SharedPtr<Peer> >::Iterator i(_peers);
	Address *a = (Address *)0;
	SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
	while (i.next(a,p)) {
		if ( (!(*p)->alive(RR->node->now())) && (_roots.count((*p)->identity()) == 0) ) {
			(*p)->save((void *)0);
		}
	}
}

void Topology::_loadCached(void *tPtr,const Address &zta,SharedPtr<Peer> &peer)
{
}

} // namespace ZeroTier
