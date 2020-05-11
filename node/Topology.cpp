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

#include "Topology.hpp"

namespace ZeroTier {

Topology::Topology(const RuntimeEnvironment *renv,void *tPtr) :
	RR(renv),
	m_numConfiguredPhysicalPaths(0)
{
	uint64_t idtmp[2]; idtmp[0] = 0; idtmp[1] = 0;
	Vector<uint8_t> data(RR->node->stateObjectGet(tPtr,ZT_STATE_OBJECT_ROOTS,idtmp));
	if (!data.empty()) {
		uint8_t *dptr = data.data();
		int drem = (int)data.size();
		while (drem > 0) {
			Identity id;
			int l = id.unmarshal(dptr,drem);
			if (l > 0) {
				m_roots.insert(id);
				dptr += l;
				drem -= l;
				ZT_SPEW("loaded root %s",id.address().toString().c_str());
			}
		}
	}

	for(Set<Identity>::const_iterator r(m_roots.begin());r != m_roots.end();++r) {
		SharedPtr<Peer> p;
		m_loadCached(tPtr,r->address(),p);
		if ((!p)||(p->identity() != *r)) {
			p.set(new Peer(RR));
			p->init(*r);
		}
		m_rootPeers.push_back(p);
		m_peers[p->address()] = p;
	}
}

SharedPtr<Peer> Topology::add(void *tPtr,const SharedPtr<Peer> &peer)
{
	RWMutex::Lock _l(m_peers_l);
	SharedPtr<Peer> &hp = m_peers[peer->address()];
	if (hp)
		return hp;
	m_loadCached(tPtr,peer->address(),hp);
	if (hp)
		return hp;
	hp = peer;
	return peer;
}

PeerList Topology::peersByProbeToken(const uint32_t probeToken) const
{
	Mutex::Lock l(m_peersByProbeToken_l);
	std::pair< MultiMap< uint32_t,SharedPtr<Peer> >::const_iterator,MultiMap< uint32_t,SharedPtr<Peer> >::const_iterator > r(m_peersByProbeToken.equal_range(probeToken));
	PeerList pl;
	if (r.first == r.second)
		return pl;
	const unsigned int cnt = (unsigned int)std::distance(r.first,r.second);
	pl.resize(cnt);
	MultiMap< uint32_t,SharedPtr<Peer> >::const_iterator pi(r.first);
	for(unsigned int i=0;i<cnt;++i) {
		pl[i] = pi->second;
		++pi;
	}
	return pl;
}

void Topology::updateProbeToken(const SharedPtr<Peer> &peer,const uint32_t oldToken,const uint32_t newToken)
{
	Mutex::Lock l(m_peersByProbeToken_l);
	if (oldToken != 0) {
		std::pair< MultiMap< uint32_t,SharedPtr<Peer> >::iterator,MultiMap< uint32_t,SharedPtr<Peer> >::iterator > r(m_peersByProbeToken.equal_range(oldToken));
		for(MultiMap< uint32_t,SharedPtr<Peer> >::iterator i(r.first);i!=r.second;) {
			if (i->second == peer)
				m_peersByProbeToken.erase(i++);
			else ++i;
		}
	}
	if (newToken != 0)
		m_peersByProbeToken.insert(std::pair< uint32_t,SharedPtr<Peer> >(newToken,peer));
}

void Topology::setPhysicalPathConfiguration(const struct sockaddr_storage *pathNetwork,const ZT_PhysicalPathConfiguration *pathConfig)
{
	if (!pathNetwork) {
		m_numConfiguredPhysicalPaths = 0;
	} else {
		std::map<InetAddress,ZT_PhysicalPathConfiguration> cpaths;
		for(unsigned int i=0,j=m_numConfiguredPhysicalPaths;i < j;++i)
			cpaths[m_physicalPathConfig[i].first] = m_physicalPathConfig[i].second;

		if (pathConfig) {
			ZT_PhysicalPathConfiguration pc(*pathConfig);

			if (pc.mtu <= 0)
				pc.mtu = ZT_DEFAULT_UDP_MTU;
			else if (pc.mtu < ZT_MIN_UDP_MTU)
				pc.mtu = ZT_MIN_UDP_MTU;
			else if (pc.mtu > ZT_MAX_UDP_MTU)
				pc.mtu = ZT_MAX_UDP_MTU;

			cpaths[*(reinterpret_cast<const InetAddress *>(pathNetwork))] = pc;
		} else {
			cpaths.erase(*(reinterpret_cast<const InetAddress *>(pathNetwork)));
		}

		unsigned int cnt = 0;
		for(std::map<InetAddress,ZT_PhysicalPathConfiguration>::const_iterator i(cpaths.begin());((i!=cpaths.end())&&(cnt<ZT_MAX_CONFIGURABLE_PATHS));++i) {
			m_physicalPathConfig[cnt].first = i->first;
			m_physicalPathConfig[cnt].second = i->second;
			++cnt;
		}
		m_numConfiguredPhysicalPaths = cnt;
	}
}

struct p_RootSortComparisonOperator
{
	ZT_INLINE bool operator()(const SharedPtr<Peer> &a,const SharedPtr<Peer> &b) const noexcept
	{
		// Sort in inverse order of latency with lowest latency first (and -1 last).
		const int bb = b->latency();
		if (bb < 0)
			return true;
		return bb < a->latency();
	}
};

void Topology::addRoot(void *const tPtr,const Identity &id,const InetAddress &bootstrap)
{
	if (id == RR->identity)
		return;

	RWMutex::Lock l1(m_peers_l);
	std::pair< Set<Identity>::iterator,bool > ir(m_roots.insert(id));
	if (ir.second) {
		SharedPtr<Peer> &p = m_peers[id.address()];
		if (!p) {
			p.set(new Peer(RR));
			p->init(id);
			if (bootstrap)
				p->setBootstrap(Endpoint(bootstrap));
		}
		m_rootPeers.push_back(p);
		std::sort(m_rootPeers.begin(),m_rootPeers.end(),p_RootSortComparisonOperator());
		m_writeRootList(tPtr);
	}
}

bool Topology::removeRoot(void *const tPtr,const Identity &id)
{
	RWMutex::Lock l1(m_peers_l);
	Set<Identity>::iterator r(m_roots.find(id));
	if (r != m_roots.end()) {
		for(Vector< SharedPtr<Peer> >::iterator p(m_rootPeers.begin());p != m_rootPeers.end();++p) {
			if ((*p)->identity() == id) {
				m_rootPeers.erase(p);
				break;
			}
		}
		m_roots.erase(r);
		m_writeRootList(tPtr);
		return true;
	}
	return false;
}

void Topology::rankRoots()
{
	RWMutex::Lock l1(m_peers_l);
	std::sort(m_rootPeers.begin(),m_rootPeers.end(),p_RootSortComparisonOperator());
}

void Topology::doPeriodicTasks(void *tPtr,const int64_t now)
{
	// Delete peers that haven't said anything in ZT_PEER_ALIVE_TIMEOUT.
	{
		RWMutex::Lock l1(m_peers_l);
		for(Map< Address,SharedPtr<Peer> >::iterator i(m_peers.begin());i != m_peers.end();) {
			if ( ((now - i->second->lastReceive()) > ZT_PEER_ALIVE_TIMEOUT) && (m_roots.count(i->second->identity()) == 0) ) {
				updateProbeToken(i->second,i->second->probeToken(),0);
				i->second->save(tPtr);
				m_peers.erase(i++);
			} else ++i;
		}
	}

	// Delete paths that are no longer held by anyone else ("weak reference" type behavior).
	{
		RWMutex::Lock l1(m_paths_l);
		for(Map< uint64_t,SharedPtr<Path> >::iterator i(m_paths.begin());i != m_paths.end();) {
			if (i->second.weakGC())
				m_paths.erase(i++);
			else ++i;
		}
	}
}

void Topology::saveAll(void *tPtr)
{
	RWMutex::RLock l(m_peers_l);
	for(Map< Address,SharedPtr<Peer> >::iterator i(m_peers.begin());i!=m_peers.end();++i)
		i->second->save(tPtr);
}

void Topology::m_loadCached(void *tPtr, const Address &zta, SharedPtr<Peer> &peer)
{
	try {
		uint64_t id[2];
		id[0] = zta.toInt();
		id[1] = 0;
		Vector<uint8_t> data(RR->node->stateObjectGet(tPtr,ZT_STATE_OBJECT_PEER,id));
		if (data.size() > 8) {
			const uint8_t *d = data.data();
			int dl = (int)data.size();

			const int64_t ts = (int64_t)Utils::loadBigEndian<uint64_t>(d);
			Peer *const p = new Peer(RR);
			int n = p->unmarshal(d + 8,dl - 8);
			if (n < 0) {
				delete p;
				return;
			}
			if ((RR->node->now() - ts) < ZT_PEER_GLOBAL_TIMEOUT) {
				// TODO: handle many peers, same address (?)
				peer.set(p);
				return;
			}
		}
	} catch ( ... ) {
		peer.zero();
	}
}

void Topology::m_writeRootList(void *tPtr)
{
	// assumes m_peers_l is locked
	uint8_t *const roots = (uint8_t *)malloc(ZT_IDENTITY_MARSHAL_SIZE_MAX * m_roots.size());
	if (roots) { // sanity check
		int p = 0;
		for(Set<Identity>::const_iterator i(m_roots.begin());i != m_roots.end();++i) {
			const int pp = i->marshal(roots + p,false);
			if (pp > 0)
				p += pp;
		}
		uint64_t id[2];
		id[0] = 0;
		id[1] = 0;
		RR->node->stateObjectPut(tPtr,ZT_STATE_OBJECT_ROOTS,id,roots,(unsigned int)p);
		free(roots);
	}
}

} // namespace ZeroTier
