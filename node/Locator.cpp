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

namespace ZeroTier {

bool Locator::sign(const int64_t ts,const Identity &id) noexcept
{
	uint8_t signData[ZT_LOCATOR_MARSHAL_SIZE_MAX];
	if (!id.hasPrivate())
		return false;
	m_ts = ts;
	if (m_endpointCount > 0)
		std::sort(m_at, m_at + m_endpointCount);
	const unsigned int signLen = marshal(signData,true);
	m_signatureLength = id.sign(signData, signLen, m_signature, sizeof(m_signature));
	return (m_signatureLength > 0);
}

bool Locator::verify(const Identity &id) const noexcept
{
	if ((m_ts == 0) || (m_endpointCount > ZT_LOCATOR_MAX_ENDPOINTS) || (m_signatureLength > ZT_SIGNATURE_BUFFER_SIZE))
		return false;
	uint8_t signData[ZT_LOCATOR_MARSHAL_SIZE_MAX];
	const unsigned int signLen = marshal(signData,true);
	return id.verify(signData, signLen, m_signature, m_signatureLength);
}

int Locator::marshal(uint8_t data[ZT_LOCATOR_MARSHAL_SIZE_MAX],const bool excludeSignature) const noexcept
{
	if ((m_endpointCount > ZT_LOCATOR_MAX_ENDPOINTS) || (m_signatureLength > ZT_SIGNATURE_BUFFER_SIZE))
		return -1;

	data[0] = 0xff; // version byte, currently 0xff to never be the same as byte 0 of an identity for legacy compatibility reasons
	Utils::storeBigEndian<int64_t>(data + 1, m_ts);
	int p = 9;

	if (m_ts > 0) {
		Utils::storeBigEndian(data + p,(uint16_t)m_endpointCount);
		p += 2;
		for (unsigned int i = 0;i < m_endpointCount;++i) {
			int tmp = m_at[i].marshal(data + p);
			if (tmp < 0)
				return -1;
			p += tmp;
		}

		if (!excludeSignature) {
			Utils::storeBigEndian(data + p,(uint16_t)m_signatureLength);
			p += 2;
			Utils::copy(data + p, m_signature, m_signatureLength);
			p += (int)m_signatureLength;
		}

		Utils::storeBigEndian(data + p, m_flags);
		p += 2;
	}

	return p;
}

int Locator::unmarshal(const uint8_t *restrict data,const int len) noexcept
{
	if (len <= (1 + 8 + 2 + 48))
		return -1;

	if (data[0] != 0xff)
		return -1;
	m_ts = Utils::loadBigEndian<int64_t>(data + 1);
	int p = 9;

	if (m_ts > 0) {
		const unsigned int ec = Utils::loadBigEndian<uint16_t>(data + p);
		p += 2;
		if (ec > ZT_LOCATOR_MAX_ENDPOINTS)
			return -1;
		m_endpointCount = ec;
		for (unsigned int i = 0; i < ec; ++i) {
			int tmp = m_at[i].unmarshal(data + p, len - p);
			if (tmp < 0)
				return -1;
			p += tmp;
		}

		if ((p + 2) > len)
			return -1;
		const unsigned int sl = Utils::loadBigEndian<uint16_t>(data + p);
		p += 2;
		if (sl > ZT_SIGNATURE_BUFFER_SIZE)
			return -1;
		m_signatureLength = sl;
		if ((p + (int)sl) > len)
			return -1;
		Utils::copy(m_signature, data + p, sl);
		p += (int)sl;

		if ((p + 2) > len)
			return -1;
		m_flags = Utils::loadBigEndian<uint16_t>(data + p);
		p += 2;
	} else {
		m_ts = 0;
	}

	return p;
}

} // namespace ZeroTier
