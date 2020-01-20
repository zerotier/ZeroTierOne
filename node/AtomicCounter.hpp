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

#ifndef ZT_ATOMICCOUNTER_HPP
#define ZT_ATOMICCOUNTER_HPP

#include "Constants.hpp"

#ifndef __GNUC__
#include <atomic>
#endif

namespace ZeroTier {

/**
 * Simple atomic counter supporting increment and decrement
 *
 * This is used as the reference counter in reference counted objects that
 * work with SharedPtr<>.
 */
class AtomicCounter
{
public:
	ZT_ALWAYS_INLINE AtomicCounter() : _v(0) {}

	ZT_ALWAYS_INLINE int load() const
	{
#ifdef __GNUC__
		return _v;
#else
		return _v.load();
#endif
	}

	ZT_ALWAYS_INLINE void zero() { _v = 0; }

	ZT_ALWAYS_INLINE int operator++()
	{
#ifdef __GNUC__
		return __sync_add_and_fetch((int *)&_v,1);
#else
		return ++_v;
#endif
	}

	ZT_ALWAYS_INLINE int operator--()
	{
#ifdef __GNUC__
		return __sync_sub_and_fetch((int *)&_v,1);
#else
		return --_v;
#endif
	}

private:
	ZT_ALWAYS_INLINE AtomicCounter(const AtomicCounter &) {}
	ZT_ALWAYS_INLINE const AtomicCounter &operator=(const AtomicCounter &) { return *this; }

#ifdef __GNUC__
	volatile int _v;
#else
	std::atomic_int _v;
#endif
};

} // namespace ZeroTier

#endif
