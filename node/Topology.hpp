/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_TOPOLOGY_HPP
#define ZT_TOPOLOGY_HPP

#include <stdio.h>
#include <string.h>

#include <vector>
#include <stdexcept>
#include <algorithm>
#include <utility>

#include "Constants.hpp"
#include "../include/ZeroTierOne.h"

#include "Address.hpp"
#include "Identity.hpp"
#include "Peer.hpp"
#include "Path.hpp"
#include "Mutex.hpp"
#include "InetAddress.hpp"
#include "Hashtable.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Database of network topology
 */
class Topology
{
public:
	inline Topology(const RuntimeEnvironment *renv,void *tPtr) :
		RR(renv),
		_numConfiguredPhysicalPaths(0) {}
	inline ~Topology() {}

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
	inline SharedPtr<Peer> addPeer(void *tPtr,const SharedPtr<Peer> &peer)
	{
		SharedPtr<Peer> np;
		{
			Mutex::Lock _l(_peers_m);
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
	inline SharedPtr<Peer> getPeer(void *tPtr,const Address &zta) const
	{
		if (zta == RR->identity.address())
			return SharedPtr<Peer>();
		{
			Mutex::Lock _l(_peers_m);
			const SharedPtr<Peer> *const ap = _peers.get(zta);
			if (ap)
				return *ap;
		}
		return SharedPtr<Peer>();
	}

	/**
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param zta ZeroTier address of peer
	 * @return Identity or NULL identity if not found
	 */
	inline Identity getIdentity(void *tPtr,const Address &zta)
	{
		if (zta == RR->identity.address()) {
			return RR->identity;
		} else {
			Mutex::Lock _l(_peers_m);
			const SharedPtr<Peer> *const ap = _peers.get(zta);
			if (ap)
				return (*ap)->identity();
		}
		return Identity();
	}
	
	/**
	 * Get a peer only if it is presently in memory (no disk cache)
	 *
	 * This also does not update the lastUsed() time for peers, which means
	 * that it won't prevent them from falling out of RAM. This is currently
	 * used in the Cluster code to update peer info without forcing all peers
	 * across the entire cluster to remain in memory cache.
	 *
	 * @param zta ZeroTier address
	 */
	inline SharedPtr<Peer> getPeerNoCache(const Address &zta)
	{
		Mutex::Lock _l(_peers_m);
		const SharedPtr<Peer> *const ap = _peers.get(zta);
		if (ap)
			return *ap;
		return SharedPtr<Peer>();
	}

	/**
	 * Get a Path object for a given local and remote physical address, creating if needed
	 *
	 * @param l Local socket
	 * @param r Remote address
	 * @return Pointer to canonicalized Path object
	 */
	inline SharedPtr<Path> getPath(const int64_t l,const InetAddress &r)
	{
		Mutex::Lock _l(_paths_m);
		SharedPtr<Path> &p = _paths[Path::HashKey(l,r)];
		if (!p)
			p.set(new Path(l,r));
		return p;
	}

	/**
	 * Get the current best upstream peer
	 *
	 * @return Upstream or NULL if none available
	 */
	inline SharedPtr<Peer> getUpstreamPeer() const
	{
		return SharedPtr<Peer>();
	}

	inline bool isUpstream(const Identity &id) const
	{
		return false;
	}
	
	inline ZT_PeerRole role(const Address &ztaddr) const
	{
		return ZT_PEER_ROLE_LEAF;
	}

	/**
	 * Gets upstreams to contact and their stable endpoints (if known)
	 *
	 * @param eps Hash table to fill with addresses and their stable endpoints
	 */
	inline void getUpstreamsToContact(Hashtable< Address,std::vector<InetAddress> > &eps) const
	{
	}

	/**
	 * @return Vector of active upstream addresses (including roots)
	 */
	inline std::vector<Address> upstreamAddresses() const
	{
		// TODO
		return std::vector<Address>();
	}

	inline void doPeriodicTasks(void *tPtr,int64_t now)
	{
		{
			Mutex::Lock _l1(_peers_m);
			Hashtable< Address,SharedPtr<Peer> >::Iterator i(_peers);
			Address *a = (Address *)0;
			SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
			while (i.next(a,p)) {
				if (!(*p)->isAlive(now)) {
					_peers.erase(*a);
				}
			}
		}
		{
			Mutex::Lock _l(_paths_m);
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
		Mutex::Lock _l(_peers_m);
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
	 * @param f Function to apply
	 * @tparam F Function or function object type
	 */
	template<typename F>
	inline void eachPeer(F f)
	{
		Mutex::Lock _l(_peers_m);
		Hashtable< Address,SharedPtr<Peer> >::Iterator i(_peers);
		Address *a = (Address *)0;
		SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
		while (i.next(a,p)) {
			f(*this,*((const SharedPtr<Peer> *)p));
		}
	}

	/**
	 * @return All peers by address (unsorted)
	 */
	inline std::vector< std::pair< Address,SharedPtr<Peer> > > allPeers() const
	{
		Mutex::Lock _l(_peers_m);
		return _peers.entries();
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
	inline void getOutboundPathInfo(const InetAddress &physicalAddress,unsigned int &mtu,uint64_t &trustedPathId)
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
	inline unsigned int getOutboundPathMtu(const InetAddress &physicalAddress)
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
	inline uint64_t getOutboundPathTrust(const InetAddress &physicalAddress)
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
	inline bool shouldInboundPathBeTrusted(const InetAddress &physicalAddress,const uint64_t trustedPathId)
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
	std::pair<InetAddress,ZT_PhysicalPathConfiguration> _physicalPathConfig[ZT_MAX_CONFIGURABLE_PATHS];
	unsigned int _numConfiguredPhysicalPaths;
	Hashtable< Address,SharedPtr<Peer> > _peers;
	Mutex _peers_m;
	Hashtable< Path::HashKey,SharedPtr<Path> > _paths;
	Mutex _paths_m;
};

} // namespace ZeroTier

#endif
