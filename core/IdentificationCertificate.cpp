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

#include "IdentificationCertificate.hpp"
#include "SHA512.hpp"

namespace ZeroTier {

void IdentificationCertificate::clear()
{
	Utils::zero< sizeof(ZT_IdentificationCertificate) >((ZT_IdentificationCertificate *)this);
	m_identities.clear();
	m_locators.clear();
	m_nodes.clear();
	m_networks.clear();
}

IdentificationCertificate &IdentificationCertificate::operator=(const ZT_IdentificationCertificate &apiCert)
{
	clear();
	Utils::copy< sizeof(ZT_IdentificationCertificate) >((ZT_IdentificationCertificate *)this, &apiCert);
	return *this;
}

IdentificationCertificate &IdentificationCertificate::operator=(const IdentificationCertificate &cert)
{
	*this = *((const ZT_IdentificationCertificate *)(&cert));

	this->subject.nodeCount = 0;
	this->subject.networkCount = 0;

	if (cert.issuer) {
		m_identities.push_back(*reinterpret_cast<const Identity *>(cert.issuer));
		this->issuer = reinterpret_cast<ZT_Identity *>(&(m_identities.back()));
	}

	for (unsigned int i = 0; i < cert.subject.nodeCount; ++i) {
		if (cert.subject.nodes[i].locator)
			addSubjectNode(*reinterpret_cast<const Identity *>(cert.subject.nodes[i].identity), *reinterpret_cast<const Locator *>(cert.subject.nodes[i].locator));
		else if (cert.subject.nodes[i].identity)
			addSubjectNode(*reinterpret_cast<const Identity *>(cert.subject.nodes[i].identity));
	}

	for (unsigned int i = 0; i < cert.subject.networkCount; ++i)
		addSubjectNetwork(cert.subject.networks[i].id, cert.subject.networks[i].controller);

	return *this;
}

ZT_IdentificationCertificate_Node *IdentificationCertificate::addSubjectNode(const Identity &id)
{
	m_nodes.resize(++this->subject.nodeCount);
	this->subject.nodes = m_nodes.data();
	m_identities.push_back(id);
	m_nodes.back().identity = reinterpret_cast<ZT_Identity *>(&(m_identities.back()));
	m_nodes.back().locator = nullptr;
	return &(m_nodes.back());
}

ZT_IdentificationCertificate_Node *IdentificationCertificate::addSubjectNode(const Identity &id, const Locator &loc)
{
	ZT_IdentificationCertificate_Node *n = addSubjectNode(id);
	m_locators.push_back(loc);
	n->locator = reinterpret_cast<ZT_Locator *>(&(m_locators.back()));
	return n;
}

ZT_IdentificationCertificate_Network *IdentificationCertificate::addSubjectNetwork(const uint64_t id, const ZT_Fingerprint &controller)
{
	m_networks.resize(++this->subject.networkCount);
	this->subject.networks = m_networks.data();
	m_networks.back().id = id;
	Utils::copy< sizeof(ZT_Fingerprint) >(&(m_networks.back().controller), &controller);
	return &(m_networks.back());
}

Vector< uint8_t > IdentificationCertificate::encode(const bool omitSignature) const
{
	char tmp[256];
	Vector< uint8_t > enc;
	Dictionary d;

	d.add("v", (uint64_t)this->version);
	d.add("mP", (uint64_t)this->maxPathLength);
	d.add("f", this->flags);
	d.add("v0", this->validity[0]);
	d.add("v1", this->validity[1]);

	d.add("s.n[]", (uint64_t)this->subject.nodeCount);
	for (unsigned int i = 0; i < this->subject.nodeCount; ++i) {
		d.addO(Dictionary::arraySubscript(tmp, "s.n[].i", i), *reinterpret_cast<const Identity *>(this->subject.nodes[i].identity));
		if (this->subject.nodes[i].locator)
			d.addO(Dictionary::arraySubscript(tmp, "s.n[].l", i), *reinterpret_cast<const Locator *>(this->subject.nodes[i].locator));
	}

	d.add("s.nw[]", (uint64_t)this->subject.networkCount);
	for (unsigned int i = 0; i < this->subject.networkCount; ++i) {
		d.add(Dictionary::arraySubscript(tmp, "s.nw[].i", i), this->subject.networks[i].id);
		Fingerprint fp(this->subject.networks[i].controller);
		d.addO(Dictionary::arraySubscript(tmp, "s.nw[].c", i), fp);
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

bool IdentificationCertificate::decode(const Vector< uint8_t > &data)
{
	char tmp[256];

	clear();

	Dictionary d;
	if (!d.decode(data.data(), (unsigned int)data.size()))
		return false;

	this->version = (unsigned int)d.getUI("v");
	this->maxPathLength = (unsigned int)d.getUI("mP");
	this->flags = d.getUI("f");
	this->validity[0] = (int64_t)d.getUI("v0");
	this->validity[1] = (int64_t)d.getUI("v1");

	unsigned int cnt = (unsigned int)d.getUI("s.n[]");
	for (unsigned int i = 0; i < cnt; ++i) {
		const Vector< uint8_t > &identityData = d[Dictionary::arraySubscript(tmp, "s.n[].i", i)];
		if (identityData.empty())
			return false;
		Identity id;
		if (id.unmarshal(identityData.data(), (unsigned int)identityData.size()) <= 0)
			return false;
		const Vector< uint8_t > &locatorData = d[Dictionary::arraySubscript(tmp, "s.n[].l", i)];
		if (!locatorData.empty()) {
			Locator loc;
			if (loc.unmarshal(locatorData.data(), (unsigned int)locatorData.size()) <= 0)
				return false;
			this->addSubjectNode(id, loc);
		} else {
			this->addSubjectNode(id);
		}
	}

	cnt = (unsigned int)d.getUI("s.nw[]");
	for (unsigned int i = 0; i < cnt; ++i) {
		const uint64_t nwid = d.getUI(Dictionary::arraySubscript(tmp, "s.nw[].i", i));
		if (nwid == 0)
			return false;
		const Vector< uint8_t > &fingerprintData = d[Dictionary::arraySubscript(tmp, "s.nw[].c", i)];
		if (fingerprintData.empty())
			return false;
		Fingerprint fp;
		if (fp.unmarshal(fingerprintData.data(), (unsigned int)fingerprintData.size()) <= 0)
			return false;
		this->addSubjectNetwork(nwid, fp);
	}

	d.getS("s.n.c", this->subject.name.country, sizeof(this->subject.name.country));
	d.getS("s.n.o", this->subject.name.organization, sizeof(this->subject.name.organization));
	d.getS("s.n.u", this->subject.name.unit, sizeof(this->subject.name.unit));
	d.getS("s.n.l", this->subject.name.locality, sizeof(this->subject.name.locality));
	d.getS("s.n.p", this->subject.name.province, sizeof(this->subject.name.province));
	d.getS("s.n.sA", this->subject.name.streetAddress, sizeof(this->subject.name.streetAddress));
	d.getS("s.n.pC", this->subject.name.postalCode, sizeof(this->subject.name.postalCode));
	d.getS("s.n.cN", this->subject.name.commonName, sizeof(this->subject.name.commonName));
	d.getS("s.n.sN", this->subject.name.serialNo, sizeof(this->subject.name.serialNo));
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

	d.getS("iN.c", this->issuerName.country, sizeof(this->issuerName.country));
	d.getS("iN.o", this->issuerName.organization, sizeof(this->issuerName.organization));
	d.getS("iN.u", this->issuerName.unit, sizeof(this->issuerName.unit));
	d.getS("iN.l", this->issuerName.locality, sizeof(this->issuerName.locality));
	d.getS("iN.p", this->issuerName.province, sizeof(this->issuerName.province));
	d.getS("iN.sA", this->issuerName.streetAddress, sizeof(this->issuerName.streetAddress));
	d.getS("iN.pC", this->issuerName.postalCode, sizeof(this->issuerName.postalCode));
	d.getS("iN.cN", this->issuerName.commonName, sizeof(this->issuerName.commonName));
	d.getS("iN.sN", this->issuerName.serialNo, sizeof(this->issuerName.serialNo));
	d.getS("iN.e", this->issuerName.email, sizeof(this->issuerName.email));
	d.getS("iN.ur", this->issuerName.url, sizeof(this->issuerName.url));

	const Vector< uint8_t > &sig = d["si"];
	if (sig.size() > sizeof(this->signature))
		return false;
	Utils::copy(this->signature, sig.data(), (unsigned int)sig.size());
	this->signatureSize = (unsigned int)sig.size();

	Vector< uint8_t > enc(encode(true));
	SHA384(this->serialNo, enc.data(), (unsigned int)enc.size());

	return true;
}

bool IdentificationCertificate::sign(const Identity &issuer)
{
	Vector< uint8_t > enc(encode(true));
	SHA384(this->serialNo, enc.data(), (unsigned int)enc.size());
	return (this->signatureSize = issuer.sign(enc.data(), (unsigned int)enc.size(), this->signature, sizeof(this->signature))) > 0;
}

bool IdentificationCertificate::verify() const
{
	if (this->issuer) {
		Vector< uint8_t > enc(encode(true));
		return reinterpret_cast<const Identity *>(this->issuer)->verify(enc.data(), (unsigned int)enc.size(), this->signature, this->signatureSize);
	}
	return false;
}

} // namespace ZeroTier
