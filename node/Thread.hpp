/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#ifndef _ZT_THREAD_HPP
#define _ZT_THREAD_HPP

#include <stdexcept>

#include "Constants.hpp"
#include "AtomicCounter.hpp"

#ifdef __WINDOWS__

todo need windows;

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
	}

	Thread(const Thread &t)
		throw()
	{
		memcpy(&_tid,&(t._tid),sizeof(_tid));
	}

	inline Thread &operator=(const Thread &t)
		throw()
	{
		memcpy(&_tid,&(t._tid),sizeof(_tid));
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
		if (pthread_create(&t._tid,(const pthread_attr_t *)0,&___zt_threadMain<C>,instance))
			throw std::runtime_error("pthread_create() failed, unable to create thread");
		return t;
	}

	/**
	 * Join to a thread, waiting for it to terminate
	 *
	 * @param t Thread to join
	 */
	static inline void join(const Thread &t)
	{
		pthread_join(t._tid,(void **)0);
	}

	/**
	 * Sleep the current thread
	 *
	 * @param ms Number of milliseconds to sleep
	 */
	static inline void sleep(unsigned long ms)
	{
		usleep(ms * 1000);
	}

private:
	pthread_t _tid;
};

} // namespace ZeroTier

#endif // __WINDOWS__ / !__WINDOWS__

#endif
