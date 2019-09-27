/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_UTILS_HPP
#define ZT_UTILS_HPP

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <string>
#include <stdexcept>
#include <vector>
#include <map>

#include "Constants.hpp"

namespace ZeroTier {

/**
 * Miscellaneous utility functions and global constants
 */
class Utils
{
public:
	/**
	 * Hexadecimal characters 0-f
	 */
	static const char HEXCHARS[16];

	/**
	 * Perform a time-invariant binary comparison
	 *
	 * @param a First binary string
	 * @param b Second binary string
	 * @param len Length of strings
	 * @return True if strings are equal
	 */
	static ZT_ALWAYS_INLINE bool secureEq(const void *a,const void *b,unsigned int len)
	{
		uint8_t diff = 0;
		for(unsigned int i=0;i<len;++i)
			diff |= ( (reinterpret_cast<const uint8_t *>(a))[i] ^ (reinterpret_cast<const uint8_t *>(b))[i] );
		return (diff == 0);
	}

	/**
	 * Zero memory, ensuring to avoid any compiler optimizations or other things that may stop this.
	 */
	static void burn(void *ptr,unsigned int len);

	/**
	 * @param n Number to convert
	 * @param s Buffer, at least 24 bytes in size
	 * @return String containing 'n' in base 10 form
	 */
	static char *decimal(unsigned long n,char s[24]);

	/**
	 * Compute CRC16-CCITT
	 */
	static uint16_t crc16(const void *buf,unsigned int len);

	/**
	 * Convert an unsigned integer into hex
	 *
	 * @param i Any unsigned integer
	 * @param s Buffer to receive hex, must be at least (2*sizeof(i))+1 in size or overflow will occur.
	 * @return Pointer to s containing hex string with trailing zero byte
	 */
	template<typename I>
	static ZT_ALWAYS_INLINE char *hex(I x,char *s)
	{
		char *const r = s;
		for(unsigned int i=0,b=(sizeof(x)*8);i<sizeof(x);++i) {
			*(s++) = HEXCHARS[(x >> (b -= 4)) & 0xf];
			*(s++) = HEXCHARS[(x >> (b -= 4)) & 0xf];
		}
		*s = (char)0;
		return r;
	}

	/**
	 * Convert the least significant 40 bits of a uint64_t to hex
	 *
	 * @param i Unsigned 64-bit int
	 * @param s Buffer of size [11] to receive 10 hex characters
	 * @return Pointer to buffer
	 */
	static ZT_ALWAYS_INLINE char *hex10(uint64_t i,char s[11])
	{
		s[0] = HEXCHARS[(i >> 36) & 0xf];
		s[1] = HEXCHARS[(i >> 32) & 0xf];
		s[2] = HEXCHARS[(i >> 28) & 0xf];
		s[3] = HEXCHARS[(i >> 24) & 0xf];
		s[4] = HEXCHARS[(i >> 20) & 0xf];
		s[5] = HEXCHARS[(i >> 16) & 0xf];
		s[6] = HEXCHARS[(i >> 12) & 0xf];
		s[7] = HEXCHARS[(i >> 8) & 0xf];
		s[8] = HEXCHARS[(i >> 4) & 0xf];
		s[9] = HEXCHARS[i & 0xf];
		s[10] = (char)0;
		return s;
	}

	/**
	 * Convert a byte array into hex
	 *
	 * @param d Bytes
	 * @param l Length of bytes
	 * @param s String buffer, must be at least (l*2)+1 in size or overflow will occur
	 * @return Pointer to filled string buffer
	 */
	static ZT_ALWAYS_INLINE char *hex(const void *d,unsigned int l,char *s)
	{
		char *const save = s;
		for(unsigned int i=0;i<l;++i) {
			const unsigned int b = reinterpret_cast<const uint8_t *>(d)[i];
			*(s++) = HEXCHARS[b >> 4];
			*(s++) = HEXCHARS[b & 0xf];
		}
		*s = (char)0;
		return save;
	}

	static unsigned int unhex(const char *h,void *buf,unsigned int buflen);
	static unsigned int unhex(const char *h,unsigned int hlen,void *buf,unsigned int buflen);

	/**
	 * Generate secure random bytes
	 *
	 * This will try to use whatever OS sources of entropy are available. It's
	 * guarded by an internal mutex so it's thread-safe.
	 *
	 * @param buf Buffer to fill
	 * @param bytes Number of random bytes to generate
	 */
	static void getSecureRandom(void *buf,unsigned int bytes);

	/**
	 * Get a 64-bit unsigned secure random number
	 */
	static ZT_ALWAYS_INLINE uint64_t getSecureRandom64()
	{
		uint64_t x;
		getSecureRandom(&x,sizeof(x));
		return x;
	}

	static int b32e(const uint8_t *data,int length,char *result,int bufSize);
	static int b32d(const char *encoded, uint8_t *result, int bufSize);

	static ZT_ALWAYS_INLINE unsigned int b64MaxEncodedSize(const unsigned int s) { return ((((s + 2) / 3) * 4) + 1); }
	static unsigned int b64e(const uint8_t *in,unsigned int inlen,char *out,unsigned int outlen);
	static unsigned int b64d(const char *in,uint8_t *out,unsigned int outlen);

	/**
	 * Get a non-cryptographic random integer
	 */
	static uint64_t random();

	static ZT_ALWAYS_INLINE float normalize(float value, int64_t bigMin, int64_t bigMax, int32_t targetMin, int32_t targetMax)
	{
		int64_t bigSpan = bigMax - bigMin;
		int64_t smallSpan = targetMax - targetMin;
		float valueScaled = (value - (float)bigMin) / (float)bigSpan;
		return (float)targetMin + valueScaled * (float)smallSpan;
	}

	/**
	 * Tokenize a string (alias for strtok_r or strtok_s depending on platform)
	 *
	 * @param str String to split
	 * @param delim Delimiters
	 * @param saveptr Pointer to a char * for temporary reentrant storage
	 */
	static ZT_ALWAYS_INLINE char *stok(char *str,const char *delim,char **saveptr)
	{
#ifdef __WINDOWS__
		return strtok_s(str,delim,saveptr);
#else
		return strtok_r(str,delim,saveptr);
#endif
	}

	static ZT_ALWAYS_INLINE unsigned int strToUInt(const char *s) { return (unsigned int)strtoul(s,(char **)0,10); }
	static ZT_ALWAYS_INLINE int strToInt(const char *s) { return (int)strtol(s,(char **)0,10); }
	static ZT_ALWAYS_INLINE unsigned long strToULong(const char *s) { return strtoul(s,(char **)0,10); }
	static ZT_ALWAYS_INLINE long strToLong(const char *s) { return strtol(s,(char **)0,10); }
	static ZT_ALWAYS_INLINE unsigned long long strToU64(const char *s)
	{
#ifdef __WINDOWS__
		return (unsigned long long)_strtoui64(s,(char **)0,10);
#else
		return strtoull(s,(char **)0,10);
#endif
	}
	static ZT_ALWAYS_INLINE long long strTo64(const char *s)
	{
#ifdef __WINDOWS__
		return (long long)_strtoi64(s,(char **)0,10);
#else
		return strtoll(s,(char **)0,10);
#endif
	}
	static ZT_ALWAYS_INLINE unsigned int hexStrToUInt(const char *s) { return (unsigned int)strtoul(s,(char **)0,16); }
	static ZT_ALWAYS_INLINE int hexStrToInt(const char *s) { return (int)strtol(s,(char **)0,16); }
	static ZT_ALWAYS_INLINE unsigned long hexStrToULong(const char *s) { return strtoul(s,(char **)0,16); }
	static ZT_ALWAYS_INLINE long hexStrToLong(const char *s) { return strtol(s,(char **)0,16); }
	static ZT_ALWAYS_INLINE unsigned long long hexStrToU64(const char *s)
	{
#ifdef __WINDOWS__
		return (unsigned long long)_strtoui64(s,(char **)0,16);
#else
		return strtoull(s,(char **)0,16);
#endif
	}
	static ZT_ALWAYS_INLINE long long hexStrTo64(const char *s)
	{
#ifdef __WINDOWS__
		return (long long)_strtoi64(s,(char **)0,16);
#else
		return strtoll(s,(char **)0,16);
#endif
	}

	/**
	 * Perform a safe C string copy, ALWAYS null-terminating the result
	 *
	 * This will never ever EVER result in dest[] not being null-terminated
	 * regardless of any input parameter (other than len==0 which is invalid).
	 *
	 * @param dest Destination buffer (must not be NULL)
	 * @param len Length of dest[] (if zero, false is returned and nothing happens)
	 * @param src Source string (if NULL, dest will receive a zero-length string and true is returned)
	 * @return True on success, false on overflow (buffer will still be 0-terminated)
	 */
	static ZT_ALWAYS_INLINE bool scopy(char *dest,unsigned int len,const char *src)
	{
		if (!len)
			return false; // sanity check
		if (!src) {
			*dest = (char)0;
			return true;
		}
		char *const end = dest + len;
		while ((*dest++ = *src++)) {
			if (dest == end) {
				*(--dest) = (char)0;
				return false;
			}
		}
		return true;
	}

#ifdef __GNUC__
	static ZT_ALWAYS_INLINE unsigned int countBits(const uint8_t v) { return (unsigned int)__builtin_popcount((unsigned int)v); }
	static ZT_ALWAYS_INLINE unsigned int countBits(const uint16_t v) { return (unsigned int)__builtin_popcount((unsigned int)v); }
	static ZT_ALWAYS_INLINE unsigned int countBits(const uint32_t v) { return (unsigned int)__builtin_popcountl((unsigned long)v); }
	static ZT_ALWAYS_INLINE unsigned int countBits(const uint64_t v) { return (unsigned int)__builtin_popcountll((unsigned long long)v); }
#else
	/**
	 * Count the number of bits set in an integer
	 *
	 * @param v Unsigned integer
	 * @return Number of bits set in this integer (0-bits in integer)
	 */
	template<typename T>
	static ZT_ALWAYS_INLINE unsigned int countBits(T v)
	{
		v = v - ((v >> 1) & (T)~(T)0/3);
		v = (v & (T)~(T)0/15*3) + ((v >> 2) & (T)~(T)0/15*3);
		v = (v + (v >> 4)) & (T)~(T)0/255*15;
		return (unsigned int)((v * ((~((T)0))/((T)255))) >> ((sizeof(T) - 1) * 8));
	}
#endif

// Byte swappers for big/little endian conversion
#if __BYTE_ORDER == __LITTLE_ENDIAN
	static ZT_ALWAYS_INLINE uint8_t hton(uint8_t n) { return n; }
	static ZT_ALWAYS_INLINE int8_t hton(int8_t n) { return n; }
	static ZT_ALWAYS_INLINE uint16_t hton(uint16_t n) { return htons(n); }
	static ZT_ALWAYS_INLINE int16_t hton(int16_t n) { return (int16_t)Utils::hton((uint16_t)n); }
	static ZT_ALWAYS_INLINE uint32_t hton(uint32_t n)
	{
#if defined(__GNUC__)
#if defined(__FreeBSD__)
		return htonl(n);
#elif (!defined(__OpenBSD__))
		return __builtin_bswap32(n);
#endif
#else
		return htonl(n);
#endif
	}
	static ZT_ALWAYS_INLINE int32_t hton(int32_t n) { return (int32_t)Utils::hton((uint32_t)n); }
	static ZT_ALWAYS_INLINE uint64_t hton(uint64_t n)
	{
#if defined(__GNUC__)
#if defined(__FreeBSD__)
		return bswap64(n);
#elif (!defined(__OpenBSD__))
		return __builtin_bswap64(n);
#endif
#else
		return (
			((n & 0x00000000000000FFULL) << 56) |
			((n & 0x000000000000FF00ULL) << 40) |
			((n & 0x0000000000FF0000ULL) << 24) |
			((n & 0x00000000FF000000ULL) <<  8) |
			((n & 0x000000FF00000000ULL) >>  8) |
			((n & 0x0000FF0000000000ULL) >> 24) |
			((n & 0x00FF000000000000ULL) >> 40) |
			((n & 0xFF00000000000000ULL) >> 56)
		);
#endif
	}
	static ZT_ALWAYS_INLINE int64_t hton(int64_t n) { return (int64_t)hton((uint64_t)n); }
#else
	template<typename T>
	static ZT_ALWAYS_INLINE T hton(T n) { return n; }
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
	static ZT_ALWAYS_INLINE uint8_t ntoh(uint8_t n) { return n; }
	static ZT_ALWAYS_INLINE int8_t ntoh(int8_t n) { return n; }
	static ZT_ALWAYS_INLINE uint16_t ntoh(uint16_t n) { return ntohs(n); }
	static ZT_ALWAYS_INLINE int16_t ntoh(int16_t n) { return (int16_t)Utils::ntoh((uint16_t)n); }
	static ZT_ALWAYS_INLINE uint32_t ntoh(uint32_t n)
	{
#if defined(__GNUC__)
#if defined(__FreeBSD__)
		return ntohl(n);
#elif (!defined(__OpenBSD__))
		return __builtin_bswap32(n);
#endif
#else
		return ntohl(n);
#endif
	}
	static ZT_ALWAYS_INLINE int32_t ntoh(int32_t n) { return (int32_t)Utils::ntoh((uint32_t)n); }
	static ZT_ALWAYS_INLINE uint64_t ntoh(uint64_t n)
	{
#if defined(__GNUC__)
#if defined(__FreeBSD__)
		return bswap64(n);
#elif (!defined(__OpenBSD__))
		return __builtin_bswap64(n);
#endif
#else
		return (
			((n & 0x00000000000000FFULL) << 56) |
			((n & 0x000000000000FF00ULL) << 40) |
			((n & 0x0000000000FF0000ULL) << 24) |
			((n & 0x00000000FF000000ULL) <<  8) |
			((n & 0x000000FF00000000ULL) >>  8) |
			((n & 0x0000FF0000000000ULL) >> 24) |
			((n & 0x00FF000000000000ULL) >> 40) |
			((n & 0xFF00000000000000ULL) >> 56)
		);
#endif
	}
	static ZT_ALWAYS_INLINE int64_t ntoh(int64_t n) { return (int64_t)ntoh((uint64_t)n); }
#else
	template<typename T>
	static ZT_ALWAYS_INLINE T ntoh(T n) { return n; }
#endif
};

} // namespace ZeroTier

#endif
