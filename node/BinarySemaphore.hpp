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

#ifndef ZT_BINARYSEMAPHORE_HPP
#define ZT_BINARYSEMAPHORE_HPP

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "Constants.hpp"
#include "NonCopyable.hpp"

#ifdef __WINDOWS__

#include <Windows.h>

namespace ZeroTier {

class BinarySemaphore : NonCopyable
{
public:
	BinarySemaphore() throw() { _sem = CreateSemaphore(NULL,0,1,NULL); }
	~BinarySemaphore() { CloseHandle(_sem); }
	inline void wait() { WaitForSingleObject(_sem,INFINITE); }
	inline void post() { ReleaseSemaphore(_sem,1,NULL); }
private:
	HANDLE _sem;
};

} // namespace ZeroTier

#else // !__WINDOWS__

#include <pthread.h>

namespace ZeroTier {

class BinarySemaphore : NonCopyable
{
public:
	BinarySemaphore()
	{
		pthread_mutex_init(&_mh,(const pthread_mutexattr_t *)0);
		pthread_cond_init(&_cond,(const pthread_condattr_t *)0);
		_f = false;
	}

	~BinarySemaphore()
	{
		pthread_cond_destroy(&_cond);
		pthread_mutex_destroy(&_mh);
	}

	inline void wait()
	{
		pthread_mutex_lock(const_cast <pthread_mutex_t *>(&_mh));
		while (!_f)
			pthread_cond_wait(const_cast <pthread_cond_t *>(&_cond),const_cast <pthread_mutex_t *>(&_mh));
		_f = false;
		pthread_mutex_unlock(const_cast <pthread_mutex_t *>(&_mh));
	}

	inline void post()
	{
		pthread_mutex_lock(const_cast <pthread_mutex_t *>(&_mh));
		_f = true;
		pthread_mutex_unlock(const_cast <pthread_mutex_t *>(&_mh));
		pthread_cond_signal(const_cast <pthread_cond_t *>(&_cond));
	}

private:
	pthread_cond_t _cond;
	pthread_mutex_t _mh;
	volatile bool _f;
};

} // namespace ZeroTier

#endif // !__WINDOWS__

#endif
