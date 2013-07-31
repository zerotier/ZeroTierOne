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

#include "Thread.hpp"

#if defined(__APPLE__) || defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdexcept>

extern "C" {
static void *__m_thread_main(void *ptr)
{
	((ZeroTier::Thread *)ptr)->__intl_run();
	return (void *)0;
}
}

namespace ZeroTier {

Thread::Thread() :
	_impl(malloc(sizeof(pthread_t))),
	_running()
{
	memset(_impl,0,sizeof(pthread_t));
}

Thread::~Thread()
{
	free(_impl);
}

void Thread::start()
{
	if (!*_running) {
		++_running;
		pthread_create((pthread_t *)_impl,(const pthread_attr_t *)0,&__m_thread_main,(void *)this);
	}
}

void Thread::join()
{
	void *tmp;
	if (*_running)
		pthread_join(*((pthread_t *)_impl),&tmp);
}

void Thread::sleep(unsigned long ms)
{
	usleep(ms);
}

void Thread::__intl_run()
{
	for(;;) {
		_notInit = false;
		this->main();
		if (_notInit) // UGLY ASS HACK: see main()
			usleep(50);
		else break;
	}
	--_running;
}

void Thread::main()
	throw()
{
	_notInit = true; // UGLY ASS HACK: retry if subclass has not defined virtual function pointer yet
}

} // namespace ZeroTier

#endif

#ifdef _WIN32

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>

DWORD WINAPI __m_thread_main(LPVOID lpParam)
{
	((ZeroTier::Thread *)lpParam)->__intl_run();
	return 0;
}

struct __m_thread_info
{
	HANDLE threadHandle;
	DWORD threadId;
	bool started;
};

namespace ZeroTier {

Thread::Thread() :
	_impl(malloc(sizeof(__m_thread_info))),
	_running()
{
	memset(_impl,0,sizeof(__m_thread_info));
}

Thread::~Thread()
{
	if (((__m_thread_info *)_impl)->started)
		CloseHandle(((__m_thread_info *)_impl)->threadHandle);
	free(_impl);
}

void Thread::start()
{
	if (!*_running) {
		++_running;
		if ((((__m_thread_info *)_impl)->threadHandle = CreateThread(NULL,0,__m_thread_main,this,0,&(((__m_thread_info *)_impl)->threadId))) != NULL) {
			((__m_thread_info *)_impl)->started = true;
		}
	}
}

void Thread::join()
{
	if (*_running)
		WaitForSingleObject(((__m_thread_info *)_impl)->threadHandle,INFINITE);
}

void Thread::__intl_run()
{
	for(;;) {
		_notInit = false;
		this->main();
		if (_notInit)
			Thread::sleep(50);
		else break;
	}
	--_running;
}

void Thread::main()
	throw()
{
	_notInit = true; // HACK: retry if subclass has not defined virtual function pointer yet
}

struct _Thread_RunInBackgroundData
{
	void (*func)(void *);
	void *ptr;
	HANDLE threadHandle;
	DWORD threadId;
};

} // namespace ZeroTier

#endif
