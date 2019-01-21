/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_ATOMICCOUNTER_HPP
#define ZT_ATOMICCOUNTER_HPP

#include "Constants.hpp"

#ifndef __GNUC__
#include <atomic>
#endif

namespace ZeroTier {

/**
 * Simple atomic counter supporting increment and decrement
 */
class AtomicCounter
{
public:
	AtomicCounter() { _v = 0; }

	inline int load() const
	{
#ifdef __GNUC__
		return __sync_or_and_fetch(const_cast<int *>(&_v),0);
#else
		return _v.load();
#endif
	}

	inline int operator++()
	{
#ifdef __GNUC__
		return __sync_add_and_fetch(&_v,1);
#else
		return ++_v;
#endif
	}

	inline int operator--()
	{
#ifdef __GNUC__
		return __sync_sub_and_fetch(&_v,1);
#else
		return --_v;
#endif
	}

private:
	AtomicCounter(const AtomicCounter &) {}
	const AtomicCounter &operator=(const AtomicCounter &) { return *this; }

#ifdef __GNUC__
	int _v;
#else
	std::atomic_int _v;
#endif
};

} // namespace ZeroTier

#endif
