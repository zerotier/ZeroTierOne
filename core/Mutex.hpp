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

#ifndef __WINDOWS__
#include <pthread.h>
#endif

namespace ZeroTier {

class Mutex
{
public:
	ZT_INLINE Mutex() noexcept { pthread_mutex_init(&_mh,nullptr); }
	ZT_INLINE ~Mutex() noexcept { pthread_mutex_destroy(&_mh); }

	ZT_INLINE void lock() const noexcept { pthread_mutex_lock(&((const_cast <Mutex *> (this))->_mh)); }
	ZT_INLINE void unlock() const noexcept { pthread_mutex_unlock(&((const_cast <Mutex *> (this))->_mh)); }

	class Lock
	{
	public:
		explicit ZT_INLINE Lock(Mutex &m) noexcept : _m(&m) { m.lock(); }
		explicit ZT_INLINE Lock(const Mutex &m) noexcept : _m(const_cast<Mutex *>(&m)) { _m->lock(); }
		ZT_INLINE ~Lock() { _m->unlock(); }
	private:
		Mutex *const _m;
	};

private:
	ZT_INLINE Mutex(const Mutex &) noexcept {}
	ZT_INLINE const Mutex &operator=(const Mutex &) noexcept { return *this; }

	pthread_mutex_t _mh;
};

class RWMutex
{
public:
	ZT_INLINE RWMutex() noexcept { pthread_rwlock_init(&_mh,nullptr); }
	ZT_INLINE ~RWMutex() noexcept { pthread_rwlock_destroy(&_mh); }

	ZT_INLINE void lock() const noexcept { pthread_rwlock_wrlock(&((const_cast <RWMutex *> (this))->_mh)); }
	ZT_INLINE void rlock() const noexcept { pthread_rwlock_rdlock(&((const_cast <RWMutex *> (this))->_mh)); }
	ZT_INLINE void unlock() const noexcept { pthread_rwlock_unlock(&((const_cast <RWMutex *> (this))->_mh)); }
	ZT_INLINE void runlock() const noexcept { pthread_rwlock_unlock(&((const_cast <RWMutex *> (this))->_mh)); }

	/**
	 * RAAI locker that acquires only the read lock (shared read)
	 */
	class RLock
	{
	public:
		explicit ZT_INLINE RLock(RWMutex &m) noexcept : _m(&m) { m.rlock(); }
		explicit ZT_INLINE RLock(const RWMutex &m) noexcept : _m(const_cast<RWMutex *>(&m)) { _m->rlock(); }
		ZT_INLINE ~RLock() { _m->runlock(); }
	private:
		RWMutex *const _m;
	};

	/**
	 * RAAI locker that acquires the write lock (exclusive write, no readers)
	 */
	class Lock
	{
	public:
		explicit ZT_INLINE Lock(RWMutex &m) noexcept : _m(&m) { m.lock(); }
		explicit ZT_INLINE Lock(const RWMutex &m) noexcept : _m(const_cast<RWMutex *>(&m)) { _m->lock(); }
		ZT_INLINE ~Lock() { _m->unlock(); }
	private:
		RWMutex *const _m;
	};

	/**
	 * RAAI locker that acquires the read lock first and can switch modes
	 *
	 * Use writing() to acquire the write lock if not already acquired. Use reading() to
	 * let go of the write lock and go back to only holding the read lock.
	 */
	class RMaybeWLock
	{
	public:
		explicit ZT_INLINE RMaybeWLock(RWMutex &m) noexcept : _m(&m),_w(false) { m.rlock(); }
		explicit ZT_INLINE RMaybeWLock(const RWMutex &m) noexcept : _m(const_cast<RWMutex *>(&m)),_w(false) { _m->rlock(); }
		ZT_INLINE void writing() noexcept { if (!_w) { _w = true; _m->runlock(); _m->lock(); } }
		ZT_INLINE void reading() noexcept { if (_w) { _w = false; _m->unlock(); _m->rlock(); } }
		ZT_INLINE ~RMaybeWLock() { if (_w) _m->unlock(); else _m->runlock(); }
	private:
		RWMutex *const _m;
		bool _w;
	};

private:
	ZT_INLINE RWMutex(const RWMutex &) noexcept {}
	ZT_INLINE const RWMutex &operator=(const RWMutex &) noexcept { return *this; }

	pthread_rwlock_t _mh;
};

} // namespace ZeroTier

#if 0
#include <Windows.h>

namespace ZeroTier {

class Mutex
{
public:
	ZT_INLINE Mutex() { InitializeCriticalSection(&_cs); }
	ZT_INLINE ~Mutex() { DeleteCriticalSection(&_cs); }
	ZT_INLINE void lock() { EnterCriticalSection(&_cs); }
	ZT_INLINE void unlock() { LeaveCriticalSection(&_cs); }
	ZT_INLINE void lock() const { (const_cast <Mutex *> (this))->lock(); }
	ZT_INLINE void unlock() const { (const_cast <Mutex *> (this))->unlock(); }

	class Lock
	{
	public:
		ZT_INLINE Lock(Mutex &m) : _m(&m) { m.lock(); }
		ZT_INLINE Lock(const Mutex &m) : _m(const_cast<Mutex *>(&m)) { _m->lock(); }
		ZT_INLINE ~Lock() { _m->unlock(); }
	private:
		Mutex *const _m;
	};

private:
	ZT_INLINE Mutex(const Mutex &) {}
	ZT_INLINE const Mutex &operator=(const Mutex &) { return *this; }

	CRITICAL_SECTION _cs;
};

} // namespace ZeroTier
#endif

#endif
