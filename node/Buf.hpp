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

#ifndef ZT_BUF_HPP
#define ZT_BUF_HPP

#include "Constants.hpp"
#include "AtomicCounter.hpp"
#include "Utils.hpp"
#include "SharedPtr.hpp"
#include "Mutex.hpp"

#include <cstdint>
#include <cstring>
#include <cstdlib>

#ifndef __GNUC__
#include <atomic>
#endif

// Buffers are 16384 bytes in size because this is the smallest size that can hold any packet
// and is a power of two. It needs to be a power of two because masking is significantly faster
// than integer division modulus.
#define ZT_BUF_MEM_SIZE 0x00004000
#define ZT_BUF_MEM_MASK 0x00003fffU

namespace ZeroTier {

#ifdef __GNUC__
extern uintptr_t Buf_pool;
#else
extern std::atomic<uintptr_t> Buf_pool;
#endif

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
template<typename U = void>
class Buf
{
	friend class SharedPtr< Buf<U> >;

private:
	// Direct construction isn't allowed; use get().
	ZT_ALWAYS_INLINE Buf() {}

	template<typename X>
	ZT_ALWAYS_INLINE Buf(const Buf<X> &b) { memcpy(data.bytes,b.data.bytes,ZT_BUF_MEM_SIZE); }

public:
	static void operator delete(void *ptr,std::size_t sz)
	{
		if (ptr) {
			uintptr_t bb;
			const uintptr_t locked = ~((uintptr_t)0);
			for (;;) {
#ifdef __GNUC__
				bb = __sync_fetch_and_or(&Buf_pool,locked); // get value of s_pool and "lock" by filling with all 1's
#else
				bb = s_pool.fetch_or(locked);
#endif
				if (bb != locked)
					break;
			}

			((Buf *)ptr)->__nextInPool = bb;
#ifdef __GNUC__
			__sync_fetch_and_and(&Buf_pool,(uintptr_t)ptr);
#else
			s_pool.store((uintptr_t)ptr);
#endif
		}
	}

	/**
	 * Get obtains a buffer from the pool or allocates a new buffer if the pool is empty
	 *
	 * @return Buffer
	 */
	static ZT_ALWAYS_INLINE SharedPtr< Buf<U> > get()
	{
		uintptr_t bb;
		const uintptr_t locked = ~((uintptr_t)0);
		for (;;) {
#ifdef __GNUC__
			bb = __sync_fetch_and_or(&Buf_pool,locked); // get value of s_pool and "lock" by filling with all 1's
#else
			bb = s_pool.fetch_or(locked);
#endif
			if (bb != locked)
				break;
		}

		Buf *b;
		if (bb == 0) {
#ifdef __GNUC__
			__sync_fetch_and_and(&Buf_pool,bb);
#else
			s_pool.store(bb);
#endif
			b = (Buf *)malloc(sizeof(Buf));
			if (!b)
				return SharedPtr<Buf>();
		} else {
			b = (Buf *)bb;
#ifdef __GNUC__
			__sync_fetch_and_and(&Buf_pool,b->__nextInPool);
#else
			s_pool.store(b->__nextInPool);
#endif
		}

		b->__refCount.zero();
		return SharedPtr<Buf>(b);
	}

	/**
	 * Free buffers in the pool
	 *
	 * New buffers will be created and the pool repopulated if get() is called
	 * and outstanding buffers will still be returned to the pool. This just
	 * frees buffers currently held in reserve.
	 */
	static inline void freePool()
	{
		uintptr_t bb;
		const uintptr_t locked = ~((uintptr_t)0);
		for (;;) {
#ifdef __GNUC__
			bb = __sync_fetch_and_or(&Buf_pool,locked); // get value of s_pool and "lock" by filling with all 1's
#else
			bb = s_pool.fetch_or(locked);
#endif
			if (bb != locked)
				break;
		}

#ifdef __GNUC__
		__sync_fetch_and_and(&Buf_pool,(uintptr_t)0);
#else
		s_pool.store((uintptr_t)0);
#endif

		while (bb != 0) {
			uintptr_t next = ((Buf *)bb)->__nextInPool;
			free((void *)bb);
			bb = next;
		}
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
	static ZT_ALWAYS_INLINE bool writeOverflow(const int &ii) { return ((ii - ZT_BUF_MEM_SIZE) > 0); }

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
	static ZT_ALWAYS_INLINE bool readOverflow(const int &ii,const unsigned int size) { return ((ii - (int)size) > 0); }

	template<typename X>
	ZT_ALWAYS_INLINE Buf &operator=(const Buf<X> &b) const
	{
		memcpy(data.bytes,b.data.bytes,ZT_BUF_MEM_SIZE);
		return *this;
	}

	/**
	 * Shortcut to cast between buffers whose data can be viewed through a different struct type
	 *
	 * @tparam X A packed struct or other primitive type that should be placed in the data union
	 * @return Reference to this Buf templated with the supplied parameter
	 */
	template<typename X>
	ZT_ALWAYS_INLINE Buf<X> &view() { return *reinterpret_cast< Buf<X> * >(this); }

	/**
	 * Shortcut to cast between buffers whose data can be viewed through a different struct type
	 *
	 * @tparam X A packed struct or other primitive type that should be placed in the data union
	 * @return Reference to this Buf templated with the supplied parameter
	 */
	template<typename X>
	ZT_ALWAYS_INLINE const Buf<X> &view() const { return *reinterpret_cast< Buf<X> * >(this); }

	/**
	 * Zero memory
	 *
	 * For performance reasons Buf does not do this on every get().
	 */
	ZT_ALWAYS_INLINE void clear() { memset(data.bytes,0,ZT_BUF_MEM_SIZE); }

	/**
	 * Read a byte
	 *
	 * @param ii Iterator
	 * @return Byte (undefined on overflow)
	 */
	ZT_ALWAYS_INLINE uint8_t rI8(int &ii) const
	{
		const int s = ii++;
		return data.bytes[(unsigned int)s & ZT_BUF_MEM_MASK];
	}

	/**
	 * Read a 16-bit integer
	 *
	 * @param ii Integer
	 * @return Integer (undefined on overflow)
	 */
	ZT_ALWAYS_INLINE uint16_t rI16(int &ii) const
	{
		const unsigned int s = (unsigned int)ii & ZT_BUF_MEM_MASK;
		ii += 2;
#ifdef ZT_NO_UNALIGNED_ACCESS
		return (
			((uint16_t)data.bytes[s] << 8U) |
			(uint16_t)data.bytes[s + 1]);
#else
		return Utils::ntoh(*reinterpret_cast<const uint16_t *>(data.bytes + s));
#endif
	}

	/**
	 * Read a 32-bit integer
	 *
	 * @param ii Integer
	 * @return Integer (undefined on overflow)
	 */
	ZT_ALWAYS_INLINE uint32_t rI32(int &ii) const
	{
		const unsigned int s = (unsigned int)ii & ZT_BUF_MEM_MASK;
		ii += 4;
#ifdef ZT_NO_UNALIGNED_ACCESS
		return (
			((uint32_t)data.bytes[s] << 24U) |
			((uint32_t)data.bytes[s + 1] << 16U) |
			((uint32_t)data.bytes[s + 2] << 8U) |
			(uint32_t)data.bytes[s + 3]);
#else
		return Utils::ntoh(*reinterpret_cast<const uint32_t *>(data.bytes + s));
#endif
	}

	/**
	 * Read a 64-bit integer
	 *
	 * @param ii Integer
	 * @return Integer (undefined on overflow)
	 */
	ZT_ALWAYS_INLINE uint64_t rI64(int &ii) const
	{
		const unsigned int s = (unsigned int)ii & ZT_BUF_MEM_MASK;
		ii += 8;
#ifdef ZT_NO_UNALIGNED_ACCESS
		return (
			((uint64_t)data.bytes[s] << 56U) |
			((uint64_t)data.bytes[s + 1] << 48U) |
			((uint64_t)data.bytes[s + 2] << 40U) |
			((uint64_t)data.bytes[s + 3] << 32U) |
			((uint64_t)data.bytes[s + 4] << 24U) |
			((uint64_t)data.bytes[s + 5] << 16U) |
			((uint64_t)data.bytes[s + 6] << 8U) |
			(uint64_t)data.bytes[s + 7]);
#else
		return Utils::ntoh(*reinterpret_cast<const uint64_t *>(data.bytes + s));
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
	 * @param ii Iterator
	 * @param obj Object to read
	 * @return Bytes read or a negative value on unmarshal error (passed from object) or overflow
	 */
	template<typename T>
	ZT_ALWAYS_INLINE int rO(int &ii,T &obj) const
	{
		if (ii < ZT_BUF_MEM_SIZE) {
			int ms = obj.unmarshal(data.bytes + ii,ZT_BUF_MEM_SIZE - ii);
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
	 * @param ii Iterator
	 * @param buf Buffer to receive string
	 * @param bufSize Capacity of buffer in bytes
	 * @return Pointer to buf or NULL on overflow or error
	 */
	ZT_ALWAYS_INLINE char *rS(int &ii,char *const buf,const unsigned int bufSize) const
	{
		const char *const s = (const char *)(data.bytes + ii);
		const int sii = ii;
		while (ii < ZT_BUF_MEM_SIZE) {
			if (data.bytes[ii++] == 0) {
				memcpy(buf,s,ii - sii);
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
	 * @param ii Iterator
	 * @return Pointer to null-terminated C-style string or NULL on overflow or error
	 */
	ZT_ALWAYS_INLINE const char *rSnc(int &ii) const
	{
		const char *const s = (const char *)(data.bytes + ii);
		while (ii < ZT_BUF_MEM_SIZE) {
			if (data.bytes[ii++] == 0)
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
	 * @param ii Iterator
	 * @param bytes Buffer to contain data to read
	 * @param len Length of buffer
	 * @return Pointer to data or NULL on overflow or error
	 */
	ZT_ALWAYS_INLINE void *rB(int &ii,void *bytes,unsigned int len) const
	{
		const void *const b = (const void *)(data.bytes + ii);
		if ((ii += (int)len) <= ZT_BUF_MEM_SIZE) {
			memcpy(bytes,b,len);
			return bytes;
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
	 * @param ii Iterator
	 * @param len Length of data field to obtain a pointer to
	 * @return Pointer to field or NULL on overflow
	 */
	ZT_ALWAYS_INLINE const void *rBnc(int &ii,unsigned int len) const
	{
		const void *const b = (const void *)(data.bytes + ii);
		return ((ii += (int)len) <= ZT_BUF_MEM_SIZE) ? b : nullptr;
	}

	/**
	 * Write a byte
	 *
	 * @param ii Iterator
	 * @param n Byte
	 */
	ZT_ALWAYS_INLINE void wI(int &ii,uint8_t n)
	{
		const int s = ii++;
		data[(unsigned int)s & ZT_BUF_MEM_MASK] = n;
	}

	/**
	 * Write a 16-bit integer in big-endian byte order
	 *
	 * @param ii Iterator
	 * @param n Integer
	 */
	ZT_ALWAYS_INLINE void wI(int &ii,uint16_t n)
	{
		const unsigned int s = ((unsigned int)ii) & ZT_BUF_MEM_MASK;
		ii += 2;
#ifdef ZT_NO_UNALIGNED_ACCESS
		data[s] = (uint8_t)(n >> 8U);
		data[s + 1] = (uint8_t)n;
#else
		*reinterpret_cast<uint16_t *>(data.bytes + s) = Utils::hton(n);
#endif
	}

	/**
	 * Write a 32-bit integer in big-endian byte order
	 *
	 * @param ii Iterator
	 * @param n Integer
	 */
	ZT_ALWAYS_INLINE void wI(int &ii,uint32_t n)
	{
		const unsigned int s = ((unsigned int)ii) & ZT_BUF_MEM_MASK;
		ii += 4;
#ifdef ZT_NO_UNALIGNED_ACCESS
		data[s] = (uint8_t)(n >> 24U);
		data[s + 1] = (uint8_t)(n >> 16U);
		data[s + 2] = (uint8_t)(n >> 8U);
		data[s + 3] = (uint8_t)n;
#else
		*reinterpret_cast<uint32_t *>(data.bytes + s) = Utils::hton(n);
#endif
	}

	/**
	 * Write a 64-bit integer in big-endian byte order
	 *
	 * @param ii Iterator
	 * @param n Integer
	 */
	ZT_ALWAYS_INLINE void wI(int &ii,uint64_t n)
	{
		const unsigned int s = ((unsigned int)ii) & ZT_BUF_MEM_MASK;
		ii += 8;
#ifdef ZT_NO_UNALIGNED_ACCESS
		data[s] = (uint8_t)(n >> 56U);
		data[s + 1] = (uint8_t)(n >> 48U);
		data[s + 2] = (uint8_t)(n >> 40U);
		data[s + 3] = (uint8_t)(n >> 32U);
		data[s + 4] = (uint8_t)(n >> 24U);
		data[s + 5] = (uint8_t)(n >> 16U);
		data[s + 6] = (uint8_t)(n >> 8U);
		data[s + 7] = (uint8_t)n;
#else
		*reinterpret_cast<uint64_t *>(data.bytes + s) = Utils::hton(n);
#endif
	}

	/**
	 * Write an object implementing the marshal interface
	 *
	 * @tparam T Object type
	 * @param ii Iterator
	 * @param t Object to write
	 */
	template<typename T>
	ZT_ALWAYS_INLINE void wO(int &ii,T &t)
	{
		const int s = ii;
		if ((s + T::marshalSizeMax()) <= ZT_BUF_MEM_SIZE) {
			int ms = t.marshal(data.bytes + s);
			if (ms > 0)
				ii += ms;
		} else {
			ii += T::marshalSizeMax(); // mark as overflowed even if we didn't do anything
		}
	}

	/**
	 * Write a C-style null-terminated string (including the trailing zero)
	 *
	 * @param ii Iterator
	 * @param s String to write (writes an empty string if this is NULL)
	 */
	ZT_ALWAYS_INLINE void wS(int &ii,const char *s)
	{
		if (s) {
			char c;
			do {
				c = *(s++);
				wI(ii,(uint8_t)c);
			} while (c);
		} else {
			wI(ii,(uint8_t)0);
		}
	}

	/**
	 * Write a byte array
	 *
	 * @param ii Iterator
	 * @param bytes Bytes to write
	 * @param len Size of data in bytes
	 */
	ZT_ALWAYS_INLINE void wB(int &ii,const void *const bytes,const unsigned int len)
	{
		const int s = ii;
		if ((ii += (int)len) <= ZT_BUF_MEM_SIZE)
			memcpy(data.bytes + s,bytes,len);
	}

	/**
	 * Raw data and fields (if U template parameter is set)
	 *
	 * The extra eight bytes permit silent overflow of integer types without reading or writing
	 * beyond Buf's memory and without branching or extra masks. They can be ignored otherwise.
	 */
	ZT_PACKED_STRUCT(union {
		uint8_t bytes[ZT_BUF_MEM_SIZE + 8];
		U fields;
	}) data;

private:
	volatile uintptr_t __nextInPool; // next item in free pool if this Buf is in Buf_pool
	AtomicCounter __refCount;
};

} // namespace ZeroTier

#endif
