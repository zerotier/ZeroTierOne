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

#ifndef ZT_BLOB_HPP
#define ZT_BLOB_HPP

#include "Constants.hpp"
#include "Utils.hpp"

namespace ZeroTier {

/**
 * Container for arbitrary bytes for use in collections
 *
 * @tparam S Size of container in bytes
 */
template<unsigned int S>
struct Blob
{
	uint8_t data[S];

	ZT_INLINE Blob() noexcept { Utils::zero<S>(data); }
	explicit ZT_INLINE Blob(const void *const d) noexcept { Utils::copy<S>(data,d); }

	ZT_INLINE unsigned long hashCode() const noexcept
	{ return (unsigned long)Utils::fnv1a32(data, S); }

	ZT_INLINE operator bool() const noexcept { return !Utils::allZero(data); }

	ZT_INLINE bool operator==(const Blob &b) const noexcept { return (memcmp(data,b.data,S) == 0); }
	ZT_INLINE bool operator!=(const Blob &b) const noexcept { return (memcmp(data,b.data,S) != 0); }
	ZT_INLINE bool operator<(const Blob &b) const noexcept { return (memcmp(data,b.data,S) < 0); }
	ZT_INLINE bool operator>(const Blob &b) const noexcept { return (memcmp(data,b.data,S) > 0); }
	ZT_INLINE bool operator<=(const Blob &b) const noexcept { return (memcmp(data,b.data,S) <= 0); }
	ZT_INLINE bool operator>=(const Blob &b) const noexcept { return (memcmp(data,b.data,S) >= 0); }
};

typedef Blob<48> SHA384Hash;
typedef Blob<16> GUID;

} // namespace ZeroTier

#endif
