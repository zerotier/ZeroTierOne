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

#include "Constants.hpp"
#include "Address.hpp"
#include "Identity.hpp"
#include "Peer.hpp"
#include "Path.hpp"
#include "Mutex.hpp"
#include "InetAddress.hpp"
#include "SharedPtr.hpp"
#include "ScopedPtr.hpp"
#include "Fingerprint.hpp"
#include "Containers.hpp"
#include "PeerList.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Database of network topology
 */
class Topology
{
public:
	Topology(const RuntimeEnvironment *renv,void *tPtr);

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
	 * @param loadFromCached If false do not load from cache if not in memory (default: true)
	 * @return Peer or NULL if not found
	 */
	ZT_INLINE SharedPtr<Peer> peer(void *tPtr,const Address &zta,const bool loadFromCached = true)
	{
		{
			RWMutex::RLock l(m_peers_l);
			const SharedPtr<Peer> *const ap = m_peers.get(zta);
			if (ap)
				return *ap;
		}
		{
			SharedPtr<Peer> p;
			if (loadFromCached) {
				m_loadCached(tPtr, zta, p);
				if (p) {
					RWMutex::Lock l(m_peers_l);
					SharedPtr<Peer> &hp = m_peers[zta];
					if (hp)
						return hp;
					hp = p;
				}
			}
			return p;
		}
	}

	/**
	 * Get a Path object for a given local and remote physical address, creating if needed
	 *
	 * @param l Local socket
	 * @param r Remote address
	 * @return Pointer to canonicalized Path object or NULL on error
	 */
	ZT_INLINE SharedPtr<Path> path(const int64_t l,const InetAddress &r)
	{
		const uint64_t k = s_getPathKey(l, r);
		{
			RWMutex::RLock lck(m_paths_l);
			SharedPtr<Path> *const p = m_paths.get(k);
			if (p)
				return *p;
		}
		{
			SharedPtr<Path> p(new Path(l,r));
			RWMutex::Lock lck(m_paths_l);
			SharedPtr<Path> &p2 = m_paths[k];
			if (p2)
				return p2;
			p2 = p;
			return p;
		}
	}

	/**
	 * @return Current best root server
	 */
	ZT_INLINE SharedPtr<Peer> root() const
	{
		RWMutex::RLock l(m_peers_l);
		if (m_rootPeers.empty())
			return SharedPtr<Peer>();
		return m_rootPeers.front();
	}

	/**
	 * @param id Identity to check
	 * @return True if this identity corresponds to a root
	 */
	ZT_INLINE bool isRoot(const Identity &id) const
	{
		RWMutex::RLock l(m_peers_l);
		return (m_roots.find(id) != m_roots.end());
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
	ZT_INLINE void eachPeer(F f) const
	{
		RWMutex::RLock l(m_peers_l);
		for(Map< Address,SharedPtr<Peer> >::const_iterator i(m_peers.begin());i != m_peers.end();++i)
			f(i->second);
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
	ZT_INLINE void eachPeerWithRoot(F f) const
	{
		RWMutex::RLock l(m_peers_l);

		Vector<uintptr_t> rootPeerPtrs;
		rootPeerPtrs.reserve(m_rootPeers.size());
		for(Vector< SharedPtr<Peer> >::const_iterator rp(m_rootPeers.begin());rp != m_rootPeers.end();++rp)
			rootPeerPtrs.push_back((uintptr_t)rp->ptr());
		std::sort(rootPeerPtrs.begin(),rootPeerPtrs.end());

		try {
			for(Map< Address,SharedPtr<Peer> >::const_iterator i(m_peers.begin());i != m_peers.end();++i)
				f(i->second,std::binary_search(rootPeerPtrs.begin(),rootPeerPtrs.end(),(uintptr_t)i->second.ptr()));
		} catch ( ... ) {} // should not throw
	}

	/**
	 * @param allPeers vector to fill with all current peers
	 */
	ZT_INLINE void getAllPeers(Vector< SharedPtr<Peer> > &allPeers) const
	{
		RWMutex::RLock l(m_peers_l);
		allPeers.clear();
		allPeers.reserve(m_peers.size());
		for(Map< Address,SharedPtr<Peer> >::const_iterator i(m_peers.begin());i != m_peers.end();++i)
			allPeers.push_back(i->second);
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
	ZT_INLINE void getOutboundPathInfo(const InetAddress &physicalAddress,unsigned int &mtu,uint64_t &trustedPathId)
	{
		for(unsigned int i=0,j=m_numConfiguredPhysicalPaths;i < j;++i) {
			if (m_physicalPathConfig[i].first.containsAddress(physicalAddress)) {
				trustedPathId = m_physicalPathConfig[i].second.trustedPathId;
				mtu = m_physicalPathConfig[i].second.mtu;
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
	ZT_INLINE uint64_t getOutboundPathTrust(const InetAddress &physicalAddress)
	{
		for(unsigned int i=0,j=m_numConfiguredPhysicalPaths;i < j;++i) {
			if (m_physicalPathConfig[i].first.containsAddress(physicalAddress))
				return m_physicalPathConfig[i].second.trustedPathId;
		}
		return 0;
	}

	/**
	 * Check whether in incoming trusted path marked packet is valid
	 *
	 * @param physicalAddress Originating physical address
	 * @param trustedPathId Trusted path ID from packet (from MAC field)
	 */
	ZT_INLINE bool shouldInboundPathBeTrusted(const InetAddress &physicalAddress,const uint64_t trustedPathId)
	{
		for(unsigned int i=0,j=m_numConfiguredPhysicalPaths;i < j;++i) {
			if ((m_physicalPathConfig[i].second.trustedPathId == trustedPathId) && (m_physicalPathConfig[i].first.containsAddress(physicalAddress)))
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
	 * @param tPtr Thread pointer
	 * @param id Root server identity
	 * @return True if root found and removed, false if not found
	 */
	bool removeRoot(void *tPtr,const Identity &id);

	/**
	 * Sort roots in asecnding order of apparent latency
	 *
	 * @param now Current time
	 */
	void rankRoots();

	/**
	 * Do periodic tasks such as database cleanup
	 */
	void doPeriodicTasks(void *tPtr,int64_t now);

	/**
	 * Save all currently known peers to data store
	 */
	void saveAll(void *tPtr);

private:
	void m_loadCached(void *tPtr, const Address &zta, SharedPtr<Peer> &peer);
	void m_writeRootList(void *tPtr);

	// This gets an integer key from an InetAddress for looking up paths.
	static ZT_INLINE uint64_t s_getPathKey(const int64_t l,const InetAddress &r) noexcept
	{
		if (r.family() == AF_INET) {
			return ((uint64_t)(reinterpret_cast<const sockaddr_in *>(&r)->sin_addr.s_addr) << 24U) +
			       ((uint64_t)reinterpret_cast<const sockaddr_in *>(&r)->sin_port << 8U) +
						 (uint64_t)l;
		} else if (r.family() == AF_INET6) {
#ifdef ZT_NO_UNALIGNED_ACCESS
			uint64_t htmp[2];
			Utils::copy<16>(htmp,reinterpret_cast<const sockaddr_in6 *>(&r)->sin6_addr.s6_addr);
			const uint64_t h = htmp[0] ^ htmp[1];
#else
			const uint64_t h = reinterpret_cast<const uint64_t *>(reinterpret_cast<const sockaddr_in6 *>(&r)->sin6_addr.s6_addr)[0] ^
			                   reinterpret_cast<const uint64_t *>(reinterpret_cast<const sockaddr_in6 *>(&r)->sin6_addr.s6_addr)[1];
#endif
			return h + (uint64_t)Utils::ntoh(reinterpret_cast<const struct sockaddr_in6 *>(&r)->sin6_port) ^ (uint64_t)l;
		} else {
			return (uint64_t)Utils::fnv1a32(reinterpret_cast<const void *>(&r),sizeof(InetAddress)) + (uint64_t)l;
		}
	}

	const RuntimeEnvironment *const RR;

	RWMutex m_paths_l;
	RWMutex m_peers_l;

	std::pair< InetAddress,ZT_PhysicalPathConfiguration > m_physicalPathConfig[ZT_MAX_CONFIGURABLE_PATHS];
	unsigned int m_numConfiguredPhysicalPaths;

	Map< uint64_t,SharedPtr<Path> > m_paths;

	Map< Address,SharedPtr<Peer> > m_peers;
	Set< Identity > m_roots;
	Vector< SharedPtr<Peer> > m_rootPeers;
};

} // namespace ZeroTier

#endif
