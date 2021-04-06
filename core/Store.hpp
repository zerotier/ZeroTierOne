/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_STORE_HPP
#define ZT_STORE_HPP

#include "Constants.hpp"
#include "Containers.hpp"
#include "RuntimeEnvironment.hpp"
#include "CallContext.hpp"

namespace ZeroTier {

/**
 * Wrapper around API callbacks for data store
 */
class Store
{
public:
	ZT_INLINE Store(const RuntimeEnvironment *const renv): RR(renv)
	{}

	/**
	 * Get a state object
	 *
	 * @param type Object type
	 * @param id Object ID
	 * @param idSize Size of object ID in qwords
	 * @return Data or empty vector if not found
	 */
	ZT_INLINE Vector< uint8_t > get(CallContext &cc, ZT_StateObjectType type, const uint64_t *id, unsigned int idSize) const
	{
		Vector< uint8_t > dv;
		void *data = nullptr;
		void (*freeFunc)(void *) = nullptr;
		const int r = RR->cb.stateGetFunction(reinterpret_cast<ZT_Node *>(RR->node), RR->uPtr, cc.tPtr, type, id, idSize, &data, &freeFunc);
		if (r > 0)
			dv.assign(reinterpret_cast<const uint8_t *>(data), reinterpret_cast<const uint8_t *>(data) + r);
		if ((data) && (freeFunc))
			freeFunc(data);
		return dv;
	}

	/**
	 * Store a state object
	 *
	 * @param type Object type
	 * @param id Object ID
	 * @param idSize Size of object ID in qwords
	 * @param data Data to store
	 * @param len Length of data
	 */
	ZT_INLINE void put(CallContext &cc, ZT_StateObjectType type, const uint64_t *const id, const unsigned int idSize, const void *const data, const unsigned int len) noexcept
	{ RR->cb.statePutFunction(reinterpret_cast<ZT_Node *>(this), RR->uPtr, cc.tPtr, type, id, idSize, data, (int)len); }

	/**
	 * Erase a state object from the object store
	 *
	 * @param type Object type
	 * @param id Object ID
	 * @param idSize Size of object ID in qwords
	 */
	ZT_INLINE void erase(CallContext &cc, ZT_StateObjectType type, const uint64_t *const id, const unsigned int idSize) noexcept
	{ RR->cb.statePutFunction(reinterpret_cast<ZT_Node *>(this), RR->uPtr, cc.tPtr, type, id, idSize, nullptr, -1); }

private:
	const RuntimeEnvironment *RR;
};

} // namespace ZeroTier

#endif
