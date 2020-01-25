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

#include "Revocation.hpp"

namespace ZeroTier {

bool Revocation::sign(const Identity &signer)
{
	uint8_t buf[ZT_REVOCATION_MARSHAL_SIZE_MAX+32];
	if (signer.hasPrivate()) {
		_signedBy = signer.address();
		_signatureLength = signer.sign(buf,(unsigned int)marshal(buf,true),_signature,sizeof(_signature));
		return true;
	}
	return false;
}

int Revocation::marshal(uint8_t data[ZT_REVOCATION_MARSHAL_SIZE_MAX],bool forSign) const
{
	int p = 0;
	if (forSign) {
		for(int k=0;k<8;++k)
			data[p++] = 0x7f;
	}
	Utils::storeBigEndian<uint32_t>(data + p,0); p += 4;
	Utils::storeBigEndian<uint32_t>(data + p,_id); p += 4;
	Utils::storeBigEndian<uint64_t>(data + p,_networkId); p += 8;
	Utils::storeBigEndian<uint32_t>(data + p,0); p += 4;
	Utils::storeBigEndian<uint32_t>(data + p,_credentialId); p += 4;
	Utils::storeBigEndian<uint64_t>(data + p,(uint64_t)_threshold); p += 8;
	Utils::storeBigEndian<uint64_t>(data + p,_flags); p += 8;
	_target.copyTo(data + p); p += ZT_ADDRESS_LENGTH;
	_signedBy.copyTo(data + p); p += ZT_ADDRESS_LENGTH;
	data[p++] = (uint8_t)_type;
	if (!forSign) {
		data[p++] = 1;
		Utils::storeBigEndian<uint16_t>(data + p,(uint16_t)_signatureLength);
		memcpy(data + p,_signature,_signatureLength);
		p += (int)_signatureLength;
	}
	data[p++] = 0;
	data[p++] = 0;
	if (forSign) {
		for(int k=0;k<8;++k)
			data[p++] = 0x7f;
	}
	return p;
}

int Revocation::unmarshal(const uint8_t *restrict data,const int len)
{
	if (len < 54)
		return -1;
	// 4 bytes reserved
	_id = Utils::loadBigEndian<uint32_t>(data + 4);
	_networkId = Utils::loadBigEndian<uint64_t>(data + 8);
	// 4 bytes reserved
	_credentialId = Utils::loadBigEndian<uint32_t>(data + 20);
	_threshold = (int64_t)Utils::loadBigEndian<uint64_t>(data + 24);
	_flags = Utils::loadBigEndian<uint64_t>(data + 32);
	_target.setTo(data + 40);
	_signedBy.setTo(data + 45);
	_type = (ZT_CredentialType)data[50];
	// 1 byte reserved
	_signatureLength = Utils::loadBigEndian<uint16_t>(data + 52);
	int p = 54 + (int)_signatureLength;
	if ((_signatureLength > ZT_SIGNATURE_BUFFER_SIZE)||(p > len))
		return -1;
	memcpy(_signature,data + 54,_signatureLength);
	if ((p + 2) > len)
		return -1;
	p += 2 + Utils::loadBigEndian<uint16_t>(data + p);
	if (p > len)
		return -1;
	return p;
}

} // namespace ZeroTier
