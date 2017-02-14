/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include "World.hpp"
#include "CertificateOfRepresentation.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Database of network topology
 */
class Topology
{
public:
	Topology(const RuntimeEnvironment *renv);

	/**
	 * Add a peer to database
	 *
	 * This will not replace existing peers. In that case the existing peer
	 * record is returned.
	 *
	 * @param peer Peer to add
	 * @return New or existing peer (should replace 'peer')
	 */
	SharedPtr<Peer> addPeer(const SharedPtr<Peer> &peer);

	/**
	 * Get a peer from its address
	 *
	 * @param zta ZeroTier address of peer
	 * @return Peer or NULL if not found
	 */
	SharedPtr<Peer> getPeer(const Address &zta);

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
	 * @param l Local address or NULL for 'any' or 'wildcard'
	 * @param r Remote address
	 * @return Pointer to canonicalized Path object
	 */
	inline SharedPtr<Path> getPath(const InetAddress &l,const InetAddress &r)
	{
		Mutex::Lock _l(_paths_m);
		SharedPtr<Path> &p = _paths[Path::HashKey(l,r)];
		if (!p)
			p.setToUnsafe(new Path(l,r));
		return p;
	}

	/**
	 * Get the identity of a peer
	 *
	 * @param zta ZeroTier address of peer
	 * @return Identity or NULL Identity if not found
	 */
	Identity getIdentity(const Address &zta);

	/**
	 * Cache an identity
	 *
	 * This is done automatically on addPeer(), and so is only useful for
	 * cluster identity replication.
	 *
	 * @param id Identity to cache
	 */
	void saveIdentity(const Identity &id);

	/**
	 * Get the current best upstream peer
	 *
	 * @return Root server with lowest latency or NULL if none
	 */
	inline SharedPtr<Peer> getUpstreamPeer() { return getUpstreamPeer((const Address *)0,0,false); }

	/**
	 * Get the current best upstream peer, avoiding those in the supplied avoid list
	 *
	 * @param avoid Nodes to avoid
	 * @param avoidCount Number of nodes to avoid
	 * @param strictAvoid If false, consider avoided root servers anyway if no non-avoid root servers are available
	 * @return Root server or NULL if none available
	 */
	SharedPtr<Peer> getUpstreamPeer(const Address *avoid,unsigned int avoidCount,bool strictAvoid);

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
			std::vector<InetAddress> &ips = eps[i->identity.address()];
			for(std::vector<InetAddress>::const_iterator j(i->stableEndpoints.begin());j!=i->stableEndpoints.end();++j) {
				if (std::find(ips.begin(),ips.end(),*j) == ips.end())
					ips.push_back(*j);
			}
		}
		for(std::vector<World>::const_iterator m(_moons.begin());m!=_moons.end();++m) {
			for(std::vector<World::Root>::const_iterator i(m->roots().begin());i!=m->roots().end();++i) {
				std::vector<InetAddress> &ips = eps[i->identity.address()];
				for(std::vector<InetAddress>::const_iterator j(i->stableEndpoints.begin());j!=i->stableEndpoints.end();++j) {
					if (std::find(ips.begin(),ips.end(),*j) == ips.end())
						ips.push_back(*j);
				}
			}
		}
		for(std::vector< std::pair<uint64_t,Address> >::const_iterator m(_moonSeeds.begin());m!=_moonSeeds.end();++m)
			eps[m->second];
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
			if (std::find(mw.begin(),mw.end(),s->first) == mw.end())
				mw.push_back(s->first);
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
	 * @param newWorld A new or updated planet or moon to learn
	 * @param alwaysAcceptNew If true, always accept new moons even if we're not waiting for one
	 * @return True if it was valid and newer than current (or totally new for moons)
	 */
	bool addWorld(const World &newWorld,bool alwaysAcceptNew);

	/**
	 * Add a moon
	 *
	 * This loads it from moons.d if present, and if not adds it to
	 * a list of moons that we want to contact.
	 *
	 * @param id Moon ID
	 * @param seed If non-NULL, an address of any member of the moon to contact
	 */
	void addMoon(const uint64_t id,const Address &seed);

	/**
	 * Remove a moon
	 *
	 * @param id Moon's world ID
	 */
	void removeMoon(const uint64_t id);

	/**
	 * Clean and flush database
	 */
	void clean(uint64_t now);

	/**
	 * @param now Current time
	 * @return Number of peers with active direct paths
	 */
	inline unsigned long countActive(uint64_t now) const
	{
		unsigned long cnt = 0;
		Mutex::Lock _l(_peers_m);
		Hashtable< Address,SharedPtr<Peer> >::Iterator i(const_cast<Topology *>(this)->_peers);
		Address *a = (Address *)0;
		SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
		while (i.next(a,p)) {
			cnt += (unsigned long)((*p)->hasActiveDirectPath(now));
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
#ifdef ZT_TRACE
			if (!(*p)) {
				fprintf(stderr,"FATAL BUG: eachPeer() caught NULL peer for %s -- peer pointers in Topology should NEVER be NULL" ZT_EOL_S,a->toString().c_str());
				abort();
			}
#endif
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
	inline bool amRoot() const { return _amRoot; }

	/**
	 * Get the outbound trusted path ID for a physical address, or 0 if none
	 *
	 * @param physicalAddress Physical address to which we are sending the packet
	 * @return Trusted path ID or 0 if none (0 is not a valid trusted path ID)
	 */
	inline uint64_t getOutboundPathTrust(const InetAddress &physicalAddress)
	{
		for(unsigned int i=0;i<_trustedPathCount;++i) {
			if (_trustedPathNetworks[i].containsAddress(physicalAddress))
				return _trustedPathIds[i];
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
		for(unsigned int i=0;i<_trustedPathCount;++i) {
			if ((_trustedPathIds[i] == trustedPathId)&&(_trustedPathNetworks[i].containsAddress(physicalAddress)))
				return true;
		}
		return false;
	}

	/**
	 * Set trusted paths in this topology
	 *
	 * @param networks Array of networks (prefix/netmask bits)
	 * @param ids Array of trusted path IDs
	 * @param count Number of trusted paths (if larger than ZT_MAX_TRUSTED_PATHS overflow is ignored)
	 */
	inline void setTrustedPaths(const InetAddress *networks,const uint64_t *ids,unsigned int count)
	{
		if (count > ZT_MAX_TRUSTED_PATHS)
			count = ZT_MAX_TRUSTED_PATHS;
		Mutex::Lock _l(_trustedPaths_m);
		for(unsigned int i=0;i<count;++i) {
			_trustedPathIds[i] = ids[i];
			_trustedPathNetworks[i] = networks[i];
		}
		_trustedPathCount = count;
	}

	/**
	 * @return Current certificate of representation (copy)
	 */
	inline CertificateOfRepresentation certificateOfRepresentation() const
	{
		Mutex::Lock _l(_upstreams_m);
		return _cor;
	}

	/**
	 * @param buf Buffer to receive COR
	 */
	template<unsigned int C>
	void appendCertificateOfRepresentation(Buffer<C> &buf)
	{
		Mutex::Lock _l(_upstreams_m);
		_cor.serialize(buf);
	}

private:
	Identity _getIdentity(const Address &zta);
	void _memoizeUpstreams();

	const RuntimeEnvironment *const RR;

	uint64_t _trustedPathIds[ZT_MAX_TRUSTED_PATHS];
	InetAddress _trustedPathNetworks[ZT_MAX_TRUSTED_PATHS];
	unsigned int _trustedPathCount;
	Mutex _trustedPaths_m;

	Hashtable< Address,SharedPtr<Peer> > _peers;
	Mutex _peers_m;

	Hashtable< Path::HashKey,SharedPtr<Path> > _paths;
	Mutex _paths_m;

	World _planet;
	std::vector<World> _moons;
	std::vector< std::pair<uint64_t,Address> > _moonSeeds;
	std::vector<Address> _upstreamAddresses;
	CertificateOfRepresentation _cor;
	bool _amRoot;
	Mutex _upstreams_m; // locks worlds, upstream info, moon info, etc.
};

} // namespace ZeroTier

#endif
