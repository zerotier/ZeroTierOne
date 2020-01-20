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

#ifndef ZT_MUTEX_HPP
#define ZT_MUTEX_HPP

#include "Constants.hpp"

#include <cstdint>
#include <cstdlib>

#ifdef __UNIX_LIKE__

#include <pthread.h>

namespace ZeroTier {

class Mutex
{
public:
	ZT_ALWAYS_INLINE Mutex() { pthread_mutex_init(&_mh,0); }
	ZT_ALWAYS_INLINE ~Mutex() { pthread_mutex_destroy(&_mh); }

	ZT_ALWAYS_INLINE void lock() const { pthread_mutex_lock(&((const_cast <Mutex *> (this))->_mh)); }
	ZT_ALWAYS_INLINE void unlock() const { pthread_mutex_unlock(&((const_cast <Mutex *> (this))->_mh)); }

	class Lock
	{
	public:
		ZT_ALWAYS_INLINE Lock(Mutex &m) : _m(&m) { m.lock(); }
		ZT_ALWAYS_INLINE Lock(const Mutex &m) : _m(const_cast<Mutex *>(&m)) { _m->lock(); }
		ZT_ALWAYS_INLINE ~Lock() { _m->unlock(); }
	private:
		Mutex *const _m;
	};

private:
	ZT_ALWAYS_INLINE Mutex(const Mutex &) {}
	ZT_ALWAYS_INLINE const Mutex &operator=(const Mutex &) { return *this; }

	pthread_mutex_t _mh;
};

class RWMutex
{
public:
	ZT_ALWAYS_INLINE RWMutex() { pthread_rwlock_init(&_mh,0); }
	ZT_ALWAYS_INLINE ~RWMutex() { pthread_rwlock_destroy(&_mh); }

	ZT_ALWAYS_INLINE void lock() const { pthread_rwlock_wrlock(&((const_cast <RWMutex *> (this))->_mh)); }
	ZT_ALWAYS_INLINE void rlock() const { pthread_rwlock_rdlock(&((const_cast <RWMutex *> (this))->_mh)); }
	ZT_ALWAYS_INLINE void unlock() const { pthread_rwlock_unlock(&((const_cast <RWMutex *> (this))->_mh)); }
	ZT_ALWAYS_INLINE void runlock() const { pthread_rwlock_unlock(&((const_cast <RWMutex *> (this))->_mh)); }

	class RLock
	{
	public:
		ZT_ALWAYS_INLINE RLock(RWMutex &m) : _m(&m) { m.rlock(); }
		ZT_ALWAYS_INLINE RLock(const RWMutex &m) : _m(const_cast<RWMutex *>(&m)) { _m->rlock(); }
		ZT_ALWAYS_INLINE ~RLock() { _m->runlock(); }
	private:
		RWMutex *const _m;
	};

	class Lock
	{
	public:
		ZT_ALWAYS_INLINE Lock(RWMutex &m) : _m(&m) { m.lock(); }
		ZT_ALWAYS_INLINE Lock(const RWMutex &m) : _m(const_cast<RWMutex *>(&m)) { _m->lock(); }
		ZT_ALWAYS_INLINE ~Lock() { _m->unlock(); }
	private:
		RWMutex *const _m;
	};

private:
	ZT_ALWAYS_INLINE RWMutex(const RWMutex &) {}
	ZT_ALWAYS_INLINE const RWMutex &operator=(const RWMutex &) { return *this; }

	pthread_rwlock_t _mh;
};

} // namespace ZeroTier

#endif

#ifdef __WINDOWS__

#include <Windows.h>

namespace ZeroTier {

class Mutex
{
public:
	ZT_ALWAYS_INLINE Mutex() { InitializeCriticalSection(&_cs); }
	ZT_ALWAYS_INLINE ~Mutex() { DeleteCriticalSection(&_cs); }
	ZT_ALWAYS_INLINE void lock() { EnterCriticalSection(&_cs); }
	ZT_ALWAYS_INLINE void unlock() { LeaveCriticalSection(&_cs); }
	ZT_ALWAYS_INLINE void lock() const { (const_cast <Mutex *> (this))->lock(); }
	ZT_ALWAYS_INLINE void unlock() const { (const_cast <Mutex *> (this))->unlock(); }

	class Lock
	{
	public:
		ZT_ALWAYS_INLINE Lock(Mutex &m) : _m(&m) { m.lock(); }
		ZT_ALWAYS_INLINE Lock(const Mutex &m) : _m(const_cast<Mutex *>(&m)) { _m->lock(); }
		ZT_ALWAYS_INLINE ~Lock() { _m->unlock(); }
	private:
		Mutex *const _m;
	};

private:
	ZT_ALWAYS_INLINE Mutex(const Mutex &) {}
	ZT_ALWAYS_INLINE const Mutex &operator=(const Mutex &) { return *this; }

	CRITICAL_SECTION _cs;
};

} // namespace ZeroTier

#endif // _WIN32

#endif
