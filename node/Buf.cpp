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

namespace ZeroTier {

static std::atomic<uintptr_t> s_pool(0);

void *Buf::operator new(std::size_t sz) noexcept
{
	uintptr_t bb;
	for (;;) {
		bb = s_pool.exchange(~((uintptr_t)0));
		if (bb != ~((uintptr_t)0))
			break;
	}

	Buf *b;
	if (bb) {
		b = (Buf *)bb;
		s_pool.store(b->__nextInPool);
	} else {
		s_pool.store(0);
		b = (Buf *)malloc(sz);
		if (!b)
			return nullptr;
	}

	b->__refCount.store(0);
	return (void *)b;
}

void Buf::operator delete(void *ptr) noexcept
{
	if (ptr) {
		uintptr_t bb;
		for (;;) {
			bb = s_pool.exchange(~((uintptr_t)0));
			if (bb != ~((uintptr_t)0))
				break;
		}

		((Buf *)ptr)->__nextInPool = bb;
		s_pool.store((uintptr_t)ptr);
	}
}

void Buf::freePool() noexcept
{
	uintptr_t bb;
	for (;;) {
		bb = s_pool.exchange(~((uintptr_t)0));
		if (bb != ~((uintptr_t)0))
			break;
	}
	s_pool.store((uintptr_t)0);

	while (bb != 0) {
		uintptr_t next = ((Buf *)bb)->__nextInPool;
		free((void *)bb);
		bb = next;
	}
}

} // namespace ZeroTier
