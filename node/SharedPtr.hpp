/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_SHAREDPTR_HPP
#define ZT_SHAREDPTR_HPP

#include "Mutex.hpp"
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
	SharedPtr() : _ptr((T *)0) {}
	SharedPtr(T *obj) : _ptr(obj) { ++obj->__refCount; }
	SharedPtr(const SharedPtr &sp) : _ptr(sp._getAndInc()) {}

	~SharedPtr()
	{
		if (_ptr) {
			if (--_ptr->__refCount <= 0)
				delete _ptr;
		}
	}

	inline SharedPtr &operator=(const SharedPtr &sp)
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
	inline void set(T *ptr)
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
	inline void swap(SharedPtr &with)
	{
		T *tmp = _ptr;
		_ptr = with._ptr;
		with._ptr = tmp;
	}

	inline operator bool() const { return (_ptr != (T *)0); }
	inline T &operator*() const { return *_ptr; }
	inline T *operator->() const { return _ptr; }

	/**
	 * @return Raw pointer to held object
	 */
	inline T *ptr() const { return _ptr; }

	/**
	 * Set this pointer to NULL
	 */
	inline void zero()
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
	inline int references()
	{
		if (_ptr)
			return _ptr->__refCount.load();
		return 0;
	}

	inline bool operator==(const SharedPtr &sp) const { return (_ptr == sp._ptr); }
	inline bool operator!=(const SharedPtr &sp) const { return (_ptr != sp._ptr); }
	inline bool operator>(const SharedPtr &sp) const { return (_ptr > sp._ptr); }
	inline bool operator<(const SharedPtr &sp) const { return (_ptr < sp._ptr); }
	inline bool operator>=(const SharedPtr &sp) const { return (_ptr >= sp._ptr); }
	inline bool operator<=(const SharedPtr &sp) const { return (_ptr <= sp._ptr); }

private:
	inline T *_getAndInc() const
	{
		if (_ptr)
			++_ptr->__refCount;
		return _ptr;
	}
	T *_ptr;
};

} // namespace ZeroTier

#endif
