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

#ifndef ZT_FINGERPRINT_HPP
#define ZT_FINGERPRINT_HPP

#include "Constants.hpp"
#include "TriviallyCopyable.hpp"

namespace ZeroTier {

/**
 * Container for 384-bit identity hashes
 *
 * The size of the hash used with this container must be a multiple of 64 bits.
 * Currently it's used as H<384> and H<512>.
 *
 * Warning: the [] operator is not bounds checked.
 *
 * @tparam BITS Bits in hash, must be a multiple of 64
 */
class Fingerprint : public TriviallyCopyable
{
public:
	ZT_ALWAYS_INLINE Fingerprint() noexcept {}
	explicit ZT_ALWAYS_INLINE Fingerprint(const void *h384) noexcept { memcpy(_h,h384,48); }

	ZT_ALWAYS_INLINE void set(const void *h384) noexcept { memcpy(_h,h384,48); }

	ZT_ALWAYS_INLINE void zero() noexcept
	{
		for(unsigned int i=0;i<(384 / (sizeof(unsigned long) * 8));++i)
			_h[i] = 0;
	}

	ZT_ALWAYS_INLINE uint8_t *data() noexcept { return reinterpret_cast<uint8_t *>(_h); }
	ZT_ALWAYS_INLINE const uint8_t *data() const noexcept { return reinterpret_cast<const uint8_t *>(_h); }

	ZT_ALWAYS_INLINE uint8_t operator[](const unsigned int i) const noexcept { return reinterpret_cast<const uint8_t *>(_h)[i]; }
	ZT_ALWAYS_INLINE uint8_t &operator[](const unsigned int i) noexcept { return reinterpret_cast<uint8_t *>(_h)[i]; }

	static constexpr unsigned int size() noexcept { return 48; }

	ZT_ALWAYS_INLINE unsigned long hashCode() const noexcept { return _h[0]; }

	ZT_ALWAYS_INLINE operator bool() const noexcept
	{
		for(unsigned int i=0;i<(384 / (sizeof(unsigned long) * 8));++i) {
			if (_h[i] != 0)
				return true;
		}
		return false;
	}

	ZT_ALWAYS_INLINE bool operator==(const Fingerprint &h) const noexcept { return memcmp(_h,h._h,48) == 0; }
	ZT_ALWAYS_INLINE bool operator!=(const Fingerprint &h) const noexcept { return memcmp(_h,h._h,48) != 0; }
	ZT_ALWAYS_INLINE bool operator<(const Fingerprint &h) const noexcept { return memcmp(_h,h._h,48) < 0; }
	ZT_ALWAYS_INLINE bool operator>(const Fingerprint &h) const noexcept { return memcmp(_h,h._h,48) > 0; }
	ZT_ALWAYS_INLINE bool operator<=(const Fingerprint &h) const noexcept { return memcmp(_h,h._h,48) <= 0; }
	ZT_ALWAYS_INLINE bool operator>=(const Fingerprint &h) const noexcept { return memcmp(_h,h._h,48) >= 0; }

private:
	unsigned long _h[384 / (sizeof(unsigned long) * 8)];
};

} // namespace ZeroTier

#endif
