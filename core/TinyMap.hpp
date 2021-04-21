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

#ifndef ZT_TINYMAP_HPP
#define ZT_TINYMAP_HPP

#include "Constants.hpp"
#include "Containers.hpp"
#include "SharedPtr.hpp"
#include "Network.hpp"
#include "Spinlock.hpp"

// The number of buckets must be a power of two.
#define ZT_TINYMAP_BUCKETS 1024

#define ZT_TINYMAP_BUCKETS_MASK (ZT_TINYMAP_BUCKETS - 1)
#define ZT_TINYMAP_LOCKED_POINTER (~((uintptr_t)0))

namespace ZeroTier {

/**
 * A small, simple, and very fast hash map with a fixed bucket count.
 *
 * This is used where it's necessary to keep small numbers of items indexed by
 * an integer, such as networks mapping to network IDs. It's optimized for very
 * fast lookup, with lookups sometimes requiring only a few instructions. It
 * uses a "lock free" (actually pointer-as-spinlock) design.
 */
template< typename V >
class TinyMap
{
private:
	typedef Vector< std::pair< uint64_t, V > > EV;

public:
	ZT_INLINE TinyMap()
	{}

	ZT_INLINE ~TinyMap()
	{ this->clear(); }

	ZT_INLINE void clear()
	{
		for(unsigned int i=0; i < ZT_TINYMAP_BUCKETS; ++i) {
			for(;;) {
				const uintptr_t vptr = m_buckets[i].exchange(ZT_TINYMAP_LOCKED_POINTER, std::memory_order_acquire);
				if (likely(vptr != ZT_TINYMAP_LOCKED_POINTER)) {
					if (vptr != 0)
						delete reinterpret_cast<EV *>(vptr);
					m_buckets[i].store(0, std::memory_order_release);
					break;
				} else {
					Spinlock::pause();
				}
			}
		}
	}

	ZT_INLINE V get(const uint64_t key) noexcept
	{
		V tmp;
		std::atomic<uintptr_t> &bucket = m_buckets[(key ^ (key >> 32)) & ZT_TINYMAP_BUCKETS_MASK];
		for(;;) {
			const uintptr_t vptr = bucket.exchange(ZT_TINYMAP_LOCKED_POINTER, std::memory_order_acquire);
			if (likely(vptr != ZT_TINYMAP_LOCKED_POINTER)) {
				if (likely(vptr != 0)) {
					for(typename EV::const_iterator n(reinterpret_cast<const EV *>(vptr)->begin()); n != reinterpret_cast<const EV *>(vptr)->end(); ++n) {
						if (likely(n->first == key)) {
							tmp = n->second;
							break;
						}
					}
				}
				bucket.store(vptr, std::memory_order_release);
				return tmp;
			} else {
				Spinlock::pause();
			}
		}
	}

	ZT_INLINE void set(const uint64_t key, const V &value)
	{
		std::atomic<uintptr_t> &bucket = m_buckets[(key ^ (key >> 32)) & ZT_TINYMAP_BUCKETS_MASK];
		for(;;) {
			uintptr_t vptr = bucket.exchange(ZT_TINYMAP_LOCKED_POINTER, std::memory_order_acquire);
			if (likely(vptr != ZT_TINYMAP_LOCKED_POINTER)) {
				if (vptr == 0) {
					vptr = reinterpret_cast<uintptr_t>(new EV());
				} else {
					for (typename EV::iterator n(reinterpret_cast<EV *>(vptr)->begin()); n != reinterpret_cast<EV *>(vptr)->end(); ++n) {
						if (n->first == key) {
							n->second = value;
							bucket.store(vptr, std::memory_order_release);
							return;
						}
					}
				}
				reinterpret_cast<EV *>(vptr)->push_back(std::pair< uint64_t, V >(key, value));
				bucket.store(vptr, std::memory_order_release);
				return;
			} else {
				Spinlock::pause();
			}
		}
	}

	ZT_INLINE void erase(const uint64_t key)
	{
		std::atomic<uintptr_t> &bucket = m_buckets[(key ^ (key >> 32)) & ZT_TINYMAP_BUCKETS_MASK];
		for(;;) {
			uintptr_t vptr = bucket.exchange(ZT_TINYMAP_LOCKED_POINTER, std::memory_order_acquire);
			if (likely(vptr != ZT_TINYMAP_LOCKED_POINTER)) {
				if (likely(vptr != 0)) {
					for (typename EV::iterator n(reinterpret_cast<EV *>(vptr)->begin()); n != reinterpret_cast<EV *>(vptr)->end(); ++n) {
						if (n->first == key) {
							reinterpret_cast<EV *>(vptr)->erase(n);
							break;
						}
					}
					if (reinterpret_cast<EV *>(vptr)->empty()) {
						delete reinterpret_cast<EV *>(vptr);
						vptr = 0;
					}
				}
				bucket.store(vptr, std::memory_order_release);
				return;
			} else {
				Spinlock::pause();
			}
		}
	}

private:
	std::atomic<uintptr_t> m_buckets[ZT_TINYMAP_BUCKETS];
};

static_assert((ZT_TINYMAP_BUCKETS % (sizeof(uintptr_t) * 8)) == 0, "ZT_TINYMAP_BUCKETS is not a power of two");

} // namespace ZeroTier

#endif
