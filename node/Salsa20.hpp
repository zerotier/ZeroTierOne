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

#ifdef ZT_USE_LIBSODIUM

#include <sodium/crypto_stream_salsa20.h>
#include <sodium/crypto_stream_salsa2012.h>

namespace ZeroTier {

/**
 * Salsa20 stream cipher
 */
class Salsa20
{
public:
	Salsa20() {}
	~Salsa20() { Utils::burn(_k,sizeof(_k)); }

	/**
	 * @param key 256-bit (32 byte) key
	 * @param iv 64-bit initialization vector
	 */
	Salsa20(const void *key,const void *iv)
	{
		memcpy(_k,key,32);
		memcpy(&_iv,iv,8);
	}

	/**
	 * Initialize cipher
	 *
	 * @param key Key bits
	 * @param iv 64-bit initialization vector
	 */
	inline void init(const void *key,const void *iv)
	{
		memcpy(_k,key,32);
		memcpy(&_iv,iv,8);
	}

	/**
	 * Encrypt/decrypt data using Salsa20/12
	 *
	 * @param in Input data
	 * @param out Output buffer
	 * @param bytes Length of data
	 */
	inline void crypt12(const void *in,void *out,unsigned int bytes)
	{
		crypto_stream_salsa2012_xor(reinterpret_cast<unsigned char *>(out),reinterpret_cast<const unsigned char *>(in),bytes,reinterpret_cast<const unsigned char *>(&_iv),reinterpret_cast<const unsigned char *>(_k));
	}

	/**
	 * Encrypt/decrypt data using Salsa20/20
	 *
	 * @param in Input data
	 * @param out Output buffer
	 * @param bytes Length of data
	 */
	inline void crypt20(const void *in,void *out,unsigned int bytes)
	{
		crypto_stream_salsa20_xor(reinterpret_cast<unsigned char *>(out),reinterpret_cast<const unsigned char *>(in),bytes,reinterpret_cast<const unsigned char *>(&_iv),reinterpret_cast<const unsigned char *>(_k));
	}

private:
	uint64_t _k[4];
	uint64_t _iv;
};

} // namespace ZeroTier

#else // !ZT_USE_LIBSODIUM

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
	Salsa20() {}
	~Salsa20() { Utils::burn(&_state,sizeof(_state)); }

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

#endif // ZT_USE_LIBSODIUM

#endif
