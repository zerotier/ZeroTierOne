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

#ifndef ZT_SALSA20_HPP
#define ZT_SALSA20_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "TriviallyCopyable.hpp"

#ifdef ZT_ARCH_X64
#define ZT_SALSA20_SSE 1
#endif

#define ZT_SALSA20_KEY_SIZE 32

namespace ZeroTier {

/**
 * Salsa20 stream cipher
 *
 * This supports both the 12-round and 20-round variants.
 *
 * SECURITY: this code only support up to 2^32 bytes per key. This is
 * a minor optimization done here because ZeroTier messages are
 * nowhere near this large.
 */
class Salsa20 : public TriviallyCopyable
{
public:
#ifdef ZT_SALSA20_SSE
	static constexpr bool accelerated() noexcept { return true; }
#else
	static constexpr bool accelerated() noexcept { return false; }
#endif

	ZT_INLINE Salsa20() noexcept {}
	ZT_INLINE ~Salsa20() noexcept { Utils::burn(&_state,sizeof(_state)); }

	/**
	 * @param key 256-bit (32 byte) key
	 * @param iv 64-bit initialization vector
	 */
	ZT_INLINE Salsa20(const void *key,const void *iv) noexcept { init(key,iv); }

	/**
	 * Initialize cipher
	 *
	 * @param key Key bits
	 * @param iv 64-bit initialization vector
	 */
	void init(const void *key,const void *iv) noexcept;

	/**
	 * Encrypt/decrypt data using Salsa20/12
	 *
	 * @param in Input data
	 * @param out Output buffer
	 * @param bytes Length of data
	 */
	void crypt12(const void *in,void *out,unsigned int bytes) noexcept;

	/**
	 * Encrypt/decrypt data using Salsa20/20
	 *
	 * @param in Input data
	 * @param out Output buffer
	 * @param bytes Length of data
	 */
	void crypt20(const void *in,void *out,unsigned int bytes) noexcept;

private:
	union {
#ifdef ZT_SALSA20_SSE
		__m128i v[4];
#endif // ZT_SALSA20_SSE
		uint32_t i[16];
	} _state;
};

} // namespace ZeroTier

#endif
