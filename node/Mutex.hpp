/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_MUTEX_HPP
#define ZT_MUTEX_HPP

#include "Constants.hpp"
#include "NonCopyable.hpp"

#ifdef __UNIX_LIKE__

#include <stdlib.h>
#include <pthread.h>

namespace ZeroTier {

class Mutex : NonCopyable
{
public:
	Mutex()
		throw()
	{
		pthread_mutex_init(&_mh,(const pthread_mutexattr_t *)0);
	}

	~Mutex()
	{
		pthread_mutex_destroy(&_mh);
	}

	inline void lock()
		throw()
	{
		pthread_mutex_lock(&_mh);
	}

	inline void unlock()
		throw()
	{
		pthread_mutex_unlock(&_mh);
	}

	inline void lock() const
		throw()
	{
		(const_cast <Mutex *> (this))->lock();
	}

	inline void unlock() const
		throw()
	{
		(const_cast <Mutex *> (this))->unlock();
	}

	/**
	 * Uses C++ contexts and constructor/destructor to lock/unlock automatically
	 */
	class Lock : NonCopyable
	{
	public:
		Lock(Mutex &m)
			throw() :
			_m(&m)
		{
			m.lock();
		}

		Lock(const Mutex &m)
			throw() :
			_m(const_cast<Mutex *>(&m))
		{
			_m->lock();
		}

		~Lock()
		{
			_m->unlock();
		}

	private:
		Mutex *const _m;
	};

private:
	pthread_mutex_t _mh;
};

} // namespace ZeroTier

#endif // Apple / Linux

#ifdef __WINDOWS__

#include <stdlib.h>
#include <Windows.h>

namespace ZeroTier {

class Mutex : NonCopyable
{
public:
	Mutex()
		throw()
	{
		InitializeCriticalSection(&_cs);
	}

	~Mutex()
	{
		DeleteCriticalSection(&_cs);
	}

	inline void lock()
		throw()
	{
		EnterCriticalSection(&_cs);
	}

	inline void unlock()
		throw()
	{
		LeaveCriticalSection(&_cs);
	}

	inline void lock() const
		throw()
	{
		(const_cast <Mutex *> (this))->lock();
	}

	inline void unlock() const
		throw()
	{
		(const_cast <Mutex *> (this))->unlock();
	}

	class Lock : NonCopyable
	{
	public:
		Lock(Mutex &m)
			throw() :
			_m(&m)
		{
			m.lock();
		}

		Lock(const Mutex &m)
			throw() :
			_m(const_cast<Mutex *>(&m))
		{
			_m->lock();
		}

		~Lock()
		{
			_m->unlock();
		}

	private:
		Mutex *const _m;
	};

private:
	CRITICAL_SECTION _cs;
};

} // namespace ZeroTier

#endif // _WIN32

#endif
