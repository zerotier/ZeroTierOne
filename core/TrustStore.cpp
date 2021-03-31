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

#include "TrustStore.hpp"
#include "Topology.hpp"

namespace ZeroTier {

TrustStore::TrustStore()
{}

TrustStore::~TrustStore()
{}

SharedPtr< const TrustStore::Entry > TrustStore::get(const SHA384Hash &serial) const
{
	RWMutex::RLock l(m_lock);
	Map< SHA384Hash, SharedPtr< Entry > >::const_iterator i(m_bySerial.find(serial));
	return (i == m_bySerial.end()) ? SharedPtr< const TrustStore::Entry >() : i->second.constify();
}

Vector< SharedPtr< Peer > > TrustStore::roots(void *const tPtr, const RuntimeEnvironment *RR)
{
	RWMutex::RLock l(m_lock);

	Vector< SharedPtr< Peer > > r;
	r.reserve(m_bySerial.size());

	for (Map< SHA384Hash, SharedPtr< Entry > >::const_iterator c(m_bySerial.begin()); c != m_bySerial.end(); ++c) {
		if ((c->second->localTrust() & ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ZEROTIER_ROOT_SET) != 0) {
			for (unsigned int j = 0; j < c->second->certificate().subject.identityCount; ++j) {
				const Identity *const id = reinterpret_cast<const Identity *>(c->second->certificate().subject.identities[j].identity);
				if ((id != nullptr) && (*id)) { // sanity check
					SharedPtr< Peer > peer(RR->topology->peer(tPtr, id->address(), true));
					if (!peer) {
						peer.set(new Peer(RR));
						peer->init(*id);
						peer = RR->topology->add(tPtr, peer);
					}

					const Locator *const loc = reinterpret_cast<const Locator *>(c->second->certificate().subject.identities[j].locator);
					if (loc)
						peer->setLocator(SharedPtr< const Locator >(new Locator(*loc)), true);

					r.push_back(peer);
				}
			}
		}
	}

	return r;
}

Vector< SharedPtr< const TrustStore::Entry > > TrustStore::all() const
{
	Vector< SharedPtr< const TrustStore::Entry > > r;
	RWMutex::RLock l(m_lock);
	r.reserve(m_bySerial.size());
	for (Map< SHA384Hash, SharedPtr< Entry > >::const_iterator i(m_bySerial.begin()); i != m_bySerial.end(); ++i)
		r.push_back(i->second.constify());
	return r;
}

void TrustStore::add(const Certificate &cert, const unsigned int localTrust)
{
	RWMutex::Lock l(m_lock);
	m_addQueue.push_front(SharedPtr(new Entry(cert, localTrust)));
}

// Recursive function to trace a certificate up the chain to a CA, returning true
// if the CA is reached and the path length is less than the maximum.
static bool p_validatePath(const Map< SHA384Hash, Vector< SharedPtr< TrustStore::Entry > > > &bySignedCert, const SharedPtr< TrustStore::Entry > &entry, unsigned int pathLength)
{
	if (((entry->localTrust() & ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA) != 0) && (pathLength <= entry->certificate().maxPathLength))
		return true;
	if (pathLength < ZT_CERTIFICATE_MAX_PATH_LENGTH) {
		const Map< SHA384Hash, Vector< SharedPtr< TrustStore::Entry > > >::const_iterator signers(bySignedCert.find(SHA384Hash(entry->certificate().serialNo)));
		if (signers != bySignedCert.end()) {
			for (Vector< SharedPtr< TrustStore::Entry > >::const_iterator signer(signers->second.begin()); signer != signers->second.end(); ++signer) {
				if ((*signer != entry) && (p_validatePath(bySignedCert, *signer, pathLength + 1)))
					return true;
			}
		}
	}
	return false;
}

void TrustStore::update(const int64_t clock, Vector< std::pair< SharedPtr< Entry >, ZT_CertificateError > > *const purge)
{
	RWMutex::Lock l(m_lock);

	// Re-verify existing and rejected certificates, excluding signatures which
	// will have already been checked (and checking these is CPU-intensive). This
	// catches certificate expiry and un-expiry if the system's clock has been
	// changed. When a formerly rejected cert is revived it ends up getting
	// checked twice, but optimizing this out would be about as costly as just
	// doing this as verify() without signature check is cheap.
	for (Map< SharedPtr< Entry >, ZT_CertificateError >::iterator c(m_rejected.begin()); c != m_rejected.end();) {
		const ZT_CertificateError err = c->first->m_certificate.verify(clock, false);
		if (err == ZT_CERTIFICATE_ERROR_NONE) {
			m_bySerial[SHA384Hash(c->first->m_certificate.serialNo)] = c->first;
			m_rejected.erase(c++);
		} else {
			++c;
		}
	}
	for (Map< SHA384Hash, SharedPtr< Entry > >::const_iterator c(m_bySerial.begin()); c != m_bySerial.end();) {
		const ZT_CertificateError err = c->second->m_certificate.verify(clock, false);
		if (err == ZT_CERTIFICATE_ERROR_NONE) {
			++c;
		} else {
			m_rejected[c->second] = err;
			m_bySerial.erase(c++);
		}
	}

	// Add new certificates to m_bySerial, which is the master certificate set. They still
	// have yet to have their full certificate chains validated. Full signature checking is
	// performed here.
	while (!m_addQueue.empty()) {
		const ZT_CertificateError err = m_addQueue.front()->m_certificate.verify(clock, true);
		if (err == ZT_CERTIFICATE_ERROR_NONE) {
			m_bySerial[SHA384Hash(m_addQueue.front()->m_certificate.serialNo)].move(m_addQueue.front());
		} else {
			m_rejected[m_addQueue.front()] = err;
		}
		m_addQueue.pop_front();
	}

	// Verify certificate paths and replace old certificates with newer certificates
	// when subject unique ID mapping dictates, repeating the process until a stable
	// state is achieved. A loop is needed because deleting old certs when new
	// certs (with the same subject unique ID) replace them could in theory alter
	// certificate validation path checking outcomes, though in practice it should
	// not since mixing certificate roles this way would be strange.
	for (;;) {
		// Create a reverse lookup mapping from signed certs to signer certs for
		// certificate path validation.
		Map< SHA384Hash, Vector< SharedPtr< Entry > > > bySignedCert;
		for (Map< SHA384Hash, SharedPtr< Entry > >::const_iterator c(m_bySerial.begin()); c != m_bySerial.end(); ++c) {
			for (unsigned int j = 0; j < c->second->m_certificate.subject.certificateCount; ++j)
				bySignedCert[SHA384Hash(c->second->m_certificate.subject.certificates[j])].push_back(c->second);
		}

		// Validate certificate paths and reject any certificates that do not trace
		// back to a CA.
		for (Map< SHA384Hash, SharedPtr< Entry > >::const_iterator c(m_bySerial.begin()); c != m_bySerial.end();) {
			if (p_validatePath(bySignedCert, c->second, 0)) {
				++c;
			} else {
				m_rejected[c->second] = ZT_CERTIFICATE_ERROR_INVALID_CHAIN;
				m_bySerial.erase(c++);
			}
		}

		// Populate mapping of subject unique IDs to certificates and reject any
		// certificates that have been superseded by newly issued certificates with
		// the same subject.
		bool exitLoop = true;
		m_bySubjectUniqueId.clear();
		for (Map< SHA384Hash, SharedPtr< Entry > >::const_iterator c(m_bySerial.begin()); c != m_bySerial.end();) {
			const unsigned int uniqueIdSize = c->second->m_certificate.subject.uniqueIdSize;
			if ((uniqueIdSize > 0) && (uniqueIdSize <= 1024)) { // 1024 is a sanity check value, actual unique IDs are <100 bytes
				SharedPtr< Entry > &current = m_bySubjectUniqueId[Vector< uint8_t >(c->second->m_certificate.subject.uniqueId, c->second->m_certificate.subject.uniqueId + uniqueIdSize)];
				if (current) {
					if (c->second->m_certificate.subject.timestamp > current->m_certificate.subject.timestamp) {
						exitLoop = false;
						m_rejected[current] = ZT_CERTIFICATE_ERROR_HAVE_NEWER_CERT;
						m_bySerial.erase(SHA384Hash(current->m_certificate.serialNo));
						current = c->second;
					}
				} else {
					current = c->second;
				}
			}
		}

		if (exitLoop)
			break;
	}

	// Populate a mapping of identities to certificates whose subjects reference them.
	m_bySubjectIdentity.clear();
	for (Map< SHA384Hash, SharedPtr< Entry > >::const_iterator c(m_bySerial.begin()); c != m_bySerial.end();) {
		for (unsigned int i = 0; i < c->second->m_certificate.subject.identityCount; ++i)
			m_bySubjectIdentity[reinterpret_cast<const Identity *>(c->second->m_certificate.subject.identities[i].identity)->fingerprint()].push_back(c->second);
	}

	// Purge and return purged certificates if this option is selected.
	if (purge) {
		purge->reserve(m_rejected.size());
		for (Map< SharedPtr< Entry >, ZT_CertificateError >::const_iterator c(m_rejected.begin()); c != m_rejected.end(); ++c)
			purge->push_back(std::pair< SharedPtr< Entry >, ZT_CertificateError >(c->first, c->second));
		m_rejected.clear();
	}
}

Vector< std::pair< SharedPtr<TrustStore::Entry>, ZT_CertificateError > > TrustStore::rejects() const
{
	Vector< std::pair< SharedPtr<Entry>, ZT_CertificateError > > r;
	RWMutex::RLock l(m_lock);
	r.reserve(m_rejected.size());
	for (Map< SharedPtr< Entry >, ZT_CertificateError >::const_iterator c(m_rejected.begin()); c != m_rejected.end(); ++c)
		r.push_back(std::pair< SharedPtr< Entry >, ZT_CertificateError >(c->first, c->second));
	return r;
}

} // namespace ZeroTier
