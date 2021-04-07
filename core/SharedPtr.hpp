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
 * An introspective reference counted pointer.
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

	ZT_INLINE SharedPtr(const SharedPtr &sp) noexcept: m_ptr(sp._getAndInc())
	{}

	ZT_INLINE ~SharedPtr()
	{ _release(); }

	ZT_INLINE SharedPtr &operator=(const SharedPtr &sp)
	{
		if (likely(m_ptr != sp.m_ptr)) {
			T *const p = sp._getAndInc();
			_release();
			m_ptr = p;
		}
		return *this;
	}

	ZT_INLINE void set(T *ptr) noexcept
	{
		_release();
		const_cast<std::atomic< int > *>(&((m_ptr = ptr)->__refCount))->fetch_add(1, std::memory_order_acquire);
	}

	/**
	 * Swap with another pointer 'for free' without ref count overhead
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
	 * Set this value to one from another pointer and set that pointer to zero (take ownership from)
	 *
	 * This is faster than setting and zeroing the source pointer since it
	 * avoids a synchronized reference count change.
	 *
	 * @param from Origin pointer; will be zeroed
	 */
	ZT_INLINE void move(SharedPtr &from)
	{
		_release();
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
		_release();
		m_ptr = nullptr;
	}

	/**
	 * Set pointer to NULL and delete object if reference count is only 1
	 *
	 * This can be called periodically to implement something like a weak
	 * reference as it exists in other more managed languages like Java,
	 * but with the caveat that it only works if there is only one remaining
	 * SharedPtr to be treated as weak.
	 *
	 * This does not delete the object. It returns it as a naked pointer.
	 *
	 * @return Pointer to T if reference count was only one (this shared ptr is left NULL)
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

	/**
	 * Get the current reference count for this object, which can change at any time
	 *
	 * @return Number of references according to this object's ref count or 0 if NULL
	 */
	ZT_INLINE int references() noexcept
	{
		if (likely(m_ptr != nullptr))
			return m_ptr->__refCount.load(std::memory_order_relaxed);
		return 0;
	}

	ZT_INLINE unsigned long hashCode() const noexcept
	{ return (unsigned long)Utils::hash64((uint64_t)((uintptr_t)m_ptr)); }

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
	ZT_INLINE T *_getAndInc() const noexcept
	{
		if (likely(m_ptr != nullptr))
			const_cast<std::atomic< int > *>(&(m_ptr->__refCount))->fetch_add(1, std::memory_order_acquire);
		return m_ptr;
	}

	ZT_INLINE void _release() const noexcept
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
