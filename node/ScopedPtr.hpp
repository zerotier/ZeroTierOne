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

#ifndef ZT_SCOPEDPTR_HPP
#define ZT_SCOPEDPTR_HPP

#include "Constants.hpp"
#include "TriviallyCopyable.hpp"

namespace ZeroTier {

/**
 * Simple scoped pointer
 *
 * This is used in the core to avoid requiring C++11 and because auto_ptr is weird.
 */
template<typename T>
class ScopedPtr : public TriviallyCopyable
{
public:
	explicit ZT_ALWAYS_INLINE ScopedPtr(T *const p) noexcept : _p(p) {}
	ZT_ALWAYS_INLINE ~ScopedPtr() { delete _p; }

	ZT_ALWAYS_INLINE T *operator->() const noexcept { return _p; }
	ZT_ALWAYS_INLINE T &operator*() const noexcept { return *_p; }
	explicit ZT_ALWAYS_INLINE operator bool() const noexcept { return (_p != (T *)0); }
	ZT_ALWAYS_INLINE T *ptr() const noexcept { return _p; }

	ZT_ALWAYS_INLINE void swap(const ScopedPtr &p) noexcept
	{
		T *const tmp = _p;
		_p = p._p;
		p._p = tmp;
	}

	ZT_ALWAYS_INLINE bool operator==(const ScopedPtr &p) const noexcept { return (_p == p._p); }
	ZT_ALWAYS_INLINE bool operator!=(const ScopedPtr &p) const noexcept { return (_p != p._p); }
	ZT_ALWAYS_INLINE bool operator==(T *const p) const noexcept { return (_p == p); }
	ZT_ALWAYS_INLINE bool operator!=(T *const p) const noexcept { return (_p != p); }

private:
	ZT_ALWAYS_INLINE ScopedPtr() noexcept {}
	ZT_ALWAYS_INLINE ScopedPtr(const ScopedPtr &p) noexcept : _p(nullptr) {}
	ZT_ALWAYS_INLINE ScopedPtr &operator=(const ScopedPtr &p) noexcept { return *this; }

	T *const _p;
};

} // namespace ZeroTier

namespace std {
template<typename T>
ZT_ALWAYS_INLINE void swap(ZeroTier::ScopedPtr<T> &a,ZeroTier::ScopedPtr<T> &b) noexcept { a.swap(b); }
}

#endif
