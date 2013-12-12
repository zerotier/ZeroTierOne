/*
 * ip tunnel/ethertap device for MacOSX.
 *
 * Locking implementation.
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

#include "lock.h"

extern "C" {

#include <sys/syslog.h>
#include <sys/proc.h>

}

#if 0
#define dprintf(...)			log(LOG_INFO, __VA_ARGS__)
#else
#define dprintf(...)
#endif

/* class tt_lock */
lck_grp_t *tt_lock::tt_lck_grp = NULL;

bool
tt_lock::initialize()
{
	/* init if necessary */
	if (tt_lck_grp == NULL) {
		dprintf("initing lock group\n");
		tt_lck_grp = lck_grp_alloc_init("tuntap locks", LCK_GRP_ATTR_NULL);

		if (tt_lck_grp == NULL) {
			/* if something fails, the lock won't work */
			log(LOG_ERR, "tuntap: could not allocate locking group\n");
			return false;
		}
	}

	return true;
}

void
tt_lock::shutdown()
{
	/* free the locking group */
	if (tt_lck_grp != NULL) {
		dprintf("freeing lock group\n");
		lck_grp_free(tt_lck_grp);
		tt_lck_grp = NULL;
	}
}

/* tt_mutex */
tt_mutex::tt_mutex()
{
	/* fail if locking group not initialized */
	if (tt_lck_grp == NULL)
		return;

	/* allocate the lock */
	lck = lck_rw_alloc_init(tt_lck_grp, NULL);

	if (lck == NULL)
		log(LOG_ERR, "tuntap: could not allocate mutex\n");
}

tt_mutex::~tt_mutex()
{
	/* if the lock doesn't exist, this will be a no-op */
	if (lck == NULL)
		return;

	/* free the lock */
	lck_rw_free(lck, tt_lck_grp);
}

void
tt_mutex::lock()
{
	if (lck != NULL)
		lck_rw_lock_exclusive(lck);
}

void
tt_mutex::unlock()
{
	if (lck != NULL)
		lck_rw_unlock_exclusive(lck);
}

void
tt_mutex::sleep(void *cond)
{
	if (lck != NULL)
		lck_rw_sleep(lck, LCK_SLEEP_DEFAULT, cond, THREAD_INTERRUPTIBLE);
}

void
tt_mutex::sleep(void *cond, uint64_t timeout)
{
	if (lck != NULL)
		lck_rw_sleep_deadline(lck, LCK_SLEEP_DEFAULT, cond, THREAD_INTERRUPTIBLE, timeout);
}

void
tt_mutex::wakeup(void *cond)
{
	if (lck != NULL)
		::wakeup(cond);
}

/* tt_gate */
tt_gate::tt_gate()
	: ticket_number(0),
	population(0)
{
}

void
tt_gate::enter()
{
	/* just try to grab the lock, increase the ticket number and the population */
	auto_lock l(&slock);
	ticket_number++;
	population++;
}

void
tt_gate::exit()
{
	auto_lock l(&slock);
	ticket_number--;
	population--;
}

bool
tt_gate::is_anyone_in()
{
	return population != 0;
}

unsigned int
tt_gate::get_ticket_number()
{
	return ticket_number;
}

void
tt_gate::lock()
{
	slock.lock();
}

void
tt_gate::unlock()
{
	slock.unlock();
}

void
tt_gate::sleep(void* cond)
{
	slock.sleep(cond);
}

void
tt_gate::sleep(void* cond, uint64_t timeout)
{
	slock.sleep(cond, timeout);
}

void
tt_gate::wakeup(void* cond)
{
	slock.wakeup(cond);
}

