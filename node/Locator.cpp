/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "Locator.hpp"

namespace ZeroTier {

bool Locator::sign(const int64_t ts,const Identity &id)
{
	uint8_t signData[ZT_LOCATOR_MARSHAL_SIZE_MAX];
	if (!id.hasPrivate())
		return false;
	_ts = ts;
	if (_endpointCount > 0)
		std::sort(_at,_at + _endpointCount);
	const unsigned int signLen = marshal(signData,true);
	_signatureLength = id.sign(signData, signLen, _signature, sizeof(_signature));
	return (_signatureLength > 0);
}

bool Locator::verify(const Identity &id) const
{
	if ((_ts == 0)||(_endpointCount > ZT_LOCATOR_MAX_ENDPOINTS)||(_signatureLength > ZT_SIGNATURE_BUFFER_SIZE))
		return false;
	uint8_t signData[ZT_LOCATOR_MARSHAL_SIZE_MAX];
	const unsigned int signLen = marshal(signData,true);
	return id.verify(signData,signLen,_signature,_signatureLength);
}

int Locator::marshal(uint8_t data[ZT_LOCATOR_MARSHAL_SIZE_MAX],const bool excludeSignature = false) const
{
	if ((_endpointCount > ZT_LOCATOR_MAX_ENDPOINTS)||(_signatureLength > ZT_SIGNATURE_BUFFER_SIZE))
		return -1;

	Utils::putUInt64(data,(uint64_t)_ts);
	int p = 8;

	data[p++] = (uint8_t)(_endpointCount >> 8U);
	data[p++] = (uint8_t)_endpointCount;
	for(unsigned int i=0;i<_endpointCount;++i) {
		int tmp = _at[i].marshal(data + p);
		if (tmp < 0)
			return -1;
		p += tmp;
	}

	if (!excludeSignature) {
		data[p++] = (uint8_t)(_signatureLength >> 8U);
		data[p++] = (uint8_t)_signatureLength;
		memcpy(data + p,_signature,_signatureLength);
		p += (int)_signatureLength;
	}

	return p;
}

int Locator::unmarshal(const uint8_t *restrict data,const int len)
{
	if (len <= (8 + 48))
		return -1;

	_ts = (int64_t)Utils::readUInt64(data);
	int p = 8;

	if ((p + 2) > len)
		return -1;
	unsigned int ec = (int)data[p++];
	ec <<= 8U;
	ec |= data[p++];
	if (ec > ZT_LOCATOR_MAX_ENDPOINTS)
		return -1;
	_endpointCount = ec;
	for(int i=0;i<ec;++i) {
		int tmp = _at[i].unmarshal(data + p,len - p);
		if (tmp < 0)
			return -1;
		p += tmp;
	}

	if ((p + 2) > len)
		return -1;
	unsigned int sl = data[p++];
	sl <<= 8U;
	sl |= data[p++];
	if (sl > ZT_SIGNATURE_BUFFER_SIZE)
		return -1;
	_signatureLength = sl;
	if ((p + sl) > len)
		return -1;
	memcpy(_signature,data + p,sl);
	p += (int)sl;

	return p;
}

} // namespace ZeroTier
