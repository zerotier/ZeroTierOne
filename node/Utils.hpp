/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
	 * Perform a time-invariant binary comparison
	 *
	 * @param a First binary string
	 * @param b Second binary string
	 * @param len Length of strings
	 * @return True if strings are equal
	 */
	static inline bool secureEq(const void *a,const void *b,unsigned int len)
		throw()
	{
		uint8_t diff = 0;
		for(unsigned int i=0;i<len;++i)
			diff |= ( (reinterpret_cast<const uint8_t *>(a))[i] ^ (reinterpret_cast<const uint8_t *>(b))[i] );
		return (diff == 0);
	}

	/**
	 * Securely zero memory, avoiding compiler optimizations and such
	 */
	static void burn(void *ptr,unsigned int len)
		throw();

	/**
	 * Convert binary data to hexadecimal
	 *
	 * @param data Data to convert to hex
	 * @param len Length of data
	 * @return Hexadecimal string
	 */
	static std::string hex(const void *data,unsigned int len);
	static inline std::string hex(const std::string &data) { return hex(data.data(),(unsigned int)data.length()); }

	/**
	 * Convert hexadecimal to binary data
	 *
	 * This ignores all non-hex characters, just stepping over them and
	 * continuing. Upper and lower case are supported for letters a-f.
	 *
	 * @param hex Hexadecimal ASCII code (non-hex chars are ignored, stops at zero or maxlen)
	 * @param maxlen Maximum length of hex string buffer
	 * @return Binary data
	 */
	static std::string unhex(const char *hex,unsigned int maxlen);
	static inline std::string unhex(const std::string &hex) { return unhex(hex.c_str(),(unsigned int)hex.length()); }

	/**
	 * Convert hexadecimal to binary data
	 *
	 * This ignores all non-hex characters, just stepping over them and
	 * continuing. Upper and lower case are supported for letters a-f.
	 *
	 * @param hex Hexadecimal ASCII
	 * @param maxlen Maximum length of hex string buffer
	 * @param buf Buffer to fill
	 * @param len Length of buffer
	 * @return Number of characters actually written
	 */
	static unsigned int unhex(const char *hex,unsigned int maxlen,void *buf,unsigned int len);
	static inline unsigned int unhex(const std::string &hex,void *buf,unsigned int len) { return unhex(hex.c_str(),(unsigned int)hex.length(),buf,len); }

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
	 * Split a string by delimiter, with optional escape and quote characters
	 *
	 * @param s String to split
	 * @param sep One or more separators
	 * @param esc Zero or more escape characters
	 * @param quot Zero or more quote characters
	 * @return Vector of tokens
	 */
	static std::vector<std::string> split(const char *s,const char *const sep,const char *esc,const char *quot);

	/**
	 * Tokenize a string (alias for strtok_r or strtok_s depending on platform)
	 *
	 * @param str String to split
	 * @param delim Delimiters
	 * @param saveptr Pointer to a char * for temporary reentrant storage
	 */
	static inline char *stok(char *str,const char *delim,char **saveptr)
		throw()
	{
#ifdef __WINDOWS__
		return strtok_s(str,delim,saveptr);
#else
		return strtok_r(str,delim,saveptr);
#endif
	}

	// String to number converters -- defined here to permit portability
	// ifdefs for platforms that lack some of the strtoXX functions.
	static inline unsigned int strToUInt(const char *s)
		throw()
	{
		return (unsigned int)strtoul(s,(char **)0,10);
	}
	static inline int strToInt(const char *s)
		throw()
	{
		return (int)strtol(s,(char **)0,10);
	}
	static inline unsigned long strToULong(const char *s)
		throw()
	{
		return strtoul(s,(char **)0,10);
	}
	static inline long strToLong(const char *s)
		throw()
	{
		return strtol(s,(char **)0,10);
	}
	static inline unsigned long long strToU64(const char *s)
		throw()
	{
#ifdef __WINDOWS__
		return (unsigned long long)_strtoui64(s,(char **)0,10);
#else
		return strtoull(s,(char **)0,10);
#endif
	}
	static inline long long strTo64(const char *s)
		throw()
	{
#ifdef __WINDOWS__
		return (long long)_strtoi64(s,(char **)0,10);
#else
		return strtoll(s,(char **)0,10);
#endif
	}
	static inline unsigned int hexStrToUInt(const char *s)
		throw()
	{
		return (unsigned int)strtoul(s,(char **)0,16);
	}
	static inline int hexStrToInt(const char *s)
		throw()
	{
		return (int)strtol(s,(char **)0,16);
	}
	static inline unsigned long hexStrToULong(const char *s)
		throw()
	{
		return strtoul(s,(char **)0,16);
	}
	static inline long hexStrToLong(const char *s)
		throw()
	{
		return strtol(s,(char **)0,16);
	}
	static inline unsigned long long hexStrToU64(const char *s)
		throw()
	{
#ifdef __WINDOWS__
		return (unsigned long long)_strtoui64(s,(char **)0,16);
#else
		return strtoull(s,(char **)0,16);
#endif
	}
	static inline long long hexStrTo64(const char *s)
		throw()
	{
#ifdef __WINDOWS__
		return (long long)_strtoi64(s,(char **)0,16);
#else
		return strtoll(s,(char **)0,16);
#endif
	}
	static inline double strToDouble(const char *s)
		throw()
	{
		return strtod(s,(char **)0);
	}

	/**
	 * Perform a safe C string copy
	 *
	 * @param dest Destination buffer
	 * @param len Length of buffer
	 * @param src Source string
	 * @return True on success, false on overflow (buffer will still be 0-terminated)
	 */
	static inline bool scopy(char *dest,unsigned int len,const char *src)
		throw()
	{
		if (!len)
			return false; // sanity check
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
	 * Variant of snprintf that is portable and throws an exception
	 *
	 * This just wraps the local implementation whatever it's called, while
	 * performing a few other checks and adding exceptions for overflow.
	 *
	 * @param buf Buffer to write to
	 * @param len Length of buffer in bytes
	 * @param fmt Format string
	 * @param ... Format arguments
	 * @throws std::length_error buf[] too short (buf[] will still be left null-terminated)
	 */
	static unsigned int snprintf(char *buf,unsigned int len,const char *fmt,...)
		throw(std::length_error);

	/**
	 * Count the number of bits set in an integer
	 *
	 * @param v 32-bit integer
	 * @return Number of bits set in this integer (0-32)
	 */
	static inline uint32_t countBits(uint32_t v)
		throw()
	{
		v = v - ((v >> 1) & (uint32_t)0x55555555);
		v = (v & (uint32_t)0x33333333) + ((v >> 2) & (uint32_t)0x33333333);
		return ((((v + (v >> 4)) & (uint32_t)0xF0F0F0F) * (uint32_t)0x1010101) >> 24);
	}

	/**
	 * Check if a memory buffer is all-zero
	 *
	 * @param p Memory to scan
	 * @param len Length of memory
	 * @return True if memory is all zero
	 */
	static inline bool isZero(const void *p,unsigned int len)
		throw()
	{
		for(unsigned int i=0;i<len;++i) {
			if (((const unsigned char *)p)[i])
				return false;
		}
		return true;
	}

	// Byte swappers for big/little endian conversion
	static inline uint8_t hton(uint8_t n) throw() { return n; }
	static inline int8_t hton(int8_t n) throw() { return n; }
	static inline uint16_t hton(uint16_t n) throw() { return htons(n); }
	static inline int16_t hton(int16_t n) throw() { return (int16_t)htons((uint16_t)n); }
	static inline uint32_t hton(uint32_t n) throw() { return htonl(n); }
	static inline int32_t hton(int32_t n) throw() { return (int32_t)htonl((uint32_t)n); }
	static inline uint64_t hton(uint64_t n)
		throw()
	{
#if __BYTE_ORDER == __LITTLE_ENDIAN
#if defined(__GNUC__) && (!defined(__OpenBSD__))
		return __builtin_bswap64(n);
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
	static inline int64_t hton(int64_t n) throw() { return (int64_t)hton((uint64_t)n); }

	static inline uint8_t ntoh(uint8_t n) throw() { return n; }
	static inline int8_t ntoh(int8_t n) throw() { return n; }
	static inline uint16_t ntoh(uint16_t n) throw() { return ntohs(n); }
	static inline int16_t ntoh(int16_t n) throw() { return (int16_t)ntohs((uint16_t)n); }
	static inline uint32_t ntoh(uint32_t n) throw() { return ntohl(n); }
	static inline int32_t ntoh(int32_t n) throw() { return (int32_t)ntohl((uint32_t)n); }
	static inline uint64_t ntoh(uint64_t n)
		throw()
	{
#if __BYTE_ORDER == __LITTLE_ENDIAN
#if defined(__GNUC__) && !defined(__OpenBSD__)
		return __builtin_bswap64(n);
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
	static inline int64_t ntoh(int64_t n) throw() { return (int64_t)ntoh((uint64_t)n); }

	/**
	 * Compare Peer version tuples
	 *
	 * @return -1, 0, or 1 based on whether first tuple is less than, equal to, or greater than second
	 */
	static inline int compareVersion(unsigned int maj1,unsigned int min1,unsigned int rev1,unsigned int maj2,unsigned int min2,unsigned int rev2)
		throw()
	{
		if (maj1 > maj2)
			return 1;
		else if (maj1 < maj2)
			return -1;
		else {
			if (min1 > min2)
				return 1;
			else if (min1 < min2)
				return -1;
			else {
				if (rev1 > rev2)
					return 1;
				else if (rev1 < rev2)
					return -1;
				else return 0;
			}
		}
	}

	/**
	 * Hexadecimal characters 0-f
	 */
	static const char HEXCHARS[16];
};

} // namespace ZeroTier

#endif
