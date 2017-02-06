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

#include "Constants.hpp"
#include "Utils.hpp"

#if (!defined(ZT_SALSA20_SSE)) && (defined(__SSE2__) || defined(__WINDOWS__))
#define ZT_SALSA20_SSE 1
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
	Salsa20() throw() {}

	~Salsa20() { Utils::burn(&_state,sizeof(_state)); }

	/**
	 * @param key Key bits
	 * @param kbits Number of key bits: 128 or 256 (recommended)
	 * @param iv 64-bit initialization vector
	 */
	Salsa20(const void *key,unsigned int kbits,const void *iv)
		throw()
	{
		init(key,kbits,iv);
	}

	/**
	 * Initialize cipher
	 *
	 * @param key Key bits
	 * @param kbits Number of key bits: 128 or 256 (recommended)
	 * @param iv 64-bit initialization vector
	 */
	void init(const void *key,unsigned int kbits,const void *iv)
		throw();

	/**
	 * Encrypt/decrypt data using Salsa20/12
	 *
	 * @param in Input data
	 * @param out Output buffer
	 * @param bytes Length of data
	 */
	void crypt12(const void *in,void *out,unsigned int bytes)
		throw();

	/**
	 * Encrypt/decrypt data using Salsa20/20
	 *
	 * @param in Input data
	 * @param out Output buffer
	 * @param bytes Length of data
	 */
	void crypt20(const void *in,void *out,unsigned int bytes)
		throw();

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
