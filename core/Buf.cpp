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

#include "Buf.hpp"

#ifdef __WINDOWS__
#define sched_yield() Sleep(0)
#endif

namespace ZeroTier {

static std::atomic<uintptr_t> s_pool(0);
static std::atomic<long> s_allocated(0);

// uintptr_max can never be a valid pointer, so use it to indicate that s_pool is locked (very short duration spinlock)
#define ZT_ATOMIC_PTR_LOCKED (~((uintptr_t)0))

void *Buf::operator new(std::size_t sz)
{
	uintptr_t bb;
	for (;;) {
		bb = s_pool.exchange(ZT_ATOMIC_PTR_LOCKED);
		if (bb != ZT_ATOMIC_PTR_LOCKED)
			break;
		sched_yield();
	}

	Buf *b;
	if (bb) {
		s_pool.store(((Buf *) bb)->__nextInPool);
		b = (Buf *) bb;
	} else {
		s_pool.store(0);
		b = (Buf *) malloc(sz);
		if (!b)
			throw Utils::BadAllocException;
		++s_allocated;
	}

	b->__refCount.store(0);
	return (void *) b;
}

void Buf::operator delete(void *ptr)
{
	if (ptr) {
		if (s_allocated.load() > ZT_BUF_MAX_POOL_SIZE) {
			--s_allocated;
			free(ptr);
		} else {
			uintptr_t bb;
			for (;;) {
				bb = s_pool.exchange(ZT_ATOMIC_PTR_LOCKED);
				if (bb != ZT_ATOMIC_PTR_LOCKED)
					break;
				sched_yield();
			}

			((Buf *) ptr)->__nextInPool.store(bb);
			s_pool.store((uintptr_t) ptr);
		}
	}
}

void Buf::freePool() noexcept
{
	uintptr_t bb;
	for (;;) {
		bb = s_pool.exchange(ZT_ATOMIC_PTR_LOCKED);
		if (bb != ZT_ATOMIC_PTR_LOCKED)
			break;
		sched_yield();
	}
	s_pool.store(0);

	while (bb != 0) {
		const uintptr_t next = ((Buf *) bb)->__nextInPool;
		--s_allocated;
		free((void *) bb);
		bb = next;
	}
}

long Buf::poolAllocated() noexcept
{
	return s_allocated.load();
}

} // namespace ZeroTier
