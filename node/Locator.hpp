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
#include <cstdint>

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
	ZT_ALWAYS_INLINE Locator() { this->clear(); }

	/**
	 * Zero the Locator data structure
	 */
	ZT_ALWAYS_INLINE void clear() { memset(reinterpret_cast<void *>(this),0,sizeof(Locator)); }

	/**
	 * @return Timestamp (a.k.a. revision number) set by Location signer
	 */
	ZT_ALWAYS_INLINE int64_t timestamp() const { return _ts; }

	/**
	 * @return True if locator is signed
	 */
	ZT_ALWAYS_INLINE bool isSigned() const { return (_signatureLength > 0); }

	/**
	 * Add an endpoint to this locator if not already present
	 *
	 * @param ep Endpoint to add
	 * @return True if endpoint was added (or already present), false if locator is full
	 */
	inline bool add(const Endpoint &ep)
	{
		if (_endpointCount >= ZT_LOCATOR_MAX_ENDPOINTS)
			return false;
		if (!std::binary_search(_at,_at + _endpointCount,ep)) {
			_at[_endpointCount++] = ep;
			std::sort(_at,_at + _endpointCount);
		}
		return true;
	}

	/**
	 * Sign this locator
	 *
	 * @param id Identity that includes private key
	 * @return True if signature successful
	 */
	inline bool sign(const int64_t ts,const Identity &id)
	{
		uint8_t signData[ZT_LOCATOR_MARSHAL_SIZE_MAX];
		if (!id.hasPrivate())
			return false;
		_ts = ts;
		const unsigned int signLen = marshal(signData,true);
		_signatureLength = id.sign(signData, signLen, _signature, sizeof(_signature));
		return (_signatureLength > 0);
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

	explicit ZT_ALWAYS_INLINE operator bool() const { return (_ts != 0); }

	// Marshal interface ///////////////////////////////////////////////////////
	static ZT_ALWAYS_INLINE int marshalSizeMax() { return ZT_LOCATOR_MARSHAL_SIZE_MAX; }
	inline int marshal(uint8_t data[ZT_LOCATOR_MARSHAL_SIZE_MAX],const bool excludeSignature = false) const
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
	inline int unmarshal(const uint8_t *restrict data,const int len)
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
