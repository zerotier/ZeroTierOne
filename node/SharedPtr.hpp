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
template<typename T>
class SharedPtr : public TriviallyCopyable
{
public:
	ZT_INLINE SharedPtr() noexcept : _ptr((T *)0) {}
	explicit ZT_INLINE SharedPtr(T *obj) noexcept : _ptr(obj) { ++obj->__refCount; }
	ZT_INLINE SharedPtr(const SharedPtr &sp) noexcept : _ptr(sp._getAndInc()) {}

	ZT_INLINE ~SharedPtr()
	{
		if (_ptr) {
			if (--_ptr->__refCount <= 0)
				delete _ptr;
		}
	}

	ZT_INLINE SharedPtr &operator=(const SharedPtr &sp)
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
	ZT_INLINE void set(T *ptr) noexcept
	{
		zero();
		++ptr->__refCount;
		_ptr = ptr;
	}

	/**
	 * Stupidly set this SharedPtr to 'ptr', ignoring current value and not incrementing reference counter
	 *
	 * This must only be used in code that knows what it's doing. :)
	 *
	 * @param ptr Pointer to set
	 */
	ZT_INLINE void unsafeSet(T *ptr) noexcept { _ptr = ptr; }

	/**
	 * Swap with another pointer 'for free' without ref count overhead
	 *
	 * @param with Pointer to swap with
	 */
	ZT_INLINE void swap(SharedPtr &with) noexcept
	{
		T *tmp = _ptr;
		_ptr = with._ptr;
		with._ptr = tmp;
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
		if (_ptr) {
			if (--_ptr->__refCount <= 0)
				delete _ptr;
		}
		_ptr = from._ptr;
		from._ptr = nullptr;
	}

	ZT_INLINE operator bool() const noexcept { return (_ptr != nullptr); }

	ZT_INLINE T &operator*() const noexcept { return *_ptr; }
	ZT_INLINE T *operator->() const noexcept { return _ptr; }

	/**
	 * @return Raw pointer to held object
	 */
	ZT_INLINE T *ptr() const noexcept { return _ptr; }

	/**
	 * Set this pointer to NULL
	 */
	ZT_INLINE void zero()
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
	ZT_INLINE int references() noexcept
	{
		if (_ptr)
			return _ptr->__refCount;
		return 0;
	}

	ZT_INLINE bool operator==(const SharedPtr &sp) const noexcept { return (_ptr == sp._ptr); }
	ZT_INLINE bool operator!=(const SharedPtr &sp) const noexcept { return (_ptr != sp._ptr); }
	ZT_INLINE bool operator>(const SharedPtr &sp) const noexcept { return (_ptr > sp._ptr); }
	ZT_INLINE bool operator<(const SharedPtr &sp) const noexcept { return (_ptr < sp._ptr); }
	ZT_INLINE bool operator>=(const SharedPtr &sp) const noexcept { return (_ptr >= sp._ptr); }
	ZT_INLINE bool operator<=(const SharedPtr &sp) const noexcept { return (_ptr <= sp._ptr); }

private:
	ZT_INLINE T *_getAndInc() const noexcept
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
ZT_INLINE void swap(ZeroTier::SharedPtr<T> &a,ZeroTier::SharedPtr<T> &b) noexcept { a.swap(b); }
}

#endif
