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

#ifndef ZT_MUTEX_HPP
#define ZT_MUTEX_HPP

#include "Constants.hpp"

#ifdef __UNIX_LIKE__

#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

namespace ZeroTier {

// libpthread based mutex lock
class Mutex
{
public:
	inline Mutex() { pthread_mutex_init(&_mh,(const pthread_mutexattr_t *)0); }
	inline ~Mutex() { pthread_mutex_destroy(&_mh); }
	inline void lock() const { pthread_mutex_lock(&((const_cast <Mutex *> (this))->_mh)); }
	inline void unlock() const { pthread_mutex_unlock(&((const_cast <Mutex *> (this))->_mh)); }

	class Lock
	{
	public:
		inline Lock(Mutex &m) : _m(&m) { m.lock(); }
		inline Lock(const Mutex &m) : _m(const_cast<Mutex *>(&m)) { _m->lock(); }
		inline ~Lock() { _m->unlock(); }
	private:
		Mutex *const _m;
	};

private:
	inline Mutex(const Mutex &) {}
	const Mutex &operator=(const Mutex &) { return *this; }

	pthread_mutex_t _mh;
};

} // namespace ZeroTier

#endif

#ifdef __WINDOWS__

#include <stdlib.h>
#include <Windows.h>

namespace ZeroTier {

// Windows critical section based lock
class Mutex
{
public:
	inline Mutex() { InitializeCriticalSection(&_cs); }
	inline ~Mutex() { DeleteCriticalSection(&_cs); }
	inline void lock() { EnterCriticalSection(&_cs); }
	inline void unlock() { LeaveCriticalSection(&_cs); }
	inline void lock() const { (const_cast <Mutex *> (this))->lock(); }
	inline void unlock() const { (const_cast <Mutex *> (this))->unlock(); }

	class Lock
	{
	public:
		inline Lock(Mutex &m) : _m(&m) { m.lock(); }
		inline Lock(const Mutex &m) : _m(const_cast<Mutex *>(&m)) { _m->lock(); }
		inline ~Lock() { _m->unlock(); }
	private:
		Mutex *const _m;
	};

private:
	inline Mutex(const Mutex &) {}
	const Mutex &operator=(const Mutex &) { return *this; }

	CRITICAL_SECTION _cs;
};

} // namespace ZeroTier

#endif // _WIN32

#endif
