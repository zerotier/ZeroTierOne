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
#include "TriviallyCopyable.hpp"

#define ZT_LOCATOR_MAX_ENDPOINTS 8
#define ZT_LOCATOR_MARSHAL_SIZE_MAX (1 + 8 + 2 + (ZT_ENDPOINT_MARSHAL_SIZE_MAX * ZT_LOCATOR_MAX_ENDPOINTS) + 2 + 2 + ZT_SIGNATURE_BUFFER_SIZE)

namespace ZeroTier {

/**
 * Signed information about a node's location on the network
 *
 * A locator contains long-lived endpoints for a node such as IP/port pairs,
 * URLs, or other nodes, and is signed by the node it describes.
 */
class Locator : public TriviallyCopyable
{
public:
	ZT_INLINE Locator() noexcept { memoryZero(this); } // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)

	/**
	 * Zero the Locator data structure
	 */
	ZT_INLINE void clear() noexcept { memoryZero(this); }

	/**
	 * @return Timestamp (a.k.a. revision number) set by Location signer
	 */
	ZT_INLINE int64_t timestamp() const noexcept { return m_ts; }

	/**
	 * @return True if locator is signed
	 */
	ZT_INLINE bool isSigned() const noexcept { return m_signatureLength > 0; }

	/**
	 * @return Length of signature in bytes or 0 if none
	 */
	ZT_INLINE unsigned int signatureLength() const noexcept { return m_signatureLength; }

	/**
	 * @return Pointer to signature bytes
	 */
	ZT_INLINE const uint8_t *signature() const noexcept { return m_signature; }

	/**
	 * @return Number of endpoints in this locator
	 */
	ZT_INLINE unsigned int endpointCount() const noexcept { return m_endpointCount; }

	/**
	 * @return Pointer to array of endpoints
	 */
	ZT_INLINE const Endpoint *endpoints() const noexcept { return m_at; }

	/**
	 * Add an endpoint to this locator
	 *
	 * This doesn't check for the presence of the endpoint, so take
	 * care not to add duplicates.
	 *
	 * @param ep Endpoint to add
	 * @return True if endpoint was added (or already present), false if locator is full
	 */
	ZT_INLINE bool add(const Endpoint &ep) noexcept
	{
		if (m_endpointCount >= ZT_LOCATOR_MAX_ENDPOINTS)
			return false;
		m_at[m_endpointCount++] = ep;
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
	bool sign(int64_t ts,const Identity &id) noexcept;

	/**
	 * Verify this Locator's validity and signature
	 *
	 * @param id Identity corresponding to hash
	 * @return True if valid and signature checks out
	 */
	bool verify(const Identity &id) const noexcept;

	explicit ZT_INLINE operator bool() const noexcept { return m_ts != 0; }

	static constexpr int marshalSizeMax() noexcept { return ZT_LOCATOR_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_LOCATOR_MARSHAL_SIZE_MAX],bool excludeSignature = false) const noexcept;
	int unmarshal(const uint8_t *restrict data,int len) noexcept;

private:
	int64_t m_ts;
	unsigned int m_endpointCount;
	unsigned int m_signatureLength;
	Endpoint m_at[ZT_LOCATOR_MAX_ENDPOINTS];
	uint16_t m_flags;
	uint8_t m_signature[ZT_SIGNATURE_BUFFER_SIZE];
};

} // namespace ZeroTier

#endif
