/*
 * ip tunnel/ethertap device for MacOSX.
 *
 * Locking is not as straightforward for Tiger. So declare our own locking class.
 */
/*
 * Copyright (c) 2011 Mattias Nissler <mattias.nissler@gmx.de>
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *      conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list of
 *      conditions and the following disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. The name of the author may not be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __LOCK_H__
#define __LOCK_H__

extern "C" {

#include <kern/locks.h>
#include <sys/param.h>

}

/* our own locking class. declares the common interface of the locking primitives. */
class tt_lock {
	
	protected:
		/* locking group */
		static lck_grp_t *tt_lck_grp;

	public:
		/* be virtual */
		virtual ~tt_lock() { };

		/* static intialization (inits the locking group) */
		static bool initialize();
		static void shutdown();

		/* locking */
		virtual void lock() = 0;
		virtual void unlock() = 0;

		/* monitor primitives */
		virtual void sleep(void* cond) = 0;
		virtual void sleep(void* cond, uint64_t) = 0;
		virtual void wakeup(void* cond) = 0;
};

/* simple mutex */
class tt_mutex : public tt_lock {

	private:
		/* underlying darwin lock */
		lck_rw_t *lck;

	public:
		tt_mutex();
		virtual ~tt_mutex();

		void lock();
		void unlock();

		/* monitor primitives */
		void sleep(void* cond);
		void sleep(void* cond, uint64_t);
		void wakeup(void* cond);
};

/* A very special locking class that we use to track threads that enter and leave the character
 * device service functions. They call enter() before entering the actual service routinge and
 * exit() when done. enter() only permits them to pass when the gate isn't locked. Furthermore, the
 * gate assigns ticket numbers to everyone that passes the gate, so you can check whether more
 * threads came through. See tuntap_mgr::shutdown() for how we use that stuff.
 */
class tt_gate : public tt_lock {

	private:
		/* synchronization lock */
		tt_mutex slock;
		/* ticket number */
		unsigned int ticket_number;
		/* count of threads that are in */
		unsigned int population;

	public:
		/* construct a new gate */
		tt_gate();

		/* enter - pass the gate */
		void enter();
		/* exit - pass the gate */
		void exit();

		/* check whether anyone is in */
		bool is_anyone_in();
		/* gets the next ticket number */
		unsigned int get_ticket_number();

		/* lock the gate */
		void lock();
		/* unlock the gate */
		void unlock();

		/* monitor primitives */
		void sleep(void* cond);
		void sleep(void* cond, uint64_t);
		void wakeup(void* cond);
};

/* auto_lock and auto_rwlock serve as automatic lock managers: Create an object, passing the
 * tt_[rw]lock you want to lock to have it grab the lock. When the object goes out of scope, the
 * destructor of the class will release the lock.
 */
class auto_lock {

	protected:
		/* the lock we hold */
		tt_lock *l;

	public:
		auto_lock(tt_lock *m)
			: l(m)
		{
			lock();
		}

		~auto_lock()
		{
			unlock();
		}

		void lock()
		{
			l->lock();
		}

		void unlock()
		{
			l->unlock();
		}
};

#endif /* __LOCK_H__ */

