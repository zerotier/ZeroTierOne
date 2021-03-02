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

#ifndef ZT_SPINLOCK_HPP
#define ZT_SPINLOCK_HPP

#include "OS.hpp"

#include <thread>

/**
 * Simple spinlock
 *
 * This can be used in place of Mutex to lock things that are extremely fast
 * to access. It should be used very sparingly.
 */
class Spinlock
{
public:
	ZT_INLINE Spinlock() noexcept
	{ m_locked.clear(); }

	ZT_INLINE void lock() noexcept
	{
		if (unlikely(m_locked.test_and_set(std::memory_order_acquire))) {
			do {
				std::this_thread::yield();
			} while (m_locked.test_and_set(std::memory_order_acquire));
		}
	}

	ZT_INLINE void unlock() noexcept
	{ m_locked.clear(std::memory_order_release); }

private:
	ZT_INLINE Spinlock(const Spinlock &) noexcept {}
	ZT_INLINE const Spinlock &operator=(const Spinlock &) noexcept { return *this; }

	std::atomic_flag m_locked;
};

#endif
