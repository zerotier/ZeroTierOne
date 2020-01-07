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
#include "Str.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Database of network topology
 */
class Topology
{
public:
	ZT_ALWAYS_INLINE Topology(const RuntimeEnvironment *renv,const Identity &myId) :
		RR(renv),
		_myIdentity(myId),
		_numConfiguredPhysicalPaths(0),
		_peers(128),
		_paths(256)
	{
	}

	ZT_ALWAYS_INLINE ~Topology() {}

	/**
	 * Add a peer to database
	 *
	 * This will not replace existing peers. In that case the existing peer
	 * record is returned.
	 *
	 * @param peer Peer to add
	 * @return New or existing peer (should replace 'peer')
	 */
	ZT_ALWAYS_INLINE SharedPtr<Peer> add(const SharedPtr<Peer> &peer)
	{
		Mutex::Lock _l(_peers_l);
		SharedPtr<Peer> &hp = _peers[peer->address()];
		if (!hp)
			hp = peer;
		return hp;
	}

	/**
	 * Get a peer from its address
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param zta ZeroTier address of peer
	 * @return Peer or NULL if not found
	 */
	ZT_ALWAYS_INLINE SharedPtr<Peer> get(const Address &zta)
	{
		Mutex::Lock l1(_peers_l);
		const SharedPtr<Peer> *const ap = _peers.get(zta);
		return (ap) ? *ap : SharedPtr<Peer>();
	}

	/**
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param zta ZeroTier address of peer
	 * @return Identity or NULL identity if not found
	 */
	ZT_ALWAYS_INLINE Identity getIdentity(void *tPtr,const Address &zta)
	{
		if (zta == _myIdentity.address()) {
			return _myIdentity;
		} else {
			Mutex::Lock _l(_peers_l);
			const SharedPtr<Peer> *const ap = _peers.get(zta);
			if (ap)
				return (*ap)->identity();
		}
		return Identity();
	}

	/**
	 * Get a Path object for a given local and remote physical address, creating if needed
	 *
	 * @param l Local socket
	 * @param r Remote address
	 * @return Pointer to canonicalized Path object
	 */
	ZT_ALWAYS_INLINE SharedPtr<Path> getPath(const int64_t l,const InetAddress &r)
	{
		Mutex::Lock _l(_paths_l);
		SharedPtr<Path> &p = _paths[Path::HashKey(l,r)];
		if (!p)
			p.set(new Path(l,r));
		return p;
	}

	/**
	 * @param id Identity to check
	 * @return True if this identity corresponds to a root
	 */
	ZT_ALWAYS_INLINE bool isRoot(const Identity &id) const
	{
		Mutex::Lock l(_peers_l);
		return (_roots.count(id) > 0);
	}

	/**
	 * @param now Current time
	 * @return Number of peers with active direct paths
	 */
	ZT_ALWAYS_INLINE unsigned long countActive(const int64_t now) const
	{
		unsigned long cnt = 0;
		Mutex::Lock _l(_peers_l);
		Hashtable< Address,SharedPtr<Peer> >::Iterator i(const_cast<Topology *>(this)->_peers);
		Address *a = (Address *)0;
		SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
		while (i.next(a,p)) {
			if ((*p)->getAppropriatePath(now,false))
				++cnt;
		}
		return cnt;
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
	ZT_ALWAYS_INLINE void eachPeer(F f)
	{
		Mutex::Lock l(_peers_l);
		Hashtable< Address,SharedPtr<Peer> >::Iterator i(_peers);
		Address *a = (Address *)0;
		SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
		while (i.next(a,p)) {
			if (!f(*((const SharedPtr<Peer> *)p)))
				break;
		}
	}

	/**
	 * Get the best relay to a given address, which may or may not be a root
	 *
	 * @param now Current time
	 * @param toAddr Destination address
	 * @return Best current relay or NULL if none
	 */
	ZT_ALWAYS_INLINE SharedPtr<Peer> findRelayTo(const int64_t now,const Address &toAddr)
	{
		Mutex::Lock l(_peers_l);
		if (_rootPeers.empty())
			return SharedPtr<Peer>();
		return _rootPeers[0];
	}

	/**
	 * @param allPeers vector to fill with all current peers
	 */
	ZT_ALWAYS_INLINE void getAllPeers(std::vector< SharedPtr<Peer> > &allPeers) const
	{
		Mutex::Lock l(_peers_l);
		allPeers.clear();
		allPeers.reserve(_peers.size());
		Hashtable< Address,SharedPtr<Peer> >::Iterator i(*(const_cast<Hashtable< Address,SharedPtr<Peer> > *>(&_peers)));
		Address *a = (Address *)0;
		SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
		while (i.next(a,p))
			allPeers.push_back(*p);
	}

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
	 * Get the payload MTU for an outbound physical path (returns default if not configured)
	 *
	 * @param physicalAddress Physical endpoint address
	 * @return MTU
	 */
	ZT_ALWAYS_INLINE unsigned int getOutboundPathMtu(const InetAddress &physicalAddress)
	{
		for(unsigned int i=0,j=_numConfiguredPhysicalPaths;i<j;++i) {
			if (_physicalPathConfig[i].first.containsAddress(physicalAddress))
				return _physicalPathConfig[i].second.mtu;
		}
		return ZT_DEFAULT_PHYSMTU;
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
	inline void setPhysicalPathConfiguration(const struct sockaddr_storage *pathNetwork,const ZT_PhysicalPathConfiguration *pathConfig)
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

	/**
	 * Add a root server's identity to the root server set
	 *
	 * @param id Root server identity
	 */
	inline void addRoot(const Identity &id)
	{
		if (id == _myIdentity) return; // sanity check
		Mutex::Lock l1(_peers_l);
		std::pair< std::set<Identity>::iterator,bool > ir(_roots.insert(id));
		if (ir.second) {
			SharedPtr<Peer> &p = _peers[id.address()];
			if (!p)
				p.set(new Peer(RR,_myIdentity,id));
			_rootPeers.push_back(p);
		}
	}

	/**
	 * Remove a root server's identity from the root server set
	 *
	 * @param id Root server identity
	 * @return True if root found and removed, false if not found
	 */
	inline bool removeRoot(const Identity &id)
	{
		Mutex::Lock l1(_peers_l);
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

	/**
	 * Do periodic tasks such as database cleanup
	 */
	inline void doPeriodicTasks(const int64_t now)
	{
		{
			Mutex::Lock l1(_peers_l);
			std::sort(_rootPeers.begin(),_rootPeers.end(),_RootSortComparisonOperator(now));
			Hashtable< Address,SharedPtr<Peer> >::Iterator i(_peers);
			Address *a = (Address *)0;
			SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
			while (i.next(a,p)) {
				if ( (!(*p)->alive(now)) && (_roots.count((*p)->identity()) == 0) )
					_peers.erase(*a);
			}
		}
		{
			Mutex::Lock l1(_paths_l);
			Hashtable< Path::HashKey,SharedPtr<Path> >::Iterator i(_paths);
			Path::HashKey *k = (Path::HashKey *)0;
			SharedPtr<Path> *p = (SharedPtr<Path> *)0;
			while (i.next(k,p)) {
				if (p->references() <= 1)
					_paths.erase(*k);
			}
		}
	}

private:
	struct _RootSortComparisonOperator
	{
		ZT_ALWAYS_INLINE _RootSortComparisonOperator(const int64_t now) : _now(now) {}
		ZT_ALWAYS_INLINE bool operator()(const SharedPtr<Peer> &a,const SharedPtr<Peer> &b)
		{
			const int64_t now = _now;
			if (a->alive(now)) {
				if (b->alive(now))
					return (a->latency(now) < b->latency(now));
				return true;
			}
			return false;
		}
		const int64_t _now;
	};

	const RuntimeEnvironment *const RR;
	const Identity _myIdentity;

	Mutex _peers_l;
	Mutex _paths_l;

	std::pair< InetAddress,ZT_PhysicalPathConfiguration > _physicalPathConfig[ZT_MAX_CONFIGURABLE_PATHS];
	unsigned int _numConfiguredPhysicalPaths;

	Hashtable< Address,SharedPtr<Peer> > _peers;
	Hashtable< Path::HashKey,SharedPtr<Path> > _paths;
	std::set< Identity > _roots; // locked by _peers_l
	std::vector< SharedPtr<Peer> > _rootPeers; // locked by _peers_l
};

} // namespace ZeroTier

#endif
