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

#include <stdio.h>
#include <string.h>

#include <vector>
#include <stdexcept>
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
#include "Locator.hpp"
#include "SharedPtr.hpp"
#include "ScopedPtr.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Database of network topology
 */
class Topology
{
private:
	struct _RootRankingFunction
	{
		ZT_ALWAYS_INLINE _RootRankingFunction() : bestRoot(),bestRootLatency(0xffff) {}
		ZT_ALWAYS_INLINE bool operator()(const SharedPtr<Peer> &peer,const std::vector<InetAddress> &phy)
		{
			const unsigned int lat = peer->latency(now);
			if ((!bestRoot)||((lat <= bestRootLatency)&&(peer->getAppropriatePath(now,false)))) {
				bestRoot = peer;
				bestRootLatency = lat;
			}
			return true;
		}
		int64_t now;
		SharedPtr<Peer> bestRoot;
		unsigned int bestRootLatency;
	};

	ZT_ALWAYS_INLINE void _updateRoots()
	{
		// assumes _roots_l is locked
		_rootIdentities.clear();
		Hashtable< Str,SharedPtr<const Locator> >::Iterator i(_roots);
		Str *k = (Str *)0;
		SharedPtr< const Locator > *v = (SharedPtr< const Locator > *)0;
		while (i.next(k,v)) {
			if (*v)
				_rootIdentities.set((*v)->id(),true);
		}
	}

public:
	ZT_ALWAYS_INLINE Topology(const RuntimeEnvironment *renv,const Identity &myId) :
		RR(renv),
		_myIdentity(myId),
		_numConfiguredPhysicalPaths(0),
		_peers(64),
		_paths(128),
		_roots(8),
		_rootIdentities(8),
		_lastUpdatedBestRoot(0) {}
	ZT_ALWAYS_INLINE ~Topology() {}

	/**
	 * Add a peer to database
	 *
	 * This will not replace existing peers. In that case the existing peer
	 * record is returned.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param peer Peer to add
	 * @return New or existing peer (should replace 'peer')
	 */
	ZT_ALWAYS_INLINE SharedPtr<Peer> add(const SharedPtr<Peer> &peer)
	{
		SharedPtr<Peer> np;
		{
			Mutex::Lock _l(_peers_l);
			SharedPtr<Peer> &hp = _peers[peer->address()];
			if (!hp)
				hp = peer;
			np = hp;
		}
		return np;
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
		if (zta == _myIdentity.address())
			return SharedPtr<Peer>();
		Mutex::Lock l1(_peers_l);
		const SharedPtr<Peer> *const ap = _peers.get(zta);
		if (ap)
			return *ap;
		return SharedPtr<Peer>();
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
		Mutex::Lock l(_roots_l);
		return _rootIdentities.contains(id);
	}

	/**
	 * Do periodic tasks such as database cleanup
	 */
	ZT_ALWAYS_INLINE void doPeriodicTasks(int64_t now)
	{
		{
			Mutex::Lock _l1(_peers_l);
			Hashtable< Address,SharedPtr<Peer> >::Iterator i(_peers);
			Address *a = (Address *)0;
			SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
			while (i.next(a,p)) {
				if (!(*p)->alive(now)) {
					_peers.erase(*a);
				}
			}
		}
		{
			Mutex::Lock _l(_paths_l);
			Hashtable< Path::HashKey,SharedPtr<Path> >::Iterator i(_paths);
			Path::HashKey *k = (Path::HashKey *)0;
			SharedPtr<Path> *p = (SharedPtr<Path> *)0;
			while (i.next(k,p)) {
				if (p->references() <= 1)
					_paths.erase(*k);
			}
		}
	}

	/**
	 * @param now Current time
	 * @return Number of peers with active direct paths
	 */
	inline unsigned long countActive(int64_t now) const
	{
		unsigned long cnt = 0;
		Mutex::Lock _l(_peers_l);
		Hashtable< Address,SharedPtr<Peer> >::Iterator i(const_cast<Topology *>(this)->_peers);
		Address *a = (Address *)0;
		SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
		while (i.next(a,p)) {
			const SharedPtr<Path> pp((*p)->getAppropriatePath(now,false));
			if (pp)
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
	 * Apply a function or function object to all roots
	 *
	 * This locks the root list during execution but other operations
	 * are fine.
	 *
	 * @param f Function to apply f(peer,IPs)
	 * @tparam F function or function object type
	 */
	template<typename F>
	ZT_ALWAYS_INLINE void eachRoot(F f)
	{
		Mutex::Lock l(_roots_l);
		Hashtable< Str,Locator >::Iterator i(_roots);
		Str *k = (Str *)0;
		Locator *v = (Locator *)0;
		while (i.next(k,v)) {
			if (*v) {
				for(std::vector<Identity>::const_iterator id(v->virt().begin());id!=v->virt().end();++id) {
					const SharedPtr<Peer> *ap;
					{
						Mutex::Lock l2(_peers_l);
						ap = _peers.get(id->address());
					}
					if (ap) {
						if (!f(*ap,v->phy()))
							return;
					} else {
						SharedPtr<Peer> p(new Peer(RR,_myIdentity,*id));
						{
							Mutex::Lock l2(_peers_l);
							_peers.set(id->address(),p);
						}
						if (!f(p,v->phy()))
							return;
					}
				}
			}
		}
	}

	/**
	 * @return Current best root (updated automatically each second)
	 */
	inline SharedPtr<Peer> root(const int64_t now)
	{
		Mutex::Lock l(_bestRoot_l);
		if ((!_bestRoot)||((now - _lastUpdatedBestRoot) > 1000)) {
			_lastUpdatedBestRoot = now;
			_RootRankingFunction rrf;
			rrf.now = now;
			eachRoot(rrf);
			_bestRoot = rrf.bestRoot;
		}
		return _bestRoot;
	}

	/**
	 * Iterate through all root names
	 *
	 * @param f Function of (Str,Locator)
	 */
	template<typename F>
	ZT_ALWAYS_INLINE void eachRootName(F f) const
	{
		Mutex::Lock l(_roots_l);
		Str *k = (Str *)0;
		Locator *v = (Locator *)0;
		Hashtable< Str,Locator >::Iterator i(const_cast<Topology *>(this)->_roots);
		while (i.next(k,v)) {
			if (!f(*k,*v))
				break;
		}
	}

	/**
	 * Set or update dynamic root if new locator is newer
	 *
	 * This does not check signatures or internal validity of the locator.
	 *
	 * @param name DNS name used to retrive root or simply the address for static roots
	 * @param latestLocator Latest locator
	 * @return True if locator is newer or if a new entry was created
	 */
	inline bool setRoot(const Str &name,const SharedPtr<const Locator> &latestLocator)
	{
		Mutex::Lock l(_roots_l);
		if (latestLocator) {
			SharedPtr<const Locator> &ll = _roots[name];
			if ((ll)&&(ll->timestamp() < latestLocator->timestamp())) {
				ll = latestLocator;
				_updateRoots();
				_rootsModified = true;
				return true;
			}
		} else if (!_roots.contains(name)) {
			_roots.set(name,SharedPtr<const Locator>()); // no locator known yet, but add name to name list to trigger DNS refreshing
			_rootsModified = true;
			return true;
		}
		return false;
	}

	/**
	 * Remove a dynamic root entry
	 */
	inline void removeRoot(const Str &name)
	{
		Mutex::Lock l(_roots_l);
		if (_roots.erase(name)) {
			_updateRoots();
			_rootsModified = true;
		}
	}

	/**
	 * @param Current time
	 * @return ZT_RootList as returned by the external CAPI
	 */
	inline ZT_RootList *apiRoots(const int64_t now) const
	{
		ScopedPtr< Buffer<65536> > lbuf(new Buffer<65536>());
		Mutex::Lock l2(_roots_l);
		ZT_RootList *rl = (ZT_RootList *)malloc(sizeof(ZT_RootList) + (sizeof(ZT_Root) * _roots.size()) + (256 * _roots.size()) + (65536 * _roots.size()));
		if (!rl)
			return nullptr;
		char *nptr = ((char *)rl) + sizeof(ZT_RootList) + (sizeof(ZT_Root) * _roots.size());
		uint8_t *lptr = ((uint8_t *)nptr) + (256 * _roots.size());

		unsigned int c = 0;
		Str *k = (Str *)0;
		SharedPtr<const Locator> *v = (SharedPtr<const Locator> *)0;
		Hashtable< Str,SharedPtr<const Locator> >::Iterator i(const_cast<Topology *>(this)->_roots);
		while (i.next(k,v)) {
			Utils::scopy(nptr,256,k->c_str());
			rl->roots[c].name = nptr;
			nptr += 256;
			if (*v) {
				lbuf->clear();
				(*v)->serialize(*lbuf);
				memcpy(lptr,lbuf->unsafeData(),lbuf->size());
				rl->roots[c].locator = lptr;
				rl->roots[c].locatorSize = lbuf->size();
			} else {
				rl->roots[c].locator = nullptr;
				rl->roots[c].locatorSize = 0;
			}
			lptr += 65536;
			++c;
		}

		rl->count = c;
		return rl;
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
		// TODO: in the future this will check 'mesh-like' relays and if enabled consult LF for other roots (for if this is a root)
		return root(now);
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
		while (i.next(a,p)) {
			allPeers.push_back(*p);
		}
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

private:
	const RuntimeEnvironment *const RR;
	const Identity _myIdentity;

	std::pair<InetAddress,ZT_PhysicalPathConfiguration> _physicalPathConfig[ZT_MAX_CONFIGURABLE_PATHS];
	unsigned int _numConfiguredPhysicalPaths;

	Hashtable< Address,SharedPtr<Peer> > _peers;
	Hashtable< Path::HashKey,SharedPtr<Path> > _paths;

	Hashtable< Str,SharedPtr<const Locator> > _roots;
	Hashtable< Identity,bool > _rootIdentities;
	bool _rootsModified;

	int64_t _lastUpdatedBestRoot;
	SharedPtr<Peer> _bestRoot;

	Mutex _peers_l;
	Mutex _paths_l;
	Mutex _roots_l;
	Mutex _bestRoot_l;
};

} // namespace ZeroTier

#endif
