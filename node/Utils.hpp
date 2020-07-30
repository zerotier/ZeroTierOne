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

#if defined(__FreeBSD__)
#include <sys/endian.h>
#endif

#include "Constants.hpp"

namespace ZeroTier {

/**
 * Miscellaneous utility functions and global constants
 */
class Utils
{
public:
	/**
	 * Perform a time-invariant binary comparison
	 *
	 * @param a First binary string
	 * @param b Second binary string
	 * @param len Length of strings
	 * @return True if strings are equal
	 */
	static inline bool secureEq(const void *a,const void *b,unsigned int len)
	{
		uint8_t diff = 0;
		for(unsigned int i=0;i<len;++i)
			diff |= ( (reinterpret_cast<const uint8_t *>(a))[i] ^ (reinterpret_cast<const uint8_t *>(b))[i] );
		return (diff == 0);
	}

	/**
	 * Securely zero memory, avoiding compiler optimizations and such
	 */
	static void burn(void *ptr,unsigned int len);

	/**
	 * @param n Number to convert
	 * @param s Buffer, at least 24 bytes in size
	 * @return String containing 'n' in base 10 form
	 */
	static char *decimal(unsigned long n,char s[24]);

	static inline char *hex(uint64_t i,char s[17])
	{
		s[0] = HEXCHARS[(i >> 60) & 0xf];
		s[1] = HEXCHARS[(i >> 56) & 0xf];
		s[2] = HEXCHARS[(i >> 52) & 0xf];
		s[3] = HEXCHARS[(i >> 48) & 0xf];
		s[4] = HEXCHARS[(i >> 44) & 0xf];
		s[5] = HEXCHARS[(i >> 40) & 0xf];
		s[6] = HEXCHARS[(i >> 36) & 0xf];
		s[7] = HEXCHARS[(i >> 32) & 0xf];
		s[8] = HEXCHARS[(i >> 28) & 0xf];
		s[9] = HEXCHARS[(i >> 24) & 0xf];
		s[10] = HEXCHARS[(i >> 20) & 0xf];
		s[11] = HEXCHARS[(i >> 16) & 0xf];
		s[12] = HEXCHARS[(i >> 12) & 0xf];
		s[13] = HEXCHARS[(i >> 8) & 0xf];
		s[14] = HEXCHARS[(i >> 4) & 0xf];
		s[15] = HEXCHARS[i & 0xf];
		s[16] = (char)0;
		return s;
	}

	static inline char *hex10(uint64_t i,char s[11])
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

	static inline char *hex(uint32_t i,char s[9])
	{
		s[0] = HEXCHARS[(i >> 28) & 0xf];
		s[1] = HEXCHARS[(i >> 24) & 0xf];
		s[2] = HEXCHARS[(i >> 20) & 0xf];
		s[3] = HEXCHARS[(i >> 16) & 0xf];
		s[4] = HEXCHARS[(i >> 12) & 0xf];
		s[5] = HEXCHARS[(i >> 8) & 0xf];
		s[6] = HEXCHARS[(i >> 4) & 0xf];
		s[7] = HEXCHARS[i & 0xf];
		s[8] = (char)0;
		return s;
	}

	static inline char *hex(uint16_t i,char s[5])
	{
		s[0] = HEXCHARS[(i >> 12) & 0xf];
		s[1] = HEXCHARS[(i >> 8) & 0xf];
		s[2] = HEXCHARS[(i >> 4) & 0xf];
		s[3] = HEXCHARS[i & 0xf];
		s[4] = (char)0;
		return s;
	}

	static inline char *hex(uint8_t i,char s[3])
	{
		s[0] = HEXCHARS[(i >> 4) & 0xf];
		s[1] = HEXCHARS[i & 0xf];
		s[2] = (char)0;
		return s;
	}

	static inline char *hex(const void *d,unsigned int l,char *s)
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

	static inline unsigned int unhex(const char *h,void *buf,unsigned int buflen)
	{
		unsigned int l = 0;
		while (l < buflen) {
			uint8_t hc = *(reinterpret_cast<const uint8_t *>(h++));
			if (!hc) break;

			uint8_t c = 0;
			if ((hc >= 48)&&(hc <= 57)) // 0..9
				c = hc - 48;
			else if ((hc >= 97)&&(hc <= 102)) // a..f
				c = hc - 87;
			else if ((hc >= 65)&&(hc <= 70)) // A..F
				c = hc - 55;

			hc = *(reinterpret_cast<const uint8_t *>(h++));
			if (!hc) break;

			c <<= 4;
			if ((hc >= 48)&&(hc <= 57))
				c |= hc - 48;
			else if ((hc >= 97)&&(hc <= 102))
				c |= hc - 87;
			else if ((hc >= 65)&&(hc <= 70))
				c |= hc - 55;

			reinterpret_cast<uint8_t *>(buf)[l++] = c;
		}
		return l;
	}

	static inline unsigned int unhex(const char *h,unsigned int hlen,void *buf,unsigned int buflen)
	{
		unsigned int l = 0;
		const char *hend = h + hlen;
		while (l < buflen) {
			if (h == hend) break;
			uint8_t hc = *(reinterpret_cast<const uint8_t *>(h++));
			if (!hc) break;

			uint8_t c = 0;
			if ((hc >= 48)&&(hc <= 57))
				c = hc - 48;
			else if ((hc >= 97)&&(hc <= 102))
				c = hc - 87;
			else if ((hc >= 65)&&(hc <= 70))
				c = hc - 55;

			if (h == hend) break;
			hc = *(reinterpret_cast<const uint8_t *>(h++));
			if (!hc) break;

			c <<= 4;
			if ((hc >= 48)&&(hc <= 57))
				c |= hc - 48;
			else if ((hc >= 97)&&(hc <= 102))
				c |= hc - 87;
			else if ((hc >= 65)&&(hc <= 70))
				c |= hc - 55;

			reinterpret_cast<uint8_t *>(buf)[l++] = c;
		}
		return l;
	}

	static inline float normalize(float value, float bigMin, float bigMax, float targetMin, float targetMax)
	{
		float bigSpan = bigMax - bigMin;
		float smallSpan = targetMax - targetMin;
		float valueScaled = (value - bigMin) / bigSpan;
		return targetMin + valueScaled * smallSpan;
	}

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
	 * Tokenize a string (alias for strtok_r or strtok_s depending on platform)
	 *
	 * @param str String to split
	 * @param delim Delimiters
	 * @param saveptr Pointer to a char * for temporary reentrant storage
	 */
	static inline char *stok(char *str,const char *delim,char **saveptr)
	{
#ifdef __WINDOWS__
		return strtok_s(str,delim,saveptr);
#else
		return strtok_r(str,delim,saveptr);
#endif
	}

	static inline unsigned int strToUInt(const char *s) { return (unsigned int)strtoul(s,(char **)0,10); }
	static inline int strToInt(const char *s) { return (int)strtol(s,(char **)0,10); }
	static inline unsigned long strToULong(const char *s) { return strtoul(s,(char **)0,10); }
	static inline long strToLong(const char *s) { return strtol(s,(char **)0,10); }
	static inline double strToDouble(const char *s) { return strtod(s,NULL); }
	static inline unsigned long long strToU64(const char *s)
	{
#ifdef __WINDOWS__
		return (unsigned long long)_strtoui64(s,(char **)0,10);
#else
		return strtoull(s,(char **)0,10);
#endif
	}
	static inline long long strTo64(const char *s)
	{
#ifdef __WINDOWS__
		return (long long)_strtoi64(s,(char **)0,10);
#else
		return strtoll(s,(char **)0,10);
#endif
	}
	static inline unsigned int hexStrToUInt(const char *s) { return (unsigned int)strtoul(s,(char **)0,16); }
	static inline int hexStrToInt(const char *s) { return (int)strtol(s,(char **)0,16); }
	static inline unsigned long hexStrToULong(const char *s) { return strtoul(s,(char **)0,16); }
	static inline long hexStrToLong(const char *s) { return strtol(s,(char **)0,16); }
	static inline unsigned long long hexStrToU64(const char *s)
	{
#ifdef __WINDOWS__
		return (unsigned long long)_strtoui64(s,(char **)0,16);
#else
		return strtoull(s,(char **)0,16);
#endif
	}
	static inline long long hexStrTo64(const char *s)
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
	static inline bool scopy(char *dest,unsigned int len,const char *src)
	{
		if (!len)
			return false; // sanity check
		if (!src) {
			*dest = (char)0;
			return true;
		}
		char *end = dest + len;
		while ((*dest++ = *src++)) {
			if (dest == end) {
				*(--dest) = (char)0;
				return false;
			}
		}
		return true;
	}

	/**
	 * Count the number of bits set in an integer
	 *
	 * @param v 32-bit integer
	 * @return Number of bits set in this integer (0-32)
	 */
	static inline uint32_t countBits(uint32_t v)
	{
		v = v - ((v >> 1) & (uint32_t)0x55555555);
		v = (v & (uint32_t)0x33333333) + ((v >> 2) & (uint32_t)0x33333333);
		return ((((v + (v >> 4)) & (uint32_t)0xF0F0F0F) * (uint32_t)0x1010101) >> 24);
	}

	/**
	 * Count the number of bits set in an integer
	 *
	 * @param v 64-bit integer
	 * @return Number of bits set in this integer (0-64)
	 */
	static inline uint64_t countBits(uint64_t v)
	{
		v = v - ((v >> 1) & (uint64_t)~(uint64_t)0/3);
		v = (v & (uint64_t)~(uint64_t)0/15*3) + ((v >> 2) & (uint64_t)~(uint64_t)0/15*3);
		v = (v + (v >> 4)) & (uint64_t)~(uint64_t)0/255*15;
		return (uint64_t)(v * ((uint64_t)~(uint64_t)0/255)) >> 56;
	}

	/**
	 * Check if a memory buffer is all-zero
	 *
	 * @param p Memory to scan
	 * @param len Length of memory
	 * @return True if memory is all zero
	 */
	static inline bool isZero(const void *p,unsigned int len)
	{
		for(unsigned int i=0;i<len;++i) {
			if (((const unsigned char *)p)[i])
				return false;
		}
		return true;
	}

	// Byte swappers for big/little endian conversion
	static inline uint8_t hton(uint8_t n) { return n; }
	static inline int8_t hton(int8_t n) { return n; }
	static inline uint16_t hton(uint16_t n) { return htons(n); }
	static inline int16_t hton(int16_t n) { return (int16_t)htons((uint16_t)n); }
	static inline uint32_t hton(uint32_t n) { return htonl(n); }
	static inline int32_t hton(int32_t n) { return (int32_t)htonl((uint32_t)n); }
	static inline uint64_t hton(uint64_t n)
	{
#if __BYTE_ORDER == __LITTLE_ENDIAN
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
#else
		return n;
#endif
	}
	static inline int64_t hton(int64_t n) { return (int64_t)hton((uint64_t)n); }

	static inline uint8_t ntoh(uint8_t n) { return n; }
	static inline int8_t ntoh(int8_t n) { return n; }
	static inline uint16_t ntoh(uint16_t n) { return ntohs(n); }
	static inline int16_t ntoh(int16_t n) { return (int16_t)ntohs((uint16_t)n); }
	static inline uint32_t ntoh(uint32_t n) { return ntohl(n); }
	static inline int32_t ntoh(int32_t n) { return (int32_t)ntohl((uint32_t)n); }
	static inline uint64_t ntoh(uint64_t n)
	{
#if __BYTE_ORDER == __LITTLE_ENDIAN
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
#else
		return n;
#endif
	}
	static inline int64_t ntoh(int64_t n) { return (int64_t)ntoh((uint64_t)n); }

	/**
	 * Hexadecimal characters 0-f
	 */
	static const char HEXCHARS[16];
};

} // namespace ZeroTier

#endif
