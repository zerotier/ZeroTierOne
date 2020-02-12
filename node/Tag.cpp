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
		_signedBy = signer.address();
		_signatureLength = signer.sign(buf,(unsigned int)marshal(buf,true),_signature,sizeof(_signature));
		return true;
	}
	return false;
}

int Tag::marshal(uint8_t data[ZT_TAG_MARSHAL_SIZE_MAX],bool forSign) const noexcept
{
	int p = 0;
	if (forSign) {
		for(int k=0;k<8;++k)
			data[p++] = 0x7f;
	}
	Utils::storeBigEndian<uint64_t>(data + p,_networkId); p += 8;
	Utils::storeBigEndian<uint64_t>(data + p,(uint64_t)_ts); p += 8;
	Utils::storeBigEndian<uint32_t>(data + p,_id); p += 4;
	Utils::storeBigEndian<uint32_t>(data + p,_value); p += 4;
	_issuedTo.copyTo(data + p); p += ZT_ADDRESS_LENGTH;
	_signedBy.copyTo(data + p); p += ZT_ADDRESS_LENGTH;
	if (!forSign) {
		data[p++] = 1;
		Utils::storeBigEndian<uint16_t>(data + p,(uint16_t)_signatureLength); p += 2;
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

int Tag::unmarshal(const uint8_t *data,int len) noexcept
{
	if (len < 37)
		return -1;
	_networkId = Utils::loadBigEndian<uint64_t>(data);
	_ts = (int64_t)Utils::loadBigEndian<uint64_t>(data + 8);
	_id = Utils::loadBigEndian<uint32_t>(data + 16);
	_value = Utils::loadBigEndian<uint32_t>(data + 20);
	_issuedTo.setTo(data + 24);
	_signedBy.setTo(data + 29);
	// 1 byte reserved
	_signatureLength = Utils::loadBigEndian<uint16_t>(data + 35);
	int p = 37 + (int)_signatureLength;
	if ((_signatureLength > ZT_SIGNATURE_BUFFER_SIZE)||(p > len))
		return -1;
	memcpy(_signature,data + p,_signatureLength);
	if ((p + 2) > len)
		return -1;
	p += 2 + Utils::loadBigEndian<uint16_t>(data + p);
	if (p > len)
		return -1;
	return p;
}

} // namespace ZeroTier
