/*
 * Based on public domain code available at: http://cr.yp.to/snuffle.html
 *
 * This therefore is public domain.
 */

#ifndef _ZT_SALSA20_HPP
#define _ZT_SALSA20_HPP

#include <stdint.h>

#include "Constants.hpp"

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
	uint32_t _state[16];
	unsigned int _roundsDiv2;
};

} // namespace ZeroTier

#endif
