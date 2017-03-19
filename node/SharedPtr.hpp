/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ZT_SHAREDPTR_HPP
#define ZT_SHAREDPTR_HPP

#include "Mutex.hpp"
#include "AtomicCounter.hpp"

namespace ZeroTier {

/**
 * Simple reference counted pointer
 *
 * This is an introspective shared pointer. Classes that need to be reference
 * counted must list this as a 'friend' and must have a private instance of
 * AtomicCounter called __refCount. They should also have private destructors,
 * since only this class should delete them.
 *
 * Because this is introspective, it is safe to apply to a naked pointer
 * multiple times provided there is always at least one holding SharedPtr.
 *
 * Once C++11 is ubiquitous, this and a few other things like Thread might get
 * torn out for their standard equivalents.
 */
template<typename T>
class SharedPtr
{
public:
	SharedPtr()
		throw() :
		_ptr((T *)0)
	{
	}

	SharedPtr(T *obj)
		throw() :
		_ptr(obj)
	{
		++obj->__refCount;
	}

	SharedPtr(const SharedPtr &sp)
		throw() :
		_ptr(sp._getAndInc())
	{
	}

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
	inline void setToUnsafe(T *ptr)
	{
		++ptr->__refCount;
		_ptr = ptr;
	}

	/**
	 * Swap with another pointer 'for free' without ref count overhead
	 *
	 * @param with Pointer to swap with
	 */
	inline void swap(SharedPtr &with)
		throw()
	{
		T *tmp = _ptr;
		_ptr = with._ptr;
		with._ptr = tmp;
	}

	inline operator bool() const throw() { return (_ptr != (T *)0); }
	inline T &operator*() const throw() { return *_ptr; }
	inline T *operator->() const throw() { return _ptr; }

	/**
	 * @return Raw pointer to held object
	 */
	inline T *ptr() const throw() { return _ptr; }

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
	 * Set this pointer to NULL if this is the only pointer holding the object
	 *
	 * @return True if object was deleted and SharedPtr is now NULL (or was already NULL)
	 */
	inline bool reclaimIfWeak()
	{
		if (_ptr) {
			if (++_ptr->__refCount <= 2) {
				if (--_ptr->__refCount <= 1) {
					delete _ptr;
					_ptr = (T *)0;
					return true;
				} else {
					return false;
				}
			} else {
				return false;
			}
		} else {
			return true;
		}
	}

	inline bool operator==(const SharedPtr &sp) const throw() { return (_ptr == sp._ptr); }
	inline bool operator!=(const SharedPtr &sp) const throw() { return (_ptr != sp._ptr); }
	inline bool operator>(const SharedPtr &sp) const throw() { return (_ptr > sp._ptr); }
	inline bool operator<(const SharedPtr &sp) const throw() { return (_ptr < sp._ptr); }
	inline bool operator>=(const SharedPtr &sp) const throw() { return (_ptr >= sp._ptr); }
	inline bool operator<=(const SharedPtr &sp) const throw() { return (_ptr <= sp._ptr); }

private:
	inline T *_getAndInc() const
		throw()
	{
		if (_ptr)
			++_ptr->__refCount;
		return _ptr;
	}

	T *_ptr;
};

} // namespace ZeroTier

#endif
