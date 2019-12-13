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
	inline AtomicCounter() { _v = 0; }

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
	inline AtomicCounter(const AtomicCounter &) {}
	inline const AtomicCounter &operator=(const AtomicCounter &) { return *this; }

#ifdef __GNUC__
	int _v;
#else
	std::atomic_int _v;
#endif
};

} // namespace ZeroTier

#endif
