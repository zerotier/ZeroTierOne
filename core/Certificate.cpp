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

#include "Certificate.hpp"
#include "SHA512.hpp"
#include "ECC384.hpp"
#include "ScopedPtr.hpp"

namespace ZeroTier {

Certificate::Certificate() noexcept
{
	ZT_Certificate *const sup = this;
	Utils::zero< sizeof(ZT_Certificate) >(sup);
}

Certificate::Certificate(const ZT_Certificate &apiCert) :
	Certificate()
{ *this = apiCert; }

Certificate::Certificate(const Certificate &cert) :
	Certificate()
{ *this = cert; }

Certificate::~Certificate()
{}

Certificate &Certificate::operator=(const ZT_Certificate &cert)
{
	m_clear();

	Utils::copy< sizeof(this->serialNo) >(this->serialNo, cert.serialNo);
	this->flags = cert.flags;
	this->timestamp = cert.timestamp;
	this->validity[0] = cert.validity[0];
	this->validity[1] = cert.validity[1];

	this->subject.timestamp = cert.subject.timestamp;

	if (cert.subject.identities != nullptr) {
		for (unsigned int i = 0; i < cert.subject.identityCount; ++i) {
			if (cert.subject.identities[i].identity) {
				if (cert.subject.identities[i].locator) {
					addSubjectIdentity(*reinterpret_cast<const Identity *>(cert.subject.identities[i].identity), *reinterpret_cast<const Locator *>(cert.subject.identities[i].locator));
				} else {
					addSubjectIdentity(*reinterpret_cast<const Identity *>(cert.subject.identities[i].identity));
				}
			}
		}
	}

	if (cert.subject.networks != nullptr) {
		for (unsigned int i = 0; i < cert.subject.networkCount; ++i) {
			if (cert.subject.networks[i].id) {
				addSubjectNetwork(cert.subject.networks[i].id, cert.subject.networks[i].controller);
			}
		}
	}

	if (cert.subject.updateURLs != nullptr) {
		for (unsigned int i = 0; i < cert.subject.updateURLCount; ++i) {
			if (cert.subject.updateURLs[i]) {
				addSubjectUpdateUrl(cert.subject.updateURLs[i]);
			}
		}
	}

	this->subject.identityCount = cert.subject.identityCount;
	this->subject.networkCount = cert.subject.networkCount;
	this->subject.updateURLCount = cert.subject.updateURLCount;

	Utils::copy< sizeof(ZT_Certificate_Name) >(&(this->subject.name), &(cert.subject.name));

	Utils::copy< sizeof(this->subject.uniqueId) >(this->subject.uniqueId, cert.subject.uniqueId);
	Utils::copy< sizeof(this->subject.uniqueIdSignature) >(this->subject.uniqueIdSignature, cert.subject.uniqueIdSignature);
	this->subject.uniqueIdSize = cert.subject.uniqueIdSize;
	this->subject.uniqueIdSignatureSize = cert.subject.uniqueIdSignatureSize;

	Utils::copy< sizeof(this->issuer) >(this->issuer, cert.issuer);

	Utils::copy< sizeof(this->issuerPublicKey) >(this->issuerPublicKey, cert.issuerPublicKey);
	Utils::copy< sizeof(this->publicKey) >(this->publicKey, cert.publicKey);
	this->issuerPublicKeySize = cert.issuerPublicKeySize;
	this->publicKeySize = cert.publicKeySize;

	if ((cert.extendedAttributes != nullptr) && (cert.extendedAttributesSize > 0)) {
		m_extendedAttributes.assign(cert.extendedAttributes, cert.extendedAttributes + cert.extendedAttributesSize);
		this->extendedAttributes = m_extendedAttributes.data();
		this->extendedAttributesSize = (unsigned int)m_extendedAttributes.size();
	}

	Utils::copy< sizeof(this->signature) >(this->signature, cert.signature);
	this->signatureSize = cert.signatureSize;

	this->maxPathLength = cert.maxPathLength;

	return *this;
}

ZT_Certificate_Identity *Certificate::addSubjectIdentity(const Identity &id)
{
	// Store a local copy of the actual identity.
	m_identities.push_front(id);
	m_identities.front().erasePrivateKey();

	// Enlarge array of ZT_Certificate_Identity structs and set pointer to potentially reallocated array.
	m_subjectIdentities.push_back(ZT_Certificate_Identity());
	m_subjectIdentities.back().identity = &(m_identities.front());
	m_subjectIdentities.back().locator = nullptr;

	this->subject.identities = m_subjectIdentities.data();
	this->subject.identityCount = (unsigned int)m_subjectIdentities.size();

	return &(m_subjectIdentities.back());
}

ZT_Certificate_Identity *Certificate::addSubjectIdentity(const Identity &id, const Locator &loc)
{
	// Add identity as above.
	ZT_Certificate_Identity *const n = addSubjectIdentity(id);

	// Store local copy of locator.
	m_locators.push_front(loc);

	// Set pointer to stored local copy of locator.
	n->locator = &(m_locators.front());

	return n;
}

ZT_Certificate_Network *Certificate::addSubjectNetwork(const uint64_t id, const ZT_Fingerprint &controller)
{
	// Enlarge array of ZT_Certificate_Network and set pointer to potentially reallocated array.
	m_subjectNetworks.resize(++this->subject.networkCount);
	this->subject.networks = m_subjectNetworks.data();

	// Set fields in new ZT_Certificate_Network structure.
	m_subjectNetworks.back().id = id;
	Utils::copy< sizeof(ZT_Fingerprint) >(&(m_subjectNetworks.back().controller), &controller);

	return &(m_subjectNetworks.back());
}

void Certificate::addSubjectUpdateUrl(const char *url)
{
	if ((url != nullptr) && (url[0] != 0)) {
		// Store local copy of URL.
		m_strings.push_front(url);

		// Add pointer to local copy to pointer array and update C structure to point to
		// potentially reallocated array.
		m_updateUrls.push_back(m_strings.front().c_str());
		this->subject.updateURLs = m_updateUrls.data();
		this->subject.updateURLCount = (unsigned int)m_updateUrls.size();
	}
}

Vector< uint8_t > Certificate::encode(const bool omitSignature) const
{
	Vector< uint8_t > enc;
	Dictionary d;

	/*
	 * A Dictionary is used to encode certificates as it's a common and extensible
	 * format. Custom packed formats are used for credentials as these are smaller
	 * and faster to marshal/unmarshal.
	 *
	 * We use the slower actually-insert-keys method of building a dictionary
	 * instead of the faster append method because for signing and verification
	 * purposes the keys must be always be in order.
	 */

	if (this->flags != 0)
		d.add("f", this->flags);
	if (this->timestamp > 0)
		d.add("t", (uint64_t)this->timestamp);
	if (this->validity[0] > 0)
		d.add("v#0", (uint64_t)this->validity[0]);
	if (this->validity[1] > 0)
		d.add("v#1", (uint64_t)this->validity[1]);

	m_encodeSubject(this->subject, d, false);

	if (!Utils::allZero(this->issuer, sizeof(this->issuer)))
		d.add("i", this->issuer, sizeof(this->issuer));

	if (this->issuerPublicKeySize > 0)
		d.add("iPK", this->issuerPublicKey, this->issuerPublicKeySize);

	if (this->publicKeySize > 0)
		d.add("pK", this->publicKey, this->publicKeySize);

	if ((this->extendedAttributes != nullptr) && (this->extendedAttributesSize > 0))
		d["x"].assign(this->extendedAttributes, this->extendedAttributes + this->extendedAttributesSize);

	if ((!omitSignature) && (this->signatureSize > 0))
		d["si"].assign(this->signature, this->signature + this->signatureSize);

	if (this->maxPathLength > 0)
		d.add("l", (uint64_t)this->maxPathLength);

	d.encode(enc);
	return enc;
}

bool Certificate::decode(const void *const data, const unsigned int len)
{
	char tmp[32], tmp2[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];

	Dictionary d;
	if (!d.decode(data, len))
		return false;

	m_clear();

	this->flags = d.getUI("f");
	this->timestamp = (int64_t)d.getUI("t");
	this->validity[0] = (int64_t)d.getUI("v#0");
	this->validity[1] = (int64_t)d.getUI("v#1");

	this->subject.timestamp = (int64_t)d.getUI("s.t");

	unsigned int cnt = (unsigned int)d.getUI("s.i$");
	for (unsigned int i = 0; i < cnt; ++i) {
		const Vector< uint8_t > &identityData = d[Dictionary::arraySubscript(tmp, sizeof(tmp), "s.i$.i", i)];
		const Vector< uint8_t > &locatorData = d[Dictionary::arraySubscript(tmp, sizeof(tmp), "s.i$.l", i)];
		if (identityData.empty())
			return false;
		Identity id;
		if (id.unmarshal(identityData.data(), (unsigned int)identityData.size()) <= 0)
			return false;
		if (locatorData.empty()) {
			this->addSubjectIdentity(id);
		} else {
			Locator loc;
			if (loc.unmarshal(locatorData.data(), (unsigned int)locatorData.size()) <= 0)
				return false;
			this->addSubjectIdentity(id, loc);
		}
	}

	cnt = (unsigned int)d.getUI("s.nw$");
	for (unsigned int i = 0; i < cnt; ++i) {
		const uint64_t nwid = d.getUI(Dictionary::arraySubscript(tmp, sizeof(tmp), "s.nw$.i", i));
		const Vector< uint8_t > &fingerprintData = d[Dictionary::arraySubscript(tmp, sizeof(tmp), "s.nw$.c", i)];
		if ((nwid == 0) || (fingerprintData.empty()))
			return false;
		Fingerprint fp;
		if (fp.unmarshal(fingerprintData.data(), (unsigned int)fingerprintData.size()) <= 0)
			return false;
		this->addSubjectNetwork(nwid, fp);
	}

	cnt = (unsigned int)d.getUI("s.u$");
	for (unsigned int i = 0; i < cnt; ++i)
		addSubjectUpdateUrl(d.getS(Dictionary::arraySubscript(tmp, sizeof(tmp), "s.u$", i), tmp2, sizeof(tmp2)));

	d.getS("s.n.sN", this->subject.name.serialNo, sizeof(this->subject.name.serialNo));
	d.getS("s.n.cN", this->subject.name.commonName, sizeof(this->subject.name.commonName));
	d.getS("s.n.c", this->subject.name.country, sizeof(this->subject.name.country));
	d.getS("s.n.o", this->subject.name.organization, sizeof(this->subject.name.organization));
	d.getS("s.n.u", this->subject.name.unit, sizeof(this->subject.name.unit));
	d.getS("s.n.l", this->subject.name.locality, sizeof(this->subject.name.locality));
	d.getS("s.n.p", this->subject.name.province, sizeof(this->subject.name.province));
	d.getS("s.n.sA", this->subject.name.streetAddress, sizeof(this->subject.name.streetAddress));
	d.getS("s.n.pC", this->subject.name.postalCode, sizeof(this->subject.name.postalCode));
	d.getS("s.n.e", this->subject.name.email, sizeof(this->subject.name.email));
	d.getS("s.n.ur", this->subject.name.url, sizeof(this->subject.name.url));
	d.getS("s.n.h", this->subject.name.host, sizeof(this->subject.name.host));

	const Vector< uint8_t > &uniqueId = d["s.uI"];
	if ((!uniqueId.empty()) && (uniqueId.size() <= sizeof(this->subject.uniqueId))) {
		Utils::copy(this->subject.uniqueId, uniqueId.data(), uniqueId.size());
		this->subject.uniqueIdSize = (unsigned int)uniqueId.size();
	}
	const Vector< uint8_t > &uniqueIdSignature = d["s.uS"];
	if ((!uniqueIdSignature.empty()) && (uniqueIdSignature.size() <= sizeof(this->subject.uniqueIdSignature))) {
		Utils::copy(this->subject.uniqueIdSignature, uniqueIdSignature.data(), uniqueIdSignature.size());
		this->subject.uniqueIdSignatureSize = (unsigned int)uniqueIdSignature.size();
	}

	const Vector< uint8_t > &issuerData = d["i"];
	if (issuerData.size() == sizeof(this->issuer)) {
		Utils::copy< sizeof(this->issuer) >(this->issuer, issuerData.data());
	}

	const Vector< uint8_t > &issuerPublicKey = d["iPK"];
	if ((!issuerPublicKey.empty()) && (issuerPublicKey.size() <= sizeof(this->issuerPublicKey))) {
		Utils::copy(this->issuerPublicKey, issuerPublicKey.data(), issuerPublicKey.size());
		this->issuerPublicKeySize = (unsigned int)issuerPublicKey.size();
	}

	const Vector< uint8_t > &publicKey = d["pK"];
	if ((!publicKey.empty()) && (publicKey.size() <= sizeof(this->publicKey))) {
		Utils::copy(this->publicKey, publicKey.data(), publicKey.size());
		this->publicKeySize = (unsigned int)publicKey.size();
	}

	m_extendedAttributes = d["x"];
	if (!m_extendedAttributes.empty()) {
		this->extendedAttributes = m_extendedAttributes.data();
		this->extendedAttributesSize = (unsigned int)m_extendedAttributes.size();
	}

	const Vector< uint8_t > &signature = d["si"];
	if ((!signature.empty()) && (signature.size() <= sizeof(this->signature))) {
		Utils::copy(this->signature, signature.data(), signature.size());
		this->signatureSize = (unsigned int)signature.size();
	}

	this->maxPathLength = (unsigned int)d.getUI("l");

	const Vector< uint8_t > enc(encode(true));
	SHA384(this->serialNo, enc.data(), (unsigned int)enc.size());

	return true;
}

bool Certificate::sign(const uint8_t issuer[ZT_CERTIFICATE_HASH_SIZE], const void *const issuerPrivateKey, const unsigned int issuerPrivateKeySize)
{
	if ((!issuerPrivateKey) || (issuerPrivateKeySize == 0))
		return false;

	switch (reinterpret_cast<const uint8_t *>(issuerPrivateKey)[0]) {
		default:
			return false;
		case ZT_CERTIFICATE_PUBLIC_KEY_ALGORITHM_ECDSA_NIST_P_384:
			if (issuerPrivateKeySize == (1 + ZT_ECC384_PUBLIC_KEY_SIZE + ZT_ECC384_PRIVATE_KEY_SIZE)) {
				Utils::copy< sizeof(this->issuer) >(this->issuer, issuer);
				Utils::copy< 1 + ZT_ECC384_PUBLIC_KEY_SIZE >(this->issuerPublicKey, issuerPrivateKey); // private is prefixed with public
				this->issuerPublicKeySize = 1 + ZT_ECC384_PUBLIC_KEY_SIZE;

				const Vector< uint8_t > enc(encode(true));
				SHA384(this->serialNo, enc.data(), (unsigned int)enc.size());

				ECC384ECDSASign(reinterpret_cast<const uint8_t *>(issuerPrivateKey) + 1 + ZT_ECC384_PUBLIC_KEY_SIZE, this->serialNo, this->signature);
				this->signatureSize = ZT_ECC384_SIGNATURE_SIZE;

				return true;
			}
			break;
	}

	return false;
}

ZT_CertificateError Certificate::verify(const int64_t clock, const bool checkSignatures) const
{
	try {
		if (this->validity[0] > this->validity[1]) {
			return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
		}

		if (this->subject.identityCount > 0) {
			if (this->subject.identities) {
				for (unsigned int i = 0; i < this->subject.identityCount; ++i) {
					if (!this->subject.identities[i].identity) {
						return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
					}
					if (checkSignatures) {
						if (!reinterpret_cast<const Identity *>(this->subject.identities[i].identity)->locallyValidate()) {
							return ZT_CERTIFICATE_ERROR_INVALID_IDENTITY;
						}
						if ((this->subject.identities[i].locator) && (!reinterpret_cast<const Locator *>(this->subject.identities[i].locator)->verify(*reinterpret_cast<const Identity *>(this->subject.identities[i].identity)))) {
							return ZT_CERTIFICATE_ERROR_INVALID_COMPONENT_SIGNATURE;
						}
					}
				}
			} else {
				return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
			}
		}

		if (this->subject.networkCount > 0) {
			if (this->subject.networks) {
				for (unsigned int i = 0; i < this->subject.networkCount; ++i) {
					if (!this->subject.networks[i].id) {
						return ZT_CERTIFICATE_ERROR_MISSING_REQUIRED_FIELDS;
					}
				}
			} else {
				return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
			}
		}

		if (this->subject.updateURLCount > 0) {
			if (this->subject.updateURLs) {
				for (unsigned int i = 0; i < this->subject.updateURLCount; ++i) {
					if (!this->subject.updateURLs[i])
						return ZT_CERTIFICATE_ERROR_MISSING_REQUIRED_FIELDS;
				}
			} else {
				return ZT_CERTIFICATE_ERROR_MISSING_REQUIRED_FIELDS;
			}
		}

		if ((this->subject.uniqueIdSize > sizeof(this->subject.uniqueId)) || (this->subject.uniqueIdSignatureSize > sizeof(this->subject.uniqueIdSignature))) {
			return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
		}

		if ((this->issuerPublicKeySize > sizeof(this->issuerPublicKey)) || (this->publicKeySize > sizeof(this->publicKey))) {
			return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
		}

		if ((this->extendedAttributesSize > 0) && (!this->extendedAttributes)) {
			return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
		}

		if (this->signatureSize > sizeof(this->signature)) {
			return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
		}

		if (checkSignatures) {
			// Signature check fails if main signature is not present or invalid.
			// Note that the serial number / SHA384 hash is computed on decode(), so
			// this value is not something we blindly trust from input.
			if ((this->issuerPublicKeySize > 0) && (this->issuerPublicKeySize <= (unsigned int)sizeof(this->issuerPublicKey))) {
				switch (this->issuerPublicKey[0]) {
					case ZT_CERTIFICATE_PUBLIC_KEY_ALGORITHM_ECDSA_NIST_P_384:
						if ((this->issuerPublicKeySize == (ZT_ECC384_PUBLIC_KEY_SIZE + 1)) && (this->signatureSize == ZT_ECC384_SIGNATURE_SIZE)) {
							if (!ECC384ECDSAVerify(this->issuerPublicKey + 1, this->serialNo, this->signature)) {
								return ZT_CERTIFICATE_ERROR_INVALID_PRIMARY_SIGNATURE;
							}
						} else {
							return ZT_CERTIFICATE_ERROR_INVALID_PRIMARY_SIGNATURE;
						}
						break;
					default:
						return ZT_CERTIFICATE_ERROR_INVALID_PRIMARY_SIGNATURE;
				}
			} else {
				return ZT_CERTIFICATE_ERROR_INVALID_PRIMARY_SIGNATURE;
			}

			// Subject unique ID signatures are optional, so this only fails if it
			// is present and invalid. A unique ID with type ALGORITHM_NONE is also
			// allowed, but this means its signature is not checked.
			if (this->subject.uniqueIdSize > 0) {
				if (this->subject.uniqueIdSize <= (unsigned int)sizeof(this->subject.uniqueId)) {
					switch (this->subject.uniqueId[0]) {
						case ZT_CERTIFICATE_PUBLIC_KEY_ALGORITHM_NONE:
							break;
						case ZT_CERTIFICATE_PUBLIC_KEY_ALGORITHM_ECDSA_NIST_P_384:
							if ((this->subject.uniqueIdSize == (ZT_ECC384_PUBLIC_KEY_SIZE + 1)) && (this->subject.uniqueIdSignatureSize == ZT_ECC384_SIGNATURE_SIZE)) {
								Dictionary d;
								m_encodeSubject(this->subject, d, true);

								Vector< uint8_t > enc;
								enc.reserve(1024);
								d.encode(enc);

								static_assert(ZT_ECC384_SIGNATURE_HASH_SIZE == ZT_SHA384_DIGEST_SIZE, "ECC384 should take 384-bit hash");
								uint8_t h[ZT_SHA384_DIGEST_SIZE];
								SHA384(h, enc.data(), (unsigned int)enc.size());

								if (!ECC384ECDSAVerify(this->subject.uniqueId + 1, h, this->subject.uniqueIdSignature)) {
									return ZT_CERTIFICATE_ERROR_INVALID_UNIQUE_ID_PROOF;
								}
							} else {
								return ZT_CERTIFICATE_ERROR_INVALID_UNIQUE_ID_PROOF;
							}
							break;
						default:
							return ZT_CERTIFICATE_ERROR_INVALID_UNIQUE_ID_PROOF;
					}
				} else {
					return ZT_CERTIFICATE_ERROR_INVALID_UNIQUE_ID_PROOF;
				}
			}
		}

		if (clock >= 0) {
			if (!this->verifyTimeWindow(clock))
				return ZT_CERTIFICATE_ERROR_OUT_OF_VALID_TIME_WINDOW;
		}
	} catch (...) {
		return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
	}

	return ZT_CERTIFICATE_ERROR_NONE;
}

bool Certificate::newKeyPair(const ZT_CertificatePublicKeyAlgorithm type, uint8_t publicKey[ZT_CERTIFICATE_MAX_PUBLIC_KEY_SIZE], int *const publicKeySize, uint8_t privateKey[ZT_CERTIFICATE_MAX_PRIVATE_KEY_SIZE], int *const privateKeySize)
{
	switch (type) {
		case ZT_CERTIFICATE_PUBLIC_KEY_ALGORITHM_ECDSA_NIST_P_384:
			publicKey[0] = (uint8_t)ZT_CERTIFICATE_PUBLIC_KEY_ALGORITHM_ECDSA_NIST_P_384;
			ZeroTier::ECC384GenerateKey(publicKey + 1, privateKey + ZT_ECC384_PUBLIC_KEY_SIZE + 1);
			ZeroTier::Utils::copy< ZT_ECC384_PUBLIC_KEY_SIZE + 1 >(privateKey, publicKey);
			*publicKeySize = ZT_ECC384_PUBLIC_KEY_SIZE + 1;
			*privateKeySize = ZT_ECC384_PUBLIC_KEY_SIZE + 1 + ZT_ECC384_PRIVATE_KEY_SIZE;
			return true;
		default:
			break;
	}
	return false;
}

Vector< uint8_t > Certificate::createCSR(const ZT_Certificate_Subject &s, const void *const certificatePublicKey, const unsigned int certificatePublicKeySize, const void *uniqueIdPrivate, unsigned int uniqueIdPrivateSize)
{
	Vector< uint8_t > enc;

	ZT_Certificate_Subject sc;
	Utils::copy< sizeof(ZT_Certificate_Subject) >(&sc, &s);

	if (m_setSubjectUniqueId(sc, uniqueIdPrivate, uniqueIdPrivateSize)) {
		Dictionary d;
		m_encodeSubject(sc, d, false);
		if (certificatePublicKeySize > 0)
			d.add("pK", certificatePublicKey, certificatePublicKeySize);
		d.encode(enc);
	}

	return enc;
}

void Certificate::m_clear()
{
	ZT_Certificate *const sup = this;
	Utils::zero< sizeof(ZT_Certificate) >(sup);

	m_identities.clear();
	m_locators.clear();
	m_strings.clear();

	m_subjectIdentities.clear();
	m_subjectNetworks.clear();
	m_updateUrls.clear();
	m_extendedAttributes.clear();
}

bool Certificate::m_setSubjectUniqueId(ZT_Certificate_Subject &s, const void *uniqueIdPrivate, unsigned int uniqueIdPrivateSize)
{
	if (uniqueIdPrivateSize > 0) {
		if ((uniqueIdPrivate != nullptr) && (uniqueIdPrivateSize == (1 + ZT_ECC384_PUBLIC_KEY_SIZE + ZT_ECC384_PRIVATE_KEY_SIZE)) && (reinterpret_cast<const uint8_t *>(uniqueIdPrivate)[0] == (uint8_t)ZT_CERTIFICATE_PUBLIC_KEY_ALGORITHM_ECDSA_NIST_P_384)) {
			Utils::copy< 1 + ZT_ECC384_PUBLIC_KEY_SIZE >(s.uniqueId, uniqueIdPrivate);
			s.uniqueIdSize = 1 + ZT_ECC384_PUBLIC_KEY_SIZE; // private is prefixed with public

			Vector< uint8_t > enc;
			Dictionary d;
			m_encodeSubject(s, d, true);
			d.encode(enc);

			uint8_t h[ZT_SHA384_DIGEST_SIZE];
			SHA384(h, enc.data(), (unsigned int)enc.size());

			ECC384ECDSASign(reinterpret_cast<const uint8_t *>(uniqueIdPrivate) + 1 + ZT_ECC384_PUBLIC_KEY_SIZE, h, s.uniqueIdSignature);
			s.uniqueIdSignatureSize = ZT_ECC384_SIGNATURE_SIZE;
		} else {
			return false;
		}
	} else {
		Utils::zero< sizeof(s.uniqueId) >(s.uniqueId);
		s.uniqueIdSize = 0;
		Utils::zero< sizeof(s.uniqueIdSignature) >(s.uniqueIdSignature);
		s.uniqueIdSignatureSize = 0;
	}
	return true;
}

void Certificate::m_encodeSubject(const ZT_Certificate_Subject &s, Dictionary &d, bool omitUniqueIdProofSignature)
{
	char tmp[32];

	d.add("s.t", (uint64_t)s.timestamp);

	if (s.identities) {
		d.add("s.i$", (uint64_t)s.identityCount);
		for (unsigned int i = 0; i < s.identityCount; ++i) {
			if (s.identities[i].identity)
				d.addO(Dictionary::arraySubscript(tmp, sizeof(tmp), "s.i$.i", i), *reinterpret_cast<const Identity *>(s.identities[i].identity));
			if (s.identities[i].locator)
				d.addO(Dictionary::arraySubscript(tmp, sizeof(tmp), "s.i$.l", i), *reinterpret_cast<const Locator *>(s.identities[i].locator));
		}
	}

	if (s.networks) {
		d.add("s.nw$", (uint64_t)s.networkCount);
		for (unsigned int i = 0; i < s.networkCount; ++i) {
			d.add(Dictionary::arraySubscript(tmp, sizeof(tmp), "s.nw$.i", i), s.networks[i].id);
			Fingerprint fp(s.networks[i].controller);
			d.addO(Dictionary::arraySubscript(tmp, sizeof(tmp), "s.nw$.c", i), fp);
		}
	}

	if (s.updateURLs) {
		d.add("s.u$", (uint64_t)s.updateURLCount);
		for (unsigned int i = 0; i < s.updateURLCount; ++i)
			d.add(Dictionary::arraySubscript(tmp, sizeof(tmp), "s.u$", i), s.updateURLs[i]);
	}

	if (s.name.country[0])
		d.add("s.n.c", s.name.country);
	if (s.name.organization[0])
		d.add("s.n.o", s.name.organization);
	if (s.name.unit[0])
		d.add("s.n.u", s.name.unit);
	if (s.name.locality[0])
		d.add("s.n.l", s.name.locality);
	if (s.name.province[0])
		d.add("s.n.p", s.name.province);
	if (s.name.streetAddress[0])
		d.add("s.n.sA", s.name.streetAddress);
	if (s.name.postalCode[0])
		d.add("s.n.pC", s.name.postalCode);
	if (s.name.commonName[0])
		d.add("s.n.cN", s.name.commonName);
	if (s.name.serialNo[0])
		d.add("s.n.sN", s.name.serialNo);
	if (s.name.email[0])
		d.add("s.n.e", s.name.email);
	if (s.name.url[0])
		d.add("s.n.ur", s.name.url);
	if (s.name.host[0])
		d.add("s.n.h", s.name.host);

	if (s.uniqueIdSize > 0)
		d["s.uI"].assign(s.uniqueId, s.uniqueId + s.uniqueIdSize);
	if ((!omitUniqueIdProofSignature) && (s.uniqueIdSignatureSize > 0))
		d["s.uS"].assign(s.uniqueIdSignature, s.uniqueIdSignature + s.uniqueIdSignatureSize);
}

} // namespace ZeroTier
