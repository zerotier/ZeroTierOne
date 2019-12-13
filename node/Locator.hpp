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

#define ZT_LOCATOR_MAX_ENDPOINTS 8

#define ZT_LOCATOR_MARSHAL_SIZE_MAX ((ZT_ENDPOINT_MARSHAL_SIZE_MAX * ZT_LOCATOR_MAX_ENDPOINTS) + 8 + 256 + ZT_SIGNATURE_BUFFER_SIZE)

namespace ZeroTier {

/**
 * Signed information about a node's location on the network
 *
 * A locator is a signed record that contains information about where a node
 * may be found. It can contain static physical addresses or virtual ZeroTier
 * addresses of nodes that can forward to the target node. Locator records
 * can be stored in signed DNS TXT record sets, in LF by roots, in caches,
 * etc.
 */
class Locator
{
	friend class SharedPtr<Locator>;

public:
	inline Locator() : _ts(0),_signatureLength(0) {}

	inline int64_t timestamp() const { return _ts; }
	inline const Identity &id() const { return _id; }

	inline operator bool() const { return (_ts != 0); }

	inline bool create(const int64_t ts,const Identity &id,const Endpoint *restrict at,const unsigned int endpointCount)
	{
	}

private:
	int64_t _ts;
	Identity _id;
	Endpoint *_at;
	unsigned int _endpointCount;
	unsigned int _signatureLength;
	uint8_t _signature[ZT_SIGNATURE_BUFFER_SIZE];
};

} // namespace ZeroTier

#endif
