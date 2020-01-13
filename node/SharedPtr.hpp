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

#ifndef ZT_SHAREDPTR_HPP
#define ZT_SHAREDPTR_HPP

#include "AtomicCounter.hpp"

namespace ZeroTier {

/**
 * Simple zero-overhead introspective reference counted pointer
 *
 * This is an introspective shared pointer. Classes that need to be reference
 * counted must list this as a 'friend' and must have a private instance of
 * AtomicCounter called __refCount.
 */
template<typename T>
class SharedPtr
{
public:
	ZT_ALWAYS_INLINE SharedPtr() : _ptr((T *)0) {}
	ZT_ALWAYS_INLINE SharedPtr(T *obj) : _ptr(obj) { ++obj->__refCount; }
	ZT_ALWAYS_INLINE SharedPtr(const SharedPtr &sp) : _ptr(sp._getAndInc()) {}

	ZT_ALWAYS_INLINE ~SharedPtr()
	{
		if (_ptr) {
			if (--_ptr->__refCount <= 0)
				delete _ptr;
		}
	}

	ZT_ALWAYS_INLINE SharedPtr &operator=(const SharedPtr &sp)
	{
		if (_ptr != sp._ptr) {
			T *p = sp._getAndInc();
			if (_ptr) {
				if (--_ptr->__refCount <= 0)
					delete _ptr;
			}
			_ptr = p;
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
	ZT_ALWAYS_INLINE void set(T *ptr)
	{
		zero();
		++ptr->__refCount;
		_ptr = ptr;
	}

	/**
	 * Swap with another pointer 'for free' without ref count overhead
	 *
	 * @param with Pointer to swap with
	 */
	ZT_ALWAYS_INLINE void swap(SharedPtr &with)
	{
		T *tmp = _ptr;
		_ptr = with._ptr;
		with._ptr = tmp;
	}

	ZT_ALWAYS_INLINE operator bool() const { return (_ptr != (T *)0); }
	ZT_ALWAYS_INLINE T &operator*() const { return *_ptr; }
	ZT_ALWAYS_INLINE T *operator->() const { return _ptr; }

	/**
	 * @return Raw pointer to held object
	 */
	ZT_ALWAYS_INLINE T *ptr() const { return _ptr; }

	/**
	 * Set this pointer to NULL
	 */
	ZT_ALWAYS_INLINE void zero()
	{
		if (_ptr) {
			if (--_ptr->__refCount <= 0)
				delete _ptr;
			_ptr = (T *)0;
		}
	}

	/**
	 * @return Number of references according to this object's ref count or 0 if NULL
	 */
	ZT_ALWAYS_INLINE int references()
	{
		if (_ptr)
			return _ptr->__refCount.load();
		return 0;
	}

	ZT_ALWAYS_INLINE bool operator==(const SharedPtr &sp) const { return (_ptr == sp._ptr); }
	ZT_ALWAYS_INLINE bool operator!=(const SharedPtr &sp) const { return (_ptr != sp._ptr); }
	ZT_ALWAYS_INLINE bool operator>(const SharedPtr &sp) const { return (_ptr > sp._ptr); }
	ZT_ALWAYS_INLINE bool operator<(const SharedPtr &sp) const { return (_ptr < sp._ptr); }
	ZT_ALWAYS_INLINE bool operator>=(const SharedPtr &sp) const { return (_ptr >= sp._ptr); }
	ZT_ALWAYS_INLINE bool operator<=(const SharedPtr &sp) const { return (_ptr <= sp._ptr); }

private:
	ZT_ALWAYS_INLINE T *_getAndInc() const
	{
		if (_ptr)
			++_ptr->__refCount;
		return _ptr;
	}
	T *_ptr;
};

} // namespace ZeroTier

namespace std {
template<typename T>
ZT_ALWAYS_INLINE void swap(ZeroTier::SharedPtr<T> &a,ZeroTier::SharedPtr<T> &b) { a.swap(b); }
}

#endif
