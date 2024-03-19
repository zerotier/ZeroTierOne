/*
 * Copyright (c)2019 ZeroTier, Inc.
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
#include "World.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Database of network topology
 */
class Topology
{
public:
	Topology(const RuntimeEnvironment *renv,void *tPtr);
	~Topology();

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
	SharedPtr<Peer> addPeer(void *tPtr,const SharedPtr<Peer> &peer);

	/**
	 * Get a peer from its address
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param zta ZeroTier address of peer
	 * @return Peer or NULL if not found
	 */
	SharedPtr<Peer> getPeer(void *tPtr,const Address &zta);

	/**
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param zta ZeroTier address of peer
	 * @return Identity or NULL identity if not found
	 */
	Identity getIdentity(void *tPtr,const Address &zta);

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
		if (ap) {
			return *ap;
		}
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
		if (!p) {
			p.set(new Path(l,r));
		}
		return p;
	}

	/**
	 * Get the current best upstream peer
	 *
	 * @return Upstream or NULL if none available
	 */
	SharedPtr<Peer> getUpstreamPeer();

	/**
	 * @param id Identity to check
	 * @return True if this is a root server or a network preferred relay from one of our networks
	 */
	bool isUpstream(const Identity &id) const;

	/**
	 * @param addr Address to check
	 * @return True if we should accept a world update from this address
	 */
	bool shouldAcceptWorldUpdateFrom(const Address &addr) const;

	/**
	 * @param ztaddr ZeroTier address
	 * @return Peer role for this device
	 */
	ZT_PeerRole role(const Address &ztaddr) const;

	/**
	 * Check for prohibited endpoints
	 *
	 * Right now this returns true if the designated ZT address is a root and if
	 * the IP (IP only, not port) does not equal any of the IPs defined in the
	 * current World. This is an extra little security feature in case root keys
	 * get appropriated or something.
	 *
	 * Otherwise it returns false.
	 *
	 * @param ztaddr ZeroTier address
	 * @param ipaddr IP address
	 * @return True if this ZT/IP pair should not be allowed to be used
	 */
	bool isProhibitedEndpoint(const Address &ztaddr,const InetAddress &ipaddr) const;

	/**
	 * Gets upstreams to contact and their stable endpoints (if known)
	 *
	 * @param eps Hash table to fill with addresses and their stable endpoints
	 */
	inline void getUpstreamsToContact(Hashtable< Address,std::vector<InetAddress> > &eps) const
	{
		Mutex::Lock _l(_upstreams_m);
		for(std::vector<World::Root>::const_iterator i(_planet.roots().begin());i!=_planet.roots().end();++i) {
			if (i->identity != RR->identity) {
				std::vector<InetAddress> &ips = eps[i->identity.address()];
				for(std::vector<InetAddress>::const_iterator j(i->stableEndpoints.begin());j!=i->stableEndpoints.end();++j) {
					if (std::find(ips.begin(),ips.end(),*j) == ips.end()) {
						ips.push_back(*j);
					}
				}
			}
		}
		for(std::vector<World>::const_iterator m(_moons.begin());m!=_moons.end();++m) {
			for(std::vector<World::Root>::const_iterator i(m->roots().begin());i!=m->roots().end();++i) {
				if (i->identity != RR->identity) {
					std::vector<InetAddress> &ips = eps[i->identity.address()];
					for(std::vector<InetAddress>::const_iterator j(i->stableEndpoints.begin());j!=i->stableEndpoints.end();++j) {
						if (std::find(ips.begin(),ips.end(),*j) == ips.end()) {
							ips.push_back(*j);
						}
					}
				}
			}
		}
		for(std::vector< std::pair<uint64_t,Address> >::const_iterator m(_moonSeeds.begin());m!=_moonSeeds.end();++m) {
			eps[m->second];
		}
	}

	/**
	 * @return Vector of active upstream addresses (including roots)
	 */
	inline std::vector<Address> upstreamAddresses() const
	{
		Mutex::Lock _l(_upstreams_m);
		return _upstreamAddresses;
	}

	/**
	 * @return Current moons
	 */
	inline std::vector<World> moons() const
	{
		Mutex::Lock _l(_upstreams_m);
		return _moons;
	}

	/**
	 * @return Moon IDs we are waiting for from seeds
	 */
	inline std::vector<uint64_t> moonsWanted() const
	{
		Mutex::Lock _l(_upstreams_m);
		std::vector<uint64_t> mw;
		for(std::vector< std::pair<uint64_t,Address> >::const_iterator s(_moonSeeds.begin());s!=_moonSeeds.end();++s) {
			if (std::find(mw.begin(),mw.end(),s->first) == mw.end()) {
				mw.push_back(s->first);
			}
		}
		return mw;
	}

	/**
	 * @return Current planet
	 */
	inline World planet() const
	{
		Mutex::Lock _l(_upstreams_m);
		return _planet;
	}

	/**
	 * @return Current planet's world ID
	 */
	inline uint64_t planetWorldId() const
	{
		return _planet.id(); // safe to read without lock, and used from within eachPeer() so don't lock
	}

	/**
	 * @return Current planet's world timestamp
	 */
	inline uint64_t planetWorldTimestamp() const
	{
		return _planet.timestamp(); // safe to read without lock, and used from within eachPeer() so don't lock
	}

	/**
	 * Validate new world and update if newer and signature is okay
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param newWorld A new or updated planet or moon to learn
	 * @param alwaysAcceptNew If true, always accept new moons even if we're not waiting for one
	 * @return True if it was valid and newer than current (or totally new for moons)
	 */
	bool addWorld(void *tPtr,const World &newWorld,bool alwaysAcceptNew);

	/**
	 * Add a moon
	 *
	 * This loads it from moons.d if present, and if not adds it to
	 * a list of moons that we want to contact.
	 *
	 * @param id Moon ID
	 * @param seed If non-NULL, an address of any member of the moon to contact
	 */
	void addMoon(void *tPtr,const uint64_t id,const Address &seed);

	/**
	 * Remove a moon
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param id Moon's world ID
	 */
	void removeMoon(void *tPtr,const uint64_t id);

	/**
	 * Clean and flush database
	 */
	void doPeriodicTasks(void *tPtr,int64_t now);

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
			if (pp) {
				++cnt;
			}
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
	 * @return All currently active peers by address (unsorted)
	 */
	inline std::vector< std::pair< Address,SharedPtr<Peer> > > allPeers() const
	{
		Mutex::Lock _l(_peers_m);
		return _peers.entries();
	}

	/**
	 * @return True if I am a root server in a planet or moon
	 */
	inline bool amUpstream() const { return _amUpstream; }

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
			if (_physicalPathConfig[i].first.containsAddress(physicalAddress)) {
				return _physicalPathConfig[i].second.mtu;
			}
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
			if (_physicalPathConfig[i].first.containsAddress(physicalAddress)) {
				return _physicalPathConfig[i].second.trustedPathId;
			}
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
			if ((_physicalPathConfig[i].second.trustedPathId == trustedPathId)&&(_physicalPathConfig[i].first.containsAddress(physicalAddress))) {
				return true;
			}
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
			for(unsigned int i=0,j=_numConfiguredPhysicalPaths;i<j;++i) {
				cpaths[_physicalPathConfig[i].first] = _physicalPathConfig[i].second;
			}

			if (pathConfig) {
				ZT_PhysicalPathConfiguration pc(*pathConfig);

				if (pc.mtu <= 0) {
					pc.mtu = ZT_DEFAULT_PHYSMTU;
				} else if (pc.mtu < ZT_MIN_PHYSMTU) {
					pc.mtu = ZT_MIN_PHYSMTU;
				} else if (pc.mtu > ZT_MAX_PHYSMTU) {
					pc.mtu = ZT_MAX_PHYSMTU;
				}

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
	Identity _getIdentity(void *tPtr,const Address &zta);
	void _memoizeUpstreams(void *tPtr);
	void _savePeer(void *tPtr,const SharedPtr<Peer> &peer);

	const RuntimeEnvironment *const RR;

	std::pair<InetAddress,ZT_PhysicalPathConfiguration> _physicalPathConfig[ZT_MAX_CONFIGURABLE_PATHS];
	volatile unsigned int _numConfiguredPhysicalPaths;

	Hashtable< Address,SharedPtr<Peer> > _peers;
	Mutex _peers_m;

	Hashtable< Path::HashKey,SharedPtr<Path> > _paths;
	Mutex _paths_m;

	World _planet;
	std::vector<World> _moons;
	std::vector< std::pair<uint64_t,Address> > _moonSeeds;
	std::vector<Address> _upstreamAddresses;
	bool _amUpstream;
	Mutex _upstreams_m; // locks worlds, upstream info, moon info, etc.
};

} // namespace ZeroTier

#endif
