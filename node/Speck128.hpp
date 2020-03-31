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

#ifndef ZT_SPECK128_HPP
#define ZT_SPECK128_HPP

#include "Constants.hpp"
#include "Utils.hpp"

namespace ZeroTier {

/**
 * Tiny and simple 128-bit ARX block cipher
 *
 * Speck does not specify a mandatory endian-ness. This implementation is
 * little-endian for higher performance on the majority of platforms.
 *
 * This is only used as part of the work function for V1 identity generation
 * and for the built-in secure random source on systems that lack AES
 * hardware acceleration.
 *
 * @tparam R Number of rounds (default: 32)
 */
template<int R = 32>
class Speck128
{
public:
	/**
	 * Create an uninitialized instance, init() must be called to set up.
	 */
	ZT_INLINE Speck128() noexcept {} // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init,hicpp-use-equals-default,modernize-use-equals-default)

	/**
	 * Initialize Speck from a 128-bit key
	 *
	 * @param k 128-bit / 16 byte key
	 */
	ZT_INLINE Speck128(const void *k) noexcept { this->init(k); } // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init,google-explicit-constructor,hicpp-explicit-conversions)

	ZT_INLINE ~Speck128() { Utils::burn(_k,sizeof(_k)); }

	/**
	 * Initialize Speck from a 128-bit key
	 *
	 * @param k 128-bit / 16 byte key
	 */
	ZT_INLINE void init(const void *k) noexcept
	{
		initXY(Utils::loadLittleEndian<uint64_t>(k),Utils::loadLittleEndian<uint64_t>(reinterpret_cast<const uint8_t *>(k) + 8));
	}

	/**
	 * Initialize Speck from a 128-bit key in two 64-bit words
	 *
	 * @param x Least significant 64 bits
	 * @param y Most significant 64 bits
	 */
	ZT_INLINE void initXY(uint64_t x,uint64_t y) noexcept
	{
		_k[0] = x;
		for(uint64_t i=0;i<(R-1);++i) {
			x = x >> 8U | x << 56U;
			x += y;
			x ^= i;
			y = y << 3U | y >> 61U;
			y ^= x;
			_k[i + 1] = y;
		}
	}

	/**
	 * Encrypt a 128-bit block as two 64-bit words
	 *
	 * These should be in host byte order. If read or written to/from data
	 * they should be stored in little-endian byte order.
	 *
	 * @param x Least significant 64 bits
	 * @param y Most significant 64 bits
	 */
	ZT_INLINE void encryptXY(uint64_t &x,uint64_t &y) const noexcept
	{
		for (int i=0;i<R;++i) {
			const uint64_t kk = _k[i];
			x = x >> 8U | x << 56U;
			x += y;
			x ^= kk;
			y = y << 3U | y >> 61U;
			y ^= x;
		}
	}

	/**
	 * Encrypt 512 bits in parallel with the same key.
	 *
	 * Parallel in this case assumes instruction level parallelism, but even without that
	 * it may be faster due to cache/memory effects.
	 */
	ZT_INLINE void encryptXYXYXYXY(uint64_t &x0,uint64_t &y0,uint64_t &x1,uint64_t &y1,uint64_t &x2,uint64_t &y2,uint64_t &x3,uint64_t &y3) const noexcept
	{
		for (int i=0;i<R;++i) {
			const uint64_t kk = _k[i];
			x0 = x0 >> 8U | x0 << 56U;
			x1 = x1 >> 8U | x1 << 56U;
			x2 = x2 >> 8U | x2 << 56U;
			x3 = x3 >> 8U | x3 << 56U;
			x0 += y0;
			x1 += y1;
			x2 += y2;
			x3 += y3;
			x0 ^= kk;
			x1 ^= kk;
			x2 ^= kk;
			x3 ^= kk;
			y0 = y0 << 3U | y0 >> 61U;
			y1 = y1 << 3U | y1 >> 61U;
			y2 = y2 << 3U | y2 >> 61U;
			y3 = y3 << 3U | y3 >> 61U;
			y0 ^= x0;
			y1 ^= x1;
			y2 ^= x2;
			y3 ^= x3;
		}
	}

	/**
	 * Decrypt a 128-bit block as two 64-bit words
	 *
	 * These should be in host byte order. If read or written to/from data
	 * they should be stored in little-endian byte order.
	 *
	 * @param x Least significant 64 bits
	 * @param y Most significant 64 bits
	 */
	ZT_INLINE void decryptXY(uint64_t &x,uint64_t &y) const noexcept
	{
		for (int i=(R-1);i>=0;--i) {
			const uint64_t kk = _k[i];
			y ^= x;
			y = y >> 3U | y << 61U;
			x ^= kk;
			x -= y;
			x = x << 8U | x >> 56U;
		}
	}

	/**
	 * Encrypt a block
	 *
	 * @param in 128-bit / 16 byte input
	 * @param out 128-bit / 16 byte output
	 */
	ZT_INLINE void encrypt(const void *const in,void *const out) const noexcept
	{
		uint64_t x = Utils::loadLittleEndian<uint64_t>(in); // NOLINT(hicpp-use-auto,modernize-use-auto)
		uint64_t y = Utils::loadLittleEndian<uint64_t>(reinterpret_cast<const uint8_t *>(in) + 8); // NOLINT(hicpp-use-auto,modernize-use-auto)
		encryptXY(x,y);
		Utils::storeLittleEndian<uint64_t>(out,x);
		Utils::storeLittleEndian<uint64_t>(reinterpret_cast<uint8_t *>(out) + 8,y);
	}

	/**
	 * Decrypt a block
	 *
	 * @param in 128-bit / 16 byte input
	 * @param out 128-bit / 16 byte output
	 */
	ZT_INLINE void decrypt(const void *const in,void *const out) const noexcept
	{
		uint64_t x = Utils::loadLittleEndian<uint64_t>(in); // NOLINT(hicpp-use-auto,modernize-use-auto)
		uint64_t y = Utils::loadLittleEndian<uint64_t>(reinterpret_cast<const uint8_t *>(in) + 8); // NOLINT(hicpp-use-auto,modernize-use-auto)
		decryptXY(x,y);
		Utils::storeLittleEndian<uint64_t>(out,x);
		Utils::storeLittleEndian<uint64_t>(reinterpret_cast<uint8_t *>(out) + 8,y);
	}

private:
	uint64_t _k[R];
};

} // namespace ZeroTier

#endif
