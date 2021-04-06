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

#include "Locator.hpp"
#include "Identity.hpp"

#include <algorithm>

namespace ZeroTier {

const SharedPtr< const Locator::EndpointAttributes > Locator::EndpointAttributes::DEFAULT(new Locator::EndpointAttributes());

Locator::Locator(const char *const str) noexcept :
	__refCount(0)
{
	if (!fromString(str)) {
		m_revision = 0;
		m_signer.zero();
		m_endpoints.clear();
		m_signature.clear();
	}
}

bool Locator::add(const Endpoint &ep, const SharedPtr< const EndpointAttributes > &a)
{
	for (Vector< std::pair< Endpoint, SharedPtr< const EndpointAttributes > > >::iterator i(m_endpoints.begin());i!=m_endpoints.end();++i) {
		if (i->first == ep) {
			i->second = ((a) && (a->data[0] != 0)) ? a : EndpointAttributes::DEFAULT;
			return true;
		}
	}
	if (m_endpoints.size() < ZT_LOCATOR_MAX_ENDPOINTS) {
		m_endpoints.push_back(std::pair<Endpoint, SharedPtr< const EndpointAttributes > >(ep, ((a) && (a->data[0] != 0)) ? a : EndpointAttributes::DEFAULT));
		return true;
	}
	return false;
}

bool Locator::sign(const int64_t rev, const Identity &id) noexcept
{
	m_revision = rev;
	m_signer = id.fingerprint();

	m_sortEndpoints();

	uint8_t signdata[ZT_LOCATOR_MARSHAL_SIZE_MAX];
	const unsigned int signlen = marshal(signdata, true);

	const unsigned int siglen = id.sign(signdata, signlen, m_signature.data(), m_signature.capacity());
	if (siglen == 0)
		return false;
	m_signature.unsafeSetSize(siglen);

	return true;
}

bool Locator::verify(const Identity &id) const noexcept
{
	try {
		if ((m_revision > 0) && (m_signer == id.fingerprint())) {
			uint8_t signdata[ZT_LOCATOR_MARSHAL_SIZE_MAX];
			const unsigned int signlen = marshal(signdata, true);
			return id.verify(signdata, signlen, m_signature.data(), m_signature.size());
		}
	} catch (...) {} // fail verify on any unexpected exception
	return false;
}

char *Locator::toString(char s[ZT_LOCATOR_STRING_SIZE_MAX]) const noexcept
{
	static_assert(ZT_LOCATOR_STRING_SIZE_MAX > ((((ZT_LOCATOR_MARSHAL_SIZE_MAX / 5) + 1) * 8) + ZT_ADDRESS_LENGTH_HEX + 1), "overflow");
	uint8_t bin[ZT_LOCATOR_MARSHAL_SIZE_MAX];
	Address(m_signer.address).toString(s);
	s[ZT_ADDRESS_LENGTH_HEX] = '@';
	Utils::b32e(bin, marshal(bin, false), s + (ZT_ADDRESS_LENGTH_HEX + 1), ZT_LOCATOR_STRING_SIZE_MAX - (ZT_ADDRESS_LENGTH_HEX + 1));
	return s;
}

bool Locator::fromString(const char *s) noexcept
{
	if (!s)
		return false;
	if (strlen(s) < (ZT_ADDRESS_LENGTH_HEX + 1))
		return false;
	uint8_t bin[ZT_LOCATOR_MARSHAL_SIZE_MAX];
	const int bl = Utils::b32d(s + (ZT_ADDRESS_LENGTH_HEX + 1), bin, ZT_LOCATOR_MARSHAL_SIZE_MAX);
	if ((bl <= 0) || (bl > ZT_LOCATOR_MARSHAL_SIZE_MAX))
		return false;
	return unmarshal(bin, bl) > 0;
}

int Locator::marshal(uint8_t data[ZT_LOCATOR_MARSHAL_SIZE_MAX], const bool excludeSignature) const noexcept
{
	Utils::storeBigEndian<uint64_t>(data, (uint64_t) m_revision);
	int p = 8;

	int l = m_signer.marshal(data + p);
	if (l <= 0)
		return -1;
	p += l;

	Utils::storeBigEndian<uint16_t>(data + p, (uint16_t) m_endpoints.size());
	p += 2;
	for (Vector< std::pair< Endpoint, SharedPtr< const EndpointAttributes > > >::const_iterator e(m_endpoints.begin());e != m_endpoints.end();++e) {
		l = e->first.marshal(data + p);
		if (l <= 0)
			return -1;
		p += l;

		l = (int)e->second->data[0];
		if (l > 0) {
			Utils::copy(data + p, e->second->data, (unsigned int)l);
			p += l;
		} else {
			data[p++] = 0;
		}
	}

	Utils::storeMachineEndian< uint16_t >(data + p, 0); // length of meta-data, currently always 0
	p += 2;

	if (!excludeSignature) {
		Utils::storeBigEndian<uint16_t>(data + p, (uint16_t) m_signature.size());
		p += 2;
		Utils::copy(data + p, m_signature.data(), m_signature.size());
		p += (int) m_signature.size();
	}

	return p;
}

int Locator::unmarshal(const uint8_t *data, const int len) noexcept
{
	if (unlikely(len < 8))
		return -1;
	m_revision = (int64_t)Utils::loadBigEndian<uint64_t>(data);
	int p = 8;

	int l = m_signer.unmarshal(data + p, len - p);
	if (l <= 0)
		return -1;
	p += l;

	if (unlikely(p + 2) > len)
		return -1;
	unsigned int endpointCount = Utils::loadBigEndian<uint16_t>(data + p);
	p += 2;
	if (unlikely(endpointCount > ZT_LOCATOR_MAX_ENDPOINTS))
		return -1;
	m_endpoints.resize(endpointCount);
	m_endpoints.shrink_to_fit();
	for (unsigned int i = 0;i < endpointCount;++i) {
		l = m_endpoints[i].first.unmarshal(data + p, len - p);
		if (l <= 0)
			return -1;
		p += l;

		if (unlikely(p + 1) > len)
			return -1;
		l = (int)data[p];
		if (l <= 0) {
			m_endpoints[i].second = EndpointAttributes::DEFAULT;
			++p;
		} else {
			m_endpoints[i].second.set(new EndpointAttributes());
			Utils::copy(const_cast< uint8_t * >(m_endpoints[i].second->data), data + p, (unsigned int)l);
			p += l;
		}
	}

	if (unlikely((p + 2) > len))
		return -1;
	p += 2 + (int)Utils::loadBigEndian<uint16_t>(data + p);

	if (unlikely((p + 2) > len))
		return -1;
	const unsigned int siglen = Utils::loadBigEndian<uint16_t>(data + p);
	p += 2;
	if (unlikely((siglen > ZT_SIGNATURE_BUFFER_SIZE) || ((p + (int)siglen) > len)))
		return -1;
	m_signature.unsafeSetSize(siglen);
	Utils::copy(m_signature.data(), data + p, siglen);
	p += (int)siglen;
	if (unlikely(p > len))
		return -1;

	m_sortEndpoints();

	return p;
}

struct p_SortByEndpoint
{
	// There can't be more than one of the same endpoint, so only need to sort
	// by endpoint.
	ZT_INLINE bool operator()(const std::pair< Endpoint, SharedPtr< const Locator::EndpointAttributes > > &a,const std::pair< Endpoint, SharedPtr< const Locator::EndpointAttributes > > &b) const noexcept
	{ return a.first < b.first; }
};

void Locator::m_sortEndpoints() noexcept
{ std::sort(m_endpoints.begin(), m_endpoints.end(), p_SortByEndpoint()); }

} // namespace ZeroTier
