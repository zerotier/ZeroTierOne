/*
 * Based on public domain code available at: http://cr.yp.to/snuffle.html
 *
 * This therefore is public domain.
 */

#ifndef ZT_SALSA20_HPP
#define ZT_SALSA20_HPP

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "Constants.hpp"
#include "Utils.hpp"

#ifndef ZT_SALSA20_SSE
#if (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__) || defined(_M_X64))
#include <emmintrin.h>
#define ZT_SALSA20_SSE 1
#endif
#endif

namespace ZeroTier {

/**
 * Salsa20 stream cipher
 */
class Salsa20
{
public:
	ZT_ALWAYS_INLINE Salsa20() {}
	ZT_ALWAYS_INLINE ~Salsa20() { Utils::burn(&_state,sizeof(_state)); }

	/**
	 * @param key 256-bit (32 byte) key
	 * @param iv 64-bit initialization vector
	 */
	ZT_ALWAYS_INLINE Salsa20(const void *key,const void *iv) { init(key,iv); }

	/**
	 * Initialize cipher
	 *
	 * @param key Key bits
	 * @param iv 64-bit initialization vector
	 */
	void init(const void *key,const void *iv);

	/**
	 * Encrypt/decrypt data using Salsa20/12
	 *
	 * @param in Input data
	 * @param out Output buffer
	 * @param bytes Length of data
	 */
	void crypt12(const void *in,void *out,unsigned int bytes);

	/**
	 * Encrypt/decrypt data using Salsa20/20
	 *
	 * @param in Input data
	 * @param out Output buffer
	 * @param bytes Length of data
	 */
	void crypt20(const void *in,void *out,unsigned int bytes);

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
