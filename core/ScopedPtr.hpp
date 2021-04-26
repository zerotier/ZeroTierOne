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
template <typename T> class ScopedPtr : public TriviallyCopyable {
  public:
    explicit ZT_INLINE ScopedPtr(T* const p) noexcept : m_ptr(p)
    {
    }

    ZT_INLINE ~ScopedPtr()
    {
        delete m_ptr;
    }

    ZT_INLINE T* operator->() const noexcept
    {
        return m_ptr;
    }

    ZT_INLINE T& operator*() const noexcept
    {
        return *m_ptr;
    }

    ZT_INLINE T* ptr() const noexcept
    {
        return m_ptr;
    }

    ZT_INLINE void swap(const ScopedPtr& p) noexcept
    {
        T* const tmp = m_ptr;
        m_ptr = p.m_ptr;
        p.m_ptr = tmp;
    }

    explicit ZT_INLINE operator bool() const noexcept
    {
        return (m_ptr != (T*)0);
    }

    ZT_INLINE bool operator==(const ScopedPtr& p) const noexcept
    {
        return (m_ptr == p.m_ptr);
    }

    ZT_INLINE bool operator!=(const ScopedPtr& p) const noexcept
    {
        return (m_ptr != p.m_ptr);
    }

    ZT_INLINE bool operator==(T* const p) const noexcept
    {
        return (m_ptr == p);
    }

    ZT_INLINE bool operator!=(T* const p) const noexcept
    {
        return (m_ptr != p);
    }

  private:
    ZT_INLINE ScopedPtr() noexcept
    {
    }

    ZT_INLINE ScopedPtr(const ScopedPtr& p) noexcept : m_ptr(nullptr)
    {
    }

    ZT_INLINE ScopedPtr& operator=(const ScopedPtr& p) noexcept
    {
        return *this;
    }

    T* const m_ptr;
};

}   // namespace ZeroTier

namespace std {
template <typename T> ZT_INLINE void swap(ZeroTier::ScopedPtr<T>& a, ZeroTier::ScopedPtr<T>& b) noexcept
{
    a.swap(b);
}
}   // namespace std

#endif
