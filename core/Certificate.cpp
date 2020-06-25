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

#include "Certificate.hpp"
#include "SHA512.hpp"
#include "ECC384.hpp"

namespace ZeroTier {

void Certificate::clear()
{
	Utils::zero< sizeof(ZT_Certificate) >((ZT_Certificate *)this);

	m_identities.clear();
	m_locators.clear();
	m_strings.clear();
	m_serials.clear();

	m_subjectIdentities.clear();
	m_subjectNetworks.clear();
	m_updateUrls.clear();
	m_subjectCertificates.clear();
}

Certificate &Certificate::operator=(const ZT_Certificate &apiCert)
{
	clear();
	Utils::copy< sizeof(ZT_Certificate) >((ZT_Certificate *)this, &apiCert);
	return *this;
}

Certificate &Certificate::operator=(const Certificate &cert)
{
	*this = *((const ZT_Certificate *)(&cert));

	// Zero these since we must explicitly attach all the objects from
	// the other certificate to copy them into our containers.
	this->subject.identityCount = 0;
	this->subject.networkCount = 0;
	this->subject.certificateCount = 0;
	this->subject.updateUrlCount = 0;

	for (unsigned int i = 0; i < cert.subject.identityCount; ++i) {
		if (cert.subject.identities[i].identity) {
			if (cert.subject.identities[i].locator)
				addSubjectNode(*reinterpret_cast<const Identity *>(cert.subject.identities[i].identity), *reinterpret_cast<const Locator *>(cert.subject.identities[i].locator));
			else addSubjectNode(*reinterpret_cast<const Identity *>(cert.subject.identities[i].identity));
		}
	}

	for (unsigned int i = 0; i < cert.subject.networkCount; ++i) {
		if (cert.subject.networks[i].id)
			addSubjectNetwork(cert.subject.networks[i].id, cert.subject.networks[i].controller);
	}

	for (unsigned int i = 0; i < cert.subject.certificateCount; ++i) {
		if (cert.subject.certificates[i])
			addSubjectCertificate(cert.subject.certificates[i]);
	}

	if (cert.subject.updateUrls) {
		for (unsigned int i = 0; i < cert.subject.updateUrlCount; ++i) {
			if (cert.subject.updateUrls[i])
				addUpdateUrl(cert.subject.updateUrls[i]);
		}
	}

	if (cert.issuer) {
		m_identities.push_back(*reinterpret_cast<const Identity *>(cert.issuer));
		this->issuer = &(m_identities.back());
	}

	return *this;
}

ZT_Certificate_Identity *Certificate::addSubjectNode(const Identity &id)
{
	// Enlarge array of ZT_Certificate_Identity structs and set pointer to potentially reallocated array.
	m_subjectIdentities.resize(++this->subject.identityCount);
	this->subject.identities = m_subjectIdentities.data();

	// Store a local copy of the actual identity.
	m_identities.push_back(id);

	// Set ZT_Certificate_Identity struct fields to point to local copy of identity.
	m_subjectIdentities.back().identity = &(m_identities.back());
	m_subjectIdentities.back().locator = nullptr;

	return &(m_subjectIdentities.back());
}

ZT_Certificate_Identity *Certificate::addSubjectNode(const Identity &id, const Locator &loc)
{
	// Add identity as above.
	ZT_Certificate_Identity *const n = addSubjectNode(id);

	// Store local copy of locator.
	m_locators.push_back(loc);

	// Set pointer to stored local copy of locator.
	n->locator = &(m_locators.back());

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

void Certificate::addSubjectCertificate(const uint8_t serialNo[ZT_SHA384_DIGEST_SIZE])
{
	// Store local copy of serial in m_serials container.
	m_serials.push_back(SHA384Hash(serialNo));

	// Enlarge array of uint8_t pointers, set new pointer to local copy of serial, and set
	// certificates to point to potentially reallocated array.
	m_subjectCertificates.resize(++this->subject.certificateCount);
	m_subjectCertificates.back() = m_serials.back().data;
	this->subject.certificates = m_subjectCertificates.data();
}

void Certificate::addUpdateUrl(const char *url)
{
	// Store local copy of URL.
	m_strings.push_back(url);

	// Add pointer to local copy to pointer array and update C structure to point to
	// potentially reallocated array.
	m_updateUrls.push_back(m_strings.back().c_str());
	this->subject.updateUrls = m_updateUrls.data();
	this->subject.updateUrlCount = (unsigned int)m_updateUrls.size();
}

Vector< uint8_t > Certificate::encode(const bool omitSignature) const
{
	Vector< uint8_t > enc;
	Dictionary d;

	// A Dictionary is used to encode certificates as it's a common and extensible
	// format. Custom packed formats are used for credentials as these are smaller
	// and faster to marshal/unmarshal.

	d.add("f", this->flags);
	d.add("t", (uint64_t)this->timestamp);
	d.add("v0", (uint64_t)this->validity[0]);
	d.add("v1", (uint64_t)this->validity[1]);
	d.add("mP", (uint64_t)this->maxPathLength);

	m_encodeSubject(d, false);

	if (this->issuer)
		d.addO("i", *reinterpret_cast<const Identity *>(this->issuer));

	d.add("iN.c", this->issuerName.country);
	d.add("iN.o", this->issuerName.organization);
	d.add("iN.u", this->issuerName.unit);
	d.add("iN.l", this->issuerName.locality);
	d.add("iN.p", this->issuerName.province);
	d.add("iN.sA", this->issuerName.streetAddress);
	d.add("iN.pC", this->issuerName.postalCode);
	d.add("iN.cN", this->issuerName.commonName);
	d.add("iN.sN", this->issuerName.serialNo);
	d.add("iN.e", this->issuerName.email);
	d.add("iN.ur", this->issuerName.url);

	if ((!omitSignature) && (this->signatureSize > 0) && (this->signatureSize <= sizeof(this->signature)))
		d["si"].assign(this->signature, this->signature + this->signatureSize);

	d.encode(enc);
	return enc;
}

bool Certificate::decode(const Vector< uint8_t > &data)
{
	char tmp[256], tmp2[ZT_CERTIFICATE_MAX_STRING_LENGTH + 1];

	clear();

	Dictionary d;
	if (!d.decode(data.data(), (unsigned int)data.size()))
		return false;

	this->flags = d.getUI("f");
	this->timestamp = (int64_t)d.getUI("t");
	this->validity[0] = (int64_t)d.getUI("v0");
	this->validity[1] = (int64_t)d.getUI("v1");
	this->maxPathLength = (unsigned int)d.getUI("mP");

	this->subject.timestamp = (int64_t)d.getUI("s.t");

	unsigned int cnt = (unsigned int)d.getUI("s.i$");
	for (unsigned int i = 0; i < cnt; ++i) {
		const Vector< uint8_t > &identityData = d[Dictionary::arraySubscript(tmp, "s.i$.i", i)];
		if (identityData.empty())
			return false;
		Identity id;
		if (id.unmarshal(identityData.data(), (unsigned int)identityData.size()) <= 0)
			return false;
		const Vector< uint8_t > &locatorData = d[Dictionary::arraySubscript(tmp, "s.i$.l", i)];
		if (!locatorData.empty()) {
			Locator loc;
			if (loc.unmarshal(locatorData.data(), (unsigned int)locatorData.size()) <= 0)
				return false;
			this->addSubjectNode(id, loc);
		} else {
			this->addSubjectNode(id);
		}
	}

	cnt = (unsigned int)d.getUI("s.n$");
	for (unsigned int i = 0; i < cnt; ++i) {
		const uint64_t nwid = d.getUI(Dictionary::arraySubscript(tmp, "s.n$.i", i));
		const Vector< uint8_t > &fingerprintData = d[Dictionary::arraySubscript(tmp, "s.n$.c", i)];
		if ((nwid == 0) || (fingerprintData.empty()))
			return false;
		Fingerprint fp;
		if (fp.unmarshal(fingerprintData.data(), (unsigned int)fingerprintData.size()) <= 0)
			return false;
		this->addSubjectNetwork(nwid, fp);
	}

	cnt = (unsigned int)d.getUI("s.c$");
	for (unsigned int i = 0; i < cnt; ++i) {
		const Vector< uint8_t > &serial = d[Dictionary::arraySubscript(tmp, "s.c$", i)];
		if (serial.size() != ZT_SHA384_DIGEST_SIZE)
			return false;
		this->addSubjectCertificate(serial.data());
	}

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

	const Vector< uint8_t > &issuerData = d["i"];
	if (!issuerData.empty()) {
		Identity id;
		if (id.unmarshal(issuerData.data(), (int)issuerData.size()) > 0) {
			m_identities.push_back(id);
			this->issuer = reinterpret_cast<const Identity *>(&(m_identities.back()));
		}
	}

	d.getS("iN.sN", this->issuerName.serialNo, sizeof(this->issuerName.serialNo));
	d.getS("iN.cN", this->issuerName.commonName, sizeof(this->issuerName.commonName));
	d.getS("iN.c", this->issuerName.country, sizeof(this->issuerName.country));
	d.getS("iN.o", this->issuerName.organization, sizeof(this->issuerName.organization));
	d.getS("iN.u", this->issuerName.unit, sizeof(this->issuerName.unit));
	d.getS("iN.l", this->issuerName.locality, sizeof(this->issuerName.locality));
	d.getS("iN.p", this->issuerName.province, sizeof(this->issuerName.province));
	d.getS("iN.sA", this->issuerName.streetAddress, sizeof(this->issuerName.streetAddress));
	d.getS("iN.pC", this->issuerName.postalCode, sizeof(this->issuerName.postalCode));
	d.getS("iN.e", this->issuerName.email, sizeof(this->issuerName.email));
	d.getS("iN.ur", this->issuerName.url, sizeof(this->issuerName.url));

	cnt = (unsigned int)d.getUI("u$");
	for (unsigned int i = 0; i < cnt; ++i) {
		const char *const url = d.getS(Dictionary::arraySubscript(tmp, "u$", i), tmp2, sizeof(tmp2));
		if (url)
			addUpdateUrl(tmp2);
		else return false;
	}

	const Vector< uint8_t > &sig = d["si"];
	if (sig.size() > sizeof(this->signature))
		return false;
	Utils::copy(this->signature, sig.data(), (unsigned int)sig.size());
	this->signatureSize = (unsigned int)sig.size();

	Vector< uint8_t > enc(encode(true));
	SHA384(this->serialNo, enc.data(), (unsigned int)enc.size());

	return true;
}

bool Certificate::sign(const Identity &issuer)
{
	Vector< uint8_t > enc(encode(true));
	SHA384(this->serialNo, enc.data(), (unsigned int)enc.size());
	return (this->signatureSize = issuer.sign(enc.data(), (unsigned int)enc.size(), this->signature, sizeof(this->signature))) > 0;
}

ZT_CertificateError Certificate::verify() const
{
	try {
		if (this->issuer) {
			const Vector< uint8_t > enc(encode(true));
			if (!reinterpret_cast<const Identity *>(this->issuer)->verify(enc.data(), (unsigned int)enc.size(), this->signature, this->signatureSize))
				return ZT_CERTIFICATE_ERROR_INVALID_PRIMARY_SIGNATURE;
		} else {
			return ZT_CERTIFICATE_ERROR_INVALID_PRIMARY_SIGNATURE;
		}

		if (this->subject.uniqueIdProofSignatureSize > 0) {
			static_assert(ZT_ECC384_SIGNATURE_SIZE <= ZT_CERTIFICATE_MAX_SIGNATURE_SIZE, "overflow");
			static_assert((ZT_ECC384_PUBLIC_KEY_SIZE + 1) <= ZT_CERTIFICATE_MAX_UNIQUE_ID_SIZE, "overflow");
			if (
				(this->subject.uniqueIdProofSignatureSize != ZT_ECC384_SIGNATURE_SIZE) ||
				(this->subject.uniqueIdSize != (ZT_ECC384_PUBLIC_KEY_SIZE + 1)) ||
				(this->subject.uniqueId[0] != ZT_CERTIFICATE_UNIQUE_ID_PUBLIC_KEY_TYPE_NIST_P_384))
				return ZT_CERTIFICATE_ERROR_INVALID_UNIQUE_ID_PROOF;
			Dictionary tmp;
			m_encodeSubject(tmp, true);
			Vector< uint8_t > enc;
			tmp.encode(enc);
			uint8_t h[ZT_SHA384_DIGEST_SIZE];
			SHA384(h, enc.data(), (unsigned int)enc.size());
			if (!ECC384ECDSAVerify(this->subject.uniqueId + 1, h, this->subject.uniqueIdProofSignature))
				return ZT_CERTIFICATE_ERROR_INVALID_UNIQUE_ID_PROOF;
		} else if (this->subject.uniqueIdSize > ZT_CERTIFICATE_MAX_UNIQUE_ID_SIZE) {
			return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
		}

		for (unsigned int i = 0; i < this->subject.identityCount; ++i) {
			if (!this->subject.identities[i].identity)
				return ZT_CERTIFICATE_ERROR_MISSING_REQUIRED_FIELDS;
			if (!reinterpret_cast<const Identity *>(this->subject.identities[i].identity)->locallyValidate())
				return ZT_CERTIFICATE_ERROR_INVALID_IDENTITY;
			if (this->subject.identities[i].locator) {
				if (!reinterpret_cast<const Locator *>(this->subject.identities[i].locator)->verify(*reinterpret_cast<const Identity *>(this->subject.identities[i].identity)))
					return ZT_CERTIFICATE_ERROR_INVALID_COMPONENT_SIGNATURE;
			}
		}

		for (unsigned int i = 0; i < this->subject.networkCount; ++i) {
			if (!this->subject.networks[i].id)
				return ZT_CERTIFICATE_ERROR_MISSING_REQUIRED_FIELDS;
		}

		if (this->subject.updateUrlCount) {
			if (!this->subject.updateUrls)
				return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
			for (unsigned int i = 0; i < this->subject.updateUrlCount; ++i) {
				if (!this->subject.updateUrls[i])
					return ZT_CERTIFICATE_ERROR_MISSING_REQUIRED_FIELDS;
			}
		} else if (this->subject.updateUrls) {
			return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
		}
	} catch (...) {}

	return ZT_CERTIFICATE_ERROR_NONE;
}

void Certificate::m_encodeSubject(Dictionary &d, bool omitUniqueIdProofSignature) const
{
	char tmp[256];

	d.add("s.t", (uint64_t)this->subject.timestamp);

	d.add("s.i$", (uint64_t)this->subject.identityCount);
	for (unsigned int i = 0; i < this->subject.identityCount; ++i) {
		if (this->subject.identities[i].identity)
			d.addO(Dictionary::arraySubscript(tmp, "s.i$.i", i), *reinterpret_cast<const Identity *>(this->subject.identities[i].identity));
		if (this->subject.identities[i].locator)
			d.addO(Dictionary::arraySubscript(tmp, "s.i$.l", i), *reinterpret_cast<const Locator *>(this->subject.identities[i].locator));
	}

	d.add("s.n$", (uint64_t)this->subject.networkCount);
	for (unsigned int i = 0; i < this->subject.networkCount; ++i) {
		d.add(Dictionary::arraySubscript(tmp, "s.n$.i", i), this->subject.networks[i].id);
		Fingerprint fp(this->subject.networks[i].controller);
		d.addO(Dictionary::arraySubscript(tmp, "s.n$.c", i), fp);
	}

	d.add("s.c$", (uint64_t)this->subject.certificateCount);
	for (unsigned int i = 0; i < this->subject.certificateCount; ++i) {
		if (this->subject.certificates[i])
			d[Dictionary::arraySubscript(tmp, "s.c$", i)].assign(this->subject.certificates[i], this->subject.certificates[i] + ZT_SHA384_DIGEST_SIZE);
	}

	d.add("s.u$", (uint64_t)this->subject.updateUrlCount);
	if (this->subject.updateUrls) {
		for (unsigned int i = 0; i < this->subject.updateUrlCount; ++i)
			d.add(Dictionary::arraySubscript(tmp, "s.u$", i), this->subject.updateUrls[i]);
	}

	d.add("s.n.c", this->subject.name.country);
	d.add("s.n.o", this->subject.name.organization);
	d.add("s.n.u", this->subject.name.unit);
	d.add("s.n.l", this->subject.name.locality);
	d.add("s.n.p", this->subject.name.province);
	d.add("s.n.sA", this->subject.name.streetAddress);
	d.add("s.n.pC", this->subject.name.postalCode);
	d.add("s.n.cN", this->subject.name.commonName);
	d.add("s.n.sN", this->subject.name.serialNo);
	d.add("s.n.e", this->subject.name.email);
	d.add("s.n.ur", this->subject.name.url);

	if ((this->subject.uniqueIdSize > 0) && (this->subject.uniqueIdSize <= ZT_CERTIFICATE_MAX_UNIQUE_ID_SIZE))
		d["s.uI"].assign(this->subject.uniqueId, this->subject.uniqueId + this->subject.uniqueIdSize);
	if ((!omitUniqueIdProofSignature) && (this->subject.uniqueIdProofSignatureSize > 0) && (this->subject.uniqueIdProofSignatureSize <= ZT_CERTIFICATE_MAX_SIGNATURE_SIZE))
		d["s.uS"].assign(this->subject.uniqueIdProofSignature, this->subject.uniqueIdProofSignature + this->subject.uniqueIdProofSignatureSize);
}

} // namespace ZeroTier
