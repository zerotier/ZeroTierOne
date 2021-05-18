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

#include "Constants.hpp"
#include "Mutex.hpp"

#ifdef __LINUX__
#include <sched.h>
#else
#include <thread>
#endif

/**
 * Simple spinlock
 *
 * This can be used in place of Mutex to lock things that are extremely fast
 * to access. It should be used very sparingly.
 */
class Spinlock {
  public:
    /**
     * Pause current thread using whatever methods might be available
     *
     * This is broken out since it's used in a few other places where
     * spinlock-like constructions are used.
     */
    ZT_INLINE static void pause() noexcept
    {
#ifdef ZT_ARCH_X64
        _mm_pause();
#endif
#ifdef __LINUX__
        sched_yield();
#else
        std::this_thread::yield();
#endif
    }

    ZT_INLINE Spinlock() noexcept : m_locked(false) {}

    ZT_INLINE void lock() noexcept
    {
        if (unlikely(m_locked.test_and_set(std::memory_order_acquire))) {
            do {
                Spinlock::pause();
            } while (m_locked.test_and_set(std::memory_order_acquire));
        }
    }

    ZT_INLINE void unlock() noexcept { m_locked.clear(std::memory_order_release); }

  private:
    ZT_INLINE Spinlock(const Spinlock &) noexcept {}
    ZT_INLINE const Spinlock &operator=(const Spinlock &) noexcept { return *this; }

    std::atomic_flag m_locked;
};

#endif
