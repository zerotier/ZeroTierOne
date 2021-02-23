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
	RR(renv),
	m_lastRankedRoots(0)
{
	char tmp[32];
	Dictionary d;

	Vector< uint8_t > trustData(RR->node->stateObjectGet(tPtr, ZT_STATE_OBJECT_TRUST_STORE, Utils::ZERO256, 0));
	if (trustData.empty() || (!d.decode(trustData.data(), (unsigned int)trustData.size()))) {
		if (!d.decode(Defaults::CERTIFICATES, Defaults::CERTIFICATES_BYTES))
			d.clear();
	}

	if (!d.empty()) {
		const unsigned long certCount = (unsigned long)d.getUI("c$");
		for (unsigned long idx = 0; idx < certCount; ++idx) {
			uint64_t id[6];
			const Vector< uint8_t > &serialNo = d[Dictionary::arraySubscript(tmp, sizeof(tmp), "c$.s", idx)];
			if (serialNo.size() == ZT_SHA384_DIGEST_SIZE) {
				Utils::copy< 48 >(id, serialNo.data());
				Certificate cert;
				Vector< uint8_t > enc(RR->node->stateObjectGet(tPtr, ZT_STATE_OBJECT_CERT, id, 6));
				if (cert.decode(enc.data(), (unsigned int)enc.size()))
					addCertificate(tPtr, cert, now, (unsigned int)d.getUI(Dictionary::arraySubscript(tmp, sizeof(tmp), "c$.lt", idx)), false, false, false);
			}
		}
		m_cleanCertificates(tPtr, now);
		m_updateRootPeers(tPtr, now);
	}
}

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
		RWMutex::RLock l(m_roots_l);
		rootPeers = m_roots;
	}
}

void Topology::doPeriodicTasks(void *tPtr, const int64_t now)
{
	// Clean any expired certificates, updating roots if they have changed.
	{
		Mutex::Lock l1(m_certs_l);
		if (m_cleanCertificates(tPtr, now)) {
			m_writeTrustStore(tPtr);
			{
				RWMutex::Lock l3(m_peers_l);
				RWMutex::Lock l2(m_roots_l);
				m_updateRootPeers(tPtr, now);
			}
		}
	}

	// Cleaning of peers and paths uses a two pass method to avoid write locking
	// m_peers or m_paths for any significant amount of time. This avoids pauses
	// on nodes with large numbers of peers or paths.

	// Delete peers that are stale or offline and are not roots. First pass: grab
	// peers to delete in read lock mode. Second pass: delete peers one by one,
	// acquiring hard write lock each time to avoid pauses.
	{
		Vector< uintptr_t > rootLookup;
		{
			RWMutex::RLock l2(m_roots_l);
			rootLookup.reserve(m_roots.size());
			for (Vector< SharedPtr< Peer > >::const_iterator r(m_roots.begin()); r != m_roots.end(); ++r)
				rootLookup.push_back((uintptr_t)r->ptr());
		}

		Vector< Address > toDelete;
		{
			RWMutex::RLock l1(m_peers_l);
			for (Map< Address, SharedPtr< Peer > >::iterator i(m_peers.begin()); i != m_peers.end(); ++i) {
				// TODO: also delete if the peer has not exchanged meaningful communication in a while, such as
				// a network frame or non-trivial control packet.
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
		Vector< UniqueID > possibleDelete;
		{
			RWMutex::RLock l1(m_paths_l);
			for (Map< UniqueID, SharedPtr< Path > >::iterator i(m_paths.begin()); i != m_paths.end(); ++i) {
				if (i->second.references() <= 1)
					possibleDelete.push_back(i->first);
			}
		}
		if (!possibleDelete.empty()) {
			ZT_SPEW("garbage collecting (likely) %u orphaned paths", (unsigned int)possibleDelete.size());
			for (Vector< UniqueID >::const_iterator i(possibleDelete.begin()); i != possibleDelete.end(); ++i) {
				RWMutex::Lock l1(m_paths_l);
				Map< UniqueID, SharedPtr< Path > >::iterator p(m_paths.find(*i));
				if ((p != m_paths.end()) && p->second.weakGC())
					m_paths.erase(p);
			}
		}
	}
}

void Topology::saveAll(void *tPtr)
{
	{
		RWMutex::RLock l(m_peers_l);
		for (Map< Address, SharedPtr< Peer > >::iterator i(m_peers.begin()); i != m_peers.end(); ++i)
			i->second->save(tPtr);
	}
	{
		Mutex::Lock l(m_certs_l);
		m_writeTrustStore(tPtr);
	}
}

ZT_CertificateError Topology::addCertificate(void *tPtr, const Certificate &cert, const int64_t now, const unsigned int localTrust, const bool writeToLocalStore, const bool refreshRootSets, const bool verify)
{
	{
		const SHA384Hash serial(cert.serialNo);
		p_CertEntry certEntry;
		Mutex::Lock l1(m_certs_l);

		{
			Map< SHA384Hash, p_CertEntry >::iterator c(m_certs.find(serial));
			if (c != m_certs.end()) {
				if (c->second.localTrust == localTrust)
					return ZT_CERTIFICATE_ERROR_NONE;
				certEntry.certificate = c->second.certificate;
			}
		}
		if (!certEntry.certificate) {
			certEntry.certificate.set(new Certificate(cert));
			if (verify) {
				m_cleanCertificates(tPtr, now);
				const ZT_CertificateError err = m_verifyCertificate(cert, now, localTrust, false);
				if (err != ZT_CERTIFICATE_ERROR_NONE)
					return err;
			}
		}

		certEntry.localTrust = localTrust;

		if ((cert.subject.uniqueId) && (cert.subject.uniqueIdSize > 0)) {
			SHA384Hash uniqueIdHash;
			SHA384(uniqueIdHash.data, cert.subject.uniqueId, cert.subject.uniqueIdSize);
			p_CertEntry &bySubjectUniqueId = m_certsBySubjectUniqueID[uniqueIdHash];
			if (bySubjectUniqueId.certificate) {
				if (bySubjectUniqueId.certificate->subject.timestamp >= cert.subject.timestamp)
					return ZT_CERTIFICATE_ERROR_HAVE_NEWER_CERT;
				m_eraseCertificate(tPtr, bySubjectUniqueId.certificate, &uniqueIdHash);
				m_certsBySubjectUniqueID[uniqueIdHash] = certEntry;
			} else {
				bySubjectUniqueId = certEntry;
			}
		}

		for (unsigned int i = 0; i < cert.subject.identityCount; ++i) {
			const Identity *const ii = reinterpret_cast<const Identity *>(cert.subject.identities[i].identity);
			if (ii)
				m_certsBySubjectIdentity[ii->fingerprint()][certEntry.certificate] = localTrust;
		}

		m_certs[serial] = certEntry;

		if (refreshRootSets) {
			RWMutex::Lock l3(m_peers_l);
			RWMutex::Lock l2(m_roots_l);
			m_updateRootPeers(tPtr, now);
		}

		if (writeToLocalStore)
			m_writeTrustStore(tPtr);
	}

	if (writeToLocalStore) {
		Vector< uint8_t > certData(cert.encode());
		uint64_t id[6];
		Utils::copy< 48 >(id, cert.serialNo);
		RR->node->stateObjectPut(tPtr, ZT_STATE_OBJECT_CERT, id, 6, certData.data(), (unsigned int)certData.size());
	}

	return ZT_CERTIFICATE_ERROR_NONE;
}

unsigned int Topology::deleteCertificate(void *tPtr,const uint8_t serialNo[ZT_SHA384_DIGEST_SIZE])
{
	Mutex::Lock l(m_certs_l);
	const unsigned long origCertCount = (unsigned long)m_certs.size();
	Map< SHA384Hash, p_CertEntry >::const_iterator c(m_certs.find(SHA384Hash(serialNo)));
	if (c != m_certs.end()) {
		if ((c->second.certificate->subject.uniqueId) && (c->second.certificate->subject.uniqueIdSize > 0)) {
			SHA384Hash uniqueIdHash;
			SHA384(uniqueIdHash.data, c->second.certificate->subject.uniqueId, c->second.certificate->subject.uniqueIdSize);
			m_eraseCertificate(tPtr, c->second.certificate, &uniqueIdHash);
		} else {
			m_eraseCertificate(tPtr, c->second.certificate, nullptr);
		}

		const int64_t now = RR->node->now();
		m_cleanCertificates(tPtr, now);
		m_writeTrustStore(tPtr);
		{
			RWMutex::Lock l3(m_peers_l);
			RWMutex::Lock l2(m_roots_l);
			m_updateRootPeers(tPtr, now);
		}
	}
	return (unsigned int)(origCertCount - (unsigned long)m_certs.size());
}

void Topology::allCerts(Vector< SharedPtr<const Certificate> > &c,Vector< unsigned int > &t) const noexcept
{
	Mutex::Lock l(m_certs_l);
	const unsigned long cs = (unsigned long)m_certs.size();
	c.reserve(cs);
	t.reserve(cs);
	for(Map< SHA384Hash, p_CertEntry >::const_iterator i(m_certs.begin());i!=m_certs.end();++i) {
		c.push_back(i->second.certificate);
		t.push_back(i->second.localTrust);
	}
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
	m_lastRankedRoots = now;
	std::sort(m_roots.begin(), m_roots.end(), p_RootRankingComparisonOperator());
}

void Topology::m_eraseCertificate(void *tPtr, const SharedPtr< const Certificate > &cert, const SHA384Hash *uniqueIdHash)
{
	// assumes m_certs is locked for writing

	const SHA384Hash serialNo(cert->serialNo);
	m_certs.erase(serialNo);

	if (uniqueIdHash)
		m_certsBySubjectUniqueID.erase(*uniqueIdHash);

	for (unsigned int i = 0; i < cert->subject.identityCount; ++i) {
		const Identity *const ii = reinterpret_cast<const Identity *>(cert->subject.identities[i].identity);
		Map< Fingerprint, Map< SharedPtr< const Certificate >, unsigned int > >::iterator bySubjectIdentity(m_certsBySubjectIdentity.find(ii->fingerprint()));
		if (bySubjectIdentity != m_certsBySubjectIdentity.end()) {
			bySubjectIdentity->second.erase(cert);
			if (bySubjectIdentity->second.empty())
				m_certsBySubjectIdentity.erase(bySubjectIdentity);
		}
	}

	RR->node->stateObjectDelete(tPtr, ZT_STATE_OBJECT_CERT, serialNo.data, 6);
}

bool Topology::m_cleanCertificates(void *tPtr, int64_t now)
{
	// assumes m_certs is locked for writing

	bool deleted = false;
	Vector< SharedPtr< const Certificate >> toDelete;
	for (;;) {
		for (Map< SHA384Hash, p_CertEntry >::iterator c(m_certs.begin()); c != m_certs.end(); ++c) {
			// Verify, but the last boolean option tells it to skip signature checks as this would
			// already have been done. This will therefore just check the path and validity times
			// of the certificate.
			const ZT_CertificateError err = m_verifyCertificate(*(c->second.certificate), now, c->second.localTrust, true);
			if (err != ZT_CERTIFICATE_ERROR_NONE)
				toDelete.push_back(c->second.certificate);
		}

		if (toDelete.empty())
			break;
		deleted = true;

		SHA384Hash uniqueIdHash;
		for (Vector< SharedPtr< const Certificate > >::iterator c(toDelete.begin()); c != toDelete.end(); ++c) {
			if ((*c)->subject.uniqueId) {
				SHA384(uniqueIdHash.data, (*c)->subject.uniqueId, (*c)->subject.uniqueIdSize);
				m_eraseCertificate(tPtr, *c, &uniqueIdHash);
			} else {
				m_eraseCertificate(tPtr, *c, nullptr);
			}
		}
		toDelete.clear();
	}

	return deleted;
}

bool Topology::m_verifyCertificateChain(const Certificate *current, const int64_t now) const
{
	// assumes m_certs is at least locked for reading

	Map< Fingerprint, Map< SharedPtr< const Certificate >, unsigned int > >::const_iterator c(m_certsBySubjectIdentity.find(reinterpret_cast<const Identity *>(current->issuer)->fingerprint()));
	if (c != m_certsBySubjectIdentity.end()) {
		for (Map< SharedPtr< const Certificate >, unsigned int >::const_iterator cc(c->second.begin()); cc != c->second.end(); ++cc) {
			if (
				(cc->first->maxPathLength > current->maxPathLength) &&
				(cc->first->validity[0] <= now) &&                  // not before now
				(cc->first->validity[1] >= now) &&                  // not after now
				(cc->first->validity[0] <= current->timestamp) &&   // not before child cert's timestamp
				(cc->first->validity[1] >= current->timestamp)      // not after child cert's timestamp
				) {
				if ((cc->second & ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA) != 0)
					return true;
				if (m_verifyCertificateChain(cc->first.ptr(), now))
					return true;
			}
		}
	}

	return false;
}

ZT_CertificateError Topology::m_verifyCertificate(const Certificate &cert, const int64_t now, unsigned int localTrust, bool skipSignatureCheck) const
{
	// assumes m_certs is at least locked for reading

	// Check certificate time window against current time.
	if ((cert.validity[0] > now) || (cert.validity[1] < now))
		return ZT_CERTIFICATE_ERROR_OUT_OF_VALID_TIME_WINDOW;

	// Verify primary and internal signatures and other objects unless the caller
	// elected to skip, which is done to re-check certs already in the DB.
	if (!skipSignatureCheck) {
		const ZT_CertificateError err = cert.verify();
		if (err != ZT_CERTIFICATE_ERROR_NONE)
			return err;
	}

	// If this is a root CA, we can skip this as we're already there. Otherwise we
	// recurse up the tree until we hit a root CA.
	if ((localTrust & ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA) == 0) {
		if (!m_verifyCertificateChain(&cert, now))
			return ZT_CERTIFICATE_ERROR_INVALID_CHAIN;
	}

	return ZT_CERTIFICATE_ERROR_NONE;
}

void Topology::m_loadCached(void *tPtr, const Address &zta, SharedPtr< Peer > &peer)
{
	// does not require any locks to be held

	try {
		uint64_t id[2];
		id[0] = zta.toInt();
		id[1] = 0;
		Vector< uint8_t > data(RR->node->stateObjectGet(tPtr, ZT_STATE_OBJECT_PEER, id, 1));
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

SharedPtr< Path > Topology::m_newPath(const int64_t l, const InetAddress &r, const UniqueID &k)
{
	SharedPtr< Path > p(new Path(l, r));
	RWMutex::Lock lck(m_paths_l);
	SharedPtr< Path > &p2 = m_paths[k];
	if (p2)
		return p2;
	p2 = p;
	return p;
}

void Topology::m_updateRootPeers(void *tPtr, const int64_t now)
{
	// assumes m_certs_l, m_peers_l, and m_roots_l are locked for write

	Set< Identity > rootIdentities;
	for (Map< Fingerprint, Map< SharedPtr< const Certificate >, unsigned int > >::const_iterator c(m_certsBySubjectIdentity.begin()); c != m_certsBySubjectIdentity.end(); ++c) {
		for (Map< SharedPtr< const Certificate >, unsigned int >::const_iterator cc(c->second.begin()); cc != c->second.end(); ++cc) {
			if ((cc->second & ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ZEROTIER_ROOT_SET) != 0) {
				for (unsigned int i = 0; i < cc->first->subject.identityCount; ++i) {
					if (cc->first->subject.identities[i].identity)
						rootIdentities.insert(*reinterpret_cast<const Identity *>(cc->first->subject.identities[i].identity));
				}
			}
		}
	}

	m_roots.clear();
	for (Set< Identity >::const_iterator i(rootIdentities.begin()); i != rootIdentities.end(); ++i) {
		SharedPtr< Peer > &p = m_peers[i->address()];
		if ((!p) || (p->identity() != *i)) {
			p.set(new Peer(RR));
			p->init(*i);
		}
		m_roots.push_back(p);
	}

	m_rankRoots(now);
}

void Topology::m_writeTrustStore(void *tPtr)
{
	// assumes m_certs is locked

	char tmp[32];
	Dictionary d;

	unsigned long idx = 0;
	d.add("c$", (uint64_t)m_certs.size());
	for (Map< SHA384Hash, p_CertEntry >::const_iterator c(m_certs.begin()); c != m_certs.end(); ++c) {
		d[Dictionary::arraySubscript(tmp, sizeof(tmp), "c$.s", idx)].assign(c->first.data, c->first.data + ZT_SHA384_DIGEST_SIZE);
		d.add(Dictionary::arraySubscript(tmp, sizeof(tmp), "c$.lt", idx), (uint64_t)c->second.localTrust);
		++idx;
	}

	Vector< uint8_t > trustStore;
	d.encode(trustStore);
	RR->node->stateObjectPut(tPtr, ZT_STATE_OBJECT_TRUST_STORE, Utils::ZERO256, 0, trustStore.data(), (unsigned int)trustStore.size());
}

} // namespace ZeroTier
