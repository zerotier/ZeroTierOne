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

#include "Topology.hpp"
#include "Defaults.hpp"

namespace ZeroTier {

Topology::Topology(const RuntimeEnvironment *renv, void *tPtr, const int64_t now) :
	RR(renv),
	m_lastRankedRoots(0)
{
	char tmp[32];
	Dictionary d;

	Vector< uint8_t > trustData(RR->node->stateObjectGet(tPtr, ZT_STATE_OBJECT_TRUST_STORE, Utils::ZERO256));
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
				Vector< uint8_t > enc(RR->node->stateObjectGet(tPtr, ZT_STATE_OBJECT_CERT, id));
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
	// Peer and path delete operations are batched to avoid holding write locks on
	// these structures for any length of time. A list is compiled in read mode,
	// then the write lock is acquired for each delete. This adds overhead if there
	// are a lot of deletions, but that's not common.

	// Clean any expired certificates
	{
		Mutex::Lock l1(m_certs_l);
		if (m_cleanCertificates(tPtr, now)) {
			RWMutex::Lock l3(m_peers_l);
			RWMutex::Lock l2(m_roots_l);
			m_updateRootPeers(tPtr, now);
		}
	}

	// Delete peers that are stale or offline and are not roots.
	{
		Vector< uintptr_t > rootLookup;
		{
			RWMutex::RLock l2(m_roots_l);
			rootLookup.reserve(m_roots.size());
			for (Vector< SharedPtr< Peer > >::const_iterator r(m_roots.begin()); r != m_roots.end(); ++r)
				rootLookup.push_back((uintptr_t)r->ptr());
		}
		std::sort(rootLookup.begin(), rootLookup.end());

		Vector< Address > toDelete;
		{
			RWMutex::RLock l1(m_peers_l);
			for (Map< Address, SharedPtr< Peer > >::iterator i(m_peers.begin()); i != m_peers.end(); ++i) {
				// TODO: also delete if the peer has not exchanged meaningful communication in a while, such as
				// a network frame or non-trivial control packet.
				if (((now - i->second->lastReceive()) > ZT_PEER_ALIVE_TIMEOUT) && (!std::binary_search(rootLookup.begin(), rootLookup.end(), (uintptr_t)i->second.ptr())))
					toDelete.push_back(i->first);
			}
		}
		for (Vector< Address >::iterator i(toDelete.begin()); i != toDelete.end(); ++i) {
			RWMutex::Lock l1(m_peers_l);
			const Map< Address, SharedPtr< Peer > >::iterator p(m_peers.find(*i));
			if (likely(p != m_peers.end())) {
				p->second->save(tPtr);
				m_peers.erase(p);
			}
		}
	}

	// Delete paths that are no longer held by anyone else ("weak reference" type behavior).
	{
		Vector< UniqueID > toDelete;
		{
			RWMutex::RLock l1(m_paths_l);
			for (Map< UniqueID, SharedPtr< Path > >::iterator i(m_paths.begin()); i != m_paths.end(); ++i) {
				if (i->second.weakGC())
					toDelete.push_back(i->first);
			}
		}
		for (Vector< UniqueID >::iterator i(toDelete.begin()); i != toDelete.end(); ++i) {
			RWMutex::Lock l1(m_paths_l);
			const Map< UniqueID, SharedPtr< Path > >::iterator p(m_paths.find(*i));
			if (likely(p != m_paths.end()))
				m_paths.erase(p);
		}
	}
}

void Topology::saveAll(void *tPtr)
{
	{
		RWMutex::RLock l(m_peers_l);
		for (Map< Address, SharedPtr< Peer > >::iterator i(m_peers.begin()); i != m_peers.end(); ++i) {
			i->second->save(tPtr);
		}
	}
	{
		char tmp[32];
		Dictionary d;
		{
			Mutex::Lock l(m_certs_l);
			unsigned long idx = 0;
			d.add("c$", (uint64_t)m_certs.size());
			for (Map< SHA384Hash, std::pair< SharedPtr< const Certificate >, unsigned int > >::const_iterator c(m_certs.begin()); c != m_certs.end(); ++c) {
				d[Dictionary::arraySubscript(tmp, sizeof(tmp), "c$.s", idx)].assign(c->first.data, c->first.data + ZT_SHA384_DIGEST_SIZE);
				d.add(Dictionary::arraySubscript(tmp, sizeof(tmp), "c$.lt", idx), (uint64_t)c->second.second);
				++idx;
			}
		}
		Vector< uint8_t > trustStore;
		d.encode(trustStore);
		RR->node->stateObjectPut(tPtr, ZT_STATE_OBJECT_TRUST_STORE, Utils::ZERO256, trustStore.data(), (unsigned int)trustStore.size());
	}
}

ZT_CertificateError Topology::addCertificate(void *tPtr, const Certificate &cert, const int64_t now, const unsigned int localTrust, const bool writeToLocalStore, const bool refreshRootSets, const bool verify)
{
	{
		Mutex::Lock l1(m_certs_l);

		// Check to see if we already have this specific certificate.
		const SHA384Hash serial(cert.serialNo);
		if (m_certs.find(serial) != m_certs.end())
			return ZT_CERTIFICATE_ERROR_NONE;

		// Verify certificate all the way to a trusted root. This also verifies inner
		// signatures such as those of locators or the subject unique ID.
		if (verify) {
			const ZT_CertificateError err = m_verifyCertificate(cert, now, localTrust, false);
			if (err != ZT_CERTIFICATE_ERROR_NONE)
				return err;
		}

		// Create entry containing copy of certificate and trust flags.
		const std::pair< SharedPtr< const Certificate >, unsigned int > certEntry(SharedPtr< const Certificate >(new Certificate(cert)), localTrust);

		// If the subject contains a unique ID, check if we already have a cert for the
		// same uniquely identified subject. If so, check its subject timestamp and keep
		// the one we have if newer. Otherwise replace it. Note that the verification
		// function will have checked the unique ID proof signature already if a unique
		// ID was present.
		if ((cert.subject.uniqueId) && (cert.subject.uniqueIdSize > 0)) {
			SHA384Hash uniqueIdHash;
			SHA384(uniqueIdHash.data, cert.subject.uniqueId, cert.subject.uniqueIdSize);
			std::pair< SharedPtr< const Certificate >, unsigned int > &bySubjectUniqueId = m_certsBySubjectUniqueId[uniqueIdHash];
			if (bySubjectUniqueId.first) {
				if (bySubjectUniqueId.first->subject.timestamp >= cert.subject.timestamp)
					return ZT_CERTIFICATE_ERROR_HAVE_NEWER_CERT;
				m_eraseCertificate(tPtr, bySubjectUniqueId.first, &uniqueIdHash);
				m_certsBySubjectUniqueId[uniqueIdHash] = certEntry;
			} else {
				bySubjectUniqueId = certEntry;
			}
		}

		// Save certificate by serial number.
		m_certs[serial] = certEntry;

		// Add certificate to sets of certificates whose subject references a given identity.
		for (unsigned int i = 0; i < cert.subject.identityCount; ++i) {
			const Identity *const ii = reinterpret_cast<const Identity *>(cert.subject.identities[i].identity);
			if (ii)
				m_certsBySubjectIdentity[ii->fingerprint()].insert(certEntry);
		}

		// Clean any certificates whose chains are now broken, which can happen if there was
		// an update that replaced an old cert with a given unique ID. Otherwise this generally
		// does nothing here. Skip if verify is false since this means we're mindlessly loading
		// certificates, which right now only happens on startup when they're loaded from the
		// local certificate cache.
		if (verify)
			m_cleanCertificates(tPtr, now);

		// Refresh the root peers lists, since certs may enumerate roots.
		if (refreshRootSets) {
			RWMutex::Lock l3(m_peers_l);
			RWMutex::Lock l2(m_roots_l);
			m_updateRootPeers(tPtr, now);
		}
	}

	if (writeToLocalStore) {
		// Write certificate data prefixed by local trust flags as a 32-bit integer.
		Vector< uint8_t > certData(cert.encode());
		uint64_t id[6];
		Utils::copy< 48 >(id, cert.serialNo);
		RR->node->stateObjectPut(tPtr, ZT_STATE_OBJECT_CERT, id, certData.data(), (unsigned int)certData.size());
	}

	return ZT_CERTIFICATE_ERROR_NONE;
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

	RR->node->stateObjectDelete(tPtr, ZT_STATE_OBJECT_CERT, serialNo.data);

	if (uniqueIdHash)
		m_certsBySubjectUniqueId.erase(*uniqueIdHash);

	for (unsigned int i = 0; i < cert->subject.identityCount; ++i) {
		const Identity *const ii = reinterpret_cast<const Identity *>(cert->subject.identities[i].identity);
		Map< Fingerprint, Map< SharedPtr< const Certificate >, unsigned int > >::iterator
			bySubjectIdentity(m_certsBySubjectIdentity.find(ii->fingerprint()));
		if (bySubjectIdentity != m_certsBySubjectIdentity.end()) {
			bySubjectIdentity->second.erase(cert);
			if (bySubjectIdentity->second.empty())
				m_certsBySubjectIdentity.erase(bySubjectIdentity);
		}
	}
}

bool Topology::m_cleanCertificates(void *tPtr, int64_t now)
{
	// assumes m_certs is locked for writing

	bool deleted = false;
	Vector< SharedPtr< const Certificate >> toDelete;
	for (;;) {
		for (Map< SHA384Hash, std::pair< SharedPtr< const Certificate >, unsigned int > >::iterator c(m_certs.begin()); c != m_certs.end(); ++c) {
			// Verify, but the last boolean option tells it to skip signature checks as this would
			// already have been done. This will therefore just check the path and validity times
			// of the certificate.
			const ZT_CertificateError err = m_verifyCertificate(*(c->second.first), now, c->second.second, true);
			if (err != ZT_CERTIFICATE_ERROR_NONE)
				toDelete.push_back(c->second.first);
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
		Vector< uint8_t > data(RR->node->stateObjectGet(tPtr, ZT_STATE_OBJECT_PEER, id));
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

} // namespace ZeroTier
