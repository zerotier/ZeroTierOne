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
		return m_newPath(l, r, k);
	}

	/**
	 * @return Current best root (lowest latency active root)
	 */
	ZT_INLINE SharedPtr< Peer > root(const int64_t now)
	{
		RWMutex::RMaybeWLock l(m_roots_l);
		if (unlikely(m_roots.empty()))
			return SharedPtr< Peer >();
		if (unlikely((now - m_lastRankedRoots) > (ZT_PATH_KEEPALIVE_PERIOD / 2))) {
			l.writing();
			m_rankRoots(now);
		}
		return m_roots.front();
	}

	/**
	 * @param allPeers vector to fill with all current peers
	 */
	void allPeers(Vector< SharedPtr< Peer > > &allPeers, Vector< SharedPtr< Peer > > &rootPeers) const;

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
	ZT_CertificateError addCertificate(
		void *tPtr,
		const Certificate &cert,
		int64_t now,
		unsigned int localTrust,
		bool writeToLocalStore,
		bool refreshRootSets = true,
		bool verify = true);

	/**
	 * Delete certificate
	 *
	 * @param tPtr Thread pointer
	 * @param serialNo Serial number to delete
	 * @return Number of deleted certificates
	 */
	unsigned int deleteCertificate(void *tPtr,const uint8_t serialNo[ZT_SHA384_DIGEST_SIZE]);

	/**
	 * Fill vectors with all certificates and their corresponding local trust flags
	 *
	 * @param c Certificate vector
	 * @param t Local trust vector
	 */
	void allCerts(Vector< SharedPtr<const Certificate> > &c,Vector< unsigned int > &t) const noexcept;

private:
	void m_rankRoots(int64_t now);
	void m_eraseCertificate(void *tPtr, const SharedPtr< const Certificate > &cert, const SHA384Hash *uniqueIdHash);
	bool m_cleanCertificates(void *tPtr, int64_t now);
	bool m_verifyCertificateChain(const Certificate *current, int64_t now) const;
	ZT_CertificateError m_verifyCertificate(const Certificate &cert, int64_t now, unsigned int localTrust, bool skipSignatureCheck) const;
	void m_loadCached(void *tPtr, const Address &zta, SharedPtr< Peer > &peer);
	SharedPtr< Peer > m_peerFromCached(void *tPtr, const Address &zta);
	SharedPtr< Path > m_newPath(int64_t l, const InetAddress &r, const UniqueID &k);
	void m_updateRootPeers(void *tPtr, int64_t now);
	void m_writeTrustStore(void *tPtr);

	const RuntimeEnvironment *const RR;

	int64_t m_lastRankedRoots;
	Vector< SharedPtr< Peer > > m_roots;
	Map< Address, SharedPtr< Peer > > m_peers;
	Map< UniqueID, SharedPtr< Path > > m_paths;

	struct p_CertEntry
	{
		ZT_INLINE p_CertEntry() :
			certificate(),
			localTrust(0)
		{}
		SharedPtr< const Certificate > certificate;
		unsigned int localTrust;
	};
	Map< SHA384Hash, p_CertEntry > m_certs;
	Map< SHA384Hash, p_CertEntry > m_certsBySubjectUniqueID;
	Map< Fingerprint, Map< SharedPtr< const Certificate >, unsigned int > > m_certsBySubjectIdentity;

	RWMutex m_paths_l; // m_paths
	RWMutex m_peers_l; // m_peers
	RWMutex m_roots_l; // m_roots and m_lastRankedRoots
	Mutex m_certs_l;   // m_certs and friends
};

} // namespace ZeroTier

#endif
