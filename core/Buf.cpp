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

#include "Buf.hpp"

#include "Spinlock.hpp"

namespace ZeroTier {

static std::atomic<uintptr_t> s_pool(0);
static std::atomic<long> s_allocated(0);

// uintptr_max can never be a valid pointer, so use it to indicate that s_pool is locked (very short duration spinlock)
#define ZT_ATOMIC_PTR_LOCKED (~((uintptr_t)0))

void* Buf::operator new(std::size_t sz)
{
    uintptr_t bb;
    for (;;) {
        bb = s_pool.exchange(ZT_ATOMIC_PTR_LOCKED, std::memory_order_acquire);

        if (likely(bb != ZT_ATOMIC_PTR_LOCKED)) {
            Buf* b;
            if (likely(bb != 0)) {
                b = reinterpret_cast<Buf*>(bb);
                s_pool.store(b->__nextInPool, std::memory_order_release);
            }
            else {
                s_pool.store(0, std::memory_order_release);
                b = reinterpret_cast<Buf*>(malloc(sz));
                if (! b)
                    throw Utils::BadAllocException;
                s_allocated.fetch_add(1, std::memory_order_relaxed);
            }

            b->__refCount.store(0, std::memory_order_relaxed);

            return reinterpret_cast<void*>(b);
        }

        Spinlock::pause();
    }
}

void Buf::operator delete(void* ptr)
{
    if (likely(ptr != nullptr)) {
        if (s_allocated.load(std::memory_order_relaxed) > ZT_BUF_MAX_POOL_SIZE) {
            s_allocated.fetch_sub(1, std::memory_order_relaxed);
            free(ptr);
        }
        else {
            uintptr_t bb;
            for (;;) {
                bb = s_pool.exchange(ZT_ATOMIC_PTR_LOCKED, std::memory_order_acquire);
                if (likely(bb != ZT_ATOMIC_PTR_LOCKED)) {
                    reinterpret_cast<Buf*>(ptr)->__nextInPool = bb;
                    s_pool.store(reinterpret_cast<uintptr_t>(ptr), std::memory_order_release);
                    return;
                }
                Spinlock::pause();
            }
        }
    }
}

void Buf::freePool() noexcept
{
    uintptr_t bb;
    for (;;) {
        bb = s_pool.exchange(ZT_ATOMIC_PTR_LOCKED, std::memory_order_acquire);

        if (likely(bb != ZT_ATOMIC_PTR_LOCKED)) {
            s_pool.store(0, std::memory_order_release);

            while (bb != 0) {
                const uintptr_t next = reinterpret_cast<Buf*>(bb)->__nextInPool;
                s_allocated.fetch_sub(1, std::memory_order_relaxed);
                free(reinterpret_cast<void*>(bb));
                bb = next;
            }

            return;
        }

        Spinlock::pause();
    }
}

long Buf::poolAllocated() noexcept
{
    return s_allocated.load(std::memory_order_relaxed);
}

}   // namespace ZeroTier
