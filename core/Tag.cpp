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

#include "Tag.hpp"

namespace ZeroTier {

bool Tag::sign(const Identity &signer) noexcept
{
	uint8_t buf[ZT_TAG_MARSHAL_SIZE_MAX];
	if (signer.hasPrivate()) {
		m_signedBy = signer.address();
		m_signatureLength = signer.sign(buf, (unsigned int) marshal(buf, true), m_signature, sizeof(m_signature));
		return true;
	}
	return false;
}

int Tag::marshal(uint8_t data[ZT_TAG_MARSHAL_SIZE_MAX], bool forSign) const noexcept
{
	int p = 0;
	if (forSign) {
		for (int k = 0;k < 8;++k)
			data[p++] = 0x7f;
	}
	Utils::storeBigEndian<uint64_t>(data + p, m_networkId);
	p += 8;
	Utils::storeBigEndian<uint64_t>(data + p, (uint64_t) m_ts);
	p += 8;
	Utils::storeBigEndian<uint32_t>(data + p, m_id);
	p += 4;
	Utils::storeBigEndian<uint32_t>(data + p, m_value);
	p += 4;
	m_issuedTo.copyTo(data + p);
	p += ZT_ADDRESS_LENGTH;
	m_signedBy.copyTo(data + p);
	p += ZT_ADDRESS_LENGTH;
	if (!forSign) {
		data[p++] = 1;
		Utils::storeBigEndian<uint16_t>(data + p, (uint16_t) m_signatureLength);
		p += 2;
		Utils::copy(data + p, m_signature, m_signatureLength);
		p += (int) m_signatureLength;
	}
	data[p++] = 0;
	data[p++] = 0;
	if (forSign) {
		for (int k = 0;k < 8;++k)
			data[p++] = 0x7f;
	}
	return p;
}

int Tag::unmarshal(const uint8_t *data, int len) noexcept
{
	if (len < 37)
		return -1;
	m_networkId = Utils::loadBigEndian<uint64_t>(data);
	m_ts = (int64_t) Utils::loadBigEndian<uint64_t>(data + 8);
	m_id = Utils::loadBigEndian<uint32_t>(data + 16);
	m_value = Utils::loadBigEndian<uint32_t>(data + 20);
	m_issuedTo.setTo(data + 24);
	m_signedBy.setTo(data + 29);
	// 1 byte reserved
	m_signatureLength = Utils::loadBigEndian<uint16_t>(data + 35);
	int p = 37 + (int) m_signatureLength;
	if ((m_signatureLength > ZT_SIGNATURE_BUFFER_SIZE) || (p > len))
		return -1;
	Utils::copy(m_signature, data + p, m_signatureLength);
	if ((p + 2) > len)
		return -1;
	p += 2 + Utils::loadBigEndian<uint16_t>(data + p);
	if (p > len)
		return -1;
	return p;
}

} // namespace ZeroTier
