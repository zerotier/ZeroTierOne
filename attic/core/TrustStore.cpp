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

TrustStore::TrustStore() {}

TrustStore::~TrustStore() {}

SharedPtr<TrustStore::Entry> TrustStore::get(const H384 &serial) const
{
    RWMutex::RLock l(m_lock);
    Map<H384, SharedPtr<Entry>>::const_iterator c(m_bySerial.find(serial));
    return (c != m_bySerial.end()) ? c->second : SharedPtr<TrustStore::Entry>();
}

Map<Identity, SharedPtr<const Locator>> TrustStore::roots()
{
    RWMutex::RLock l(m_lock);
    Map<Identity, SharedPtr<const Locator>> r;

    // Iterate using m_bySubjectIdentity to only scan certificates with subject identities.
    // This map also does not contian error or deprecated certificates.
    for (Map<Fingerprint, Vector<SharedPtr<Entry>>>::const_iterator cv(m_bySubjectIdentity.begin()); cv != m_bySubjectIdentity.end(); ++cv) {
        for (Vector<SharedPtr<Entry>>::const_iterator c(cv->second.begin()); c != cv->second.end(); ++c) {
            // A root set cert must be marked for this use and authorized to influence this node's config.
            if ((((*c)->m_certificate.usageFlags & ZT_CERTIFICATE_USAGE_ZEROTIER_ROOT_SET) != 0) && (((*c)->m_localTrust & ZT_CERTIFICATE_LOCAL_TRUST_FLAG_CONFIG) != 0)) {
                // Add all identities to the root set, and for each entry in the set make sure we have the latest
                // locator if there's more than one cert with one.
                for (unsigned int j = 0; j < (*c)->certificate().subject.identityCount; ++j) {
                    auto id = Identity::from((*c)->certificate().subject.identities[j].identity);
                    if ((id) && (*id)) {   // sanity check
                        SharedPtr<const Locator> &existingLoc = r[*id];
                        auto loc                              = Locator::from((*c)->certificate().subject.identities[j].locator);
                        if (loc) {
                            if ((!existingLoc) || (existingLoc->revision() < loc->revision()))
                                existingLoc.set(new Locator(*loc));
                        }
                    }
                }
            }
        }
    }
    return r;
}

Vector<SharedPtr<TrustStore::Entry>> TrustStore::all(const bool includeRejectedCertificates) const
{
    RWMutex::RLock l(m_lock);
    Vector<SharedPtr<Entry>> r;
    r.reserve(m_bySerial.size());
    for (Map<H384, SharedPtr<Entry>>::const_iterator c(m_bySerial.begin()); c != m_bySerial.end(); ++c) {
        if ((includeRejectedCertificates) || (c->second->error() == ZT_CERTIFICATE_ERROR_NONE))
            r.push_back(c->second);
    }
    return r;
}

void TrustStore::add(const Certificate &cert, const unsigned int localTrust)
{
    RWMutex::Lock l(m_lock);
    m_addQueue.push_front(SharedPtr<Entry>(new Entry(this->m_lock, cert, localTrust)));
}

void TrustStore::erase(const H384 &serial)
{
    RWMutex::Lock l(m_lock);
    m_deleteQueue.push_front(serial);
}

bool TrustStore::update(const int64_t clock, Vector<SharedPtr<Entry>> *const purge)
{
    RWMutex::Lock l(m_lock);

    // Check for certificate time validity status changes. If any of these occur then
    // full re-validation is required.
    bool errorStateModified = false;
    for (Map<H384, SharedPtr<Entry>>::const_iterator c(m_bySerial.begin()); c != m_bySerial.end(); ++c) {
        const bool timeValid = c->second->m_certificate.verifyTimeWindow(clock);
        switch (c->second->m_error) {
            case ZT_CERTIFICATE_ERROR_NONE:
            case ZT_CERTIFICATE_ERROR_INVALID_CHAIN:
                if (!timeValid) {
                    c->second->m_error = ZT_CERTIFICATE_ERROR_OUT_OF_VALID_TIME_WINDOW;
                    errorStateModified = true;
                }
                break;
            case ZT_CERTIFICATE_ERROR_OUT_OF_VALID_TIME_WINDOW:
                if (timeValid) {
                    c->second->m_error = c->second->m_certificate.verify(-1, false);
                    errorStateModified = true;
                }
                break;
            default: break;
        }
    }

    // If there were not any such changes and if the add and delete queues are empty,
    // there is nothing more to be done.
    if ((!errorStateModified) && (m_addQueue.empty()) && (m_deleteQueue.empty()))
        return false;

    // Add new certificates to m_bySerial, which is the master certificate set. They still
    // have yet to have their full certificate chains validated. Full signature checking is
    // performed here.
    while (!m_addQueue.empty()) {
        SharedPtr<Entry> &qi = m_addQueue.front();
        qi->m_error          = qi->m_certificate.verify(clock, true);
        m_bySerial[H384(qi->m_certificate.serialNo)].move(qi);
        m_addQueue.pop_front();
    }

    // Delete any certificates enqueued to be deleted.
    while (!m_deleteQueue.empty()) {
        m_bySerial.erase(m_deleteQueue.front());
        m_deleteQueue.pop_front();
    }

    // Reset flags for deprecation and a cert being on a trust path, which are
    // recomputed when chain and subjects are checked below.
    for (Map<H384, SharedPtr<Entry>>::const_iterator c(m_bySerial.begin()); c != m_bySerial.end(); ++c) {
        if (c->second->m_error == ZT_CERTIFICATE_ERROR_NONE) {
            c->second->m_subjectDeprecated = false;
            c->second->m_onTrustPath       = false;
        }
    }

    // Validate certificate trust paths.
    {
        Vector<Entry *> visited;
        visited.reserve(8);
        for (Map<H384, SharedPtr<Entry>>::iterator c(m_bySerial.begin()); c != m_bySerial.end(); ++c) {
            if (c->second->m_error == ZT_CERTIFICATE_ERROR_NONE) {
                if (c->second->m_certificate.isSelfSigned()) {
                    // If this is a self-signed certificate it's only valid if it's trusted as a CA.
                    if ((c->second->m_localTrust & ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA) == 0) {
                        c->second->m_error = ZT_CERTIFICATE_ERROR_INVALID_CHAIN;
                    }
                }
                else {
                    if ((!c->second->m_onTrustPath) && ((c->second->m_localTrust & ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA) == 0)) {
                        // Trace the path of each certificate all the way back to a trusted CA.
                        unsigned int pathLength = 0;
                        Map<H384, SharedPtr<Entry>>::const_iterator current(c);
                        visited.clear();
                        for (;;) {
                            if (pathLength <= current->second->m_certificate.maxPathLength) {
                                // Check if this cert isn't a CA or already part of a valid trust path. If so then step
                                // upward toward CA.
                                if (((current->second->m_localTrust & ZT_CERTIFICATE_LOCAL_TRUST_FLAG_ROOT_CA) == 0) && (!current->second->m_onTrustPath)) {
                                    // If the issuer (parent) certificiate is (1) valid, (2) not already visited (to
                                    // prevent loops), and (3) has a public key that matches this cert's issuer public
                                    // key (sanity check), proceed up the certificate graph toward a potential CA.
                                    visited.push_back(current->second.ptr());
                                    const Map<H384, SharedPtr<Entry>>::const_iterator prevChild(current);
                                    current = m_bySerial.find(H384(current->second->m_certificate.issuer));
                                    if ((current != m_bySerial.end()) && (std::find(visited.begin(), visited.end(), current->second.ptr()) == visited.end()) && (current->second->m_error == ZT_CERTIFICATE_ERROR_NONE) && (current->second->m_certificate.publicKeySize == prevChild->second->m_certificate.issuerPublicKeySize)
                                        && (memcmp(current->second->m_certificate.publicKey, prevChild->second->m_certificate.issuerPublicKey, current->second->m_certificate.publicKeySize) == 0)) {
                                        ++pathLength;
                                        continue;
                                    }
                                }
                                else {
                                    // If we've traced this to a root CA, flag its parents as also being on a trust
                                    // path. Then break the loop without setting an error. We don't flag the current
                                    // cert as being on a trust path since no other certificates depend on it.
                                    for (Vector<Entry *>::const_iterator v(visited.begin()); v != visited.end(); ++v) {
                                        if (*v != c->second.ptr())
                                            (*v)->m_onTrustPath = true;
                                    }
                                    break;
                                }
                            }

                            // If we made it here without breaking or continuing, no path to a
                            // CA was found and the certificate's chain is invalid.
                            c->second->m_error = ZT_CERTIFICATE_ERROR_INVALID_CHAIN;
                            break;
                        }
                    }
                }
            }
        }
    }

    // Repopulate mapping of subject unique IDs to their certificates, marking older
    // certificates for the same subject as deprecated. A deprecated certificate is not invalid
    // but will be purged if it is also not part of a trust path. Error certificates are ignored.
    m_bySubjectUniqueId.clear();
    for (Map<H384, SharedPtr<Entry>>::const_iterator c(m_bySerial.begin()); c != m_bySerial.end();) {
        if (c->second->m_error == ZT_CERTIFICATE_ERROR_NONE) {
            const unsigned int uniqueIdSize = c->second->m_certificate.subject.uniqueIdSize;
            if ((uniqueIdSize > 0) && (uniqueIdSize <= ZT_CERTIFICATE_MAX_PUBLIC_KEY_SIZE)) {
                SharedPtr<Entry> &entry = m_bySubjectUniqueId[Blob<ZT_CERTIFICATE_MAX_PUBLIC_KEY_SIZE>(c->second->m_certificate.subject.uniqueId, uniqueIdSize)];
                if (entry) {
                    // If there's already an entry, see if there's a newer certificate for this subject.
                    if (c->second->m_certificate.subject.timestamp > entry->m_certificate.subject.timestamp) {
                        entry->m_subjectDeprecated = true;
                        entry                      = c->second;
                    }
                    else if (c->second->m_certificate.subject.timestamp < entry->m_certificate.subject.timestamp) {
                        c->second->m_subjectDeprecated = true;
                    }
                    else {
                        // Equal timestamps should never happen, but handle it anyway by comparing serials.
                        if (memcmp(c->second->m_certificate.serialNo, entry->m_certificate.serialNo, ZT_CERTIFICATE_HASH_SIZE) > 0) {
                            entry->m_subjectDeprecated = true;
                            entry                      = c->second;
                        }
                        else {
                            c->second->m_subjectDeprecated = true;
                        }
                    }
                }
                else {
                    entry = c->second;
                }
            }
        }
    }

    // Populate mapping of identities to certificates whose subjects reference them, ignoring
    // error or deprecated certificates.
    m_bySubjectIdentity.clear();
    for (Map<H384, SharedPtr<Entry>>::const_iterator c(m_bySerial.begin()); c != m_bySerial.end(); ++c) {
        if ((c->second->m_error == ZT_CERTIFICATE_ERROR_NONE) && (!c->second->m_subjectDeprecated)) {
            for (unsigned int i = 0; i < c->second->m_certificate.subject.identityCount; ++i) {
                auto id = Identity::from(c->second->m_certificate.subject.identities[i].identity);
                if ((id) && (*id))   // sanity check
                    m_bySubjectIdentity[id->fingerprint()].push_back(c->second);
            }
        }
    }

    // If purge is set, erase and return error and deprecated certs (that are not on a trust path).
    if (purge) {
        for (Map<H384, SharedPtr<Entry>>::const_iterator c(m_bySerial.begin()); c != m_bySerial.end();) {
            if ((c->second->error() != ZT_CERTIFICATE_ERROR_NONE) || ((c->second->m_subjectDeprecated) && (!c->second->m_onTrustPath))) {
                purge->push_back(c->second);
                m_bySerial.erase(c++);
            }
            else {
                ++c;
            }
        }
    }

    return true;
}

Vector<uint8_t> TrustStore::save() const
{
    Vector<uint8_t> comp;

    int compSize;
    {
        RWMutex::RLock l(m_lock);

        Vector<uint8_t> b;
        b.reserve(4096);

        // A version byte.
        b.push_back(0);

        // <size[2]> <certificate[...]> <trust[2]> tuples terminated by a 0 size.
        for (Map<H384, SharedPtr<Entry>>::const_iterator c(m_bySerial.begin()); c != m_bySerial.end(); ++c) {
            const Vector<uint8_t> cdata(c->second->certificate().encode());
            const unsigned long size = (uint32_t)cdata.size();
            if ((size > 0) && (size <= 0xffff)) {
                b.push_back((uint8_t)(size >> 8U));
                b.push_back((uint8_t)size);
                b.insert(b.end(), cdata.begin(), cdata.end());
                const uint32_t localTrust = (uint32_t)c->second->localTrust();
                b.push_back((uint8_t)(localTrust >> 8U));
                b.push_back((uint8_t)localTrust);
            }
        }
        b.push_back(0);
        b.push_back(0);

        comp.resize((unsigned long)LZ4_COMPRESSBOUND(b.size()) + 8);
        compSize = LZ4_compress_fast(reinterpret_cast<const char *>(b.data()), reinterpret_cast<char *>(comp.data() + 8), (int)b.size(), (int)(comp.size() - 8));
        if (unlikely(compSize <= 0))   // shouldn't be possible
            return Vector<uint8_t>();

        const uint32_t uncompSize = (uint32_t)b.size();
        Utils::storeBigEndian(comp.data(), uncompSize);
        Utils::storeBigEndian(comp.data() + 4, Utils::fnv1a32(b.data(), (unsigned int)uncompSize));
        compSize += 8;
    }

    comp.resize((unsigned long)compSize);
    comp.shrink_to_fit();

    return comp;
}

int TrustStore::load(const Vector<uint8_t> &data)
{
    if (data.size() < 8)
        return -1;

    const unsigned int uncompSize = Utils::loadBigEndian<uint32_t>(data.data());
    if ((uncompSize == 0) || (uncompSize > (unsigned int)(data.size() * 128)))
        return -1;

    Vector<uint8_t> uncomp;
    uncomp.resize(uncompSize);

    if (LZ4_decompress_safe(reinterpret_cast<const char *>(data.data() + 8), reinterpret_cast<char *>(uncomp.data()), (int)(data.size() - 8), (int)uncompSize) != (int)uncompSize)
        return -1;
    const uint8_t *b = uncomp.data();
    if (Utils::fnv1a32(b, (unsigned int)uncompSize) != Utils::loadBigEndian<uint32_t>(data.data() + 4))
        return -1;
    const uint8_t *const eof = b + uncompSize;

    if (*(b++) != 0)   // unrecognized version
        return -1;

    int readCount = 0;

    for (;;) {
        if ((b + 2) > eof)
            break;
        const uint32_t certDataSize = Utils::loadBigEndian<uint16_t>(b);
        b += 2;

        if (certDataSize == 0)
            break;

        if ((b + certDataSize + 2) > eof)   // certificate length + 2 bytes for trust flags
            break;
        Certificate c;
        if (c.decode(b, (unsigned int)certDataSize)) {
            b += certDataSize;
            this->add(c, Utils::loadBigEndian<uint16_t>(b));
            b += 2;

            ++readCount;
        }
    }

    return readCount;
}

}   // namespace ZeroTier
