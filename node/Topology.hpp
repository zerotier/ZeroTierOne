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

#ifndef ZT_TOPOLOGY_HPP
#define ZT_TOPOLOGY_HPP

#include <cstdio>
#include <cstring>

#include <vector>
#include <algorithm>
#include <utility>
#include <set>

#include "Constants.hpp"
#include "../include/ZeroTierOne.h"

#include "Address.hpp"
#include "Identity.hpp"
#include "Peer.hpp"
#include "Path.hpp"
#include "Mutex.hpp"
#include "InetAddress.hpp"
#include "Hashtable.hpp"
#include "SharedPtr.hpp"
#include "ScopedPtr.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Database of network topology
 */
class Topology
{
public:
	Topology(const RuntimeEnvironment *renv,const Identity &myId,void *tPtr);
	~Topology();

	/**
	 * Add peer to database
	 *
	 * This will not replace existing peers. In that case the existing peer
	 * record is returned.
	 *
	 * @param peer Peer to add
	 * @return New or existing peer (should replace 'peer')
	 */
	SharedPtr<Peer> add(void *tPtr,const SharedPtr<Peer> &peer);

	/**
	 * Get a peer from its address
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param zta ZeroTier address of peer
	 * @return Peer or NULL if not found
	 */
	ZT_ALWAYS_INLINE SharedPtr<Peer> get(void *tPtr,const Address &zta)
	{
		{
			RWMutex::RLock _l(_peers_l);
			const SharedPtr<Peer> *const ap = _peers.get(zta);
			if (ap)
				return *ap;
		}

		SharedPtr<Peer> p;
		_loadCached(tPtr,zta,p);
		if (p) {
			RWMutex::Lock _l(_peers_l);
			SharedPtr<Peer> &hp = _peers[zta];
			if (!hp)
				hp = p;
		}

		return p;
	}

	/**
	 * Get a Path object for a given local and remote physical address, creating if needed
	 *
	 * @param l Local socket
	 * @param r Remote address
	 * @return Pointer to canonicalized Path object or NULL on error
	 */
	ZT_ALWAYS_INLINE SharedPtr<Path> getPath(const int64_t l,const InetAddress &r)
	{
		const Path::HashKey k(l,r);

		_paths_l.rlock();
		SharedPtr<Path> p(_paths[k]);
		_paths_l.runlock();
		if (p)
			return p;

		_paths_l.lock();
		SharedPtr<Path> &p2 = _paths[k];
		if (p2) {
			p = p2;
		} else {
			try {
				p.set(new Path(l,r));
			} catch ( ... ) {
				_paths_l.unlock();
				return SharedPtr<Path>();
			}
			p2 = p;
		}
		_paths_l.unlock();

		return p;
	}

	/**
	 * @return Current best root server
	 */
	ZT_ALWAYS_INLINE SharedPtr<Peer> root() const
	{
		RWMutex::RLock l(_peers_l);
		if (_rootPeers.empty())
			return SharedPtr<Peer>();
		return _rootPeers.front();
	}

	/**
	 * @param id Identity to check
	 * @return True if this identity corresponds to a root
	 */
	ZT_ALWAYS_INLINE bool isRoot(const Identity &id) const
	{
		RWMutex::RLock l(_peers_l);
		return (_roots.count(id) > 0);
	}

	/**
	 * Apply a function or function object to all peers
	 *
	 * This locks the peer map during execution, so calls to get() etc. during
	 * eachPeer() will deadlock.
	 *
	 * @param f Function to apply
	 * @tparam F Function or function object type
	 */
	template<typename F>
	ZT_ALWAYS_INLINE void eachPeer(F f) const
	{
		RWMutex::RLock l(_peers_l);
		Hashtable< Address,SharedPtr<Peer> >::Iterator i(const_cast<Topology *>(this)->_peers);
		Address *a = (Address *)0;
		SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
		while (i.next(a,p)) {
			f(*((const SharedPtr<Peer> *)p));
		}
	}

	/**
	 * Apply a function or function object to all peers
	 *
	 * This locks the peer map during execution, so calls to get() etc. during
	 * eachPeer() will deadlock.
	 *
	 * @param f Function to apply
	 * @tparam F Function or function object type
	 */
	template<typename F>
	ZT_ALWAYS_INLINE void eachPeerWithRoot(F f) const
	{
		RWMutex::RLock l(_peers_l);

		const unsigned long rootPeerCnt = _rootPeers.size();
		uintptr_t *const rootPeerPtrs = (uintptr_t *)malloc(sizeof(uintptr_t) * rootPeerCnt);
		if (!rootPeerPtrs)
			throw std::bad_alloc();
		for(unsigned long i=0;i<rootPeerCnt;++i)
			rootPeerPtrs[i] = (uintptr_t)_rootPeers[i].ptr();
		std::sort(rootPeerPtrs,rootPeerPtrs + rootPeerCnt);
		uintptr_t *const rootPeerPtrsEnd = rootPeerPtrs + rootPeerCnt;

		try {
			Hashtable< Address,SharedPtr<Peer> >::Iterator i(const_cast<Topology *>(this)->_peers);
			Address *a = (Address *)0;
			SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
			while (i.next(a,p)) {
				f(*((const SharedPtr<Peer> *)p),std::binary_search(rootPeerPtrs,rootPeerPtrsEnd,(uintptr_t)p->ptr()));
			}
		} catch ( ... ) {} // should not throw

		free((void *)rootPeerPtrs);
	}

	/**
	 * Iterate through all paths in the system
	 *
	 * @tparam F Function to call for each path
	 * @param f
	 */
	template<typename F>
	ZT_ALWAYS_INLINE void eachPath(F f) const
	{
		RWMutex::RLock l(_paths_l);
		Hashtable< Path::HashKey,SharedPtr<Path> >::Iterator i(const_cast<Topology *>(this)->_paths);
		Path::HashKey *k = (Path::HashKey *)0;
		SharedPtr<Path> *p = (SharedPtr<Path> *)0;
		while (i.next(k,p)) {
			f(*((const SharedPtr<Peer> *)p));
		}
	}

	/**
	 * @param allPeers vector to fill with all current peers
	 */
	void getAllPeers(std::vector< SharedPtr<Peer> > &allPeers) const;

	/**
	 * Get info about a path
	 *
	 * The supplied result variables are not modified if no special config info is found.
	 *
	 * @param physicalAddress Physical endpoint address
	 * @param mtu Variable set to MTU
	 * @param trustedPathId Variable set to trusted path ID
	 */
	ZT_ALWAYS_INLINE void getOutboundPathInfo(const InetAddress &physicalAddress,unsigned int &mtu,uint64_t &trustedPathId)
	{
		for(unsigned int i=0,j=_numConfiguredPhysicalPaths;i<j;++i) {
			if (_physicalPathConfig[i].first.containsAddress(physicalAddress)) {
				trustedPathId = _physicalPathConfig[i].second.trustedPathId;
				mtu = _physicalPathConfig[i].second.mtu;
				return;
			}
		}
	}

	/**
	 * Get the outbound trusted path ID for a physical address, or 0 if none
	 *
	 * @param physicalAddress Physical address to which we are sending the packet
	 * @return Trusted path ID or 0 if none (0 is not a valid trusted path ID)
	 */
	ZT_ALWAYS_INLINE uint64_t getOutboundPathTrust(const InetAddress &physicalAddress)
	{
		for(unsigned int i=0,j=_numConfiguredPhysicalPaths;i<j;++i) {
			if (_physicalPathConfig[i].first.containsAddress(physicalAddress))
				return _physicalPathConfig[i].second.trustedPathId;
		}
		return 0;
	}

	/**
	 * Check whether in incoming trusted path marked packet is valid
	 *
	 * @param physicalAddress Originating physical address
	 * @param trustedPathId Trusted path ID from packet (from MAC field)
	 */
	ZT_ALWAYS_INLINE bool shouldInboundPathBeTrusted(const InetAddress &physicalAddress,const uint64_t trustedPathId)
	{
		for(unsigned int i=0,j=_numConfiguredPhysicalPaths;i<j;++i) {
			if ((_physicalPathConfig[i].second.trustedPathId == trustedPathId)&&(_physicalPathConfig[i].first.containsAddress(physicalAddress)))
				return true;
		}
		return false;
	}

	/**
	 * Set or clear physical path configuration (called via Node::setPhysicalPathConfiguration)
	 */
	void setPhysicalPathConfiguration(const struct sockaddr_storage *pathNetwork,const ZT_PhysicalPathConfiguration *pathConfig);

	/**
	 * Add a root server's identity to the root server set
	 *
	 * @param tPtr Thread pointer
	 * @param id Root server identity
	 * @param bootstrap If non-NULL, a bootstrap address to attempt to find this root
	 */
	void addRoot(void *tPtr,const Identity &id,const InetAddress &bootstrap);

	/**
	 * Remove a root server's identity from the root server set
	 *
	 * @param id Root server identity
	 * @return True if root found and removed, false if not found
	 */
	bool removeRoot(const Identity &id);

	/**
	 * Sort roots in asecnding order of apparent latency
	 *
	 * @param now Current time
	 */
	void rankRoots(int64_t now);

	/**
	 * Do periodic tasks such as database cleanup
	 */
	void doPeriodicTasks(void *tPtr,int64_t now);

	/**
	 * Save all currently known peers to data store
	 */
	void saveAll(void *tPtr);

private:
	void _loadCached(void *tPtr,const Address &zta,SharedPtr<Peer> &peer);

	const RuntimeEnvironment *const RR;
	const Identity _myIdentity;

	RWMutex _peers_l;
	RWMutex _paths_l;

	std::pair< InetAddress,ZT_PhysicalPathConfiguration > _physicalPathConfig[ZT_MAX_CONFIGURABLE_PATHS];
	unsigned int _numConfiguredPhysicalPaths;

	Hashtable< Address,SharedPtr<Peer> > _peers;
	Hashtable< Path::HashKey,SharedPtr<Path> > _paths;
	std::set< Identity > _roots; // locked by _peers_l
	std::vector< SharedPtr<Peer> > _rootPeers; // locked by _peers_l
};

} // namespace ZeroTier

#endif
