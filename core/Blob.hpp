/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_BLOB_HPP
#define ZT_BLOB_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "TriviallyCopyable.hpp"

#include <algorithm>

// This header contains simple statically sized binary object types.

namespace ZeroTier {

/**
 * Blob type for SHA384 hashes
 */
struct SHA384Hash
{
	uint64_t data[6];

	ZT_INLINE SHA384Hash() noexcept
	{ Utils::zero<sizeof(data)>(data); }

	explicit ZT_INLINE SHA384Hash(const void *const d) noexcept
	{ Utils::copy< 48 >(data, d); }

	ZT_INLINE const uint8_t *bytes() const noexcept
	{ return reinterpret_cast<const uint8_t *>(data); }

	ZT_INLINE unsigned long hashCode() const noexcept
	{ return (unsigned long)data[0]; }

	ZT_INLINE operator bool() const noexcept
	{ return ((data[0] != 0) && (data[1] != 0) && (data[2] != 0) && (data[3] != 0) && (data[4] != 0) && (data[5] != 0)); }

	ZT_INLINE bool operator==(const SHA384Hash &b) const noexcept
	{ return ((data[0] == b.data[0]) && (data[1] == b.data[1]) && (data[2] == b.data[2]) && (data[3] == b.data[3]) && (data[4] == b.data[4]) && (data[5] == b.data[5])); }

	ZT_INLINE bool operator!=(const SHA384Hash &b) const noexcept
	{ return !(*this == b); }

	ZT_INLINE bool operator<(const SHA384Hash &b) const noexcept
	{ return (memcmp(data, b.data, 48) < 0); }

	ZT_INLINE bool operator>(const SHA384Hash &b) const noexcept
	{ return (memcmp(data, b.data, 48) > 0); }

	ZT_INLINE bool operator<=(const SHA384Hash &b) const noexcept
	{ return (memcmp(data, b.data, 48) <= 0); }

	ZT_INLINE bool operator>=(const SHA384Hash &b) const noexcept
	{ return (memcmp(data, b.data, 48) >= 0); }
};

/**
 * Blob type for 128-bit GUIDs, UUIDs, etc.
 */
struct UniqueID
{
	uint64_t data[2];

	ZT_INLINE UniqueID() noexcept
	{ Utils::zero<sizeof(data)>(data); }

	ZT_INLINE UniqueID(const uint64_t a, const uint64_t b) noexcept
	{
		data[0] = a;
		data[1] = b;
	}

	explicit ZT_INLINE UniqueID(const void *const d) noexcept
	{ Utils::copy< 16 >(data, d); }

	ZT_INLINE const uint8_t *bytes() const noexcept
	{ return reinterpret_cast<const uint8_t *>(data); }

	ZT_INLINE unsigned long hashCode() const noexcept
	{ return (unsigned long)(data[0] ^ data[1]); }

	ZT_INLINE operator bool() const noexcept
	{ return ((data[0] != 0) && (data[1] != 0)); }

	ZT_INLINE bool operator==(const UniqueID &b) const noexcept
	{ return ((data[0] == b.data[0]) && (data[1] == b.data[1])); }

	ZT_INLINE bool operator!=(const UniqueID &b) const noexcept
	{ return !(*this == b); }

	ZT_INLINE bool operator<(const UniqueID &b) const noexcept
	{ return (memcmp(data, b.data, 16) < 0); }

	ZT_INLINE bool operator>(const UniqueID &b) const noexcept
	{ return (memcmp(data, b.data, 16) > 0); }

	ZT_INLINE bool operator<=(const UniqueID &b) const noexcept
	{ return (memcmp(data, b.data, 16) <= 0); }

	ZT_INLINE bool operator>=(const UniqueID &b) const noexcept
	{ return (memcmp(data, b.data, 16) >= 0); }
};

static_assert(sizeof(SHA384Hash) == 48,"SHA384Hash contains unnecessary padding");
static_assert(sizeof(UniqueID) == 16,"UniqueID contains unnecessary padding");

} // namespace ZeroTier

#endif
