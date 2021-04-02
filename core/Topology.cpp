/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "Topology.hpp"
#include "Defaults.hpp"

namespace ZeroTier {

Topology::Topology(const RuntimeEnvironment *renv, void *tPtr, const int64_t now) :
	RR(renv)
{}

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

void Topology::allPeers(Vector< SharedPtr< Peer > > &allPeers, Vector< SharedPtr< Peer > > &rootPeers) const
{
	allPeers.clear();
	{
		RWMutex::RLock l(m_peers_l);
		allPeers.reserve(m_peers.size());
		for (Map< Address, SharedPtr< Peer > >::const_iterator i(m_peers.begin()); i != m_peers.end(); ++i)
			allPeers.push_back(i->second);
	}
	{
		Mutex::Lock l(m_roots_l);
		rootPeers = m_roots;
	}
}

void Topology::doPeriodicTasks(void *tPtr, const int64_t now)
{
	// Get a list of root peer pointer addresses for filtering during peer cleanup.
	Vector< uintptr_t > rootLookup;
	{
		Mutex::Lock l(m_roots_l);
		m_rankRoots(now);
		rootLookup.reserve(m_roots.size());
		for (Vector< SharedPtr< Peer > >::const_iterator r(m_roots.begin()); r != m_roots.end(); ++r)
			rootLookup.push_back((uintptr_t)r->ptr());
	}

	// Cleaning of peers and paths uses a two pass method to avoid write locking
	// m_peers or m_paths for any significant amount of time. This avoids pauses
	// on nodes with large numbers of peers or paths.
	{
		Vector< Address > toDelete;
		{
			RWMutex::RLock l1(m_peers_l);
			for (Map< Address, SharedPtr< Peer > >::iterator i(m_peers.begin()); i != m_peers.end(); ++i) {
				// TODO: also delete if the peer has not exchanged meaningful communication in a while, such as a network frame or non-trivial control packet.
				if (((now - i->second->lastReceive()) > ZT_PEER_ALIVE_TIMEOUT) && (std::find(rootLookup.begin(), rootLookup.end(), (uintptr_t)(i->second.ptr())) == rootLookup.end()))
					toDelete.push_back(i->first);
			}
		}
		if (!toDelete.empty()) {
			ZT_SPEW("garbage collecting %u offline or stale peer objects", (unsigned int)toDelete.size());
			for (Vector< Address >::iterator i(toDelete.begin()); i != toDelete.end(); ++i) {
				SharedPtr< Peer > toSave;
				{
					RWMutex::Lock l1(m_peers_l);
					const Map< Address, SharedPtr< Peer > >::iterator p(m_peers.find(*i));
					if (p != m_peers.end()) {
						p->second.swap(toSave);
						m_peers.erase(p);
					}
				}
				if (toSave)
					toSave->save(tPtr);
			}
		}
	}

	// Delete paths that are no longer held by anyone else ("weak reference" type behavior).
	// First pass: make a list of paths with a reference count of 1 meaning they are likely
	// orphaned. Second pass: call weakGC() on each of these which does a hard compare/exchange
	// and delete those that actually are GC'd. Write lock is aquired only briefly on delete
	// just as with peers.
	{
		Vector< Path::Key > possibleDelete;
		{
			RWMutex::RLock l1(m_paths_l);
			for (Map< Path::Key, SharedPtr< Path > >::iterator i(m_paths.begin()); i != m_paths.end(); ++i) {
				if (i->second.references() <= 1)
					possibleDelete.push_back(i->first);
			}
		}
		if (!possibleDelete.empty()) {
			ZT_SPEW("garbage collecting (likely) %u orphaned paths", (unsigned int)possibleDelete.size());
			for (Vector< Path::Key >::const_iterator i(possibleDelete.begin()); i != possibleDelete.end(); ++i) {
				RWMutex::Lock l1(m_paths_l);
				Map< Path::Key, SharedPtr< Path > >::iterator p(m_paths.find(*i));
				if ((p != m_paths.end()) && p->second.weakGC())
					m_paths.erase(p);
			}
		}
	}
}

void Topology::saveAll(void *tPtr)
{
	RWMutex::RLock l(m_peers_l);
	for (Map< Address, SharedPtr< Peer > >::iterator i(m_peers.begin()); i != m_peers.end(); ++i)
		i->second->save(tPtr);
}

struct p_RootRankingComparisonOperator
{
	ZT_INLINE bool operator()(const SharedPtr< Peer > &a, const SharedPtr< Peer > &b) const noexcept
	{
		// Sort roots first in order of which root has spoken most recently, but
		// only at a resolution of ZT_PATH_KEEPALIVE_PERIOD/2 units of time. This
		// means that living roots that seem responsive are ranked the same. Then
		// they're sorted in descending order of latency so that the apparently
		// fastest root is ranked first.
		const int64_t alr = a->lastReceive() / (ZT_PATH_KEEPALIVE_PERIOD / 2);
		const int64_t blr = b->lastReceive() / (ZT_PATH_KEEPALIVE_PERIOD / 2);
		if (alr < blr) {
			return true;
		} else if (blr == alr) {
			const int bb = b->latency();
			if (bb < 0)
				return true;
			return bb < a->latency();
		}
		return false;
	}
};

void Topology::m_rankRoots(const int64_t now)
{
	// assumes m_roots is locked
	if (unlikely(m_roots.empty())) {
		l_bestRoot.lock();
		m_bestRoot.zero();
		l_bestRoot.unlock();
	} else {
		std::sort(m_roots.begin(), m_roots.end(), p_RootRankingComparisonOperator());
		l_bestRoot.lock();
		m_bestRoot = m_roots.front();
		l_bestRoot.unlock();
	}
}

void Topology::m_loadCached(void *tPtr, const Address &zta, SharedPtr< Peer > &peer)
{
	// does not require any locks to be held

	try {
		uint64_t id[2];
		id[0] = zta.toInt();
		id[1] = 0;
		Vector< uint8_t > data(RR->store->get(tPtr, ZT_STATE_OBJECT_PEER, id, 1));
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

SharedPtr< Peer > Topology::m_peerFromCached(void *tPtr, const Address &zta)
{
	SharedPtr< Peer > p;
	m_loadCached(tPtr, zta, p);
	if (p) {
		RWMutex::Lock l(m_peers_l);
		SharedPtr< Peer > &hp = m_peers[zta];
		if (hp)
			return hp;
		hp = p;
	}
	return p;
}

SharedPtr< Path > Topology::m_newPath(const int64_t l, const InetAddress &r, const Path::Key &k)
{
	SharedPtr< Path > p(new Path(l, r));
	RWMutex::Lock lck(m_paths_l);
	SharedPtr< Path > &p2 = m_paths[k];
	if (p2)
		return p2;
	p2 = p;
	return p;
}

} // namespace ZeroTier
