/*
 * Based on public domain code available at: http://cr.yp.to/snuffle.html
 *
 * This therefore is public domain.
 */

#ifndef ZT_SALSA20_HPP
#define ZT_SALSA20_HPP

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Constants.hpp"
#include "Utils.hpp"

#if (!defined(ZT_SALSA20_SSE)) && (defined(__SSE2__) || defined(__WINDOWS__))
#define ZT_SALSA20_SSE 1
#endif

#if defined(ZT_SALSA20_SSE) && !ZT_SALSA20_SSE
#undef ZT_SALSA20_SSE
#endif

#ifdef ZT_SALSA20_SSE
#include <emmintrin.h>
#endif // ZT_SALSA20_SSE

namespace ZeroTier {

/**
 * Salsa20 stream cipher
 */
class Salsa20
{
public:
	Salsa20() {}
	~Salsa20() { Utils::burn(&_state,sizeof(_state)); }

	/**
	 * XOR d with s
	 *
	 * This is done efficiently using e.g. SSE if available. It's used when
	 * alternative Salsa20 implementations are used in Packet and is here
	 * since this is where all the SSE stuff is already included.
	 *
	 * @param d Destination to XOR
	 * @param s Source bytes to XOR with destination
	 * @param len Length of s and d
	 */
	static inline void memxor(uint8_t *d,const uint8_t *s,unsigned int len)
	{
#ifdef ZT_SALSA20_SSE
		while (len >= 128) {
			__m128i s0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s));
			__m128i s1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s + 16));
			__m128i s2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s + 32));
			__m128i s3 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s + 48));
			__m128i s4 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s + 64));
			__m128i s5 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s + 80));
			__m128i s6 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s + 96));
			__m128i s7 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s + 112));
			__m128i d0 = _mm_loadu_si128(reinterpret_cast<__m128i *>(d));
			__m128i d1 = _mm_loadu_si128(reinterpret_cast<__m128i *>(d + 16));
			__m128i d2 = _mm_loadu_si128(reinterpret_cast<__m128i *>(d + 32));
			__m128i d3 = _mm_loadu_si128(reinterpret_cast<__m128i *>(d + 48));
			__m128i d4 = _mm_loadu_si128(reinterpret_cast<__m128i *>(d + 64));
			__m128i d5 = _mm_loadu_si128(reinterpret_cast<__m128i *>(d + 80));
			__m128i d6 = _mm_loadu_si128(reinterpret_cast<__m128i *>(d + 96));
			__m128i d7 = _mm_loadu_si128(reinterpret_cast<__m128i *>(d + 112));
			d0 = _mm_xor_si128(d0,s0);
			d1 = _mm_xor_si128(d1,s1);
			d2 = _mm_xor_si128(d2,s2);
			d3 = _mm_xor_si128(d3,s3);
			d4 = _mm_xor_si128(d4,s4);
			d5 = _mm_xor_si128(d5,s5);
			d6 = _mm_xor_si128(d6,s6);
			d7 = _mm_xor_si128(d7,s7);
			_mm_storeu_si128(reinterpret_cast<__m128i *>(d),d0);
			_mm_storeu_si128(reinterpret_cast<__m128i *>(d + 16),d1);
			_mm_storeu_si128(reinterpret_cast<__m128i *>(d + 32),d2);
			_mm_storeu_si128(reinterpret_cast<__m128i *>(d + 48),d3);
			_mm_storeu_si128(reinterpret_cast<__m128i *>(d + 64),d4);
			_mm_storeu_si128(reinterpret_cast<__m128i *>(d + 80),d5);
			_mm_storeu_si128(reinterpret_cast<__m128i *>(d + 96),d6);
			_mm_storeu_si128(reinterpret_cast<__m128i *>(d + 112),d7);
			s += 128;
			d += 128;
			len -= 128;
		}
		while (len >= 16) {
			_mm_storeu_si128(reinterpret_cast<__m128i *>(d),_mm_xor_si128(_mm_loadu_si128(reinterpret_cast<__m128i *>(d)),_mm_loadu_si128(reinterpret_cast<const __m128i *>(s))));
			s += 16;
			d += 16;
			len -= 16;
		}
#else
#ifndef ZT_NO_TYPE_PUNNING
		while (len >= 16) {
			(*reinterpret_cast<uint64_t *>(d)) ^= (*reinterpret_cast<const uint64_t *>(s));
			s += 8;
			d += 8;
			(*reinterpret_cast<uint64_t *>(d)) ^= (*reinterpret_cast<const uint64_t *>(s));
			s += 8;
			d += 8;
			len -= 16;
		}
#endif
#endif
		while (len) {
			--len;
			*(d++) ^= *(s++);
		}
	}

	/**
	 * @param key 256-bit (32 byte) key
	 * @param iv 64-bit initialization vector
	 */
	Salsa20(const void *key,const void *iv)
	{
		init(key,iv);
	}

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
