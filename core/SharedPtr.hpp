/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
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
 * Simple zero-overhead introspective reference counted pointer
 *
 * This is an introspective shared pointer. Classes that need to be reference
 * counted must list this as a 'friend' and must have a private instance of
 * atomic<int> called __refCount.
 */
template< typename T >
class SharedPtr : public TriviallyCopyable
{
public:
	ZT_INLINE SharedPtr() noexcept: m_ptr((T *)0)
	{}

	explicit ZT_INLINE SharedPtr(T *obj) noexcept: m_ptr(obj)
	{ if (likely(obj != nullptr)) ++*const_cast<std::atomic< int > *>(&(obj->__refCount)); }

	ZT_INLINE SharedPtr(const SharedPtr &sp) noexcept: m_ptr(sp._getAndInc())
	{}

	ZT_INLINE ~SharedPtr()
	{
		if (likely(m_ptr != nullptr)) {
			if (unlikely(--*const_cast<std::atomic< int > *>(&(m_ptr->__refCount)) <= 0))
				delete m_ptr;
		}
	}

	ZT_INLINE SharedPtr &operator=(const SharedPtr &sp)
	{
		if (likely(m_ptr != sp.m_ptr)) {
			T *p = sp._getAndInc();
			if (likely(m_ptr != nullptr)) {
				if (unlikely(--*const_cast<std::atomic< int > *>(&(m_ptr->__refCount)) <= 0))
					delete m_ptr;
			}
			m_ptr = p;
		}
		return *this;
	}

	/**
	 * Set to a naked pointer and increment its reference count
	 *
	 * This assumes this SharedPtr is NULL and that ptr is not a 'zombie.' No
	 * checks are performed.
	 *
	 * @param ptr Naked pointer to assign
	 */
	ZT_INLINE void set(T *ptr) noexcept
	{
		zero();
		++*const_cast<std::atomic< int > *>(&(ptr->__refCount));
		m_ptr = ptr;
	}

	/**
	 * Stupidly set this SharedPtr to 'ptr', ignoring current value and not incrementing reference counter
	 *
	 * This must only be used in code that knows what it's doing. :)
	 *
	 * @param ptr Pointer to set
	 */
	ZT_INLINE void unsafeSet(T *ptr) noexcept
	{ m_ptr = ptr; }

	/**
	 * Swap with another pointer 'for free' without ref count overhead
	 *
	 * @param with Pointer to swap with
	 */
	ZT_INLINE void swap(SharedPtr &with) noexcept
	{
		T *tmp = m_ptr;
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
		if (likely(m_ptr != nullptr)) {
			if (--*const_cast<std::atomic< int > *>(&(m_ptr->__refCount)) <= 0)
				delete m_ptr;
		}
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
		if (likely(m_ptr != nullptr)) {
			if (unlikely(--*const_cast<std::atomic< int > *>(&(m_ptr->__refCount)) <= 0))
				delete m_ptr;
			m_ptr = nullptr;
		}
	}

	/**
	 * Set pointer to NULL and delete object if reference count is only 1
	 *
	 * This can be called periodically to implement something like a weak
	 * reference as it exists in other more managed languages like Java,
	 * but with the caveat that it only works if there is only one remaining
	 * SharedPtr to be treated as weak.
	 *
	 * @return True if object was in fact deleted OR this pointer was already NULL
	 */
	ZT_INLINE bool weakGC()
	{
		if (likely(m_ptr != nullptr)) {
			int one = 1;
			if (const_cast<std::atomic< int > *>(&(m_ptr->__refCount))->compare_exchange_strong(one, (int)0)) {
				delete m_ptr;
				m_ptr = nullptr;
				return true;
			}
			return false;
		} else {
			return true;
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
			return m_ptr->__refCount;
		return 0;
	}

	ZT_INLINE unsigned long hashCode() const noexcept
	{ return (unsigned long)Utils::hash64((uint64_t)((uintptr_t)m_ptr)); }

	ZT_INLINE bool operator==(const SharedPtr &sp) const noexcept
	{ return (m_ptr == sp.m_ptr); }

	ZT_INLINE bool operator!=(const SharedPtr &sp) const noexcept
	{ return (m_ptr != sp.m_ptr); }

	ZT_INLINE bool operator>(const SharedPtr &sp) const noexcept
	{ return (m_ptr > sp.m_ptr); }

	ZT_INLINE bool operator<(const SharedPtr &sp) const noexcept
	{ return (m_ptr < sp.m_ptr); }

	ZT_INLINE bool operator>=(const SharedPtr &sp) const noexcept
	{ return (m_ptr >= sp.m_ptr); }

	ZT_INLINE bool operator<=(const SharedPtr &sp) const noexcept
	{ return (m_ptr <= sp.m_ptr); }

private:
	ZT_INLINE T *_getAndInc() const noexcept
	{
		if (m_ptr)
			++*const_cast<std::atomic< int > *>(&(m_ptr->__refCount));
		return m_ptr;
	}

	T *m_ptr;
};

} // namespace ZeroTier

// Augment std::swap to speed up some operations with SharedPtr.
namespace std {

template< typename T >
ZT_INLINE void swap(ZeroTier::SharedPtr< T > &a, ZeroTier::SharedPtr< T > &b) noexcept
{ a.swap(b); }

template< typename T >
constexpr bool is_swappable(ZeroTier::SharedPtr< T > &a) noexcept
{ return true; }

template< typename T >
constexpr bool is_swappable_with(ZeroTier::SharedPtr< T > &a, ZeroTier::SharedPtr< T > &b) noexcept
{ return true; }

template< typename T >
constexpr bool is_nothrow_swappable(ZeroTier::SharedPtr< T > &a) noexcept
{ return true; }

template< typename T >
constexpr bool is_nothrow_swappable_with(ZeroTier::SharedPtr< T > &a, ZeroTier::SharedPtr< T > &b) noexcept
{ return true; }

} // namespace std

#endif
