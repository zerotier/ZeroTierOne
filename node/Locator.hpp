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
	 * @return Length of signature in bytes or 0 if none
	 */
	ZT_ALWAYS_INLINE unsigned int signatureLength() const { return _signatureLength; }

	/**
	 * @return Pointer to signature bytes
	 */
	ZT_ALWAYS_INLINE const uint8_t *signature() const { return _signature; }

	/**
	 * @return Number of endpoints in this locator
	 */
	ZT_ALWAYS_INLINE unsigned int endpointCount() const { return _endpointCount; }

	/**
	 * @return Pointer to array of endpoints
	 */
	ZT_ALWAYS_INLINE const Endpoint *endpoints() const { return _at; }

	/**
	 * Add an endpoint to this locator
	 *
	 * This doesn't check for the presence of the endpoint, so take
	 * care not to add duplicates.
	 *
	 * @param ep Endpoint to add
	 * @return True if endpoint was added (or already present), false if locator is full
	 */
	ZT_ALWAYS_INLINE bool add(const Endpoint &ep)
	{
		if (_endpointCount >= ZT_LOCATOR_MAX_ENDPOINTS)
			return false;
		_at[_endpointCount++] = ep;
		return true;
	}

	/**
	 * Sign this locator
	 *
	 * This sets timestamp, sorts endpoints so that the same set of endpoints
	 * will always produce the same locator, and signs.
	 *
	 * @param id Identity that includes private key
	 * @return True if signature successful
	 */
	bool sign(int64_t ts,const Identity &id);

	/**
	 * Verify this Locator's validity and signature
	 *
	 * @param id Identity corresponding to hash
	 * @return True if valid and signature checks out
	 */
	bool verify(const Identity &id) const;

	explicit ZT_ALWAYS_INLINE operator bool() const { return (_ts != 0); }

	static ZT_ALWAYS_INLINE int marshalSizeMax() { return ZT_LOCATOR_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_LOCATOR_MARSHAL_SIZE_MAX],bool excludeSignature = false) const;
	int unmarshal(const uint8_t *restrict data,int len);

private:
	int64_t _ts;
	unsigned int _endpointCount;
	unsigned int _signatureLength;
	Endpoint _at[ZT_LOCATOR_MAX_ENDPOINTS];
	uint8_t _signature[ZT_SIGNATURE_BUFFER_SIZE];
};

} // namespace ZeroTier

#endif
