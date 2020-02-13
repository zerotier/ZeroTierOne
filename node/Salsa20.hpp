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

#ifndef ZT_SALSA20_HPP
#define ZT_SALSA20_HPP

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "Constants.hpp"
#include "Utils.hpp"
#include "TriviallyCopyable.hpp"

#if (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__) || defined(_M_X64))
#include <xmmintrin.h>
#include <emmintrin.h>
#include <immintrin.h>
#define ZT_SALSA20_SSE 1
#endif

namespace ZeroTier {

/**
 * Salsa20 stream cipher
 */
class Salsa20 : public TriviallyCopyable
{
public:
	ZT_ALWAYS_INLINE Salsa20() noexcept {}
	ZT_ALWAYS_INLINE ~Salsa20() { Utils::burn(&_state,sizeof(_state)); }

	/**
	 * @param key 256-bit (32 byte) key
	 * @param iv 64-bit initialization vector
	 */
	ZT_ALWAYS_INLINE Salsa20(const void *key,const void *iv) noexcept { init(key,iv); }

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
