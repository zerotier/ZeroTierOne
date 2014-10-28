/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2014  ZeroTier Networks LLC
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
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef ZT_MTQ_HPP
#define ZT_MTQ_HPP

#include <stdlib.h>
#include <stdint.h>

#include <queue>

#include "../node/Constants.hpp"
#include "../node/NonCopyable.hpp"
#include "../node/Utils.hpp"

#ifdef __WINDOWS__
#include <Windows.h>
#else
#include <time.h>
#include <pthread.h>
#endif

namespace ZeroTier {

/**
 * A synchronized multithreaded FIFO queue
 *
 * This is designed for a use case where one thread pushes, the
 * other pops.
 */
template<typename T>
class MTQ : NonCopyable
{
public:
	MTQ()
	{
#ifdef __WINDOWS__
		_sem = CreateSemaphore(NULL,0,0x7fffffff,NULL);
		InitializeCriticalSection(&_cs);
#else
		pthread_mutex_init(&_mh,(const pthread_mutexattr_t *)0);
		pthread_cond_init(&_cond,(const pthread_condattr_t *)0);
#endif
	}

	~MTQ()
	{
#ifdef __WINDOWS__
		CloseHandle(_sem);
		DeleteCriticalSection(&_cs);
#else
		pthread_cond_destroy(&_cond);
		pthread_mutex_destroy(&_mh);
#endif
	}

	/**
	 * Push something onto the end of the FIFO and signal waiting thread(s)
	 *
	 * @param v Value to push
	 */
	inline void push(const T &v)
	{
#ifdef __WINDOWS__
		EnterCriticalSection(&_cs);
		try {
			_q.push(v);
			LeaveCriticalSection(&_cs);
			ReleaseSemaphore(_sem,1,NULL);
		} catch ( ... ) {
			LeaveCriticalSection(&_cs);
			throw;
		}
#else
		pthread_mutex_lock(const_cast <pthread_mutex_t *>(&_mh));
		try {
			_q.push(v);
			pthread_mutex_unlock(const_cast <pthread_mutex_t *>(&_mh));
			pthread_cond_signal(const_cast <pthread_cond_t *>(&_cond));
		} catch ( ... ) {
			pthread_mutex_unlock(const_cast <pthread_mutex_t *>(&_mh));
			throw;
		}
#endif
	}

	/**
	 * Pop fron queue with optional timeout
	 *
	 * @param v Result parameter to set to next value
	 * @param ms Milliseconds timeout or 0 for none
	 * @return True if v was set to something, false on timeout
	 */
	inline bool pop(T &v,unsigned long ms = 0)
	{
#ifdef __WINDOWS__
		if (ms > 0)
			WaitForSingleObject(_sem,(DWORD)ms);
		else WaitForSingleObject(_sem,INFINITE);
		EnterCriticalSection(&_cs);
		try {
			if (_q.empty()) {
				LeaveCriticalSection(&_cs);
				return false;
			} else {
				v = _q.front();
				_q.pop();
				LeaveCriticalSection(&_cs);
				return true;
			}
		} catch ( ... ) {
			LeaveCriticalSection(&_cs);
			throw;
		}
#else
		pthread_mutex_lock(const_cast <pthread_mutex_t *>(&_mh));
		try {
			if (_q.empty()) {
				if (ms > 0) {
					uint64_t when = Utils::now() + (uint64_t)ms;
					struct timespec ts;
					ts.tv_sec = (unsigned long)(when / 1000);
					ts.tv_nsec = (unsigned long)(when % 1000) * (unsigned long)1000000;
					pthread_cond_timedwait(const_cast <pthread_cond_t *>(&_cond),const_cast <pthread_mutex_t *>(&_mh),&ts);
				} else {
					pthread_cond_wait(const_cast <pthread_cond_t *>(&_cond),const_cast <pthread_mutex_t *>(&_mh));
				}
				if (_q.empty()) {
					pthread_mutex_unlock(const_cast <pthread_mutex_t *>(&_mh));
					return false;
				}
			}
			v = _q.front();
			_q.pop();
			pthread_mutex_unlock(const_cast <pthread_mutex_t *>(&_mh));
			return true;
		} catch ( ... ) {
			pthread_mutex_unlock(const_cast <pthread_mutex_t *>(&_mh));
			throw;
		}
#endif
	}

private:
	std::queue<T> _q;
#ifdef __WINDOWS__
	HANDLE _sem;
	CRITICAL_SECTION _cs;
#else
	pthread_cond_t _cond;
	pthread_mutex_t _mh;
#endif
};

} // namespace ZeroTier

#endif
