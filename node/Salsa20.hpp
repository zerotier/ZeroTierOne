/*
 * Based on public domain code available at: http://cr.yp.to/snuffle.html
 *
 * This therefore is public domain.
 */

#ifndef ZT_SALSA20_HPP
#define ZT_SALSA20_HPP

#include <stdint.h>

#include "Constants.hpp"

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

	/**
	 * @param key Key bits
	 * @param kbits Number of key bits: 128 or 256 (recommended)
	 * @param iv 64-bit initialization vector
	 * @param rounds Number of rounds: 8, 12, or 20
	 */
	Salsa20(const void *key,unsigned int kbits,const void *iv,unsigned int rounds)
		throw()
	{
		init(key,kbits,iv,rounds);
	}

	/**
	 * Initialize cipher
	 *
	 * @param key Key bits
	 * @param kbits Number of key bits: 128 or 256 (recommended)
	 * @param iv 64-bit initialization vector
	 * @param rounds Number of rounds: 8, 12, or 20
	 */
	void init(const void *key,unsigned int kbits,const void *iv,unsigned int rounds)
		throw();

	/**
	 * Encrypt data
	 *
	 * @param in Input data
	 * @param out Output buffer
	 * @param bytes Length of data
	 */
	void encrypt(const void *in,void *out,unsigned int bytes)
		throw();

	/**
	 * Decrypt data
	 *
	 * @param in Input data
	 * @param out Output buffer
	 * @param bytes Length of data
	 */
	inline void decrypt(const void *in,void *out,unsigned int bytes)
		throw()
	{
		encrypt(in,out,bytes);
	}

private:
	volatile union {
#ifdef ZT_SALSA20_SSE
		__m128i v[4];
#endif // ZT_SALSA20_SSE
		uint32_t i[16];
	} _state;
	unsigned int _roundsDiv2;
};

} // namespace ZeroTier

#endif
