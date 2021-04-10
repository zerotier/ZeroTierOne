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

#ifndef ZT_SHAREDPTR_HPP
#define ZT_SHAREDPTR_HPP

#include "Constants.hpp"
#include "TriviallyCopyable.hpp"

namespace ZeroTier {

/**
 * An intrusive reference counted pointer.
 *
 * Classes must have an atomic<int> field called __refCount and set this class
 * as a friend to be used with this.
 */
template< typename T >
class SharedPtr : public TriviallyCopyable
{
public:
	ZT_INLINE SharedPtr() noexcept: m_ptr(nullptr)
	{}

	explicit ZT_INLINE SharedPtr(T *obj) noexcept: m_ptr(obj)
	{ if (likely(obj != nullptr)) const_cast<std::atomic< int > *>(&(obj->__refCount))->fetch_add(1, std::memory_order_acquire); }

	ZT_INLINE SharedPtr(const SharedPtr &sp) noexcept: m_ptr(sp.m_acquire())
	{}

	ZT_INLINE ~SharedPtr()
	{ m_release(); }

	ZT_INLINE SharedPtr &operator=(const SharedPtr &sp)
	{
		if (likely(m_ptr != sp.m_ptr)) {
			T *const p = sp.m_acquire();
			m_release();
			m_ptr = p;
		}
		return *this;
	}

	ZT_INLINE void set(T *ptr) noexcept
	{
		m_release();
		const_cast<std::atomic< int > *>(&((m_ptr = ptr)->__refCount))->fetch_add(1, std::memory_order_acquire);
	}

	/**
	 * Swap with another pointer.
	 *
	 * This is much faster than using assignment as it requires no atomic
	 * operations at all.
	 *
	 * @param with Pointer to swap with
	 */
	ZT_INLINE void swap(SharedPtr &with) noexcept
	{
		T *const tmp = m_ptr;
		m_ptr = with.m_ptr;
		with.m_ptr = tmp;
	}

	/**
	 * Move pointer from another SharedPtr to this one, zeroing target.
	 *
	 * This is faster than assignment as it saves one atomically synchronized
	 * increment. If this pointer is null there are no atomic operations at
	 * all.
	 *
	 * @param from Source pointer; will be changed to NULL
	 */
	ZT_INLINE void move(SharedPtr &from)
	{
		m_release();
		m_ptr = from.m_ptr;
		from.m_ptr = nullptr;
	}

	ZT_INLINE operator bool() const noexcept
	{ return (m_ptr != nullptr); }

	ZT_INLINE T &operator*() const noexcept
	{ return *m_ptr; }

	ZT_INLINE T *operator->() const noexcept
	{ return m_ptr; }

	/**
	 * @return Raw pointer to held object
	 */
	ZT_INLINE T *ptr() const noexcept
	{ return m_ptr; }

	/**
	 * Set this pointer to NULL
	 */
	ZT_INLINE void zero()
	{
		m_release();
		m_ptr = nullptr;
	}

	/**
	 * Return held object and null this pointer if reference count is one.
	 *
	 * If the reference count is one, the reference count is changed to zero
	 * and the object is returned. It is not deleted; the caller must do that
	 * if that is desired. This pointer will be set to NULL. If the reference
	 * count is not one nothing happens and NULL is returned.
	 *
	 * @return Pointer or NULL if more than one reference
	 */
	ZT_INLINE T *weakGC()
	{
		if (likely(m_ptr != nullptr)) {
			int one = 1;
			if (const_cast<std::atomic< int > *>(&(m_ptr->__refCount))->compare_exchange_strong(one, (int)0)) {
				T *const ptr = m_ptr;
				m_ptr = nullptr;
				return ptr;
			} else {
				return nullptr;
			}
		} else {
			return nullptr;
		}
	}

	ZT_INLINE unsigned long hashCode() const noexcept
	{ return (unsigned long)((uintptr_t)m_ptr + (uintptr_t)Utils::hash32((uint32_t)m_ptr)); }

	ZT_INLINE bool operator==(const SharedPtr &sp) const noexcept
	{ return (m_ptr == sp.m_ptr); }

	ZT_INLINE bool operator!=(const SharedPtr &sp) const noexcept
	{ return (m_ptr != sp.m_ptr); }

	ZT_INLINE bool operator>(const SharedPtr &sp) const noexcept
	{ return (reinterpret_cast<const uint8_t *>(m_ptr) > reinterpret_cast<const uint8_t *>(sp.m_ptr)); }

	ZT_INLINE bool operator<(const SharedPtr &sp) const noexcept
	{ return (reinterpret_cast<const uint8_t *>(m_ptr) < reinterpret_cast<const uint8_t *>(sp.m_ptr)); }

	ZT_INLINE bool operator>=(const SharedPtr &sp) const noexcept
	{ return (reinterpret_cast<const uint8_t *>(m_ptr) >= reinterpret_cast<const uint8_t *>(sp.m_ptr)); }

	ZT_INLINE bool operator<=(const SharedPtr &sp) const noexcept
	{ return (reinterpret_cast<const uint8_t *>(m_ptr) <= reinterpret_cast<const uint8_t *>(sp.m_ptr)); }

private:
	ZT_INLINE T *m_acquire() const noexcept
	{
		if (likely(m_ptr != nullptr))
			const_cast<std::atomic< int > *>(&(m_ptr->__refCount))->fetch_add(1, std::memory_order_acquire);
		return m_ptr;
	}

	ZT_INLINE void m_release() const noexcept
	{
		if (unlikely((m_ptr != nullptr)&&(const_cast<std::atomic< int > *>(&(m_ptr->__refCount))->fetch_sub(1, std::memory_order_release) <= 1)))
			delete m_ptr;
	}

	T *m_ptr;
};

} // namespace ZeroTier

// Augment std::swap to speed up some operations with SharedPtr.
namespace std {

template< typename T >
ZT_MAYBE_UNUSED ZT_INLINE void swap(ZeroTier::SharedPtr< T > &a, ZeroTier::SharedPtr< T > &b) noexcept
{ a.swap(b); }

template< typename T >
ZT_MAYBE_UNUSED ZT_INLINE void move(ZeroTier::SharedPtr< T > &a, ZeroTier::SharedPtr< T > &b) noexcept
{ a.move(b); }

} // namespace std

#endif
