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

#ifndef ZT_THREAD_HPP
#define ZT_THREAD_HPP

#include <stdexcept>

#include "../node/Constants.hpp"

#ifdef __WINDOWS__

#include <WinSock2.h>
#include <Windows.h>
#include <string.h>
#include "../node/Mutex.hpp"

namespace ZeroTier {

template<typename C>
static DWORD WINAPI ___zt_threadMain(LPVOID lpParam)
{
	try {
		((C *)lpParam)->threadMain();
	} catch ( ... ) {}
	return 0;
}

class Thread
{
public:
	Thread()
		throw()
	{
		_th = NULL;
		_tid = 0;
	}

	template<typename C>
	static inline Thread start(C *instance)
		throw(std::runtime_error)
	{
		Thread t;
		t._th = CreateThread(NULL,0,&___zt_threadMain<C>,(LPVOID)instance,0,&t._tid);
		if (t._th == NULL)
			throw std::runtime_error("CreateThread() failed");
		return t;
	}

	static inline void join(const Thread &t)
	{
		if (t._th != NULL) {
			for(;;) {
				DWORD ec = STILL_ACTIVE;
				GetExitCodeThread(t._th,&ec);
				if (ec == STILL_ACTIVE)
					WaitForSingleObject(t._th,1000);
				else break;
			}
		}
	}

	static inline void sleep(unsigned long ms)
	{
		Sleep((DWORD)ms);
	}

	// Not available on *nix platforms
	static inline void cancelIO(const Thread &t)
	{
		if (t._th != NULL)
			CancelSynchronousIo(t._th);
	}

	inline operator bool() const throw() { return (_th != NULL); }

private:
	HANDLE _th;
	DWORD _tid;
};

} // namespace ZeroTier

#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

namespace ZeroTier {

template<typename C>
static void *___zt_threadMain(void *instance)
{
	try {
		((C *)instance)->threadMain();
	} catch ( ... ) {}
	return (void *)0;
}

/**
 * A thread identifier, and static methods to start and join threads
 */
class Thread
{
public:
	Thread()
		throw()
	{
		memset(&_tid,0,sizeof(_tid));
		pthread_attr_init(&_tattr);
#ifdef __LINUX__
		pthread_attr_setstacksize(&_tattr,8388608); // for MUSL libc and others, has no effect in normal glibc environments
#endif
		_started = false;
	}

	Thread(const Thread &t)
		throw()
	{
		memcpy(&_tid,&(t._tid),sizeof(_tid));
		_started = t._started;
	}

	inline Thread &operator=(const Thread &t)
		throw()
	{
		memcpy(&_tid,&(t._tid),sizeof(_tid));
		_started = t._started;
		return *this;
	}

	/**
	 * Start a new thread
	 *
	 * @param instance Instance whose threadMain() method gets called by new thread
	 * @return Thread identifier
	 * @throws std::runtime_error Unable to create thread
	 * @tparam C Class containing threadMain()
	 */
	template<typename C>
	static inline Thread start(C *instance)
		throw(std::runtime_error)
	{
		Thread t;
		t._started = true;
		if (pthread_create(&t._tid,&t._tattr,&___zt_threadMain<C>,instance))
			throw std::runtime_error("pthread_create() failed, unable to create thread");
		return t;
	}

	/**
	 * Join to a thread, waiting for it to terminate (does nothing on null Thread values)
	 *
	 * @param t Thread to join
	 */
	static inline void join(const Thread &t)
	{
		if (t._started)
			pthread_join(t._tid,(void **)0);
	}

	/**
	 * Sleep the current thread
	 *
	 * @param ms Number of milliseconds to sleep
	 */
	static inline void sleep(unsigned long ms) { usleep(ms * 1000); }

	inline operator bool() const throw() { return (_started); }

private:
	pthread_t _tid;
	pthread_attr_t _tattr;
	volatile bool _started;
};

} // namespace ZeroTier

#endif // __WINDOWS__ / !__WINDOWS__

#endif
