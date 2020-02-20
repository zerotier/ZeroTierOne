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
		b = (Buf *)bb;
		s_pool.store(b->__nextInPool);
	} else {
		s_pool.store(0);
		b = (Buf *)malloc(sz);
		if (!b)
			throw std::bad_alloc();
	}

	b->__refCount.store(0);
	return (void *)b;
}

void Buf::operator delete(void *ptr)
{
	if (ptr) {
		uintptr_t bb;
		for (;;) {
			bb = s_pool.exchange(ZT_ATOMIC_PTR_LOCKED);
			if (bb != ZT_ATOMIC_PTR_LOCKED)
				break;
			sched_yield();
		}

		((Buf *)ptr)->__nextInPool = bb;
		s_pool.store((uintptr_t)ptr);
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
		uintptr_t next = ((Buf *)bb)->__nextInPool;
		free((void *)bb);
		bb = next;
	}
}

} // namespace ZeroTier
