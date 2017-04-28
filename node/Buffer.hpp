/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

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
		throw(std::out_of_range)
	{
		if (l > C)
			throw std::out_of_range("Buffer: construct with size larger than capacity");
		_l = l;
	}

	template<unsigned int C2>
	Buffer(const Buffer<C2> &b)
		throw(std::out_of_range)
	{
		*this = b;
	}

	Buffer(const void *b,unsigned int l)
		throw(std::out_of_range)
	{
		copyFrom(b,l);
	}

	Buffer(const std::string &s)
		throw(std::out_of_range)
	{
		copyFrom(s.data(),s.length());
	}

	template<unsigned int C2>
	inline Buffer &operator=(const Buffer<C2> &b)
		throw(std::out_of_range)
	{
		if (b._l > C)
			throw std::out_of_range("Buffer: assignment from buffer larger than capacity");
		memcpy(_b,b._b,_l = b._l);
		return *this;
	}

	inline Buffer &operator=(const std::string &s)
		throw(std::out_of_range)
	{
		copyFrom(s.data(),s.length());
		return *this;
	}

	inline void copyFrom(const void *b,unsigned int l)
		throw(std::out_of_range)
	{
		if (l > C)
			throw std::out_of_range("Buffer: set from C array larger than capacity");
		_l = l;
		memcpy(_b,b,l);
	}

	unsigned char operator[](const unsigned int i) const
		throw(std::out_of_range)
	{
		if (i >= _l)
			throw std::out_of_range("Buffer: [] beyond end of data");
		return (unsigned char)_b[i];
	}

	unsigned char &operator[](const unsigned int i)
		throw(std::out_of_range)
	{
		if (i >= _l)
			throw std::out_of_range("Buffer: [] beyond end of data");
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
		throw(std::out_of_range)
	{
		if ((i + l) > _l)
			throw std::out_of_range("Buffer: field() beyond end of data");
		return (unsigned char *)(_b + i);
	}
	const unsigned char *field(unsigned int i,unsigned int l) const
		throw(std::out_of_range)
	{
		if ((i + l) > _l)
			throw std::out_of_range("Buffer: field() beyond end of data");
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
		throw(std::out_of_range)
	{
		if ((i + sizeof(T)) > _l)
			throw std::out_of_range("Buffer: setAt() beyond end of data");
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
		throw(std::out_of_range)
	{
		if ((i + sizeof(T)) > _l)
			throw std::out_of_range("Buffer: at() beyond end of data");
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
		throw(std::out_of_range)
	{
		if ((_l + sizeof(T)) > C)
			throw std::out_of_range("Buffer: append beyond capacity");
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
		throw(std::out_of_range)
	{
		if ((_l + n) > C)
			throw std::out_of_range("Buffer: append beyond capacity");
		for(unsigned int i=0;i<n;++i)
			_b[_l++] = (char)c;
	}

	/**
	 * Append a C-array of bytes
	 *
	 * @param b Data
	 * @param l Length
	 * @throws std::out_of_range Attempt to append beyond capacity
	 */
	inline void append(const void *b,unsigned int l)
		throw(std::out_of_range)
	{
		if ((_l + l) > C)
			throw std::out_of_range("Buffer: append beyond capacity");
		memcpy(_b + _l,b,l);
		_l += l;
	}

	/**
	 * Append a string
	 *
	 * @param s String to append
	 * @throws std::out_of_range Attempt to append beyond capacity
	 */
	inline void append(const std::string &s)
		throw(std::out_of_range)
	{
		append(s.data(),(unsigned int)s.length());
	}

	/**
	 * Append a C string including null termination byte
	 *
	 * @param s C string
	 * @throws std::out_of_range Attempt to append beyond capacity
	 */
	inline void appendCString(const char *s)
		throw(std::out_of_range)
	{
		for(;;) {
			if (_l >= C)
				throw std::out_of_range("Buffer: append beyond capacity");
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
		throw(std::out_of_range)
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
		throw(std::out_of_range)
	{
		if ((_l + l) > C)
			throw std::out_of_range("Buffer: append beyond capacity");
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
		throw(std::out_of_range)
	{
		if ((i + _l) > C)
			throw std::out_of_range("Buffer: setSize to larger than capacity");
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
		throw(std::out_of_range)
	{
		if (i > C)
			throw std::out_of_range("Buffer: setSize to larger than capacity");
		_l = i;
	}

	/**
	 * Move everything after 'at' to the buffer's front and truncate
	 *
	 * @param at Truncate before this position
	 * @throw std::out_of_range Position is beyond size of buffer
	 */
	inline void behead(const unsigned int at)
		throw(std::out_of_range)
	{
		if (!at)
			return;
		if (at > _l)
			throw std::out_of_range("Buffer: behead() beyond capacity");
		::memmove(_b,_b + at,_l -= at);
	}

	/**
	 * Erase something from the middle of the buffer
	 *
	 * @param start Starting position
	 * @param length Length of block to erase
	 * @throw std::out_of_range Position plus length is beyond size of buffer
	 */
	inline void erase(const unsigned int at,const unsigned int length)
		throw(std::out_of_range)
	{
		const unsigned int endr = at + length;
		if (endr > _l)
			throw std::out_of_range("Buffer: erase() range beyond end of buffer");
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
	unsigned int _l;
	char ZT_VAR_MAY_ALIAS _b[C];
};

} // namespace ZeroTier

#endif
