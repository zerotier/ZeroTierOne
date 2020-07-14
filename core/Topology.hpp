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
#include "Blob.hpp"
#include "FCV.hpp"
#include "Certificate.hpp"
#include "Containers.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Database of network topology
 */
class Topology
{
public:
	Topology(const RuntimeEnvironment *renv, void *tPtr, int64_t now);

	/**
	 * Add peer to database
	 *
	 * This will not replace existing peers. In that case the existing peer
	 * record is returned.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param peer Peer to add
	 * @return New or existing peer (should replace 'peer')
	 */
	SharedPtr< Peer > add(void *tPtr, const SharedPtr< Peer > &peer);

	/**
	 * Get a peer from its address
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param zta ZeroTier address of peer
	 * @param loadFromCached If false do not load from cache if not in memory (default: true)
	 * @return Peer or NULL if not found
	 */
	ZT_INLINE SharedPtr< Peer > peer(void *tPtr, const Address &zta, const bool loadFromCached = true)
	{
		{
			RWMutex::RLock l(m_peers_l);
			Map< Address, SharedPtr< Peer > >::const_iterator ap(m_peers.find(zta));
			if (likely(ap != m_peers.end()))
				return ap->second;
		}
		if (loadFromCached)
			return m_peerFromCached(tPtr, zta);
		return SharedPtr< Peer >();
	}

	/**
	 * Get a Path object for a given local and remote physical address, creating if needed
	 *
	 * @param l Local socket
	 * @param r Remote address
	 * @return Pointer to canonicalized Path object or NULL on error
	 */
	ZT_INLINE SharedPtr< Path > path(const int64_t l, const InetAddress &r)
	{
		const UniqueID k(r.key());
		{
			RWMutex::RLock lck(m_paths_l);
			Map< UniqueID, SharedPtr< Path > >::const_iterator p(m_paths.find(k));
			if (likely(p != m_paths.end()))
				return p->second;
		}
		{
			SharedPtr< Path > p(new Path(l, r));
			RWMutex::Lock lck(m_paths_l);
			SharedPtr< Path > &p2 = m_paths[k];
			if (p2)
				return p2;
			p2 = p;
			return p;
		}
	}

	/**
	 * @return Current best root server
	 */
	ZT_INLINE SharedPtr< Peer > root() const
	{
		RWMutex::RLock l(m_roots_l);
		if (unlikely(m_rootPeers.empty()))
			return SharedPtr< Peer >();
		return m_rootPeers.front();
	}

	/**
	 * @param id Identity to check
	 * @return True if this identity corresponds to a root
	 */
	ZT_INLINE bool isRoot(const Identity &id) const
	{
		RWMutex::RLock l(m_roots_l);
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
	template< typename F >
	ZT_INLINE void eachPeer(F f) const
	{
		RWMutex::RLock l(m_peers_l);
		for (Map< Address, SharedPtr< Peer > >::const_iterator i(m_peers.begin()); i != m_peers.end(); ++i)
			f(i->second);
	}

	/**
	 * @param allPeers vector to fill with all current peers
	 */
	ZT_INLINE void getAllPeers(Vector< SharedPtr< Peer > > &allPeers) const
	{
		allPeers.clear();
		RWMutex::RLock l(m_peers_l);
		allPeers.reserve(m_peers.size());
		for (Map< Address, SharedPtr< Peer > >::const_iterator i(m_peers.begin()); i != m_peers.end(); ++i)
			allPeers.push_back(i->second);
	}

	/**
	 * @param allPeers vector to fill with all current peers
	 */
	ZT_INLINE void getAllPeers(Vector< SharedPtr< Peer > > &allPeers, Vector< SharedPtr< Peer > > &rootPeers) const
	{
		allPeers.clear();
		RWMutex::RLock l(m_peers_l);
		allPeers.reserve(m_peers.size());
		for (Map< Address, SharedPtr< Peer > >::const_iterator i(m_peers.begin()); i != m_peers.end(); ++i)
			allPeers.push_back(i->second);
		rootPeers = m_rootPeers;
	}

	/**
	 * Flag a peer as a root, adding the peer if it is not known
	 *
	 * @param tPtr Thread pointer
	 * @param id Root identity (will be locally validated)
	 * @return Root peer or NULL if some problem occurred
	 */
	SharedPtr< Peer > addRoot(void *tPtr, const Identity &id);

	/**
	 * Remove a root server's identity from the root server set
	 *
	 * @param tPtr Thread pointer
	 * @param address Root address
	 * @return True if root found and removed, false if not found
	 */
	bool removeRoot(void *tPtr, Address address);

	/**
	 * Sort roots in ascending order of apparent latency
	 *
	 * @param now Current time
	 */
	void rankRoots();

	/**
	 * Do periodic tasks such as database cleanup
	 *
	 * @param tPtr Thread pointer
	 * @param now Current time
	 */
	void doPeriodicTasks(void *tPtr, int64_t now);

	/**
	 * Save all currently known peers to data store
	 *
	 * @param tPtr Thread pointer
	 */
	void saveAll(void *tPtr);

	/**
	 * Add a certificate to the local certificate store
	 *
	 * @param tPtr Thread pointer
	 * @param cert Certificate to add (a copy will be made if added)
	 * @param now Current time
	 * @param localTrust Local trust bit flags
	 * @param writeToLocalStore If true, write to local object store (via API callbacks)
	 * @param refreshRootSets If true, refresh root sets in case a root set changed (default: true)
	 * @param verify If true, verify certificate and certificate chain (default: true)
	 * @return Error or 0 on success
	 */
	ZT_CertificateError addCertificate(void *tPtr, const Certificate &cert, const int64_t now, unsigned int localTrust, bool writeToLocalStore, bool refreshRootSets = true, bool verify = true);

private:
	void m_eraseCertificate_l_certs(const SharedPtr< const Certificate > &cert);

	bool m_cleanCertificates_l_certs(int64_t now);

	bool m_verifyCertificateChain_l_certs(const Certificate *current, const int64_t now) const;

	ZT_CertificateError m_verifyCertificate_l_certs(const Certificate &cert, const int64_t now, unsigned int localTrust, bool skipSignatureCheck) const;

	void m_loadCached(void *tPtr, const Address &zta, SharedPtr< Peer > &peer);

	SharedPtr< Peer > m_peerFromCached(void *tPtr, const Address &zta);

	void m_updateRootPeers_l_roots_certs(void *tPtr);

	void m_writeTrustStore_l_roots_certs(void *tPtr) const;

	const RuntimeEnvironment *const RR;

	RWMutex m_paths_l; // m_paths
	RWMutex m_peers_l; // m_peers
	RWMutex m_roots_l; // m_roots, m_rootPeers
	Mutex m_certs_l;   // m_certs, m_certsBySubjectIdentity

	Map< UniqueID, SharedPtr< Path > > m_paths;

	Map< Address, SharedPtr< Peer > > m_peers;

	Map< Identity, Set< SharedPtr< const Certificate > > > m_roots;
	Vector< SharedPtr< Peer > > m_rootPeers;

	Map< SHA384Hash, std::pair< SharedPtr< const Certificate >, unsigned int > > m_certs;
	Map< Fingerprint, Map< SharedPtr< const Certificate >, unsigned int > > m_certsBySubjectIdentity;
	SortedMap< Vector< uint8_t >, std::pair< SharedPtr< const Certificate >, unsigned int > > m_certsBySubjectUniqueId;
};

} // namespace ZeroTier

#endif
