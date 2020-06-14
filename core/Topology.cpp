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

Topology::Topology(const RuntimeEnvironment *renv, void *tPtr) :
	RR(renv)
{
	uint64_t idtmp[2];
	idtmp[0] = 0;
	idtmp[1] = 0;
	Vector< uint8_t > data(RR->node->stateObjectGet(tPtr, ZT_STATE_OBJECT_ROOTS, idtmp));
	if (!data.empty()) {
		uint8_t *dptr = data.data();
		int drem = (int)data.size();
		for (;;) {
			Identity id;
			int l = id.unmarshal(dptr, drem);
			if ((l > 0) && (id)) {
				ZT_SPEW("restored root %s", id.address().toString().c_str());
				if ((drem -= l) <= 0)
					break;
			} else break;
		}
	}
	m_updateRootPeers(tPtr);
}

SharedPtr< Peer > Topology::add(void *tPtr, const SharedPtr< Peer > &peer)
{
	RWMutex::Lock _l(m_peers_l);
	SharedPtr< Peer > &hp = m_peers[peer->address()];
	if (hp)
		return hp;
	m_loadCached(tPtr, peer->address(), hp);
	if (hp)
		return hp;
	hp = peer;
	return peer;
}

struct p_RootSortComparisonOperator
{
	ZT_INLINE bool operator()(const SharedPtr< Peer > &a, const SharedPtr< Peer > &b) const noexcept
	{
		// Sort in inverse order of latency with lowest latency first (and -1 last).
		const int bb = b->latency();
		if (bb < 0)
			return true;
		return bb < a->latency();
	}
};

SharedPtr< Peer > Topology::addRoot(void *const tPtr, const Identity &id)
{
	if ((id != RR->identity) && id.locallyValidate()) {
		RWMutex::Lock l1(m_peers_l);
		m_roots.insert(id);

		m_updateRootPeers(tPtr);
		m_writeRootList(tPtr);

		for (Vector< SharedPtr< Peer > >::const_iterator p(m_rootPeers.begin()); p != m_rootPeers.end(); ++p) {
			if ((*p)->identity() == id)
				return *p;
		}
	}
	return SharedPtr< Peer >();
}

bool Topology::removeRoot(void *const tPtr, Address address)
{
	RWMutex::Lock l1(m_peers_l);
	for (Vector< SharedPtr< Peer > >::const_iterator r(m_rootPeers.begin()); r != m_rootPeers.end(); ++r) {
		if ((*r)->address() == address) {
			Set< Identity >::iterator rr(m_roots.find((*r)->identity()));
			if (rr != m_roots.end()) {
				m_roots.erase(rr);
				m_updateRootPeers(tPtr);
				m_writeRootList(tPtr);
				return true;
			}
		}
	}
	return false;
}

void Topology::rankRoots()
{
	RWMutex::Lock l1(m_peers_l);
	std::sort(m_rootPeers.begin(), m_rootPeers.end(), p_RootSortComparisonOperator());
}

void Topology::doPeriodicTasks(void *tPtr, const int64_t now)
{
	// Delete peers that haven't said anything in ZT_PEER_ALIVE_TIMEOUT.
	{
		RWMutex::Lock l1(m_peers_l);
		for (Map< Address, SharedPtr< Peer > >::iterator i(m_peers.begin()); i != m_peers.end();) {
			// TODO: also delete if the peer has not exchanged meaningful communication in a while, such as
			// a network frame or non-trivial control packet.
			if (((now - i->second->lastReceive()) > ZT_PEER_ALIVE_TIMEOUT) && (m_roots.count(i->second->identity()) == 0)) {
				i->second->save(tPtr);
				m_peers.erase(i++);
			} else ++i;
		}
	}

	// Delete paths that are no longer held by anyone else ("weak reference" type behavior).
	{
		RWMutex::Lock l1(m_paths_l);
		for (Map< uint64_t, SharedPtr< Path > >::iterator i(m_paths.begin()); i != m_paths.end();) {
			if (i->second.weakGC())
				m_paths.erase(i++);
			else ++i;
		}
	}
}

void Topology::saveAll(void *tPtr)
{
	RWMutex::RLock l(m_peers_l);
	for (Map< Address, SharedPtr< Peer > >::iterator i(m_peers.begin()); i != m_peers.end(); ++i)
		i->second->save(tPtr);
}

void Topology::m_loadCached(void *tPtr, const Address &zta, SharedPtr< Peer > &peer)
{
	try {
		uint64_t id[2];
		id[0] = zta.toInt();
		id[1] = 0;
		Vector< uint8_t > data(RR->node->stateObjectGet(tPtr, ZT_STATE_OBJECT_PEER, id));
		if (data.size() > 8) {
			const uint8_t *d = data.data();
			int dl = (int)data.size();

			const int64_t ts = (int64_t)Utils::loadBigEndian< uint64_t >(d);
			Peer *const p = new Peer(RR);
			int n = p->unmarshal(d + 8, dl - 8);
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
	} catch (...) {
		peer.zero();
	}
}

void Topology::m_writeRootList(void *tPtr)
{
	// assumes m_peers_l is locked for read or write
	uint8_t *const roots = (uint8_t *)malloc((ZT_IDENTITY_MARSHAL_SIZE_MAX + ZT_LOCATOR_MARSHAL_SIZE_MAX + 2) * m_roots.size());
	if (roots) { // sanity check
		int p = 0;
		for (Set< Identity >::const_iterator r(m_roots.begin()); r != m_roots.end(); ++r) {
			const int pp = r->marshal(roots + p, false);
			if (pp > 0)
				p += pp;
		}
		uint64_t id[2];
		id[0] = 0;
		id[1] = 0;
		RR->node->stateObjectPut(tPtr, ZT_STATE_OBJECT_ROOTS, id, roots, (unsigned int)p);
		free(roots);
	}
}

void Topology::m_updateRootPeers(void *tPtr)
{
	// assumes m_peers_l is locked for write
	Vector< SharedPtr< Peer > > rp;
	for (Map< Identity, Set< SubscriptionKeyHash > >::iterator r(m_roots.begin()); r != m_roots.end(); ++r) {
		Map< Address, SharedPtr< Peer > >::iterator pp(m_peers.find(r->first.address()));
		SharedPtr< Peer > p;
		if (pp != m_peers.end())
			p = pp->second;

		if (!p)
			m_loadCached(tPtr, r->first.address(), p);

		if ((!p) || (p->identity() != r->first)) {
			p.set(new Peer(RR));
			p->init(r->first);
			m_peers[r->first.address()] = p;
		}

		rp.push_back(p);
	}
	std::sort(rp.begin(), rp.end(), p_RootSortComparisonOperator());
	m_rootPeers.swap(rp);
}

} // namespace ZeroTier
