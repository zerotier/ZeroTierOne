/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
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

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ZT_CONST_TO_BE_UINT16(x) ((uint16_t)((uint16_t)((uint16_t)(x) << 8U) | (uint16_t)((uint16_t)(x) >> 8U)))
#define ZT_CONST_TO_BE_UINT64(x) ( \
  (((uint64_t)(x) & 0x00000000000000ffULL) << 56U) | \
  (((uint64_t)(x) & 0x000000000000ff00ULL) << 40U) | \
  (((uint64_t)(x) & 0x0000000000ff0000ULL) << 24U) | \
  (((uint64_t)(x) & 0x00000000ff000000ULL) <<  8U) | \
  (((uint64_t)(x) & 0x000000ff00000000ULL) >>  8U) | \
  (((uint64_t)(x) & 0x0000ff0000000000ULL) >> 24U) | \
  (((uint64_t)(x) & 0x00ff000000000000ULL) >> 40U) | \
  (((uint64_t)(x) & 0xff00000000000000ULL) >> 56U))
#else
#define ZT_CONST_TO_BE_UINT16(x) ((uint16_t)(x))
#define ZT_CONST_TO_BE_UINT64(x) ((uint64_t)(x))
#endif

#define ZT_ROR64(x, r) (((x) >> (r)) | ((x) << (64 - (r))))
#define ZT_ROL64(x, r) (((x) << (r)) | ((x) >> (64 - (r))))
#define ZT_ROR32(x, r) (((x) >> (r)) | ((x) << (32 - (r))))
#define ZT_ROL32(x, r) (((x) << (r)) | ((x) >> (32 - (r))))

namespace ZeroTier {

/**
 * Miscellaneous utility functions and global constants
 */
class Utils
{
public:
	static const uint64_t ZERO256[4];

#ifdef ZT_ARCH_ARM_HAS_NEON
	struct ARMCapabilities
	{
		ARMCapabilities() noexcept;

		bool aes;
		bool crc32;
		bool pmull;
		bool sha1;
		bool sha2;
	};
	static const ARMCapabilities ARMCAP;
#endif

#ifdef ZT_ARCH_X64
	struct CPUIDRegisters
	{
		CPUIDRegisters() noexcept;

		bool rdrand;
		bool aes;
		bool avx;
		bool vaes; // implies AVX
		bool vpclmulqdq; // implies AVX
		bool avx2;
		bool avx512f;
		bool sha;
		bool fsrm;
	};
	static const CPUIDRegisters CPUID;
#endif

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

	/**
	 * Unconditionally swap bytes regardless of host byte order
	 *
	 * @param n Integer to swap
	 * @return Integer with bytes reversed
	 */
	static ZT_INLINE uint16_t swapBytes(const uint16_t n) noexcept
	{
	#if defined(__GNUC__)
		return __builtin_bswap16(n);
	#else
	#ifdef _MSC_VER
		return (uint16_t)_byteswap_ushort((unsigned short)n);
	#else
		return htons(n);
	#endif
	#endif
	}

	// These are helper adapters to load and swap integer types special cased by size
	// to work with all typedef'd variants, signed/unsigned, etc.
	template< typename I, unsigned int S >
	class _swap_bytes_bysize;

	template< typename I >
	class _swap_bytes_bysize< I, 1 >
	{
	public:
		static ZT_INLINE I s(const I n) noexcept
		{ return n; }
	};

	template< typename I >
	class _swap_bytes_bysize< I, 2 >
	{
	public:
		static ZT_INLINE I s(const I n) noexcept
		{ return (I)swapBytes((uint16_t)n); }
	};

	template< typename I >
	class _swap_bytes_bysize< I, 4 >
	{
	public:
		static ZT_INLINE I s(const I n) noexcept
		{ return (I)swapBytes((uint32_t)n); }
	};

	template< typename I >
	class _swap_bytes_bysize< I, 8 >
	{
	public:
		static ZT_INLINE I s(const I n) noexcept
		{ return (I)swapBytes((uint64_t)n); }
	};

	template< typename I, unsigned int S >
	class _load_be_bysize;

	template< typename I >
	class _load_be_bysize< I, 1 >
	{
	public:
		static ZT_INLINE I l(const uint8_t *const p) noexcept
		{ return p[0]; }
	};

	template< typename I >
	class _load_be_bysize< I, 2 >
	{
	public:
		static ZT_INLINE I l(const uint8_t *const p) noexcept
		{ return (I)(((unsigned int)p[0] << 8U) | (unsigned int)p[1]); }
	};

	template< typename I >
	class _load_be_bysize< I, 4 >
	{
	public:
		static ZT_INLINE I l(const uint8_t *const p) noexcept
		{ return (I)(((uint32_t)p[0] << 24U) | ((uint32_t)p[1] << 16U) | ((uint32_t)p[2] << 8U) | (uint32_t)p[3]); }
	};

	template< typename I >
	class _load_be_bysize< I, 8 >
	{
	public:
		static ZT_INLINE I l(const uint8_t *const p) noexcept
		{ return (I)(((uint64_t)p[0] << 56U) | ((uint64_t)p[1] << 48U) | ((uint64_t)p[2] << 40U) | ((uint64_t)p[3] << 32U) | ((uint64_t)p[4] << 24U) | ((uint64_t)p[5] << 16U) | ((uint64_t)p[6] << 8U) | (uint64_t)p[7]); }
	};

	template< typename I, unsigned int S >
	class _load_le_bysize;

	template< typename I >
	class _load_le_bysize< I, 1 >
	{
	public:
		static ZT_INLINE I l(const uint8_t *const p) noexcept
		{ return p[0]; }
	};

	template< typename I >
	class _load_le_bysize< I, 2 >
	{
	public:
		static ZT_INLINE I l(const uint8_t *const p) noexcept
		{ return (I)((unsigned int)p[0] | ((unsigned int)p[1] << 8U)); }
	};

	template< typename I >
	class _load_le_bysize< I, 4 >
	{
	public:
		static ZT_INLINE I l(const uint8_t *const p) noexcept
		{ return (I)((uint32_t)p[0] | ((uint32_t)p[1] << 8U) | ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U)); }
	};

	template< typename I >
	class _load_le_bysize< I, 8 >
	{
	public:
		static ZT_INLINE I l(const uint8_t *const p) noexcept
		{ return (I)((uint64_t)p[0] | ((uint64_t)p[1] << 8U) | ((uint64_t)p[2] << 16U) | ((uint64_t)p[3] << 24U) | ((uint64_t)p[4] << 32U) | ((uint64_t)p[5] << 40U) | ((uint64_t)p[6] << 48U) | ((uint64_t)p[7]) << 56U); }
	};

	/**
	 * Convert any signed or unsigned integer type to big-endian ("network") byte order
	 *
	 * @tparam I Integer type (usually inferred)
	 * @param n Value to convert
	 * @return Value in big-endian order
	 */
	template< typename I >
	static ZT_INLINE I hton(const I n) noexcept
	{
	#if __BYTE_ORDER == __LITTLE_ENDIAN
		return _swap_bytes_bysize< I, sizeof(I) >::s(n);
	#else
		return n;
	#endif
	}

	/**
	 * Convert any signed or unsigned integer type to host byte order from big-endian ("network") byte order
	 *
	 * @tparam I Integer type (usually inferred)
	 * @param n Value to convert
	 * @return Value in host byte order
	 */
	template< typename I >
	static ZT_INLINE I ntoh(const I n) noexcept
	{
	#if __BYTE_ORDER == __LITTLE_ENDIAN
		return _swap_bytes_bysize< I, sizeof(I) >::s(n);
	#else
		return n;
	#endif
	}

	/**
	 * Copy bits from memory into an integer type without modifying their order
	 *
	 * @tparam I Type to load
	 * @param p Byte stream, must be at least sizeof(I) in size
	 * @return Loaded raw integer
	 */
	template< typename I >
	static ZT_INLINE I loadMachineEndian(const void *const p) noexcept
	{
	#ifdef ZT_NO_UNALIGNED_ACCESS
		I tmp;
		for(int i=0;i<(int)sizeof(I);++i)
			reinterpret_cast<uint8_t *>(&tmp)[i] = reinterpret_cast<const uint8_t *>(p)[i];
		return tmp;
	#else
		return *reinterpret_cast<const I *>(p);
	#endif
	}

	/**
	 * Copy bits from memory into an integer type without modifying their order
	 *
	 * @tparam I Type to store
	 * @param p Byte array (must be at least sizeof(I))
	 * @param i Integer to store
	 */
	template< typename I >
	static ZT_INLINE void storeMachineEndian(void *const p, const I i) noexcept
	{
	#ifdef ZT_NO_UNALIGNED_ACCESS
		for(unsigned int k=0;k<sizeof(I);++k)
			reinterpret_cast<uint8_t *>(p)[k] = reinterpret_cast<const uint8_t *>(&i)[k];
	#else
		*reinterpret_cast<I *>(p) = i;
	#endif
	}

	/**
	 * Decode a big-endian value from a byte stream
	 *
	 * @tparam I Type to decode (should be unsigned e.g. uint32_t or uint64_t)
	 * @param p Byte stream, must be at least sizeof(I) in size
	 * @return Decoded integer
	 */
	template< typename I >
	static ZT_INLINE I loadBigEndian(const void *const p) noexcept
	{
	#ifdef ZT_NO_UNALIGNED_ACCESS
		return _load_be_bysize<I,sizeof(I)>::l(reinterpret_cast<const uint8_t *>(p));
	#else
		return ntoh(*reinterpret_cast<const I *>(p));
	#endif
	}

	/**
	 * Save an integer in big-endian format
	 *
	 * @tparam I Integer type to store (usually inferred)
	 * @param p Byte stream to write (must be at least sizeof(I))
	 * #param i Integer to write
	 */
	template< typename I >
	static ZT_INLINE void storeBigEndian(void *const p, I i) noexcept
	{
	#ifdef ZT_NO_UNALIGNED_ACCESS
		storeMachineEndian(p,hton(i));
	#else
		*reinterpret_cast<I *>(p) = hton(i);
	#endif
	}

	/**
	 * Decode a little-endian value from a byte stream
	 *
	 * @tparam I Type to decode
	 * @param p Byte stream, must be at least sizeof(I) in size
	 * @return Decoded integer
	 */
	template< typename I >
	static ZT_INLINE I loadLittleEndian(const void *const p) noexcept
	{
	#if __BYTE_ORDER == __BIG_ENDIAN || defined(ZT_NO_UNALIGNED_ACCESS)
		return _load_le_bysize<I,sizeof(I)>::l(reinterpret_cast<const uint8_t *>(p));
	#else
		return *reinterpret_cast<const I *>(p);
	#endif
	}

	/**
	 * Save an integer in little-endian format
	 *
	 * @tparam I Integer type to store (usually inferred)
	 * @param p Byte stream to write (must be at least sizeof(I))
	 * #param i Integer to write
	 */
	template< typename I >
	static ZT_INLINE void storeLittleEndian(void *const p, const I i) noexcept
	{
	#if __BYTE_ORDER == __BIG_ENDIAN
		storeMachineEndian(p,_swap_bytes_bysize<I,sizeof(I)>::s(i));
	#else
	#ifdef ZT_NO_UNALIGNED_ACCESS
		storeMachineEndian(p,i);
	#else
		*reinterpret_cast<I *>(p) = i;
	#endif
	#endif
	}

	/**
	 * Copy memory block whose size is known at compile time.
	 *
	 * @tparam L Size of memory
	 * @param dest Destination memory
	 * @param src Source memory
	 */
	template< unsigned long L >
	static ZT_INLINE void copy(void *dest, const void *src) noexcept
	{
	#if defined(ZT_ARCH_X64) && defined(__GNUC__)
		uintptr_t l = L;
		__asm__ __volatile__ ("cld ; rep movsb" : "+c"(l), "+S"(src), "+D"(dest) :: "memory");
	#else
		memcpy(dest, src, L);
	#endif
	}

	/**
	 * Copy memory block whose size is known at run time
	 *
	 * @param dest Destination memory
	 * @param src Source memory
	 * @param len Bytes to copy
	 */
	static ZT_INLINE void copy(void *dest, const void *src, unsigned long len) noexcept
	{
	#if defined(ZT_ARCH_X64) && defined(__GNUC__)
		__asm__ __volatile__ ("cld ; rep movsb" : "+c"(len), "+S"(src), "+D"(dest) :: "memory");
	#else
		memcpy(dest, src, len);
	#endif
	}

	/**
	 * Zero memory block whose size is known at compile time
	 *
	 * @tparam L Size in bytes
	 * @param dest Memory to zero
	 */
	template< unsigned long L >
	static ZT_INLINE void zero(void *dest) noexcept
	{
	#if defined(ZT_ARCH_X64) && defined(__GNUC__)
		uintptr_t l = L;
		__asm__ __volatile__ ("cld ; rep stosb" :"+c" (l), "+D" (dest) : "a" (0) : "memory");
	#else
		memset(dest, 0, L);
	#endif
	}

	/**
	 * Zero memory block whose size is known at run time
	 *
	 * @param dest Memory to zero
	 * @param len Size in bytes
	 */
	static ZT_INLINE void zero(void *dest, unsigned long len) noexcept
	{
	#if defined(ZT_ARCH_X64) && defined(__GNUC__)
		__asm__ __volatile__ ("cld ; rep stosb" :"+c" (len), "+D" (dest) : "a" (0) : "memory");
	#else
		memset(dest, 0, len);
	#endif
	}

	/**
	 * Hexadecimal characters 0-f
	 */
	static const char HEXCHARS[16];
};

} // namespace ZeroTier

#endif
