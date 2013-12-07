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

#ifndef ZT_CMWC4096_HPP
#define ZT_CMWC4096_HPP

#include <stdint.h>
#include "Utils.hpp"

namespace ZeroTier {

/** 
 * Complement Multiply With Carry random number generator
 *
 * Based on original code posted to Usenet in the public domain by
 * George Marsaglia. Period is approximately 2^131086.
 *
 * This is not used for cryptographic purposes but for a very fast
 * and high-quality PRNG elsewhere in the code.
 */
class CMWC4096
{
public:
	/**
	 * Construct and initialize from secure random source
	 */
	CMWC4096()
		throw()
	{
		Utils::getSecureRandom(Q,sizeof(Q));
		Utils::getSecureRandom(&c,sizeof(c));
		c %= 809430660;
		i = 4095;
	}

	inline uint32_t next32()
		throw()
	{
		uint32_t __i = ++i & 4095;
		const uint64_t t = (18782ULL * (uint64_t)Q[__i]) + (uint64_t)c;
		c = (uint32_t)(t >> 32);
		uint32_t x = c + (uint32_t)t;
		const uint32_t p = (uint32_t)(x < c); x += p; c += p;
		return (Q[__i] = 0xfffffffe - x);
	}

	inline uint64_t next64()
		throw()
	{
		return ((((uint64_t)next32()) << 32) ^ (uint64_t)next32());
	}

	inline double nextDouble()
		throw()
	{
		return ((double)(next32()) / 4294967296.0);
	}

private:
	uint32_t Q[4096];
	uint32_t c;
	uint32_t i;
};

} // namespace ZeroTier

#endif
