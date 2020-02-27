/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_UTILS_HPP
#define ZT_UTILS_HPP

#include "Constants.hpp"

#include <cstdlib>
#include <cstring>

namespace ZeroTier {

namespace Utils {

#ifdef ZT_ARCH_X64
struct CPUIDRegisters
{
	uint32_t eax,ebx,ecx,edx;
	bool rdrand;
	bool aes;
	CPUIDRegisters();
};
extern const CPUIDRegisters CPUID;
#endif

/**
 * 256 zero bits / 32 zero bytes
 */
extern const uint64_t ZERO256[4];

/**
 * Hexadecimal characters 0-f
 */
extern const char HEXCHARS[16];

/**
 * Perform a time-invariant binary comparison
 *
 * @param a First binary string
 * @param b Second binary string
 * @param len Length of strings
 * @return True if strings are equal
 */
bool secureEq(const void *a,const void *b,unsigned int len) noexcept;

/**
 * Be absolutely sure to zero memory
 *
 * This uses some hacks to be totally sure the compiler does not optimize it out.
 *
 * @param ptr Memory to zero
 * @param len Length of memory in bytes
 */
void burn(void *ptr,unsigned int len);

/**
 * @param n Number to convert
 * @param s Buffer, at least 24 bytes in size
 * @return String containing 'n' in base 10 form
 */
char *decimal(unsigned long n,char s[24]) noexcept;

/**
 * Convert an unsigned integer into hex
 *
 * @param i Any unsigned integer
 * @param s Buffer to receive hex, must be at least (2*sizeof(i))+1 in size or overflow will occur.
 * @return Pointer to s containing hex string with trailing zero byte
 */
char *hex(uint8_t i,char s[3]) noexcept;

/**
 * Convert an unsigned integer into hex
 *
 * @param i Any unsigned integer
 * @param s Buffer to receive hex, must be at least (2*sizeof(i))+1 in size or overflow will occur.
 * @return Pointer to s containing hex string with trailing zero byte
 */
char *hex(uint16_t i,char s[5]) noexcept;

/**
 * Convert an unsigned integer into hex
 *
 * @param i Any unsigned integer
 * @param s Buffer to receive hex, must be at least (2*sizeof(i))+1 in size or overflow will occur.
 * @return Pointer to s containing hex string with trailing zero byte
 */
char *hex(uint32_t i,char s[9]) noexcept;

/**
 * Convert an unsigned integer into hex
 *
 * @param i Any unsigned integer
 * @param s Buffer to receive hex, must be at least (2*sizeof(i))+1 in size or overflow will occur.
 * @return Pointer to s containing hex string with trailing zero byte
 */
char *hex(uint64_t i,char s[17]) noexcept;

/**
 * Decode an unsigned integer in hex format
 *
 * @param s String to decode, non-hex chars are ignored
 * @return Unsigned integer
 */
uint64_t unhex(const char *s) noexcept;

/**
 * Convert a byte array into hex
 *
 * @param d Bytes
 * @param l Length of bytes
 * @param s String buffer, must be at least (l*2)+1 in size or overflow will occur
 * @return Pointer to filled string buffer
 */
char *hex(const void *d,unsigned int l,char *s) noexcept;

/**
 * Decode a hex string
 *
 * @param h Hex C-string (non hex chars are ignored)
 * @param hlen Maximum length of string (will stop at terminating zero)
 * @param buf Output buffer
 * @param buflen Length of output buffer
 * @return Number of written bytes
 */
unsigned int unhex(const char *h,unsigned int hlen,void *buf,unsigned int buflen) noexcept;

/**
 * Generate secure random bytes
 *
 * This will try to use whatever OS sources of entropy are available. It's
 * guarded by an internal mutex so it's thread-safe.
 *
 * @param buf Buffer to fill
 * @param bytes Number of random bytes to generate
 */
void getSecureRandom(void *buf,unsigned int bytes) noexcept;

/**
 * @return Secure random 64-bit integer
 */
uint64_t getSecureRandomU64() noexcept;

/**
 * Encode string to base32
 *
 * @param data Binary data to encode
 * @param length Length of data in bytes
 * @param result Result buffer
 * @param bufSize Size of result buffer
 * @return Number of bytes written
 */
int b32e(const uint8_t *data,int length,char *result,int bufSize) noexcept;

/**
 * Decode base32 string
 *
 * @param encoded C-string in base32 format (non-base32 characters are ignored)
 * @param result Result buffer
 * @param bufSize Size of result buffer
 * @return Number of bytes written or -1 on error
 */
int b32d(const char *encoded, uint8_t *result, int bufSize) noexcept;

/**
 * Get a non-cryptographic random integer
 */
uint64_t random() noexcept;

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
bool scopy(char *dest,unsigned int len,const char *src) noexcept;

/**
 * Mix bits in a 64-bit integer
 *
 * https://nullprogram.com/blog/2018/07/31/
 *
 * @param x Integer to mix
 * @return Hashed value
 */
static ZT_ALWAYS_INLINE uint64_t hash64(uint64_t x) noexcept
{
	x ^= x >> 30U;
	x *= 0xbf58476d1ce4e5b9ULL;
	x ^= x >> 27U;
	x *= 0x94d049bb133111ebULL;
	x ^= x >> 31U;
	return x;
}

/**
 * @param b Buffer to check
 * @param l Length of buffer
 * @return True if buffer is all zero
 */
static ZT_ALWAYS_INLINE bool allZero(const void *const b,const unsigned int l) noexcept
{
	const uint8_t *x = reinterpret_cast<const uint8_t *>(b);
	const uint8_t *const y = x + l;
	while (x != y) {
		if (*x != 0)
			return false;
		++x;
	}
	return true;
}

/**
 * Wrapper around reentrant strtok functions, which differ in name by platform
 *
 * @param str String to tokenize or NULL for subsequent calls
 * @param delim Delimiter
 * @param saveptr Pointer to pointer where function can save state
 * @return Next token or NULL if none
 */
static ZT_ALWAYS_INLINE char *stok(char *str,const char *delim,char **saveptr) noexcept
{
#ifdef __WINDOWS__
	return strtok_s(str,delim,saveptr);
#else
	return strtok_r(str,delim,saveptr);
#endif
}

static ZT_ALWAYS_INLINE unsigned int strToUInt(const char *s) noexcept
{
	return (unsigned int)strtoul(s,nullptr,10);
}

static ZT_ALWAYS_INLINE unsigned long long hexStrToU64(const char *s) noexcept
{
#ifdef __WINDOWS__
	return (unsigned long long)_strtoui64(s,nullptr,16);
#else
	return strtoull(s,nullptr,16);
#endif
}

/**
 * Calculate a non-cryptographic hash of a byte string
 *
 * @param key Key to hash
 * @param len Length in bytes
 * @return Non-cryptographic hash suitable for use in a hash table
 */
static ZT_ALWAYS_INLINE unsigned long hashString(const void *restrict key,const unsigned int len) noexcept
{
	const uint8_t *p = reinterpret_cast<const uint8_t *>(key);
	unsigned long h = 0;
	for (unsigned int i=0;i<len;++i) {
		h += p[i];
		h += (h << 10U);
		h ^= (h >> 6U);
	}
	h += (h << 3U);
	h ^= (h >> 11U);
	h += (h << 15U);
	return h;
}

#ifdef __GNUC__
static ZT_ALWAYS_INLINE unsigned int countBits(const uint8_t v) noexcept { return (unsigned int)__builtin_popcount((unsigned int)v); }
static ZT_ALWAYS_INLINE unsigned int countBits(const uint16_t v) noexcept { return (unsigned int)__builtin_popcount((unsigned int)v); }
static ZT_ALWAYS_INLINE unsigned int countBits(const uint32_t v) noexcept { return (unsigned int)__builtin_popcountl((unsigned long)v); }
static ZT_ALWAYS_INLINE unsigned int countBits(const uint64_t v)  noexcept{ return (unsigned int)__builtin_popcountll((unsigned long long)v); }
#else
template<typename T>
static ZT_ALWAYS_INLINE unsigned int countBits(T v) noexcept
{
	v = v - ((v >> 1) & (T)~(T)0/3);
	v = (v & (T)~(T)0/15*3) + ((v >> 2) & (T)~(T)0/15*3);
	v = (v + (v >> 4)) & (T)~(T)0/255*15;
	return (unsigned int)((v * ((~((T)0))/((T)255))) >> ((sizeof(T) - 1) * 8));
}
#endif

/**
 * Unconditionally swap bytes regardless of host byte order
 *
 * @param n Integer to swap
 * @return Integer with bytes reversed
 */
static ZT_ALWAYS_INLINE uint64_t swapBytes(const uint64_t n) noexcept
{
#ifdef __GNUC__
	return __builtin_bswap64(n);
#else
#ifdef _MSC_VER
	return (uint64_t)_byteswap_uint64((unsigned __int64)n);
#else
	return (
		((n & 0x00000000000000ffULL) << 56) |
		((n & 0x000000000000ff00ULL) << 40) |
		((n & 0x0000000000ff0000ULL) << 24) |
		((n & 0x00000000ff000000ULL) <<  8) |
		((n & 0x000000ff00000000ULL) >>  8) |
		((n & 0x0000ff0000000000ULL) >> 24) |
		((n & 0x00ff000000000000ULL) >> 40) |
		((n & 0xff00000000000000ULL) >> 56)
	);
#endif
#endif
}

/**
 * Unconditionally swap bytes regardless of host byte order
 *
 * @param n Integer to swap
 * @return Integer with bytes reversed
 */
static ZT_ALWAYS_INLINE uint32_t swapBytes(const uint32_t n) noexcept
{
#if defined(__GCC__)
	return __builtin_bswap32(n);
#else
#ifdef _MSC_VER
	return (uint32_t)_byteswap_ulong((unsigned long)n);
#else
	return htonl(n);
#endif
#endif
}

/**
 * Unconditionally swap bytes regardless of host byte order
 *
 * @param n Integer to swap
 * @return Integer with bytes reversed
 */
static ZT_ALWAYS_INLINE uint16_t swapBytes(const uint16_t n) noexcept
{
#if defined(__GCC__)
	return __builtin_bswap16(n);
#else
#ifdef _MSC_VER
	return (uint16_t)_byteswap_ushort((unsigned short)n);
#else
	return htons(n);
#endif
#endif
}

// ZZvvvzvzvvzzz... moooooo... http://www.catb.org/~esr/jargon/html/Y/yak-shaving.html
#if __BYTE_ORDER == __LITTLE_ENDIAN
template<typename I,unsigned int S>
class _hton_impl { public: static ZT_ALWAYS_INLINE I hton(const I n) { return n; } };
template<typename I>
class _hton_impl<I,2> { public: static ZT_ALWAYS_INLINE I hton(const I n) { return (I)swapBytes((uint16_t)n); } };
template<typename I>
class _hton_impl<I,4> { public: static ZT_ALWAYS_INLINE I hton(const I n) { return (I)swapBytes((uint32_t)n); } };
template<typename I>
class _hton_impl<I,8> { public: static ZT_ALWAYS_INLINE I hton(const I n) { return (I)swapBytes((uint64_t)n); } };
#endif

/**
 * Convert any signed or unsigned integer type to big-endian ("network") byte order
 *
 * @tparam I Integer type (usually inferred)
 * @param n Value to convert
 * @return Value in big-endian order
 */
template<typename I>
static ZT_ALWAYS_INLINE I hton(const I n) noexcept
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return _hton_impl<I,sizeof(I)>::hton(n);
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
template<typename I>
static ZT_ALWAYS_INLINE I ntoh(const I n) noexcept
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return _hton_impl<I,sizeof(I)>::hton(n);
#else
	return n;
#endif
}

/**
 * Decode a big-endian value from a byte stream
 *
 * @tparam I Type to decode (should be unsigned e.g. uint32_t or uint64_t)
 * @param p Byte stream, must be at least sizeof(I) in size
 * @return Decoded integer
 */
template<typename I>
static ZT_ALWAYS_INLINE I loadBigEndian(const void *const p) noexcept
{
#ifdef ZT_NO_UNALIGNED_ACCESS
	if (sizeof(I) == 8) {
		return (I)(
			((uint64_t)reinterpret_cast<const uint8_t *>(p)[0] << 56U) |
			((uint64_t)reinterpret_cast<const uint8_t *>(p)[1] << 48U) |
			((uint64_t)reinterpret_cast<const uint8_t *>(p)[2] << 40U) |
			((uint64_t)reinterpret_cast<const uint8_t *>(p)[3] << 32U) |
			((uint64_t)reinterpret_cast<const uint8_t *>(p)[4] << 24U) |
			((uint64_t)reinterpret_cast<const uint8_t *>(p)[5] << 16U) |
			((uint64_t)reinterpret_cast<const uint8_t *>(p)[6] << 8U) |
			(uint64_t)reinterpret_cast<const uint8_t *>(p)[7]
		);
	} else if (sizeof(I) == 4) {
		return (I)(
			((uint32_t)reinterpret_cast<const uint8_t *>(p)[0] << 24U) |
			((uint32_t)reinterpret_cast<const uint8_t *>(p)[1] << 16U) |
			((uint32_t)reinterpret_cast<const uint8_t *>(p)[2] << 8U) |
			(uint32_t)reinterpret_cast<const uint8_t *>(p)[3]
		);
	} else if (sizeof(I) == 2) {
		return (I)(
			((unsigned int)reinterpret_cast<const uint8_t *>(p)[0] << 8U) |
			(unsigned int)reinterpret_cast<const uint8_t *>(p)[1]
		);
	} else {
		return (I)reinterpret_cast<const uint8_t *>(p)[0];
	}
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
template<typename I>
static ZT_ALWAYS_INLINE void storeBigEndian(void *const p,I i) noexcept
{
#ifdef ZT_NO_UNALIGNED_ACCESS
	if (sizeof(I) == 8) {
		reinterpret_cast<uint8_t *>(p)[0] = (uint8_t)(reinterpret_cast<uint64_t>(i) >> 56U);
		reinterpret_cast<uint8_t *>(p)[1] = (uint8_t)(reinterpret_cast<uint64_t>(i) >> 48U);
		reinterpret_cast<uint8_t *>(p)[2] = (uint8_t)(reinterpret_cast<uint64_t>(i) >> 40U);
		reinterpret_cast<uint8_t *>(p)[3] = (uint8_t)(reinterpret_cast<uint64_t>(i) >> 32U);
		reinterpret_cast<uint8_t *>(p)[4] = (uint8_t)(reinterpret_cast<uint64_t>(i) >> 24U);
		reinterpret_cast<uint8_t *>(p)[5] = (uint8_t)(reinterpret_cast<uint64_t>(i) >> 16U);
		reinterpret_cast<uint8_t *>(p)[6] = (uint8_t)(reinterpret_cast<uint64_t>(i) >> 8U);
		reinterpret_cast<uint8_t *>(p)[7] = (uint8_t)reinterpret_cast<uint64_t>(i);
	} else if (sizeof(I) == 4) {
		reinterpret_cast<uint8_t *>(p)[0] = (uint8_t)(reinterpret_cast<uint32_t>(i) >> 24U);
		reinterpret_cast<uint8_t *>(p)[1] = (uint8_t)(reinterpret_cast<uint32_t>(i) >> 16U);
		reinterpret_cast<uint8_t *>(p)[2] = (uint8_t)(reinterpret_cast<uint32_t>(i) >> 8U);
		reinterpret_cast<uint8_t *>(p)[3] = (uint8_t)reinterpret_cast<uint32_t>(i);
	} else if (sizeof(I) == 2) {
		reinterpret_cast<uint8_t *>(p)[0] = (uint8_t)(reinterpret_cast<uint16_t>(i) >> 8U);
		reinterpret_cast<uint8_t *>(p)[1] = (uint8_t)reinterpret_cast<uint16_t>(i);
	} else {
		reinterpret_cast<uint8_t *>(p)[0] = (uint8_t)i;
	}
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
template<typename I>
static ZT_ALWAYS_INLINE I loadLittleEndian(const void *const p) noexcept
{
#if __BYTE_ORDER == __BIG_ENDIAN || defined(ZT_NO_UNALIGNED_ACCESS)
	if (sizeof(I) == 8) {
		return (I)(
			(uint64_t)reinterpret_cast<const uint8_t *>(p)[0] |
			((uint64_t)reinterpret_cast<const uint8_t *>(p)[1] << 8U) |
			((uint64_t)reinterpret_cast<const uint8_t *>(p)[2] << 16U) |
			((uint64_t)reinterpret_cast<const uint8_t *>(p)[3] << 24U) |
			((uint64_t)reinterpret_cast<const uint8_t *>(p)[4] << 32U) |
			((uint64_t)reinterpret_cast<const uint8_t *>(p)[5] << 40U) |
			((uint64_t)reinterpret_cast<const uint8_t *>(p)[6] << 48U) |
			((uint64_t)reinterpret_cast<const uint8_t *>(p)[7] << 56U)
		);
	} else if (sizeof(I) == 4) {
		return (I)(
			(uint32_t)reinterpret_cast<const uint8_t *>(p)[0] |
			((uint32_t)reinterpret_cast<const uint8_t *>(p)[1] << 8U) |
			((uint32_t)reinterpret_cast<const uint8_t *>(p)[2] << 16U) |
			((uint32_t)reinterpret_cast<const uint8_t *>(p)[3] << 24U)
		);
	} else if (sizeof(I) == 2) {
		return (I)(
			(unsigned int)reinterpret_cast<const uint8_t *>(p)[0] |
			((unsigned int)reinterpret_cast<const uint8_t *>(p)[1] << 8U)
		);
	} else {
		return (I)reinterpret_cast<const uint8_t *>(p)[0];
	}
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
template<typename I>
static ZT_ALWAYS_INLINE void storeLittleEndian(void *const p,const I i) noexcept
{
#if __BYTE_ORDER == __BIG_ENDIAN || defined(ZT_NO_UNALIGNED_ACCESS)
	if (sizeof(I) == 8) {
		reinterpret_cast<uint8_t *>(p)[0] = (uint8_t)reinterpret_cast<uint64_t>(i);
		reinterpret_cast<uint8_t *>(p)[1] = (uint8_t)(reinterpret_cast<uint64_t>(i) >> 8U);
		reinterpret_cast<uint8_t *>(p)[2] = (uint8_t)(reinterpret_cast<uint64_t>(i) >> 16U);
		reinterpret_cast<uint8_t *>(p)[3] = (uint8_t)(reinterpret_cast<uint64_t>(i) >> 24U);
		reinterpret_cast<uint8_t *>(p)[4] = (uint8_t)(reinterpret_cast<uint64_t>(i) >> 32U);
		reinterpret_cast<uint8_t *>(p)[5] = (uint8_t)(reinterpret_cast<uint64_t>(i) >> 40U);
		reinterpret_cast<uint8_t *>(p)[6] = (uint8_t)(reinterpret_cast<uint64_t>(i) >> 48U);
		reinterpret_cast<uint8_t *>(p)[7] = (uint8_t)(reinterpret_cast<uint64_t>(i) >> 56U);
	} else if (sizeof(I) == 4) {
		reinterpret_cast<uint8_t *>(p)[0] = (uint8_t)reinterpret_cast<uint32_t>(i);
		reinterpret_cast<uint8_t *>(p)[1] = (uint8_t)(reinterpret_cast<uint32_t>(i) >> 8U);
		reinterpret_cast<uint8_t *>(p)[2] = (uint8_t)(reinterpret_cast<uint32_t>(i) >> 16U);
		reinterpret_cast<uint8_t *>(p)[3] = (uint8_t)(reinterpret_cast<uint32_t>(i) >> 24U);
	} else if (sizeof(I) == 2) {
		reinterpret_cast<uint8_t *>(p)[0] = (uint8_t)reinterpret_cast<uint16_t>(i);
		reinterpret_cast<uint8_t *>(p)[1] = (uint8_t)(reinterpret_cast<uint16_t>(i) >> 8U);
	} else {
		reinterpret_cast<uint8_t *>(p)[0] = (uint8_t)i;
	}
#else
	*reinterpret_cast<I *>(p) = i;
#endif
}

/**
 * Copy bits from memory into an integer type without modifying their order
 *
 * @tparam I Type to load
 * @param p Byte stream, must be at least sizeof(I) in size
 * @return Loaded raw integer
 */
template<typename I>
static ZT_ALWAYS_INLINE I loadAsIsEndian(const void *const p) noexcept
{
#ifdef ZT_NO_UNALIGNED_ACCESS
	I x = (I)0;
	for(unsigned int k=0;k<sizeof(I);++k)
		reinterpret_cast<uint8_t *>(&x)[k] = reinterpret_cast<const uint8_t *>(p)[k];
	return x;
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
template<typename I>
static ZT_ALWAYS_INLINE void storeAsIsEndian(void *const p,const I i) noexcept
{
#ifdef ZT_NO_UNALIGNED_ACCESS
	for(unsigned int k=0;k<sizeof(I);++k)
		reinterpret_cast<uint8_t *>(p)[k] = reinterpret_cast<const uint8_t *>(&i)[k];
#else
	*reinterpret_cast<I *>(p) = i;
#endif
}

} // namespace Utils

} // namespace ZeroTier

#endif
