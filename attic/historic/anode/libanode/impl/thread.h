/* libanode: the Anode C reference implementation
 * Copyright (C) 2009-2010 Adam Ierymenko <adam.ierymenko@gmail.com>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _ANODE_THREAD_H
#define _ANODE_THREAD_H

#ifdef WINDOWS

#include <windows.h>
#include <thread.h>
typedef DWORD AnodeThreadId;

#else /* not WINDOWS */

#include <pthread.h>
typedef pthread_t AnodeThreadId;

#define AnodeThread_self() pthread_self()
#define AnodeThreadId_equal(a,b) pthread_equal((pthread_t)(a),(pthread_t)(b))

#endif

typedef void AnodeThread;

/**
 * Create and launch a new thread
 *
 * If wait_for_join is true (nonzero), the thread can and must be joined. The
 * thread object won't be freed until join is called and returns. If
 * wait_for_join is false, the thread object frees itself automatically on
 * termination.
 *
 * If wait_for_join is false (zero), there is really no need to keep track of
 * the thread object.
 *
 * @param func Function to call as thread main
 * @param arg Argument to pass to function
 * @param wait_for_join If false, thread deletes itself when it terminates
 */
AnodeThread *AnodeThread_create(void (*func)(void *),void *arg,int wait_for_join);

/**
 * Wait for a thread to terminate and delete thread object
 *
 * This can only be used for threads created with wait_for_join set to true.
 * The thread object is no longer valid after this call.
 *
 * @param thread Thread to wait for termination and delete
 */
void AnodeThread_join(AnodeThread *thread);

#endif
