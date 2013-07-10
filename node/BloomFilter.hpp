/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef _ZT_BLOOMFILTER_HPP
#define _ZT_BLOOMFILTER_HPP

#include <string.h>
#include "Utils.hpp"

namespace ZeroTier {

/**
 * A simple bit field bloom filter
 *
 * This actually isn't a total filter, in that it does not contain a hashing
 * algorithm. It's up to the caller to hash/sum the items being remembered
 * so that the distribution of 'n' is random.
 *
 * @tparam B Size in BITS, must be a multiple of 8
 */
template<unsigned int B>
class BloomFilter
{
public:
	/**
	 * Construct an empty filter
	 */
	BloomFilter()
		throw()
	{
		memset(_field,0,sizeof(_field));
	}

	/**
	 * Construct from a raw filter
	 *
	 * @param b Raw filter bits, must be exactly bytes() in length, or NULL to construct empty
	 */
	BloomFilter(const void *b)
		throw()
	{
		if (b)
			memcpy(_field,b,sizeof(_field));
		else memset(_field,0,sizeof(_field));
	}

	/**
	 * @return Size of filter in bits
	 */
	static inline unsigned int bits() throw() { return B; }

	/**
	 * @return Size of filter in bytes
	 */
	static inline unsigned int bytes() throw() { return (B / 8); }

	/**
	 * @return Pointer to portable array of bytes of bytes() length representing filter
	 */
	inline const unsigned char *data() const throw() { return _field; }

	/**
	 * Clear all bits in filter
	 */
	inline void clear()
		throw()
	{
		memset(_field,0,sizeof(_field));
	}

	/**
	 * @param n Value to set
	 * @return True if corresponding bit was already set before this operation
	 */
	inline bool set(unsigned int n)
		throw()
	{
		n %= B;
		unsigned char *const x = _field + (n / 8);
		const unsigned char m = (1 << (n % 8));
		bool already = ((*x & m));
		*x |= m;
		return already;
	}

	/**
	 * @param n Value to test
	 * @return True if value is present is set
	 */
	inline bool contains(unsigned int n)
		throw()
	{
		n %= B;
		return (_field[n / 8] & (1 << (n % 8)));
	}

	/**
	 * Clear a random bit in this bloom filter
	 */
	inline void decay()
		throw()
	{
		const unsigned int rn = Utils::randomInt<unsigned int>();
		_field[(rn >> 3) % (B / 8)] &= ~((unsigned char)(1 << (rn & 7)));
	}

private:
	unsigned char _field[B / 8];
};

} // namespace ZeroTier

#endif
