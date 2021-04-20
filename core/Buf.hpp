/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_BUF_HPP
#define ZT_BUF_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "SharedPtr.hpp"
#include "Mutex.hpp"
#include "TriviallyCopyable.hpp"
#include "FCV.hpp"

#include <stdexcept>
#include <utility>
#include <algorithm>
#include <new>

// Buffers are 16384 bytes in size because this is the smallest size that can hold any packet
// and is a power of two. It needs to be a power of two because masking is significantly faster
// than integer division modulus.
#define ZT_BUF_MEM_SIZE 0x00004000
#define ZT_BUF_MEM_MASK 0x00003fffU

// Sanity limit on maximum buffer pool size
#define ZT_BUF_MAX_POOL_SIZE 1024

namespace ZeroTier {

/**
 * Buffer and methods for branch-free bounds-checked data assembly and parsing
 *
 * This implements an extremely fast buffer for packet assembly and parsing that avoids
 * branching whenever possible. To be safe it must be used correctly!
 *
 * The read methods are prefixed by 'r', and write methods with 'w'. All methods take
 * an iterator, which is just an int that should be initialized to 0 (or whatever starting
 * position is desired). All read methods will advance the iterator regardless of outcome.
 *
 * Read and write methods fail silently in the event of overflow. They do not corrupt or
 * access memory outside the bounds of Buf, but will otherwise produce undefined results.
 *
 * IT IS THE RESPONSIBILITY OF THE USER of this class to use the readOverflow() and
 * writeOverflow() static methods to check the iterator for overflow after each series
 * of reads and writes and BEFORE ANY PARSING or other decisions are made on the basis
 * of the data obtained from a buffer. Failure to do so can result in bugs due
 * to parsing and branching on undefined or corrupt data.
 *
 * ^^ THIS IS VERY IMPORTANT ^^
 *
 * A typical packet assembly consists of repeated calls to the write methods followed by
 * a check to writeOverflow() before final packet armoring and transport. A typical packet
 * disassembly and parsing consists of a series of read calls to obtain the packet's
 * fields followed by a call to readOverflow() to check that these fields are valid. The
 * packet is discarded if readOverflow() returns true. Some packet parsers may make
 * additional reads and in this case readOverflow() must be checked after each set of
 * reads to ensure that overflow did not occur.
 *
 * Buf uses a lock-free pool for extremely fast allocation and deallocation.
 *
 * Buf can optionally take a template parameter that will be placed in the 'data'
 * union as 'fields.' This must be a basic plain data type and must be no larger than
 * ZT_BUF_MEM_SIZE. It's typically a packed struct.
 *
 * Buf instances with different template parameters can freely be cast to one another
 * as there is no actual difference in size or layout.
 *
 * @tparam U Type to overlap with data bytes in data union (can't be larger than ZT_BUF_MEM_SIZE)
 */
class Buf
{
	friend class SharedPtr< Buf >;

public:
	// New and delete operators that allocate Buf instances from a shared lock-free memory pool.
	static void *operator new(std::size_t sz);

	static void operator delete(void *ptr);

	/**
	 * Raw data held in buffer
	 *
	 * The additional eight bytes should not be used and should be considered undefined.
	 * They exist to allow reads and writes of integer types to silently overflow if a
	 * read or write is performed at the end of the buffer.
	 */
	uint8_t unsafeData[ZT_BUF_MEM_SIZE + 8];

	/**
	 * Free all instances of Buf in shared pool.
	 *
	 * New buffers will be created and the pool repopulated if get() is called
	 * and outstanding buffers will still be returned to the pool. This just
	 * frees buffers currently held in reserve.
	 */
	static void freePool() noexcept;

	/**
	 * @return Number of Buf objects currently allocated via pool mechanism
	 */
	static long poolAllocated() noexcept;

	/**
	 * Slice is almost exactly like the built-in slice data structure in Go
	 */
	struct Slice : TriviallyCopyable
	{
		ZT_INLINE Slice(const SharedPtr< Buf > &b_, const unsigned int s_, const unsigned int e_) noexcept: b(b_), s(s_), e(e_)
		{}

		ZT_INLINE Slice() noexcept: b(), s(0), e(0)
		{}

		ZT_INLINE operator bool() const noexcept
		{ return (b); }

		ZT_INLINE unsigned int size() const noexcept
		{ return (e - s); }

		ZT_INLINE void zero() noexcept
		{
			b.zero();
			s = 0;
			e = 0;
		}

		/**
		 * Buffer holding slice data
		 */
		SharedPtr< Buf > b;

		/**
		 * Index of start of data in slice
		 */
		unsigned int s;

		/**
		 * Index of end of data in slice (make sure it's greater than or equal to 's'!)
		 */
		unsigned int e;
	};

	/**
	 * A vector of slices making up a packet that might span more than one buffer.
	 */
	class PacketVector : public ZeroTier::FCV< Slice, ZT_MAX_PACKET_FRAGMENTS >
	{
	public:
		ZT_INLINE PacketVector() : ZeroTier::FCV< Slice, ZT_MAX_PACKET_FRAGMENTS >()
		{}

		ZT_INLINE unsigned int totalSize() const noexcept
		{
			unsigned int size = 0;
			for (PacketVector::const_iterator s(begin()); s != end(); ++s)
				size += s->e - s->s;
			return size;
		}

		/**
		 * Merge this packet vector into a single destination buffer
		 * 
		 * @param b Destination buffer
		 * @return Size of data in destination or -1 on error
		 */
		ZT_INLINE int mergeCopy(Buf &b) const noexcept
		{
			unsigned int size = 0;
			for (PacketVector::const_iterator s(begin()); s != end(); ++s) {
				const unsigned int start = s->s;
				const unsigned int rem = s->e - start;
				if (likely((size + rem) <= ZT_BUF_MEM_SIZE)) {
					Utils::copy(b.unsafeData + size, s->b->unsafeData + start, rem);
					size += rem;
				} else {
					return -1;
				}
			}
			return (int)size;
		}

		/**
		 * Merge this packet vector into a single destination buffer with an arbitrary copy function
		 * 
		 * This can be used to e.g. simultaneously merge and decrypt a packet.
		 * 
		 * @param b Destination buffer
		 * @param simpleCopyBefore Don't start using copyFunction until this index (0 to always use)
		 * @param copyFunction Function to invoke with memcpy-like arguments: (dest, source, size)
		 * @tparam F Type of copyFunction (typically inferred)
		 * @return Size of data in destination or -1 on error
		 */
		template< typename F >
		ZT_INLINE int mergeMap(Buf &b, const unsigned int simpleCopyBefore, F copyFunction) const noexcept
		{
			unsigned int size = 0;
			for (PacketVector::const_iterator s(begin()); s != end(); ++s) {
				unsigned int start = s->s;
				unsigned int rem = s->e - start;
				if (likely((size + rem) <= ZT_BUF_MEM_SIZE)) {
					if (size < simpleCopyBefore) {
						unsigned int sc = simpleCopyBefore - size;
						if (unlikely(sc > rem))
							sc = rem;
						Utils::copy(b.unsafeData + size, s->b->unsafeData + start, sc);
						start += sc;
						rem -= sc;
					}

					if (likely(rem > 0)) {
						copyFunction(b.unsafeData + size, s->b->unsafeData + start, rem);
						size += rem;
					}
				} else {
					return -1;
				}
			}
			return (int)size;
		}
	};

	/**
	 * Create a new uninitialized buffer with undefined contents (use clear() to zero if needed)
	 */
	ZT_INLINE Buf() noexcept: __nextInPool(0), __refCount(0)
	{}

	/**
	 * Create a new buffer and copy data into it
	 */
	ZT_INLINE Buf(const void *const data, const unsigned int len) noexcept:
		__refCount(0)
	{
		Utils::copy(unsafeData, data, len);
	}

	ZT_INLINE Buf(const Buf &b2) noexcept:
		__nextInPool(0),
		__refCount(0)
	{
		Utils::copy< ZT_BUF_MEM_SIZE >(unsafeData, b2.unsafeData);
	}

	ZT_INLINE Buf &operator=(const Buf &b2) noexcept
	{
		if (this != &b2)
			Utils::copy< ZT_BUF_MEM_SIZE >(unsafeData, b2.unsafeData);
		return *this;
	}

	/**
	 * Check for overflow beyond the size of the buffer
	 *
	 * This is used to check for overflow when writing. It returns true if the iterator
	 * has passed beyond the capacity of the buffer.
	 *
	 * @param ii Iterator to check
	 * @return True if iterator has read past the size of the buffer
	 */
	static ZT_INLINE bool writeOverflow(const int &ii) noexcept
	{ return ((ii - ZT_BUF_MEM_SIZE) > 0); }

	/**
	 * Check for overflow beyond the size of the data that should be in the buffer
	 *
	 * This is used to check for overflow when reading, with the second argument being the
	 * size of the meaningful data actually present in the buffer.
	 *
	 * @param ii Iterator to check
	 * @param size Size of data that should be in buffer
	 * @return True if iterator has read past the size of the data
	 */
	static ZT_INLINE bool readOverflow(const int &ii, const unsigned int size) noexcept
	{ return ((ii - (int)size) > 0); }

	/**
	 * Set all memory to zero
	 */
	ZT_INLINE void clear() noexcept
	{
		Utils::zero< ZT_BUF_MEM_SIZE >(unsafeData);
	}

	/**
	 * Read a byte
	 *
	 * @param ii Index value-result parameter (incremented by 1)
	 * @return Byte (undefined on overflow)
	 */
	ZT_INLINE uint8_t rI8(int &ii) const noexcept
	{
		const int s = ii++;
		return unsafeData[(unsigned int)s & ZT_BUF_MEM_MASK];
	}

	/**
	 * Read a 16-bit integer
	 *
	 * @param ii Index value-result parameter (incremented by 2)
	 * @return Integer (undefined on overflow)
	 */
	ZT_INLINE uint16_t rI16(int &ii) const noexcept
	{
		const unsigned int s = (unsigned int)ii & ZT_BUF_MEM_MASK;
		ii += 2;
#ifdef ZT_NO_UNALIGNED_ACCESS
		return (
			((uint16_t)unsafeData[s] << 8U) |
			(uint16_t)unsafeData[s + 1]);
#else
		return Utils::ntoh(*reinterpret_cast<const uint16_t *>(unsafeData + s));
#endif
	}

	/**
	 * Read a 32-bit integer
	 *
	 * @param ii Index value-result parameter (incremented by 4)
	 * @return Integer (undefined on overflow)
	 */
	ZT_INLINE uint32_t rI32(int &ii) const noexcept
	{
		const unsigned int s = (unsigned int)ii & ZT_BUF_MEM_MASK;
		ii += 4;
#ifdef ZT_NO_UNALIGNED_ACCESS
		return (
			((uint32_t)unsafeData[s] << 24U) |
			((uint32_t)unsafeData[s + 1] << 16U) |
			((uint32_t)unsafeData[s + 2] << 8U) |
			(uint32_t)unsafeData[s + 3]);
#else
		return Utils::ntoh(*reinterpret_cast<const uint32_t *>(unsafeData + s));
#endif
	}

	/**
	 * Read a 64-bit integer
	 *
	 * @param ii Index value-result parameter (incremented by 8)
	 * @return Integer (undefined on overflow)
	 */
	ZT_INLINE uint64_t rI64(int &ii) const noexcept
	{
		const unsigned int s = (unsigned int)ii & ZT_BUF_MEM_MASK;
		ii += 8;
#ifdef ZT_NO_UNALIGNED_ACCESS
		return (
			((uint64_t)unsafeData[s] << 56U) |
			((uint64_t)unsafeData[s + 1] << 48U) |
			((uint64_t)unsafeData[s + 2] << 40U) |
			((uint64_t)unsafeData[s + 3] << 32U) |
			((uint64_t)unsafeData[s + 4] << 24U) |
			((uint64_t)unsafeData[s + 5] << 16U) |
			((uint64_t)unsafeData[s + 6] << 8U) |
			(uint64_t)unsafeData[s + 7]);
#else
		return Utils::ntoh(*reinterpret_cast<const uint64_t *>(unsafeData + s));
#endif
	}

	/**
	 * Read an object supporting the marshal/unmarshal interface
	 *
	 * If the return value is negative the object's state is undefined. A return value of
	 * zero typically also indicates a problem, though this may depend on the object type.
	 *
	 * Since objects may be invalid even if there is no overflow, it's important to check
	 * the return value of this function in all cases and discard invalid packets as it
	 * indicates.
	 *
	 * @tparam T Object type
	 * @param ii Index value-result parameter (incremented by object's size in bytes)
	 * @param obj Object to read
	 * @return Bytes read or a negative value on unmarshal error (passed from object) or overflow
	 */
	template< typename T >
	ZT_INLINE int rO(int &ii, T &obj) const noexcept
	{
		if (likely(ii < ZT_BUF_MEM_SIZE)) {
			int ms = obj.unmarshal(unsafeData + ii, ZT_BUF_MEM_SIZE - ii);
			if (ms > 0)
				ii += ms;
			return ms;
		}
		return -1;
	}

	/**
	 * Read a C-style string from the buffer, making a copy and advancing the iterator
	 *
	 * Use this if the buffer's memory may get changed between reading and processing
	 * what is read.
	 *
	 * @param ii Index value-result parameter (incremented by length of string)
	 * @param buf Buffer to receive string
	 * @param bufSize Capacity of buffer in bytes
	 * @return Pointer to buf or NULL on overflow or error
	 */
	ZT_INLINE char *rS(int &ii, char *const buf, const unsigned int bufSize) const noexcept
	{
		const char *const s = (const char *)(unsafeData + ii);
		const int sii = ii;
		while (ii < ZT_BUF_MEM_SIZE) {
			if (unsafeData[ii++] == 0) {
				const int l = ii - sii;
				if (unlikely((unsigned int)l > bufSize))
					return nullptr;
				Utils::copy(buf, s, l);
				return buf;
			}
		}
		return nullptr;
	}

	/**
	 * Obtain a pointer to a C-style string in the buffer without copying and advance the iterator
	 *
	 * The iterator is advanced even if this fails and returns NULL so that readOverflow()
	 * will indicate that an overflow occurred. As with other reads the string's contents are
	 * undefined if readOverflow() returns true.
	 *
	 * This version avoids a copy and so is faster if the buffer won't be modified between
	 * reading and processing.
	 *
	 * @param ii Index value-result parameter (incremented by length of string)
	 * @return Pointer to null-terminated C-style string or NULL on overflow or error
	 */
	ZT_INLINE const char *rSnc(int &ii) const noexcept
	{
		const char *const s = (const char *)(unsafeData + ii);
		while (ii < ZT_BUF_MEM_SIZE) {
			if (unsafeData[ii++] == 0)
				return s;
		}
		return nullptr;
	}

	/**
	 * Read a byte array from the buffer, making a copy and advancing the iterator
	 *
	 * Use this if the buffer's memory may get changed between reading and processing
	 * what is read.
	 *
	 * @param ii Index value-result parameter (incremented by len)
	 * @param bytes Buffer to contain data to read
	 * @param len Length of buffer
	 * @return Pointer to data or NULL on overflow or error
	 */
	ZT_INLINE uint8_t *rB(int &ii, void *const bytes, const unsigned int len) const noexcept
	{
		if (likely(((ii += (int)len) <= ZT_BUF_MEM_SIZE))) {
			Utils::copy(bytes, unsafeData + ii, len);
			return reinterpret_cast<uint8_t *>(bytes);
		}
		return nullptr;
	}

	/**
	 * Obtain a pointer to a field in the buffer without copying and advance the iterator
	 *
	 * The iterator is advanced even if this fails and returns NULL so that readOverflow()
	 * will indicate that an overflow occurred.
	 *
	 * This version avoids a copy and so is faster if the buffer won't be modified between
	 * reading and processing.
	 *
	 * @param ii Index value-result parameter (incremented by len)
	 * @param len Length of data field to obtain a pointer to
	 * @return Pointer to field or NULL on overflow
	 */
	ZT_INLINE const uint8_t *rBnc(int &ii, unsigned int len) const noexcept
	{
		const uint8_t *const b = unsafeData + ii;
		return ((ii += (int)len) <= ZT_BUF_MEM_SIZE) ? b : nullptr;
	}

	/**
	 * Load a value at an index that is compile time checked against the maximum buffer size
	 * 
	 * @tparam I Static index
	 * @return Value
	 */
	template< unsigned int I >
	ZT_INLINE uint8_t lI8() const noexcept
	{
		static_assert(I < ZT_BUF_MEM_SIZE, "overflow");
		return unsafeData[I];
	}

	/**
	 * Load a value at an index that is compile time checked against the maximum buffer size
	 * 
	 * @tparam I Static index
	 * @return Value
	 */
	template< unsigned int I >
	ZT_INLINE uint8_t lI16() const noexcept
	{
		static_assert((I + 1) < ZT_BUF_MEM_SIZE, "overflow");
#ifdef ZT_NO_UNALIGNED_ACCESS
		return (
			((uint16_t)unsafeData[I] << 8U) |
			(uint16_t)unsafeData[I + 1]);
#else
		return Utils::ntoh(*reinterpret_cast<const uint16_t *>(unsafeData + I));
#endif
	}

	/**
	 * Load a value at an index that is compile time checked against the maximum buffer size
	 * 
	 * @tparam I Static index
	 * @return Value
	 */
	template< unsigned int I >
	ZT_INLINE uint8_t lI32() const noexcept
	{
		static_assert((I + 3) < ZT_BUF_MEM_SIZE, "overflow");
#ifdef ZT_NO_UNALIGNED_ACCESS
		return (
			((uint32_t)unsafeData[I] << 24U) |
			((uint32_t)unsafeData[I + 1] << 16U) |
			((uint32_t)unsafeData[I + 2] << 8U) |
			(uint32_t)unsafeData[I + 3]);
#else
		return Utils::ntoh(*reinterpret_cast<const uint32_t *>(unsafeData + I));
#endif
	}

	/**
	 * Load a value at an index that is compile time checked against the maximum buffer size
	 * 
	 * @tparam I Static index
	 * @return Value
	 */
	template< unsigned int I >
	ZT_INLINE uint8_t lI64() const noexcept
	{
		static_assert((I + 7) < ZT_BUF_MEM_SIZE, "overflow");
#ifdef ZT_NO_UNALIGNED_ACCESS
		return (
			((uint64_t)unsafeData[I] << 56U) |
			((uint64_t)unsafeData[I + 1] << 48U) |
			((uint64_t)unsafeData[I + 2] << 40U) |
			((uint64_t)unsafeData[I + 3] << 32U) |
			((uint64_t)unsafeData[I + 4] << 24U) |
			((uint64_t)unsafeData[I + 5] << 16U) |
			((uint64_t)unsafeData[I + 6] << 8U) |
			(uint64_t)unsafeData[I + 7]);
#else
		return Utils::ntoh(*reinterpret_cast<const uint64_t *>(unsafeData + I));
#endif
	}

	/**
	 * Load a value at an index without advancing the index
	 *
	 * Note that unlike the rI??() methods this does not increment ii and therefore
	 * will not necessarily result in a 'true' return from readOverflow(). It does
	 * however subject 'ii' to soft bounds masking like the gI??() methods.
	 */
	ZT_INLINE uint8_t lI8(const int ii) const noexcept
	{
		return unsafeData[(unsigned int)ii & ZT_BUF_MEM_MASK];
	}

	/**
	 * Load a value at an index without advancing the index
	 *
	 * Note that unlike the rI??() methods this does not increment ii and therefore
	 * will not necessarily result in a 'true' return from readOverflow(). It does
	 * however subject 'ii' to soft bounds masking like the gI??() methods.
	 */
	ZT_INLINE uint16_t lI16(const int ii) const noexcept
	{
		const unsigned int s = (unsigned int)ii & ZT_BUF_MEM_MASK;
#ifdef ZT_NO_UNALIGNED_ACCESS
		return (
			((uint16_t)unsafeData[s] << 8U) |
			(uint16_t)unsafeData[s + 1]);
#else
		return Utils::ntoh(*reinterpret_cast<const uint16_t *>(unsafeData + s));
#endif
	}

	/**
	 * Load a value at an index without advancing the index
	 *
	 * Note that unlike the rI??() methods this does not increment ii and therefore
	 * will not necessarily result in a 'true' return from readOverflow(). It does
	 * however subject 'ii' to soft bounds masking like the gI??() methods.
	 */
	ZT_INLINE uint32_t lI32(const int ii) const noexcept
	{
		const unsigned int s = (unsigned int)ii & ZT_BUF_MEM_MASK;
#ifdef ZT_NO_UNALIGNED_ACCESS
		return (
			((uint32_t)unsafeData[s] << 24U) |
			((uint32_t)unsafeData[s + 1] << 16U) |
			((uint32_t)unsafeData[s + 2] << 8U) |
			(uint32_t)unsafeData[s + 3]);
#else
		return Utils::ntoh(*reinterpret_cast<const uint32_t *>(unsafeData + s));
#endif
	}

	/**
	 * Load a value at an index without advancing the index
	 *
	 * Note that unlike the rI??() methods this does not increment ii and therefore
	 * will not necessarily result in a 'true' return from readOverflow(). It does
	 * however subject 'ii' to soft bounds masking like the gI??() methods.
	 */
	ZT_INLINE uint8_t lI64(const int ii) const noexcept
	{
		const unsigned int s = (unsigned int)ii & ZT_BUF_MEM_MASK;
#ifdef ZT_NO_UNALIGNED_ACCESS
		return (
			((uint64_t)unsafeData[s] << 56U) |
			((uint64_t)unsafeData[s + 1] << 48U) |
			((uint64_t)unsafeData[s + 2] << 40U) |
			((uint64_t)unsafeData[s + 3] << 32U) |
			((uint64_t)unsafeData[s + 4] << 24U) |
			((uint64_t)unsafeData[s + 5] << 16U) |
			((uint64_t)unsafeData[s + 6] << 8U) |
			(uint64_t)unsafeData[s + 7]);
#else
		return Utils::ntoh(*reinterpret_cast<const uint64_t *>(unsafeData + s));
#endif
	}

	/**
	 * Write a byte
	 *
	 * @param ii Index value-result parameter (incremented by 1)
	 * @param n Byte
	 */
	ZT_INLINE void wI8(int &ii, const uint8_t n) noexcept
	{
		const int s = ii++;
		unsafeData[(unsigned int)s & ZT_BUF_MEM_MASK] = n;
	}

	/**
	 * Write a 16-bit integer in big-endian byte order
	 *
	 * @param ii Index value-result parameter (incremented by 2)
	 * @param n Integer
	 */
	ZT_INLINE void wI16(int &ii, const uint16_t n) noexcept
	{
		const unsigned int s = ((unsigned int)ii) & ZT_BUF_MEM_MASK;
		ii += 2;
#ifdef ZT_NO_UNALIGNED_ACCESS
		unsafeData[s] = (uint8_t)(n >> 8U);
		unsafeData[s + 1] = (uint8_t)n;
#else
		*reinterpret_cast<uint16_t *>(unsafeData + s) = Utils::hton(n);
#endif
	}

	/**
	 * Write a 32-bit integer in big-endian byte order
	 *
	 * @param ii Index value-result parameter (incremented by 4)
	 * @param n Integer
	 */
	ZT_INLINE void wI32(int &ii, const uint32_t n) noexcept
	{
		const unsigned int s = ((unsigned int)ii) & ZT_BUF_MEM_MASK;
		ii += 4;
#ifdef ZT_NO_UNALIGNED_ACCESS
		unsafeData[s] = (uint8_t)(n >> 24U);
		unsafeData[s + 1] = (uint8_t)(n >> 16U);
		unsafeData[s + 2] = (uint8_t)(n >> 8U);
		unsafeData[s + 3] = (uint8_t)n;
#else
		*reinterpret_cast<uint32_t *>(unsafeData + s) = Utils::hton(n);
#endif
	}

	/**
	 * Write a 64-bit integer in big-endian byte order
	 *
	 * @param ii Index value-result parameter (incremented by 8)
	 * @param n Integer
	 */
	ZT_INLINE void wI64(int &ii, const uint64_t n) noexcept
	{
		const unsigned int s = ((unsigned int)ii) & ZT_BUF_MEM_MASK;
		ii += 8;
#ifdef ZT_NO_UNALIGNED_ACCESS
		unsafeData[s] = (uint8_t)(n >> 56U);
		unsafeData[s + 1] = (uint8_t)(n >> 48U);
		unsafeData[s + 2] = (uint8_t)(n >> 40U);
		unsafeData[s + 3] = (uint8_t)(n >> 32U);
		unsafeData[s + 4] = (uint8_t)(n >> 24U);
		unsafeData[s + 5] = (uint8_t)(n >> 16U);
		unsafeData[s + 6] = (uint8_t)(n >> 8U);
		unsafeData[s + 7] = (uint8_t)n;
#else
		*reinterpret_cast<uint64_t *>(unsafeData + s) = Utils::hton(n);
#endif
	}

	/**
	 * Write an object implementing the marshal interface
	 *
	 * @tparam T Object type
	 * @param ii Index value-result parameter (incremented by size of object)
	 * @param t Object to write
	 */
	template< typename T >
	ZT_INLINE void wO(int &ii, T &t) noexcept
	{
		const int s = ii;
		if (likely((s + T::marshalSizeMax()) <= ZT_BUF_MEM_SIZE)) {
			int ms = t.marshal(unsafeData + s);
			if (ms > 0)
				ii += ms;
		} else {
			ii += T::marshalSizeMax(); // mark as overflowed even if we didn't do anything
		}
	}

	/**
	 * Write a C-style null-terminated string (including the trailing zero)
	 *
	 * @param ii Index value-result parameter (incremented by length of string)
	 * @param s String to write (writes an empty string if this is NULL)
	 */
	ZT_INLINE void wS(int &ii, const char *s) noexcept
	{
		if (s) {
			char c;
			do {
				c = *(s++);
				wI8(ii, (uint8_t)c);
			} while (c);
		} else {
			wI8(ii, 0);
		}
	}

	/**
	 * Write a byte array
	 *
	 * @param ii Index value-result parameter (incremented by len)
	 * @param bytes Bytes to write
	 * @param len Size of data in bytes
	 */
	ZT_INLINE void wB(int &ii, const void *const bytes, const unsigned int len) noexcept
	{
		const int s = ii;
		if (likely((ii += (int)len) <= ZT_BUF_MEM_SIZE))
			Utils::copy(unsafeData + s, bytes, len);
	}

	/**
	 * Write zeroes
	 *
	 * @param ii Index value-result parameter (incremented by len)
	 * @param len Number of zero bytes to write
	 */
	ZT_INLINE void wZ(int &ii, const unsigned int len) noexcept
	{
		const int s = ii;
		if (likely((ii += (int)len) <= ZT_BUF_MEM_SIZE))
			Utils::zero(unsafeData + s, len);
	}

	/**
	 * Write secure random bytes
	 *
	 * @param ii Index value-result parameter (incremented by len)
	 * @param len Number of random bytes to write
	 */
	ZT_INLINE void wR(int &ii, const unsigned int len) noexcept
	{
		const int s = ii;
		if (likely((ii += (int)len) <= ZT_BUF_MEM_SIZE))
			Utils::getSecureRandom(unsafeData + s, len);
	}

	/**
	 * Store a byte without advancing the index
	 */
	ZT_INLINE void sI8(const int ii, const uint8_t n) noexcept
	{
		unsafeData[(unsigned int)ii & ZT_BUF_MEM_MASK] = n;
	}

	/**
	 * Store an integer without advancing the index
	 */
	ZT_INLINE void sI16(const int ii, const uint16_t n) noexcept
	{
		const unsigned int s = ((unsigned int)ii) & ZT_BUF_MEM_MASK;
#ifdef ZT_NO_UNALIGNED_ACCESS
		unsafeData[s] = (uint8_t)(n >> 8U);
		unsafeData[s + 1] = (uint8_t)n;
#else
		*reinterpret_cast<uint16_t *>(unsafeData + s) = Utils::hton(n);
#endif
	}

	/**
	 * Store an integer without advancing the index
	 */
	ZT_INLINE void sI32(const int ii, const uint32_t n) noexcept
	{
		const unsigned int s = ((unsigned int)ii) & ZT_BUF_MEM_MASK;
#ifdef ZT_NO_UNALIGNED_ACCESS
		unsafeData[s] = (uint8_t)(n >> 24U);
		unsafeData[s + 1] = (uint8_t)(n >> 16U);
		unsafeData[s + 2] = (uint8_t)(n >> 8U);
		unsafeData[s + 3] = (uint8_t)n;
#else
		*reinterpret_cast<uint32_t *>(unsafeData + s) = Utils::hton(n);
#endif
	}

	/**
	 * Store an integer without advancing the index
	 */
	ZT_INLINE void sI64(const int ii, const uint64_t n) noexcept
	{
		const unsigned int s = ((unsigned int)ii) & ZT_BUF_MEM_MASK;
#ifdef ZT_NO_UNALIGNED_ACCESS
		unsafeData[s] = (uint8_t)(n >> 56U);
		unsafeData[s + 1] = (uint8_t)(n >> 48U);
		unsafeData[s + 2] = (uint8_t)(n >> 40U);
		unsafeData[s + 3] = (uint8_t)(n >> 32U);
		unsafeData[s + 4] = (uint8_t)(n >> 24U);
		unsafeData[s + 5] = (uint8_t)(n >> 16U);
		unsafeData[s + 6] = (uint8_t)(n >> 8U);
		unsafeData[s + 7] = (uint8_t)n;
#else
		*reinterpret_cast<uint64_t *>(unsafeData + s) = Utils::hton(n);
#endif
	}

	/**
	 * @return Capacity of this buffer (usable size of data.bytes)
	 */
	static constexpr unsigned int capacity() noexcept
	{ return ZT_BUF_MEM_SIZE; }

private:
	volatile uintptr_t __nextInPool;
	std::atomic< int > __refCount;
};

} // namespace ZeroTier

#endif
