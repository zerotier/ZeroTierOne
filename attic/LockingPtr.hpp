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

#ifndef ZT_LOCKINGPTR_HPP
#define ZT_LOCKINGPTR_HPP

#include "Mutex.hpp"

namespace ZeroTier {

/**
 * A simple pointer that locks and holds a mutex until destroyed
 *
 * Care must be taken when using this. It's not very sophisticated and does
 * not handle being copied except for the simple return use case. When it is
 * copied it hands off the mutex to the copy and clears it in the original,
 * meaning that the mutex is unlocked when the last LockingPtr<> in a chain
 * of such handoffs is destroyed. If this chain of handoffs "forks" (more than
 * one copy is made) then non-determinism may ensue.
 *
 * This does not delete or do anything else with the pointer. It also does not
 * take care of locking the lock. That must be done beforehand.
 */
template<typename T>
class LockingPtr
{
public:
	LockingPtr() :
		_ptr((T *)0),
		_lock((Mutex *)0)
	{
	}

	LockingPtr(T *obj,Mutex *lock) :
		_ptr(obj),
		_lock(lock)
	{
	}

	LockingPtr(const LockingPtr &p) :
		_ptr(p._ptr),
		_lock(p._lock)
	{
		const_cast<LockingPtr *>(&p)->_lock = (Mutex *)0;
	}

	~LockingPtr()
	{
		if (_lock)
			_lock->unlock();
	}

	inline LockingPtr &operator=(const LockingPtr &p)
	{
		_ptr = p._ptr;
		_lock = p._lock;
		const_cast<LockingPtr *>(&p)->_lock = (Mutex *)0;
		return *this;
	}

	inline operator bool() const throw() { return (_ptr != (T *)0); }
	inline T &operator*() const throw() { return *_ptr; }
	inline T *operator->() const throw() { return _ptr; }

	/**
	 * @return Raw pointer to held object
	 */
	inline T *ptr() const throw() { return _ptr; }

	inline bool operator==(const LockingPtr &sp) const throw() { return (_ptr == sp._ptr); }
	inline bool operator!=(const LockingPtr &sp) const throw() { return (_ptr != sp._ptr); }
	inline bool operator>(const LockingPtr &sp) const throw() { return (_ptr > sp._ptr); }
	inline bool operator<(const LockingPtr &sp) const throw() { return (_ptr < sp._ptr); }
	inline bool operator>=(const LockingPtr &sp) const throw() { return (_ptr >= sp._ptr); }
	inline bool operator<=(const LockingPtr &sp) const throw() { return (_ptr <= sp._ptr); }

private:
	T *_ptr;
	Mutex *_lock;
};

} // namespace ZeroTier

#endif
