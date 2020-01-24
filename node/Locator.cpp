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

int Locator::marshal(uint8_t data[ZT_LOCATOR_MARSHAL_SIZE_MAX],const bool excludeSignature) const
{
	if ((_endpointCount > ZT_LOCATOR_MAX_ENDPOINTS)||(_signatureLength > ZT_SIGNATURE_BUFFER_SIZE))
		return -1;

	Utils::storeBigEndian<int64_t>(data,_ts);
	int p = 8;

	if (_ts > 0) {
		Utils::storeBigEndian(data + p,(uint16_t)_endpointCount);
		p += 2;
		for (unsigned int i = 0; i < _endpointCount; ++i) {
			int tmp = _at[i].marshal(data + p);
			if (tmp < 0)
				return -1;
			p += tmp;
		}

		if (!excludeSignature) {
			Utils::storeBigEndian(data + p,(uint16_t)_signatureLength);
			p += 2;
			memcpy(data + p,_signature,_signatureLength);
			p += (int)_signatureLength;
		}

		Utils::storeBigEndian(data + p,_flags);
		p += 2;
	}

	return p;
}

int Locator::unmarshal(const uint8_t *restrict data,const int len)
{
	if (len <= (8 + 2 + 48))
		return -1;

	_ts = Utils::loadBigEndian<int64_t>(data);
	int p = 8;

	if (_ts > 0) {
		const unsigned int ec = Utils::loadBigEndian<uint16_t>(data + p);
		p += 2;
		if (ec > ZT_LOCATOR_MAX_ENDPOINTS)
			return -1;
		_endpointCount = ec;
		for (unsigned int i = 0; i < ec; ++i) {
			int tmp = _at[i].unmarshal(data + p,len - p);
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
		_signatureLength = sl;
		if ((p + (int)sl) > len)
			return -1;
		memcpy(_signature,data + p,sl);
		p += (int)sl;

		if ((p + 2) > len)
			return -1;
		_flags = Utils::loadBigEndian<uint16_t>(data + p);
		p += 2;
	} else {
		_ts = 0;
	}

	return p;
}

} // namespace ZeroTier
