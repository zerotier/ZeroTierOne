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
	Vector<uint8_t> data(RR->node->stateObjectGet(tPtr, ZT_STATE_OBJECT_ROOTS, idtmp));
	if (!data.empty()) {
		uint8_t *dptr = data.data();
		int drem = (int)data.size();
		for (;;) {
			Identity id;
			int l = id.unmarshal(dptr, drem);
			if ((l > 0)&&(id)) {
				if ((drem -= l) <= 0)
					break;
				Locator loc;
				l = loc.unmarshal(dptr, drem);
				if ((l > 0)&&(loc)) {
					m_roots[id] = loc;
					dptr += l;
					ZT_SPEW("loaded root %s", id.address().toString().c_str());
					if ((drem -= l) <= 0)
						break;
				}
			}
		}
	}
	m_updateRootPeers(tPtr);
}

SharedPtr<Peer> Topology::add(void *tPtr, const SharedPtr<Peer> &peer)
{
	RWMutex::Lock _l(m_peers_l);
	SharedPtr<Peer> &hp = m_peers[peer->address()];
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
	ZT_INLINE bool operator()(const SharedPtr<Peer> &a, const SharedPtr<Peer> &b) const noexcept
	{
		// Sort in inverse order of latency with lowest latency first (and -1 last).
		const int bb = b->latency();
		if (bb < 0)
			return true;
		return bb < a->latency();
	}
};

void Topology::addRoot(void *const tPtr, const Identity &id, const Locator &loc)
{
	if (id == RR->identity)
		return;
	RWMutex::Lock l1(m_peers_l);
	m_roots[id] = loc;
	m_updateRootPeers(tPtr);
	m_writeRootList(tPtr);
}

bool Topology::removeRoot(void *const tPtr, const Fingerprint &fp)
{
	const bool hashIsZero = !fp.haveHash();
	RWMutex::Lock l1(m_peers_l);
	for(Vector< SharedPtr<Peer> >::const_iterator r(m_rootPeers.begin());r!=m_rootPeers.end();++r) {
		if ((*r)->address() == fp.address()) {
			if ((hashIsZero)||(fp == (*r)->identity().fingerprint())) {
				Map<Identity,Locator>::iterator rr(m_roots.find((*r)->identity()));
				if (rr != m_roots.end()) {
					m_roots.erase(rr);
					m_updateRootPeers(tPtr);
					m_writeRootList(tPtr);
					return true;
				}
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
		for (Map<Address, SharedPtr<Peer> >::iterator i(m_peers.begin());i != m_peers.end();) {
			if (((now - i->second->lastReceive()) > ZT_PEER_ALIVE_TIMEOUT) && (m_roots.count(i->second->identity()) == 0)) {
				i->second->save(tPtr);
				m_peers.erase(i++);
			} else ++i;
		}
	}

	// Delete paths that are no longer held by anyone else ("weak reference" type behavior).
	{
		RWMutex::Lock l1(m_paths_l);
		for (Map<uint64_t, SharedPtr<Path> >::iterator i(m_paths.begin());i != m_paths.end();) {
			if (i->second.weakGC())
				m_paths.erase(i++);
			else ++i;
		}
	}
}

void Topology::saveAll(void *tPtr)
{
	RWMutex::RLock l(m_peers_l);
	for (Map<Address, SharedPtr<Peer> >::iterator i(m_peers.begin());i != m_peers.end();++i)
		i->second->save(tPtr);
}

void Topology::m_loadCached(void *tPtr, const Address &zta, SharedPtr<Peer> &peer)
{
	try {
		uint64_t id[2];
		id[0] = zta.toInt();
		id[1] = 0;
		Vector<uint8_t> data(RR->node->stateObjectGet(tPtr, ZT_STATE_OBJECT_PEER, id));
		if (data.size() > 8) {
			const uint8_t *d = data.data();
			int dl = (int) data.size();

			const int64_t ts = (int64_t) Utils::loadBigEndian<uint64_t>(d);
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
		for (Map<Identity,Locator>::const_iterator r(m_roots.begin());r!=m_roots.end();++r) {
			int pp = r->first.marshal(roots + p, false);
			if (pp > 0) {
				p += pp;
				pp = r->second.marshal(roots + p);
				if (pp > 0)
					p += pp;
			}
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
	Vector< SharedPtr<Peer> > rp;
	for (Map<Identity,Locator>::iterator r(m_roots.begin());r!=m_roots.end();++r) {
		Map< Address,SharedPtr<Peer> >::iterator p(m_peers.find(r->first.address()));
		if ((p == m_peers.end())||(p->second->identity() != r->first)) {
			SharedPtr<Peer> np(new Peer(RR));
			np->init(r->first);
			m_peers[r->first.address()] = np;
			rp.push_back(np);
		} else {
			rp.push_back(p->second);
		}
	}
	m_rootPeers.swap(rp);
	std::sort(m_rootPeers.begin(), m_rootPeers.end(), p_RootSortComparisonOperator());
}

} // namespace ZeroTier
