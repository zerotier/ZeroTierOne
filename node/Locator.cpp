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

#include "Locator.hpp"

#include <algorithm>

namespace ZeroTier {

bool Locator::add(const Endpoint &ep)
{
	if (m_endpoints.size() < ZT_LOCATOR_MAX_ENDPOINTS) {
		if (std::find(m_endpoints.begin(), m_endpoints.end(), ep) == m_endpoints.end())
			m_endpoints.push_back(ep);
		return true;
	}
	return false;
}

bool Locator::sign(const int64_t ts, const Identity &id) noexcept
{
	m_ts = ts;
	std::sort(m_endpoints.begin(), m_endpoints.end());

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
	if (m_ts <= 0)
		return false;
	uint8_t signdata[ZT_LOCATOR_MARSHAL_SIZE_MAX];
	const unsigned int signlen = marshal(signdata, true);
	return id.verify(signdata, signlen, m_signature.data(), m_signature.size());
}

int Locator::marshal(uint8_t data[ZT_LOCATOR_MARSHAL_SIZE_MAX], const bool excludeSignature) const noexcept
{
	Utils::storeBigEndian<uint64_t>(data, (uint64_t) m_ts);
	Utils::storeBigEndian<uint16_t>(data + 8, (uint16_t) m_endpoints.size());
	int p = 10;
	for (Vector<Endpoint>::const_iterator e(m_endpoints.begin());e != m_endpoints.end();++e) {
		int l = e->marshal(data + p);
		if (l <= 0)
			return -1;
		p += l;
	}
	Utils::storeAsIsEndian<uint16_t>(data + p, 0); // length of meta-data, currently always 0
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
	if (unlikely(len < 10))
		return -1;
	m_ts = (int64_t) Utils::loadBigEndian<uint64_t>(data);
	unsigned int endpointCount = Utils::loadBigEndian<uint16_t>(data + 8);
	if (unlikely(endpointCount > ZT_LOCATOR_MAX_ENDPOINTS))
		return -1;
	int p = 10;
	m_endpoints.resize(endpointCount);
	m_endpoints.shrink_to_fit();
	for (unsigned int i = 0;i < endpointCount;++i) {
		int l = m_endpoints[i].unmarshal(data + p, len - p);
		if (l <= 0)
			return -1;
		p += l;
	}
	if (unlikely((p + 2) > len))
		return -1;
	p += 2 + (int) Utils::loadBigEndian<uint16_t>(data + p);
	if (unlikely((p + 2) > len))
		return -1;
	unsigned int siglen = Utils::loadBigEndian<uint16_t>(data + p);
	p += 2;
	if (unlikely((siglen > ZT_SIGNATURE_BUFFER_SIZE) || ((p + (int) siglen) > len)))
		return -1;
	m_signature.unsafeSetSize(siglen);
	Utils::copy(m_signature.data(), data + p, siglen);
	p += siglen;
	if (unlikely(p > len))
		return -1;
	return p;
}

} // namespace ZeroTier
