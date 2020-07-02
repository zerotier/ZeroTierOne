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

#include <utility>
#include <algorithm>
#include <memory>
#include <stdint.h>
#include <stddef.h>

namespace ZeroTier {

namespace Utils {

#ifndef __WINDOWS__

#include <sys/mman.h>

#endif

// Macros to convert endian-ness at compile time for constants.
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
extern const CPUIDRegisters CPUID;
#endif

extern const std::bad_alloc BadAllocException;
extern const std::out_of_range OutOfRangeException;

/**
 * 256 zero bits / 32 zero bytes
 */
extern const uint64_t ZERO256[4];

/**
 * Hexadecimal characters 0-f
 */
extern const char HEXCHARS[16];

/**
 * A random integer generated at startup for Map's hash bucket calculation.
 */
extern const uint64_t s_mapNonce;

/**
 * Lock memory to prevent swapping out to secondary storage (if possible)
 *
 * This is used to attempt to prevent the swapping out of long-term stored secure
 * credentials like secret keys. It isn't supported on all platforms and may not
 * be absolutely guaranteed to work, but it's a countermeasure.
 *
 * @param p Memory to lock
 * @param l Size of memory
 */
static ZT_INLINE void memoryLock(const void *const p, const unsigned int l) noexcept
{
#ifdef __WINDOWS__
	//VirtualLock(p, l);
#else
	mlock(p, l);
#endif
}

/**
 * Unlock memory locked with memoryLock()
 *
 * @param p Memory to unlock
 * @param l Size of memory
 */
static ZT_INLINE void memoryUnlock(const void *const p, const unsigned int l) noexcept
{
#ifdef __WINDOWS__
	//VirtualUnlock(p, l);
#else
	munlock(p, l);
#endif
}

/**
 * Perform a time-invariant binary comparison
 *
 * @param a First binary string
 * @param b Second binary string
 * @param len Length of strings
 * @return True if strings are equal
 */
bool secureEq(const void *a, const void *b, unsigned int len) noexcept;

/**
 * Be absolutely sure to zero memory
 *
 * This uses some hacks to be totally sure the compiler does not optimize it out.
 *
 * @param ptr Memory to zero
 * @param len Length of memory in bytes
 */
void burn(void *ptr, unsigned int len);

/**
 * @param n Number to convert
 * @param s Buffer, at least 24 bytes in size
 * @return String containing 'n' in base 10 form
 */
char *decimal(unsigned long n, char s[24]) noexcept;

/**
 * Convert an unsigned integer into hex
 *
 * @param i Any unsigned integer
 * @param s Buffer to receive hex, must be at least (2*sizeof(i))+1 in size or overflow will occur.
 * @return Pointer to s containing hex string with trailing zero byte
 */
char *hex(uint64_t i, char buf[17]) noexcept;

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
char *hex(const void *d, unsigned int l, char *s) noexcept;

/**
 * Decode a hex string
 *
 * @param h Hex C-string (non hex chars are ignored)
 * @param hlen Maximum length of string (will stop at terminating zero)
 * @param buf Output buffer
 * @param buflen Length of output buffer
 * @return Number of written bytes
 */
unsigned int unhex(const char *h, unsigned int hlen, void *buf, unsigned int buflen) noexcept;

/**
 * Generate secure random bytes
 *
 * This will try to use whatever OS sources of entropy are available. It's
 * guarded by an internal mutex so it's thread-safe.
 *
 * @param buf Buffer to fill
 * @param bytes Number of random bytes to generate
 */
void getSecureRandom(void *buf, unsigned int bytes) noexcept;

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
int b32e(const uint8_t *data, int length, char *result, int bufSize) noexcept;

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
 * Get a non-cryptographic random integer.
 *
 * This should never be used for cryptographic use cases, not even for choosing
 * message nonce/IV values if they should not repeat. It should only be used when
 * a fast and potentially "dirty" random source is needed.
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
bool scopy(char *dest, unsigned int len, const char *src) noexcept;

/**
 * Mix bits in a 64-bit integer (non-cryptographic, for hash tables)
 *
 * https://nullprogram.com/blog/2018/07/31/
 *
 * @param x Integer to mix
 * @return Hashed value
 */
static ZT_INLINE uint64_t hash64(uint64_t x) noexcept
{
	x ^= x >> 30U;
	x *= 0xbf58476d1ce4e5b9ULL;
	x ^= x >> 27U;
	x *= 0x94d049bb133111ebULL;
	x ^= x >> 31U;
	return x;
}

/**
 * Mix bits in a 32-bit integer (non-cryptographic, for hash tables)
 *
 * https://nullprogram.com/blog/2018/07/31/
 *
 * @param x Integer to mix
 * @return Hashed value
 */
static ZT_INLINE uint32_t hash32(uint32_t x) noexcept
{
	x ^= x >> 16U;
	x *= 0x7feb352dU;
	x ^= x >> 15U;
	x *= 0x846ca68bU;
	x ^= x >> 16U;
	return x;
}

/**
 * Check if a buffer's contents are all zero
 */
static ZT_INLINE bool allZero(const void *const b, unsigned int l) noexcept
{
	const uint8_t *p = reinterpret_cast<const uint8_t *>(b);

#ifndef ZT_NO_UNALIGNED_ACCESS
	while (l >= 8) {
		if (*reinterpret_cast<const uint64_t *>(p) != 0)
			return false;
		p += 8;
		l -= 8;
	}
#endif

	for (unsigned int i = 0; i < l; ++i) {
		if (reinterpret_cast<const uint8_t *>(p)[i] != 0)
			return false;
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
static ZT_INLINE char *stok(char *str, const char *delim, char **saveptr) noexcept
{
#ifdef __WINDOWS__
	return strtok_s(str,delim,saveptr);
#else
	return strtok_r(str, delim, saveptr);
#endif
}

static ZT_INLINE unsigned int strToUInt(const char *s) noexcept
{ return (unsigned int)strtoul(s, nullptr, 10); }

static ZT_INLINE unsigned long long hexStrToU64(const char *s) noexcept
{
#ifdef __WINDOWS__
	return (unsigned long long)_strtoui64(s,nullptr,16);
#else
	return strtoull(s, nullptr, 16);
#endif
}

/**
 * Compute 32-bit FNV-1a checksum
 *
 * See: http://www.isthe.com/chongo/tech/comp/fnv/
 *
 * @param data Data to checksum
 * @param len Length of data
 * @return FNV1a checksum
 */
static ZT_INLINE uint32_t fnv1a32(const void *const data, const unsigned int len) noexcept
{
	uint32_t h = 0x811c9dc5;
	const uint32_t p = 0x01000193;
	for (unsigned int i = 0; i < len; ++i)
		h = (h ^ (uint32_t)reinterpret_cast<const uint8_t *>(data)[i]) * p;
	return h;
}

#ifdef __GNUC__

static ZT_INLINE unsigned int countBits(const uint8_t v) noexcept
{ return (unsigned int)__builtin_popcount((unsigned int)v); }

static ZT_INLINE unsigned int countBits(const uint16_t v) noexcept
{ return (unsigned int)__builtin_popcount((unsigned int)v); }

static ZT_INLINE unsigned int countBits(const uint32_t v) noexcept
{ return (unsigned int)__builtin_popcountl((unsigned long)v); }

static ZT_INLINE unsigned int countBits(const uint64_t v) noexcept
{ return (unsigned int)__builtin_popcountll((unsigned long long)v); }

#else
template<typename T>
static ZT_INLINE unsigned int countBits(T v) noexcept
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
static ZT_INLINE uint64_t swapBytes(const uint64_t n) noexcept
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
static ZT_INLINE uint32_t swapBytes(const uint32_t n) noexcept
{
#if defined(__GNUC__)
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

/*
 * Note on copy() and zero():
 *
 * On X64, rep/movsb and rep/stosb are almost always faster for small memory
 * regions on all but the oldest microarchitectures (and even there the
 * difference is not large). While more aggressive memcpy() implementations
 * may be faster in micro-benchmarks, these fail to account for real world
 * context such as instruction cache and pipeline pressure. A simple
 * instruction like rep/movsb takes up only a few spots in caches and pipelines
 * and requires no branching or function calls. Specialized memcpy() can still
 * be faster for large memory regions, but ZeroTier doesn't copy anything
 * much larger than 16KiB.
 *
 * A templated version for statically known sizes is provided since this can
 * allow some nice optimizations in some cases.
 */

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
	asm volatile ("cld ; rep movsb" : "+c"(l), "+S"(src), "+D"(dest));
#else
	memcpy(dest, src, L);
#endif
}

// Avoid rep/movsb startup time for some small common sizes.
template<>
ZT_INLINE void copy<4>(void *dest, const void *src) noexcept
{
	*reinterpret_cast<uint32_t *>(dest) = *reinterpret_cast<const uint32_t *>(src);
}
template<>
ZT_INLINE void copy<8>(void *dest, const void *src) noexcept
{
	*reinterpret_cast<uint64_t *>(dest) = *reinterpret_cast<const uint64_t *>(src);
}
template<>
ZT_INLINE void copy<12>(void *dest, const void *src) noexcept
{
	*reinterpret_cast<uint64_t *>(dest) = *reinterpret_cast<const uint64_t *>(src);
	*reinterpret_cast<uint32_t *>(reinterpret_cast<uint8_t *>(dest) + 8) = *reinterpret_cast<const uint32_t *>(reinterpret_cast<const uint8_t *>(src) + 8);
}
template<>
ZT_INLINE void copy<16>(void *dest, const void *src) noexcept
{
	*reinterpret_cast<uint64_t *>(dest) = *reinterpret_cast<const uint64_t *>(src);
	*reinterpret_cast<uint64_t *>(reinterpret_cast<uint8_t *>(dest) + 8) = *reinterpret_cast<const uint64_t *>(reinterpret_cast<const uint8_t *>(src) + 8);
}
template<>
ZT_INLINE void copy<24>(void *dest, const void *src) noexcept
{
	*reinterpret_cast<uint64_t *>(dest) = *reinterpret_cast<const uint64_t *>(src);
	*reinterpret_cast<uint64_t *>(reinterpret_cast<uint8_t *>(dest) + 8) = *reinterpret_cast<const uint64_t *>(reinterpret_cast<const uint8_t *>(src) + 8);
	*reinterpret_cast<uint64_t *>(reinterpret_cast<uint8_t *>(dest) + 16) = *reinterpret_cast<const uint64_t *>(reinterpret_cast<const uint8_t *>(src) + 16);
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
	asm volatile ("cld ; rep movsb" : "+c"(len), "+S"(src), "+D"(dest));
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
	asm volatile ("cld ; rep stosb" :"+c" (l), "+D" (dest) : "a" (0));
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
	asm volatile ("cld ; rep stosb" :"+c" (len), "+D" (dest) : "a" (0));
#else
	memset(dest, 0, len);
#endif
}

} // namespace Utils

} // namespace ZeroTier

#endif
