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

#ifndef ZT_LOCATOR_HPP
#define ZT_LOCATOR_HPP

#include <algorithm>
#include <vector>

#include "Constants.hpp"
#include "Endpoint.hpp"
#include "Identity.hpp"

#define ZT_LOCATOR_MAX_ENDPOINTS 8
#define ZT_LOCATOR_MARSHAL_SIZE_MAX (8 + 2 + (ZT_ENDPOINT_MARSHAL_SIZE_MAX * ZT_LOCATOR_MAX_ENDPOINTS) + 2 + ZT_SIGNATURE_BUFFER_SIZE)

namespace ZeroTier {

/**
 * Signed information about a node's location on the network
 *
 * A locator contains long-lived endpoints for a node such as IP/port pairs,
 * URLs, or other nodes, and is signed by the node it describes.
 */
class Locator
{
public:
	inline Locator() : _ts(0),_endpointCount(0),_signatureLength(0) {}

	/**
	 * @return Timestamp (a.k.a. revision number) set by Location signer
	 */
	inline int64_t timestamp() const { return _ts; }

	/**
	 * Create and sign a Locator
	 *
	 * @param ts Timestamp
	 * @param id Identity (must include secret to allow signing)
	 * @param at Array of Endpoint objects specifying where this peer might be found
	 * @param endpointCount Number of endpoints (max: ZT_LOCATOR_MAX_ENDPOINTS)
	 * @return True if init and sign were successful
	 */
	inline bool create(const int64_t ts,const Identity &id,const Endpoint *restrict at,const unsigned int endpointCount)
	{
		if ((endpointCount > ZT_LOCATOR_MAX_ENDPOINTS)||(!id.hasPrivate()))
			return false;
		_ts = ts;
		for(unsigned int i=0;i<endpointCount;++i)
			_at[i] = at[i];
		_endpointCount = endpointCount;

		uint8_t signData[ZT_LOCATOR_MARSHAL_SIZE_MAX];
		const unsigned int signLen = marshal(signData,true);
		if (signLen == 0)
			return false;
		if ((_signatureLength = id.sign(signData,signLen,_signature,sizeof(_signature))) == 0)
			return false;

		return true;
	}

	/**
	 * Verify this Locator's validity and signature
	 *
	 * @param id Identity corresponding to hash
	 * @return True if valid and signature checks out
	 */
	inline bool verify(const Identity &id) const
	{
		if ((_ts == 0)||(_endpointCount > ZT_LOCATOR_MAX_ENDPOINTS)||(_signatureLength > ZT_SIGNATURE_BUFFER_SIZE))
			return false;
		uint8_t signData[ZT_LOCATOR_MARSHAL_SIZE_MAX];
		const unsigned int signLen = marshal(signData,true);
		return id.verify(signData,signLen,_signature,_signatureLength);
	}

	inline operator bool() const { return (_ts != 0); }

	// Marshal interface ///////////////////////////////////////////////////////
	static inline int marshalSizeMax() { return ZT_LOCATOR_MARSHAL_SIZE_MAX; }
	inline int marshal(uint8_t restrict data[ZT_LOCATOR_MARSHAL_SIZE_MAX],const bool excludeSignature = false) const
	{
		if ((_endpointCount > ZT_LOCATOR_MAX_ENDPOINTS)||(_signatureLength > ZT_SIGNATURE_BUFFER_SIZE))
			return -1;

		data[0] = (uint8_t)((uint64_t)_ts >> 56);
		data[1] = (uint8_t)((uint64_t)_ts >> 48);
		data[2] = (uint8_t)((uint64_t)_ts >> 40);
		data[3] = (uint8_t)((uint64_t)_ts >> 32);
		data[4] = (uint8_t)((uint64_t)_ts >> 24);
		data[5] = (uint8_t)((uint64_t)_ts >> 16);
		data[6] = (uint8_t)((uint64_t)_ts >> 8);
		data[7] = (uint8_t)((uint64_t)_ts);
		int p = 8;

		data[p++] = (uint8_t)(_endpointCount >> 8);
		data[p++] = (uint8_t)_endpointCount;
		for(unsigned int i=0;i<_endpointCount;++i) {
			int tmp = _at[i].marshal(data + p);
			if (tmp < 0)
				return -1;
			p += tmp;
		}

		if (!excludeSignature) {
			data[p++] = (uint8_t)(_signatureLength >> 8);
			data[p++] = (uint8_t)_signatureLength;
			memcpy(data + p,_signature,_signatureLength);
			p += _signatureLength;
		}

		return p;
	}
	inline int unmarshal(const uint8_t *restrict data,const int len)
	{
		if (len <= (8 + 48))
			return -1;

		uint64_t ts = ((uint64_t)data[0] << 56);
		ts |= ((uint64_t)data[1] << 48);
		ts |= ((uint64_t)data[2] << 40);
		ts |= ((uint64_t)data[3] << 32);
		ts |= ((uint64_t)data[4] << 24);
		ts |= ((uint64_t)data[5] << 16);
		ts |= ((uint64_t)data[6] << 8);
		ts |= (uint64_t)data[7];
		_ts = (int64_t)ts;
		int p = 8;

		if ((p + 2) > len)
			return -1;
		unsigned int ec = (int)data[p++];
		ec <<= 8;
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
		sl <<= 8;
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
	////////////////////////////////////////////////////////////////////////////

private:
	int64_t _ts;
	unsigned int _endpointCount;
	unsigned int _signatureLength;
	Endpoint _at[ZT_LOCATOR_MAX_ENDPOINTS];
	uint8_t _signature[ZT_SIGNATURE_BUFFER_SIZE];
};

} // namespace ZeroTier

#endif
