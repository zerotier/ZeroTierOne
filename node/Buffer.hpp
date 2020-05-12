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

#ifndef ZT_BUFFER_HPP
#define ZT_BUFFER_HPP

#include <string.h>
#include <stdint.h>

#include <stdexcept>
#include <string>
#include <algorithm>
#include <utility>

#include "Constants.hpp"
#include "Utils.hpp"

#if defined(__GNUC__) && (!defined(ZT_NO_TYPE_PUNNING))
#define ZT_VAR_MAY_ALIAS __attribute__((__may_alias__))
#else
#define ZT_VAR_MAY_ALIAS
#endif

namespace ZeroTier {

/**
 * A variable length but statically allocated buffer
 *
 * Bounds-checking is done everywhere, since this is used in security
 * critical code. This supports construction and assignment from buffers
 * of differing capacities, provided the data actually in them fits.
 * It throws std::out_of_range on any boundary violation.
 *
 * The at(), append(), etc. methods encode integers larger than 8-bit in
 * big-endian (network) byte order.
 *
 * @tparam C Total capacity
 */
template<unsigned int C>
class Buffer
{
	// I love me!
	template <unsigned int C2> friend class Buffer;

public:
	// STL container idioms
	typedef unsigned char value_type;
	typedef unsigned char * pointer;
	typedef const char * const_pointer;
	typedef char & reference;
	typedef const char & const_reference;
	typedef char * iterator;
	typedef const char * const_iterator;
	typedef unsigned int size_type;
	typedef int difference_type;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
	inline iterator begin() { return _b; }
	inline iterator end() { return (_b + _l); }
	inline const_iterator begin() const { return _b; }
	inline const_iterator end() const { return (_b + _l); }
	inline reverse_iterator rbegin() { return reverse_iterator(begin()); }
	inline reverse_iterator rend() { return reverse_iterator(end()); }
	inline const_reverse_iterator rbegin() const { return const_reverse_iterator(begin()); }
	inline const_reverse_iterator rend() const { return const_reverse_iterator(end()); }

	Buffer() :
		_l(0)
	{
	}

	Buffer(unsigned int l)
	{
		if (l > C)
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		_l = l;
	}

	template<unsigned int C2>
	Buffer(const Buffer<C2> &b)
	{
		*this = b;
	}

	Buffer(const void *b,unsigned int l)
	{
		copyFrom(b,l);
	}

	template<unsigned int C2>
	inline Buffer &operator=(const Buffer<C2> &b)
	{
		if (unlikely(b._l > C))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		if (C2 == C) {
			memcpy(this,&b,sizeof(Buffer<C>));
		} else {
			memcpy(_b,b._b,_l = b._l);
		}
		return *this;
	}

	inline void copyFrom(const void *b,unsigned int l)
	{
		if (unlikely(l > C))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		memcpy(_b,b,l);
		_l = l;
	}

	unsigned char operator[](const unsigned int i) const
	{
		if (unlikely(i >= _l))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		return (unsigned char)_b[i];
	}

	unsigned char &operator[](const unsigned int i)
	{
		if (unlikely(i >= _l))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		return ((unsigned char *)_b)[i];
	}

	/**
	 * Get a raw pointer to a field with bounds checking
	 *
	 * This isn't perfectly safe in that the caller could still overflow
	 * the pointer, but its use provides both a sanity check and
	 * documentation / reminder to the calling code to treat the returned
	 * pointer as being of size [l].
	 *
	 * @param i Index of field in buffer
	 * @param l Length of field in bytes
	 * @return Pointer to field data
	 * @throws std::out_of_range Field extends beyond data size
	 */
	unsigned char *field(unsigned int i,unsigned int l)
	{
		if (unlikely((i + l) > _l))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		return (unsigned char *)(_b + i);
	}
	const unsigned char *field(unsigned int i,unsigned int l) const
	{
		if (unlikely((i + l) > _l))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		return (const unsigned char *)(_b + i);
	}

	/**
	 * Place a primitive integer value at a given position
	 *
	 * @param i Index to place value
	 * @param v Value
	 * @tparam T Integer type (e.g. uint16_t, int64_t)
	 */
	template<typename T>
	inline void setAt(unsigned int i,const T v)
	{
		if (unlikely((i + sizeof(T)) > _l))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
#ifdef ZT_NO_TYPE_PUNNING
		uint8_t *p = reinterpret_cast<uint8_t *>(_b + i);
		for(unsigned int x=1;x<=sizeof(T);++x)
			*(p++) = (uint8_t)(v >> (8 * (sizeof(T) - x)));
#else
		T *const ZT_VAR_MAY_ALIAS p = reinterpret_cast<T *>(_b + i);
		*p = Utils::hton(v);
#endif
	}

	/**
	 * Get a primitive integer value at a given position
	 *
	 * @param i Index to get integer
	 * @tparam T Integer type (e.g. uint16_t, int64_t)
	 * @return Integer value
	 */
	template<typename T>
	inline T at(unsigned int i) const
	{
		if (unlikely((i + sizeof(T)) > _l))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
#ifdef ZT_NO_TYPE_PUNNING
		T v = 0;
		const uint8_t *p = reinterpret_cast<const uint8_t *>(_b + i);
		for(unsigned int x=0;x<sizeof(T);++x) {
			v <<= 8;
			v |= (T)*(p++);
		}
		return v;
#else
		const T *const ZT_VAR_MAY_ALIAS p = reinterpret_cast<const T *>(_b + i);
		return Utils::ntoh(*p);
#endif
	}

	/**
	 * Append an integer type to this buffer
	 *
	 * @param v Value to append
	 * @tparam T Integer type (e.g. uint16_t, int64_t)
	 * @throws std::out_of_range Attempt to append beyond capacity
	 */
	template<typename T>
	inline void append(const T v)
	{
		if (unlikely((_l + sizeof(T)) > C))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
#ifdef ZT_NO_TYPE_PUNNING
		uint8_t *p = reinterpret_cast<uint8_t *>(_b + _l);
		for(unsigned int x=1;x<=sizeof(T);++x)
			*(p++) = (uint8_t)(v >> (8 * (sizeof(T) - x)));
#else
		T *const ZT_VAR_MAY_ALIAS p = reinterpret_cast<T *>(_b + _l);
		*p = Utils::hton(v);
#endif
		_l += sizeof(T);
	}

	/**
	 * Append a run of bytes
	 *
	 * @param c Character value to append
	 * @param n Number of times to append
	 * @throws std::out_of_range Attempt to append beyond capacity
	 */
	inline void append(unsigned char c,unsigned int n)
	{
		if (unlikely((_l + n) > C))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		for(unsigned int i=0;i<n;++i)
			_b[_l++] = (char)c;
	}

	/**
	 * Append secure random bytes
	 *
	 * @param n Number of random bytes to append
	 */
	inline void appendRandom(unsigned int n)
	{
		if (unlikely((_l + n) > C))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		Utils::getSecureRandom(_b + _l,n);
		_l += n;
	}

	/**
	 * Append a C-array of bytes
	 *
	 * @param b Data
	 * @param l Length
	 * @throws std::out_of_range Attempt to append beyond capacity
	 */
	inline void append(const void *b,unsigned int l)
	{
		if (unlikely((_l + l) > C))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		memcpy(_b + _l,b,l);
		_l += l;
	}

	/**
	 * Append a C string including null termination byte
	 *
	 * @param s C string
	 * @throws std::out_of_range Attempt to append beyond capacity
	 */
	inline void appendCString(const char *s)
	{
		for(;;) {
			if (unlikely(_l >= C))
				throw ZT_EXCEPTION_OUT_OF_BOUNDS;
			if (!(_b[_l++] = *(s++)))
				break;
		}
	}

	/**
	 * Append a buffer
	 *
	 * @param b Buffer to append
	 * @tparam C2 Capacity of second buffer (typically inferred)
	 * @throws std::out_of_range Attempt to append beyond capacity
	 */
	template<unsigned int C2>
	inline void append(const Buffer<C2> &b)
	{
		append(b._b,b._l);
	}

	/**
	 * Increment size and return pointer to field of specified size
	 *
	 * Nothing is actually written to the memory. This is a shortcut
	 * for addSize() followed by field() to reference the previous
	 * position and the new size.
	 *
	 * @param l Length of field to append
	 * @return Pointer to beginning of appended field of length 'l'
	 */
	inline char *appendField(unsigned int l)
	{
		if (unlikely((_l + l) > C))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		char *r = _b + _l;
		_l += l;
		return r;
	}

	/**
	 * Increment size by a given number of bytes
	 *
	 * The contents of new space are undefined.
	 *
	 * @param i Bytes to increment
	 * @throws std::out_of_range Capacity exceeded
	 */
	inline void addSize(unsigned int i)
	{
		if (unlikely((i + _l) > C))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		_l += i;
	}

	/**
	 * Set size of data in buffer
	 *
	 * The contents of new space are undefined.
	 *
	 * @param i New size
	 * @throws std::out_of_range Size larger than capacity
	 */
	inline void setSize(const unsigned int i)
	{
		if (unlikely(i > C))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		_l = i;
	}

	/**
	 * Move everything after 'at' to the buffer's front and truncate
	 *
	 * @param at Truncate before this position
	 * @throws std::out_of_range Position is beyond size of buffer
	 */
	inline void behead(const unsigned int at)
	{
		if (!at)
			return;
		if (unlikely(at > _l))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		::memmove(_b,_b + at,_l -= at);
	}

	/**
	 * Erase something from the middle of the buffer
	 *
	 * @param start Starting position
	 * @param length Length of block to erase
	 * @throws std::out_of_range Position plus length is beyond size of buffer
	 */
	inline void erase(const unsigned int at,const unsigned int length)
	{
		const unsigned int endr = at + length;
		if (unlikely(endr > _l))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		::memmove(_b + at,_b + endr,_l - endr);
		_l -= length;
	}

	/**
	 * Set buffer data length to zero
	 */
	inline void clear() { _l = 0; }

	/**
	 * Zero buffer up to size()
	 */
	inline void zero() { memset(_b,0,_l); }

	/**
	 * Zero unused capacity area
	 */
	inline void zeroUnused() { memset(_b + _l,0,C - _l); }

	/**
	 * Unconditionally and securely zero buffer's underlying memory
	 */
	inline void burn() { Utils::burn(_b,sizeof(_b)); }

	/**
	 * @return Constant pointer to data in buffer
	 */
	inline const void *data() const { return _b; }

	/**
	 * @return Non-constant pointer to data in buffer
	 */
	inline void *unsafeData() { return _b; }

	/**
	 * @return Size of data in buffer
	 */
	inline unsigned int size() const { return _l; }

	/**
	 * @return Capacity of buffer
	 */
	inline unsigned int capacity() const { return C; }

	template<unsigned int C2>
	inline bool operator==(const Buffer<C2> &b) const
	{
		return ((_l == b._l)&&(!memcmp(_b,b._b,_l)));
	}
	template<unsigned int C2>
	inline bool operator!=(const Buffer<C2> &b) const
	{
		return ((_l != b._l)||(memcmp(_b,b._b,_l)));
	}
	template<unsigned int C2>
	inline bool operator<(const Buffer<C2> &b) const
	{
		return (memcmp(_b,b._b,std::min(_l,b._l)) < 0);
	}
	template<unsigned int C2>
	inline bool operator>(const Buffer<C2> &b) const
	{
		return (b < *this);
	}
	template<unsigned int C2>
	inline bool operator<=(const Buffer<C2> &b) const
	{
		return !(b < *this);
	}
	template<unsigned int C2>
	inline bool operator>=(const Buffer<C2> &b) const
	{
		return !(*this < b);
	}

private:
	char ZT_VAR_MAY_ALIAS _b[C];
	unsigned int _l;
};

} // namespace ZeroTier

#endif
