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
#include "LZ4.hpp"

namespace ZeroTier {

TrustStore::TrustStore()
{}

TrustStore::~TrustStore()
{}

SharedPtr< TrustStore::Entry > TrustStore::get(const H384 &serial) const
{
	RWMutex::RLock l(m_lock);
	Map< H384, SharedPtr< Entry > >::const_iterator i(m_bySerial.find(serial));
	return (i != m_bySerial.end()) ? i->second : SharedPtr< TrustStore::Entry >();
}

Map< Identity, SharedPtr< const Locator > > TrustStore::roots()
{
	RWMutex::RLock l(m_lock);
	Map< Identity, SharedPtr< const Locator > > r;
	for (Map< Fingerprint, Vector< SharedPtr< Entry > > >::const_iterator cv(m_bySubjectIdentity.begin()); cv != m_bySubjectIdentity.end(); ++cv) {
		for (Vector< SharedPtr< Entry > >::const_iterator c(cv->second.begin()); c != cv->second.end(); ++c) {
			if (((*c)->error() == ZT_CERTIFICATE_ERROR_NONE) && (((*c)->localTrust() & ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ZEROTIER_ROOT_SET) != 0)) {
				for (unsigned int j = 0; j < (*c)->certificate().subject.identityCount; ++j) {
					const Identity *const id = reinterpret_cast<const Identity *>((*c)->certificate().subject.identities[j].identity);
					if (likely((id != nullptr) && (*id))) { // sanity check
						SharedPtr< const Locator > &existingLoc = r[*id];
						const Locator *const loc = reinterpret_cast<const Locator *>((*c)->certificate().subject.identities[j].locator);
						if ((loc != nullptr) && ((!existingLoc) || (existingLoc->revision() < loc->revision())))
							existingLoc.set(new Locator(*loc));
					}
				}
			}
		}
	}
	return r;
}

Vector< SharedPtr< TrustStore::Entry > > TrustStore::all(const bool includeRejectedCertificates) const
{
	RWMutex::RLock l(m_lock);
	Vector< SharedPtr< Entry > > r;
	r.reserve(m_bySerial.size());
	for (Map< H384, SharedPtr< Entry > >::const_iterator i(m_bySerial.begin()); i != m_bySerial.end(); ++i) {
		if ((includeRejectedCertificates) || (i->second->error() == ZT_CERTIFICATE_ERROR_NONE))
			r.push_back(i->second);
	}
	return r;
}

void TrustStore::add(const Certificate &cert, const unsigned int localTrust)
{
	RWMutex::Lock l(m_lock);
	m_addQueue.push_front(SharedPtr< Entry >(new Entry(cert, localTrust)));
}

void TrustStore::erase(const H384 &serial)
{
	RWMutex::Lock l(m_lock);
	m_deleteQueue.push_front(serial);
}

// Recursive function to trace a certificate up the chain to a CA, returning true
// if the CA is reached and the path length is less than the maximum. Note that only
// non-rejected (no errors) certificates will be in bySignedCert.
static bool p_validatePath(const Map< H384, Vector< SharedPtr< TrustStore::Entry > > > &bySignedCert, const SharedPtr< TrustStore::Entry > &entry, unsigned int pathLength)
{
	if (((entry->localTrust() & ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA) != 0) && (pathLength <= entry->certificate().maxPathLength))
		return true;
	if (pathLength < ZT_CERTIFICATE_MAX_PATH_LENGTH) {
		const Map< H384, Vector< SharedPtr< TrustStore::Entry > > >::const_iterator signers(bySignedCert.find(H384(entry->certificate().serialNo)));
		if (signers != bySignedCert.end()) {
			for (Vector< SharedPtr< TrustStore::Entry > >::const_iterator signer(signers->second.begin()); signer != signers->second.end(); ++signer) {
				if ((*signer != entry) && (p_validatePath(bySignedCert, *signer, pathLength + 1)))
					return true;
			}
		}
	}
	return false;
}

bool TrustStore::update(const int64_t clock, Vector< SharedPtr< Entry > > *const purge)
{
	RWMutex::Lock l(m_lock);

	// (Re)compute error codes for existing certs, but we don't have to do a full
	// signature check here since that's done when they're taken out of the add queue.
	bool errorStateModified = false;
	for (Map< H384, SharedPtr< Entry > >::const_iterator c(m_bySerial.begin()); c != m_bySerial.end(); ++c) {
		const ZT_CertificateError err = c->second->error();
		if ((err != ZT_CERTIFICATE_ERROR_HAVE_NEWER_CERT) && (err != ZT_CERTIFICATE_ERROR_INVALID_CHAIN)) {
			const ZT_CertificateError newErr = c->second->m_certificate.verify(clock, false);
			if (newErr != err) {
				c->second->m_error.store((int)newErr, std::memory_order_relaxed);
				errorStateModified = true;
			}
		}
	}

	// If no certificate error statuses changed and there are no new certificates to
	// add, there is nothing to do and we don't need to do more expensive path validation
	// and structure rebuilding.
	if ((!errorStateModified) && (m_addQueue.empty()) && (m_deleteQueue.empty()))
		return false;

	// Add new certificates to m_bySerial, which is the master certificate set. They still
	// have yet to have their full certificate chains validated. Full signature checking is
	// performed here.
	while (!m_addQueue.empty()) {
		m_addQueue.front()->m_error.store((int)m_addQueue.front()->m_certificate.verify(clock, true), std::memory_order_relaxed);
		m_bySerial[H384(m_addQueue.front()->m_certificate.serialNo)].move(m_addQueue.front());
		m_addQueue.pop_front();
	}

	// Delete any certificates enqueued to be deleted.
	while (!m_deleteQueue.empty()) {
		m_bySerial.erase(m_deleteQueue.front());
		m_deleteQueue.pop_front();
	}

	Map< H384, Vector< SharedPtr< Entry > > > bySignedCert;
	for (;;) {
		// Create a reverse lookup mapping from signed certs to signer certs for certificate
		// path validation. Only include good certificates.
		for (Map< H384, SharedPtr< Entry > >::const_iterator c(m_bySerial.begin()); c != m_bySerial.end(); ++c) {
			if (c->second->error() == ZT_CERTIFICATE_ERROR_NONE) {
				for (unsigned int j = 0; j < c->second->m_certificate.subject.certificateCount; ++j)
					bySignedCert[H384(c->second->m_certificate.subject.certificates[j])].push_back(c->second);
			}
		}

		// Validate certificate paths and reject any certificates that do not trace back to a CA.
		for (Map< H384, SharedPtr< Entry > >::const_iterator c(m_bySerial.begin()); c != m_bySerial.end(); ++c) {
			if (c->second->error() == ZT_CERTIFICATE_ERROR_NONE) {
				if (!p_validatePath(bySignedCert, c->second, 0))
					c->second->m_error.store((int)ZT_CERTIFICATE_ERROR_INVALID_CHAIN, std::memory_order_relaxed);
			}
		}

		// Populate mapping of subject unique IDs to certificates and reject any certificates
		// that have been superseded by newly issued certificates with the same subject.
		bool exitLoop = true;
		m_bySubjectUniqueId.clear();
		for (Map< H384, SharedPtr< Entry > >::const_iterator c(m_bySerial.begin()); c != m_bySerial.end();) {
			if (c->second->error() == ZT_CERTIFICATE_ERROR_NONE) {
				const unsigned int uniqueIdSize = c->second->m_certificate.subject.uniqueIdSize;
				if ((uniqueIdSize > 0) && (uniqueIdSize <= ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_SIZE)) {
					SharedPtr< Entry > &current = m_bySubjectUniqueId[Blob< ZT_CERTIFICATE_UNIQUE_ID_TYPE_NIST_P_384_SIZE >(c->second->m_certificate.subject.uniqueId, uniqueIdSize)];
					if (current) {
						exitLoop = false;
						if (c->second->m_certificate.subject.timestamp > current->m_certificate.subject.timestamp) {
							current->m_error.store((int)ZT_CERTIFICATE_ERROR_HAVE_NEWER_CERT, std::memory_order_relaxed);
							current = c->second;
						} else if (c->second->m_certificate.subject.timestamp < current->m_certificate.subject.timestamp) {
							c->second->m_error.store((int)ZT_CERTIFICATE_ERROR_HAVE_NEWER_CERT, std::memory_order_relaxed);
						} else {
							// Equal timestamps should never happen, but handle it by comparing serials for deterministic completeness.
							if (memcmp(c->second->m_certificate.serialNo, current->m_certificate.serialNo, ZT_SHA384_DIGEST_SIZE) > 0) {
								current->m_error.store((int)ZT_CERTIFICATE_ERROR_HAVE_NEWER_CERT, std::memory_order_relaxed);
								current = c->second;
							} else {
								c->second->m_error.store((int)ZT_CERTIFICATE_ERROR_HAVE_NEWER_CERT, std::memory_order_relaxed);
							}
						}
					} else {
						current = c->second;
					}
				}
			}
		}

		// If no certificates were tagged out during the unique ID pass, we can exit. Otherwise
		// the last few steps have to be repeated because removing any certificate could in
		// theory affect the result of certificate path validation.
		if (exitLoop) {
			break;
		} else {
			bySignedCert.clear();
		}
	}

	// Populate mapping of identities to certificates whose subjects reference them.
	m_bySubjectIdentity.clear();
	for (Map< H384, SharedPtr< Entry > >::const_iterator c(m_bySerial.begin()); c != m_bySerial.end(); ++c) {
		if (c->second->error() == ZT_CERTIFICATE_ERROR_NONE) {
			for (unsigned int i = 0; i < c->second->m_certificate.subject.identityCount; ++i) {
				const Identity *const id = reinterpret_cast<const Identity *>(c->second->m_certificate.subject.identities[i].identity);
				if ((id) && (*id)) // sanity check
					m_bySubjectIdentity[id->fingerprint()].push_back(c->second);
			}
		}
	}

	// Purge and return purged certificates if this option is selected.
	if (purge) {
		for (Map< H384, SharedPtr< Entry > >::const_iterator c(m_bySerial.begin()); c != m_bySerial.end();) {
			if (c->second->error() != ZT_CERTIFICATE_ERROR_NONE) {
				purge->push_back(c->second);
				m_bySerial.erase(c++);
			} else {
				++c;
			}
		}
	}

	return true;
}

Vector< uint8_t > TrustStore::save() const
{
	Vector< uint8_t> comp;

	int compSize;
	{
		Vector< uint8_t > b;
		b.reserve(65536);

		RWMutex::RLock l(m_lock);

		b.push_back(0); // version byte

		for (Map< H384, SharedPtr< Entry > >::const_iterator c(m_bySerial.begin()); c != m_bySerial.end(); ++c) {
			const Vector< uint8_t > cdata(c->second->certificate().encode());
			if (!cdata.empty()) {
				const uint32_t localTrust = (uint32_t)c->second->localTrust();
				b.push_back((uint8_t)(localTrust >> 24U));
				b.push_back((uint8_t)(localTrust >> 16U));
				b.push_back((uint8_t)(localTrust >> 8U));
				b.push_back((uint8_t)localTrust);
				const uint32_t size = (uint32_t)cdata.size();
				b.push_back((uint8_t)(size >> 24U));
				b.push_back((uint8_t)(size >> 16U));
				b.push_back((uint8_t)(size >> 8U));
				b.push_back((uint8_t)size);
				b.insert(b.end(), cdata.begin(), cdata.end());
			}
		}

		comp.resize((unsigned long)LZ4_COMPRESSBOUND(b.size()) + 8);
		compSize = LZ4_compress_fast(reinterpret_cast<const char *>(b.data()), reinterpret_cast<char *>(comp.data() + 8), (int)b.size(), (int)(comp.size() - 8));
		if (unlikely(compSize <= 0)) // shouldn't be possible
			return Vector< uint8_t >();

		const uint32_t uncompSize = (uint32_t)b.size();
		const uint32_t cksum = Utils::fnv1a32(b.data(), (unsigned int)uncompSize);
		comp[0] = (uint8_t)(uncompSize >> 24);
		comp[1] = (uint8_t)(uncompSize >> 16);
		comp[2] = (uint8_t)(uncompSize >> 8);
		comp[3] = (uint8_t)uncompSize;
		comp[4] = (uint8_t)(cksum >> 24);
		comp[5] = (uint8_t)(cksum >> 16);
		comp[6] = (uint8_t)(cksum >> 8);
		comp[7] = (uint8_t)cksum;
		compSize += 8;
	}

	comp.resize((unsigned long)compSize);
	comp.shrink_to_fit();

	return comp;
}

int TrustStore::load(const Vector< uint8_t > &data)
{
	if (data.size() < 8)
		return -1;

	const unsigned long uncompSize = Utils::loadBigEndian<uint32_t>(data.data());
	if (uncompSize > (data.size() * 256)) // sanity check
		return -1;
	if (uncompSize < 1) // no room for at least version and count
		return -1;

	Vector< uint8_t > uncomp;
	uncomp.resize(uncompSize);

	if (LZ4_decompress_safe(reinterpret_cast<const char *>(data.data() + 8), reinterpret_cast<char *>(uncomp.data()), (int)(data.size() - 8), (int)uncompSize) != (int)uncompSize)
		return -1;
	const uint8_t *b = uncomp.data();
	if (Utils::fnv1a32(b, (unsigned int)uncompSize) != Utils::loadBigEndian<uint32_t>(data.data() + 4))
		return -1;
	const uint8_t *const eof = b + uncompSize;

	if (*(b++) != 0) // unrecognized version
		return -1;

	int readCount = 0;
	for(;;) {
		if ((b + 8) > eof)
			break;
		const uint32_t localTrust = Utils::loadBigEndian<uint32_t>(b);
		b += 4;
		const uint32_t cdataSize = Utils::loadBigEndian<uint32_t>(b);
		b += 4;

		if ((b + cdataSize) > eof)
			break;
		Certificate c;
		if (c.decode(b, (unsigned int)cdataSize)) {
			this->add(c, (unsigned int)localTrust);
			++readCount;
		}
		b += cdataSize;
	}
	return readCount;
}

} // namespace ZeroTier
