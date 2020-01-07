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

#ifndef ZT_SCOPEDPTR_HPP
#define ZT_SCOPEDPTR_HPP

#include "Constants.hpp"

namespace ZeroTier {

/**
 * Simple scoped pointer
 *
 * This is used in the core to avoid requiring C++11 and because auto_ptr is weird.
 */
template<typename T>
class ScopedPtr
{
public:
	ZT_ALWAYS_INLINE ScopedPtr(T *const p) : _p(p) {}
	ZT_ALWAYS_INLINE ~ScopedPtr() { delete _p; }

	ZT_ALWAYS_INLINE T *operator->() const { return _p; }
	ZT_ALWAYS_INLINE T &operator*() const { return *_p; }
	ZT_ALWAYS_INLINE operator bool() const { return (_p != (T *)0); }
	ZT_ALWAYS_INLINE T *ptr() const { return _p; }

	ZT_ALWAYS_INLINE bool operator==(const ScopedPtr &p) const { return (_p == p._p); }
	ZT_ALWAYS_INLINE bool operator!=(const ScopedPtr &p) const { return (_p != p._p); }
	ZT_ALWAYS_INLINE bool operator==(T *const p) const { return (_p == p); }
	ZT_ALWAYS_INLINE bool operator!=(T *const p) const { return (_p != p); }

private:
	ZT_ALWAYS_INLINE ScopedPtr() {}
	ZT_ALWAYS_INLINE ScopedPtr(const ScopedPtr &p) : _p(nullptr) {}
	ZT_ALWAYS_INLINE ScopedPtr &operator=(const ScopedPtr &p) { return *this; }
	T *const _p;
};

} // namespace ZeroTier

#endif
