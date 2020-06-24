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

namespace ZeroTier {

Topology::Topology(const RuntimeEnvironment *renv, void *tPtr) :
	RR(renv)
{
	uint64_t idtmp[2];
	idtmp[0] = 0;
	idtmp[1] = 0;
	Vector< uint8_t > data(RR->node->stateObjectGet(tPtr, ZT_STATE_OBJECT_ROOTS, idtmp));
	// TODO
	m_updateRootPeers_l_roots_certs(tPtr);
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

SharedPtr< Peer > Topology::addRoot(void *const tPtr, const Identity &id)
{
	if ((id != RR->identity) && id.locallyValidate()) {
		RWMutex::Lock l1(m_roots_l);

		// A null pointer in the set of certificates specifying a root indicates that
		// the root has been directly added.
		m_roots[id.fingerprint()].insert(SharedPtr< const Certificate >());

		{
			Mutex::Lock certsLock(m_certs_l);
			m_updateRootPeers_l_roots_certs(tPtr);
		}
		m_writeRootList_l_roots(tPtr);

		for (Vector< SharedPtr< Peer > >::const_iterator p(m_rootPeers.begin()); p != m_rootPeers.end(); ++p) {
			if ((*p)->identity() == id)
				return *p;
		}
	}
	return SharedPtr< Peer >();
}

bool Topology::removeRoot(void *const tPtr, Address address)
{
	RWMutex::Lock l1(m_roots_l);
	// TODO
	return true;
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
	}
};

void Topology::rankRoots()
{
	RWMutex::Lock l1(m_roots_l);
	std::sort(m_rootPeers.begin(), m_rootPeers.end(), p_RootRankingComparisonOperator());
}

void Topology::doPeriodicTasks(void *tPtr, const int64_t now)
{
	// Peer and path delete operations are batched to avoid holding write locks on
	// these structures for any length of time. A list is compiled in read mode,
	// then the write lock is acquired for each delete. This adds overhead if there
	// are a lot of deletions, but that's not common.

	// Delete peers that are stale or offline.
	{
		Vector< Address > toDelete;
		{
			RWMutex::RLock l1(m_peers_l);
			for (Map< Address, SharedPtr< Peer > >::iterator i(m_peers.begin()); i != m_peers.end(); ++i) {
				// TODO: also delete if the peer has not exchanged meaningful communication in a while, such as
				// a network frame or non-trivial control packet.
				if (((now - i->second->lastReceive()) > ZT_PEER_ALIVE_TIMEOUT) && (m_roots.find(i->second->identity().fingerprint()) == m_roots.end()))
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
		Vector< uint64_t > toDelete;
		{
			RWMutex::RLock l1(m_paths_l);
			for (Map< uint64_t, SharedPtr< Path > >::iterator i(m_paths.begin()); i != m_paths.end(); ++i) {
				if (i->second.weakGC())
					toDelete.push_back(i->first);
			}
		}
		for (Vector< uint64_t >::iterator i(toDelete.begin()); i != toDelete.end(); ++i) {
			RWMutex::Lock l1(m_paths_l);
			const Map< uint64_t, SharedPtr< Path > >::iterator p(m_paths.find(*i));
			if (likely(p != m_paths.end()))
				m_paths.erase(p);
		}
	}

	// Clean any expired certificates
	{
		Mutex::Lock l1(m_certs_l);
		m_cleanCertificates_l_certs(now);
	}
}

void Topology::saveAll(void *tPtr)
{
	RWMutex::RLock l(m_peers_l);
	for (Map< Address, SharedPtr< Peer > >::iterator i(m_peers.begin()); i != m_peers.end(); ++i)
		i->second->save(tPtr);
}

ZT_CertificateError Topology::addCertificate(void *tPtr, const Certificate &cert, const int64_t now, const unsigned int localTrust)
{
	Mutex::Lock certsLock(m_certs_l);

	// Check to see if we already have this specific certificate.
	const SHA384Hash serial(cert.serialNo);
	if (m_certs.find(serial) != m_certs.end())
		return ZT_CERTIFICATE_ERROR_NONE;

	// Verify certificate all the way to a trusted root.
	const ZT_CertificateError err = m_verifyCertificate_l_certs(cert, now, localTrust, false);
	if (err != ZT_CERTIFICATE_ERROR_NONE)
		return err;

	// Create entry containing copy of certificate and trust flags.
	const std::pair< SharedPtr< const Certificate >, unsigned int > certEntry(SharedPtr< const Certificate >(new Certificate(cert)), localTrust);

	// If the subject contains a unique ID, check if we already have a cert for the
	// same uniquely identified subject. If so, check its subject timestamp and keep
	// the one we have if newer. Otherwise replace it. Note that the verification
	// function will have checked the unique ID proof signature already if a unique
	// ID was present.
	FCV< uint8_t, ZT_CERTIFICATE_MAX_UNIQUE_ID_SIZE > uniqueId(cert.subject.uniqueId, cert.subject.uniqueIdSize);
	if (!uniqueId.empty()) {
		std::pair< SharedPtr< const Certificate >, unsigned int > &bySubjectUniqueId = m_certsBySubjectUniqueId[uniqueId];
		if (bySubjectUniqueId.first) {
			if (bySubjectUniqueId.first->subject.timestamp >= cert.subject.timestamp)
				return ZT_CERTIFICATE_ERROR_HAVE_NEWER_CERT;
			m_eraseCertificate_l_certs(bySubjectUniqueId.first);
			m_certsBySubjectUniqueId[uniqueId] = certEntry;
		} else {
			bySubjectUniqueId = certEntry;
		}
	}

	// Save certificate by serial number.
	m_certs[serial] = certEntry;

	// Add certificate to sets of certificates whose subject references a given identity.
	for (unsigned int i = 0; i < cert.subject.identityCount; ++i) {
		const Identity *const ii = reinterpret_cast<const Identity *>(cert.subject.identities[i].identity);
		m_certsBySubjectIdentity[ii->fingerprint()].insert(certEntry);
	}

	// Clean any certificates whose chains are now broken, which can happen if there was
	// an update that replaced an old cert with a given unique ID. Otherwise this generally
	// does nothing here.
	m_cleanCertificates_l_certs(now);

	// Refresh the root peers lists, since certs may enumerate roots.
	{
		RWMutex::Lock rootsLock(m_roots_l);
		m_updateRootPeers_l_roots_certs(tPtr);
	}

	return ZT_CERTIFICATE_ERROR_NONE;
}

void Topology::m_eraseCertificate_l_certs(const SharedPtr< const Certificate > &cert)
{
	// assumes m_certs is locked for writing

	m_certsBySubjectUniqueId.erase(FCV< uint8_t, ZT_CERTIFICATE_MAX_UNIQUE_ID_SIZE >(cert->subject.uniqueId, cert->subject.uniqueIdSize));
	m_certs.erase(SHA384Hash(cert->serialNo));
	for (unsigned int i = 0; i < cert->subject.identityCount; ++i) {
		const Identity *const ii = reinterpret_cast<const Identity *>(cert->subject.identities[i].identity);
		Map< Fingerprint, Map< SharedPtr< const Certificate >, unsigned int > >::iterator bySubjectIdentity(m_certsBySubjectIdentity.find(ii->fingerprint()));
		if (bySubjectIdentity != m_certsBySubjectIdentity.end()) {
			bySubjectIdentity->second.erase(cert);
			if (bySubjectIdentity->second.empty())
				m_certsBySubjectIdentity.erase(bySubjectIdentity);
		}
	}
}

void Topology::m_cleanCertificates_l_certs(int64_t now)
{
	// assumes m_certs is locked for writing

	Vector< SharedPtr< const Certificate > > toDelete;
	for (;;) {
		for (Map< SHA384Hash, std::pair< SharedPtr< const Certificate >, unsigned int > >::iterator c(m_certs.begin()); c != m_certs.end(); ++c) {
			const ZT_CertificateError err = m_verifyCertificate_l_certs(*(c->second.first), now, c->second.second, true);
			if (err != ZT_CERTIFICATE_ERROR_NONE)
				toDelete.push_back(c->second.first);
		}

		if (toDelete.empty())
			break;

		for (Vector< SharedPtr< const Certificate > >::iterator c(toDelete.begin()); c != toDelete.end(); ++c)
			m_eraseCertificate_l_certs(*c);
		toDelete.clear();
	}
}

bool Topology::m_verifyCertificateChain_l_certs(const Certificate *current, const int64_t now) const
{
	// assumes m_certs is at least locked for reading

	Map< Fingerprint, Map< SharedPtr< const Certificate >, unsigned int > >::const_iterator c = m_certsBySubjectIdentity.find(reinterpret_cast<const Identity *>(current->issuer)->fingerprint());
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
				if (m_verifyCertificateChain_l_certs(cc->first.ptr(), now))
					return true;
			}
		}
	}

	return false;
}

ZT_CertificateError Topology::m_verifyCertificate_l_certs(const Certificate &cert, const int64_t now, unsigned int localTrust, bool skipSignatureCheck) const
{
	// assumes m_certs is at least locked for reading

	if ((cert.validity[0] > now) || (cert.validity[1] < now))
		return ZT_CERTIFICATE_ERROR_OUT_OF_VALID_TIME_WINDOW;

	if (!skipSignatureCheck) {
		const ZT_CertificateError ce = cert.verify();
		if (ce != ZT_CERTIFICATE_ERROR_NONE)
			return ce;
	}

	if ((localTrust & ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA) == 0) {
		if (!m_verifyCertificateChain_l_certs(&cert, now))
			return ZT_CERTIFICATE_ERROR_INVALID_CHAIN;
	}
}

void Topology::m_loadCached(void *tPtr, const Address &zta, SharedPtr< Peer > &peer)
{
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

void Topology::m_writeRootList_l_roots(void *tPtr)
{
	// assumes m_peers_l is locked for read or write
	// TODO
#if 0
	uint8_t *const roots = (uint8_t *)malloc((ZT_IDENTITY_MARSHAL_SIZE_MAX + ZT_LOCATOR_MARSHAL_SIZE_MAX + 2) * m_roots.size());
	if (roots) { // sanity check
		int p = 0;
		for (Set< Identity >::const_iterator r(m_roots.begin()); r != m_roots.end(); ++r) {
			const int pp = r->marshal(roots + p, false);
			if (pp > 0)
				p += pp;
		}
		uint64_t id[2];
		id[0] = 0;
		id[1] = 0;
		RR->node->stateObjectPut(tPtr, ZT_STATE_OBJECT_ROOTS, id, roots, (unsigned int)p);
		free(roots);
	}
#endif
}

void Topology::m_updateRootPeers_l_roots_certs(void *tPtr)
{
	// assumes m_peers_l and m_certs_l are locked for write
	// TODO
#if 0
	Vector< SharedPtr< Peer > > rp;
	for (Map< Identity, Set< SubscriptionKeyHash > >::iterator r(m_roots.begin()); r != m_roots.end(); ++r) {
		Map< Address, SharedPtr< Peer > >::iterator pp(m_peers.find(r->first.address()));
		SharedPtr< Peer > p;
		if (pp != m_peers.end())
			p = pp->second;

		if (!p)
			m_loadCached(tPtr, r->first.address(), p);

		if ((!p) || (p->identity() != r->first)) {
			p.set(new Peer(RR));
			p->init(r->first);
			m_peers[r->first.address()] = p;
		}

		rp.push_back(p);
	}
	std::sort(rp.begin(), rp.end(), p_RootSortComparisonOperator());
	m_rootPeers.swap(rp);
#endif
}

} // namespace ZeroTier
