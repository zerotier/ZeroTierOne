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

#ifndef _ZT_CONDITION_HPP
#define _ZT_CONDITION_HPP

#include "NonCopyable.hpp"

#if defined(__APPLE__) || defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)

#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include "Utils.hpp"

namespace ZeroTier {

class Condition : NonCopyable
{
public:
	Condition()
		throw()
	{
		pthread_mutex_init(&_mh,(const pthread_mutexattr_t *)0);
		pthread_cond_init(&_cond,(const pthread_condattr_t *)0);
	}

	~Condition()
	{
		pthread_cond_destroy(&_cond);
		pthread_mutex_destroy(&_mh);
	}

	inline void wait() const
		throw()
	{
		pthread_mutex_lock(const_cast <pthread_mutex_t *>(&_mh));
		pthread_cond_wait(const_cast <pthread_cond_t *>(&_cond),const_cast <pthread_mutex_t *>(&_mh));
		pthread_mutex_unlock(const_cast <pthread_mutex_t *>(&_mh));
	}

	inline void wait(unsigned long ms) const
		throw()
	{
		uint64_t when = Utils::now() + (uint64_t)ms;
		struct timespec ts;
		ts.tv_sec = (unsigned long)(when / 1000);
		ts.tv_nsec = (unsigned long)(when % 1000) * 1000000;
		pthread_mutex_lock(const_cast <pthread_mutex_t *>(&_mh));
		pthread_cond_timedwait(const_cast <pthread_cond_t *>(&_cond),const_cast <pthread_mutex_t *>(&_mh),&ts);
		pthread_mutex_unlock(const_cast <pthread_mutex_t *>(&_mh));
	}

	inline void signal() const
		throw()
	{
		pthread_cond_signal(const_cast <pthread_cond_t *>(&_cond));
	}

private:
	pthread_cond_t _cond;
	pthread_mutex_t _mh;
};

} // namespace ZeroTier

#endif // Apple / Linux

#ifdef _WIN32

#include <stdlib.h>
#include <Windows.h>

namespace ZeroTier {

error need windoze;
// On Windows this will probably be implemented via Semaphores

} // namespace ZeroTier

#endif // _WIN32

#endif
