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

#include "RevocationCredential.hpp"

namespace ZeroTier {

bool RevocationCredential::sign(const Identity &signer) noexcept
{
	uint8_t buf[ZT_REVOCATION_MARSHAL_SIZE_MAX + 32];
	if (signer.hasPrivate()) {
		m_signedBy = signer.address();
		m_signatureLength = signer.sign(buf, (unsigned int)marshal(buf, true), m_signature, sizeof(m_signature));
		return true;
	}
	return false;
}

int RevocationCredential::marshal(uint8_t data[ZT_REVOCATION_MARSHAL_SIZE_MAX], bool forSign) const noexcept
{
	int p = 0;
	if (forSign) {
		for (int k = 0; k < 8; ++k)
			data[p++] = 0x7f;
	}
	Utils::storeBigEndian< uint32_t >(data + p, 0);
	p += 4;
	Utils::storeBigEndian< uint32_t >(data + p, m_id);
	p += 4;
	Utils::storeBigEndian< uint64_t >(data + p, m_networkId);
	p += 8;
	Utils::storeBigEndian< uint32_t >(data + p, 0);
	p += 4;
	Utils::storeBigEndian< uint32_t >(data + p, m_credentialId);
	p += 4;
	Utils::storeBigEndian< uint64_t >(data + p, (uint64_t)m_threshold);
	p += 8;
	Utils::storeBigEndian< uint64_t >(data + p, m_flags);
	p += 8;
	m_target.copyTo(data + p);
	p += ZT_ADDRESS_LENGTH;
	m_signedBy.copyTo(data + p);
	p += ZT_ADDRESS_LENGTH;
	data[p++] = (uint8_t)m_type;
	if (!forSign) {
		data[p++] = 1;
		Utils::storeBigEndian< uint16_t >(data + p, (uint16_t)m_signatureLength);
		Utils::copy(data + p, m_signature, m_signatureLength);
		p += (int)m_signatureLength;
	}
	data[p++] = 0;
	data[p++] = 0;
	if (forSign) {
		for (int k = 0; k < 8; ++k)
			data[p++] = 0x7f;
	}
	return p;
}

int RevocationCredential::unmarshal(const uint8_t *restrict data, const int len) noexcept
{
	if (len < 54)
		return -1;
	// 4 bytes reserved
	m_id = Utils::loadBigEndian< uint32_t >(data + 4);
	m_networkId = Utils::loadBigEndian< uint64_t >(data + 8);
	// 4 bytes reserved
	m_credentialId = Utils::loadBigEndian< uint32_t >(data + 20);
	m_threshold = (int64_t)Utils::loadBigEndian< uint64_t >(data + 24);
	m_flags = Utils::loadBigEndian< uint64_t >(data + 32);
	m_target.setTo(data + 40);
	m_signedBy.setTo(data + 45);
	m_type = (ZT_CredentialType)data[50];
	// 1 byte reserved
	m_signatureLength = Utils::loadBigEndian< uint16_t >(data + 52);
	int p = 54 + (int)m_signatureLength;
	if ((m_signatureLength > ZT_SIGNATURE_BUFFER_SIZE) || (p > len))
		return -1;
	Utils::copy(m_signature, data + 54, m_signatureLength);
	if ((p + 2) > len)
		return -1;
	p += 2 + Utils::loadBigEndian< uint16_t >(data + p);
	if (p > len)
		return -1;
	return p;
}

} // namespace ZeroTier
