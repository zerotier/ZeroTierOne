/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_MUTEX_HPP
#define ZT_MUTEX_HPP

#include "Constants.hpp"

// If C++17 is available use std::mutex and std::shared_mutex as
// these will probably use whatever is fastest on a given platform.
// Older compilers require pthreads to be available. The compiler
// now used on Windows is new enough to use C++17 stuff, so no more
// need for Windows-specific implementations here.
#if __cplusplus >= 201703L
#include <mutex>
#include <shared_mutex>
#else
#define ZT_USE_PTHREADS
#ifndef __WINDOWS__
#include <pthread.h>
#endif
#endif

namespace ZeroTier {

/**
 * A simple mutual exclusion lock.
 */
class Mutex {
  public:
#ifdef ZT_USE_PTHREADS
    ZT_INLINE Mutex() noexcept { pthread_mutex_init(&_mh, nullptr); }
    ZT_INLINE ~Mutex() noexcept { pthread_mutex_destroy(&_mh); }
    ZT_INLINE void lock() const noexcept { pthread_mutex_lock(&((const_cast<Mutex *>(this))->_mh)); }
    ZT_INLINE void unlock() const noexcept { pthread_mutex_unlock(&((const_cast<Mutex *>(this))->_mh)); }
#else
    ZT_INLINE Mutex() noexcept : _m() {}
    ZT_INLINE void lock() const noexcept { const_cast<Mutex *>(this)->_m.lock(); }
    ZT_INLINE void unlock() const noexcept { const_cast<Mutex *>(this)->_m.unlock(); }
#endif

    class Lock {
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

#ifdef ZT_USE_PTHREADS
    pthread_mutex_t _mh;
#else
    std::mutex _m;
#endif
};

/**
 * A lock allowing multiple threads to read but making all wait on any writing thread.
 */
class RWMutex {
  public:
#ifdef ZT_USE_PTHREADS
    ZT_INLINE RWMutex() noexcept { pthread_rwlock_init(&_mh, nullptr); }
    ZT_INLINE ~RWMutex() noexcept { pthread_rwlock_destroy(&_mh); }
    ZT_INLINE void lock() const noexcept { pthread_rwlock_wrlock(&((const_cast<RWMutex *>(this))->_mh)); }
    ZT_INLINE void rlock() const noexcept { pthread_rwlock_rdlock(&((const_cast<RWMutex *>(this))->_mh)); }
    ZT_INLINE void unlock() const noexcept { pthread_rwlock_unlock(&((const_cast<RWMutex *>(this))->_mh)); }
    ZT_INLINE void runlock() const noexcept { pthread_rwlock_unlock(&((const_cast<RWMutex *>(this))->_mh)); }
#else
    ZT_INLINE RWMutex() noexcept : _m() {}
    ZT_INLINE void lock() const noexcept { const_cast<RWMutex *>(this)->_m.lock(); }
    ZT_INLINE void rlock() const noexcept { const_cast<RWMutex *>(this)->_m.lock_shared(); }
    ZT_INLINE void unlock() const noexcept { const_cast<RWMutex *>(this)->_m.unlock(); }
    ZT_INLINE void runlock() const noexcept { const_cast<RWMutex *>(this)->_m.unlock_shared(); }
#endif

    /**
     * RAAI locker that acquires only the read lock (shared read)
     */
    class RLock {
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
    class Lock {
      public:
        explicit ZT_INLINE Lock(RWMutex &m) noexcept : _m(&m) { m.lock(); }
        explicit ZT_INLINE Lock(const RWMutex &m) noexcept : _m(const_cast<RWMutex *>(&m)) { _m->lock(); }
        ZT_INLINE ~Lock() { _m->unlock(); }

      private:
        RWMutex *const _m;
    };

    /**
     * RAAI locker that acquires the read lock first and can switch to writing.
     *
     * Use writing() to acquire the write lock if not already acquired. Use reading() to
     * let go of the write lock and go back to only holding the read lock. Note that on
     * most platforms there's a brief moment where the lock is unlocked during the
     * transition, meaning protected variable states can change. Code must not assume
     * that the lock is held constantly if writing() is used to change mode.
     */
    class RMaybeWLock {
      public:
        explicit ZT_INLINE RMaybeWLock(RWMutex &m) noexcept : _m(&m), _w(false) { m.rlock(); }
        explicit ZT_INLINE RMaybeWLock(const RWMutex &m) noexcept : _m(const_cast<RWMutex *>(&m)), _w(false)
        {
            _m->rlock();
        }
        ZT_INLINE void writing() noexcept
        {
            if (!_w) {
                _w = true;
                _m->runlock();
                _m->lock();
            }
        }
        ZT_INLINE void reading() noexcept
        {
            if (_w) {
                _w = false;
                _m->unlock();
                _m->rlock();
            }
        }
        ZT_INLINE bool isWriting() const noexcept { return _w; }
        ZT_INLINE ~RMaybeWLock()
        {
            if (_w)
                _m->unlock();
            else
                _m->runlock();
        }

      private:
        RWMutex *const _m;
        bool _w;
    };

  private:
    ZT_INLINE RWMutex(const RWMutex &) noexcept {}
    ZT_INLINE const RWMutex &operator=(const RWMutex &) noexcept { return *this; }

#ifdef ZT_USE_PTHREADS
    pthread_rwlock_t _mh;
#else
    std::shared_mutex _m;
#endif
};

}   // namespace ZeroTier

#endif
