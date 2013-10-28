/*
 * ip tunnel/ethertap device for MacOSX.
 *
 * tuntap_manager definition.
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

#include "tuntap.h"
#include "mem.h"

extern "C" {

#include <sys/conf.h>
#include <sys/param.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <vm/vm_kern.h>

#include <miscfs/devfs/devfs.h>

}

#if 0
#define dprintf(...)			log(LOG_INFO, __VA_ARGS__)
#else
#define dprintf(...)
#endif

/* cdevsw for tuntap_manager */
static struct cdevsw mgr_cdevsw =
{
	tuntap_manager::cdev_open,
	tuntap_manager::cdev_close,
	tuntap_manager::cdev_read,
	tuntap_manager::cdev_write,
	tuntap_manager::cdev_ioctl,
	eno_stop,
	eno_reset,
	NULL,
	tuntap_manager::cdev_select,
	eno_mmap,
	eno_strat,
	eno_getc,
	eno_putc,
	0
};

/* tuntap_manager members */
tuntap_manager *tuntap_manager::mgr_map[MAX_CDEV];

bool tuntap_manager::statics_initialized = false;

/* static initializer */
void
tuntap_manager::initialize_statics()
{
	dprintf("initializing mgr_map\n");

	/* initialize the major-to-manager map */
	for (int i = 0; i < MAX_CDEV; i++)
		mgr_map[i] = NULL;

	statics_initialized = true;
}

bool
tuntap_manager::initialize(unsigned int count, char *family)
{
	this->count = count;
	this->family = family;
	this->tuntaps = NULL;

	if (!statics_initialized)
		initialize_statics();

	/* make sure noone can access the character devices until we are done */
	auto_lock l(&cdev_gate);

	/* register the switch for the tap character devices */
	dev_major = cdevsw_add(-1, &mgr_cdevsw);
	if (dev_major == -1) {
		log(LOG_ERR, "%s: could not register character device switch.\n", family);
		return false;
	}

	/* allocate memory for the interface instance table */
	tuntaps = (tuntap_interface **) mem_alloc(count * sizeof(tuntap_interface *));
	if (tuntaps == NULL)
	{
		log(LOG_ERR, "%s: no memory!\n", family);
		return false;
	}

	bzero(tuntaps, count * sizeof(tuntap_interface *));

	/* Create the interfaces. This will only add the character devices. The network devices will
	 * be created upon open()ing the corresponding character devices.
	 */
	for (int i = 0; i < (int) count; i++)
	{
		tuntaps[i] = create_interface();

		if (tuntaps[i] != NULL)
		{
			if (tuntaps[i]->initialize(dev_major, i))
			{
				continue;
			}

			/* error here. current interface needs to be shut down */
			i++;
		}

		/* something went wrong. clean up. */
		while (--i >= 0)
		{
			tuntaps[i]->shutdown();
			delete  tuntaps[i];
		}

		return false;
	}

	/* register the new family in the mgr switch */
	mgr_map[dev_major] = this;

	log(LOG_INFO, "%s kernel extension version %s <mattias.nissler@gmx.de>\n",
			family, TUNTAP_VERSION);

	return true;
}

bool
tuntap_manager::shutdown()
{
	bool ok = true;

	/* we halt the whole thing while we check whether we can shutdown */
	auto_lock l(&cdev_gate);

	/* anyone in? */
	if (cdev_gate.is_anyone_in()) {
		dprintf("tuntap_mgr: won't shutdown, threads still behind the gate.");
		ok = false;
	} else {
		/* query the interfaces to see if shutting down is ok */
		if (tuntaps != NULL) {
			for (unsigned int i = 0; i < count; i++) {
				if (tuntaps[i] != NULL)
					ok &= tuntaps[i]->idle();
			}

			/* if yes, do it now */
			if (ok) {
				for (unsigned int i = 0; i < count; i++) {
					if (tuntaps[i] != NULL) {
						tuntaps[i]->shutdown();
						delete tuntaps[i];
						tuntaps[i] = NULL;
					}
				}
			}
		}
	}

	/* unregister the character device switch */
	if (ok) {
		if (dev_major != -1 && cdevsw_remove(dev_major, &mgr_cdevsw) == -1) {
			log(LOG_WARNING,
				"%s: character device switch got lost. strange.\n", family);
		}
		mgr_map[dev_major] = NULL;
		dev_major = -1;

		/* at this point there is still a chance that some thread hangs at the cdev_gate in
		 * one of the cdev service functions. I can't imagine any way that would aviod this.
		 * So lets unblock the gate such that they fail.
		 */
		unsigned int old_number;
		do {
			old_number = cdev_gate.get_ticket_number();

			dprintf("tuntap_manager: waiting for other threads to give up.\n");

			/* wait one second */
			cdev_gate.sleep(&cdev_gate, 1000000);

		} while (cdev_gate.get_ticket_number() != old_number);

		/* I hope it is safe to unload now. */

	} else {
		log(LOG_WARNING, "%s: won't unload, at least one interface is busy.\n", family);
	}

	dprintf("tuntap manager: shutdown %s\n", ok ? "ok" : "failed");

	return ok;
}

tuntap_manager::~tuntap_manager()
{
	dprintf("freeing interface table\n");

	/* free memory */
	if (tuntaps != NULL)
		mem_free(tuntaps, count * sizeof(tuntap_interface *));
}

/* service method dispatchers */
int
tuntap_manager::cdev_open(dev_t dev, int flags, int devtype, proc_t p)
{
	return (mgr_map[major(dev)] == NULL ? ENOENT
			: mgr_map[major(dev)]->do_cdev_open(dev, flags, devtype, p));
}

int
tuntap_manager::cdev_close(dev_t dev, int flags, int devtype, proc_t p)
{
	return (mgr_map[major(dev)] == NULL ? EBADF
			: mgr_map[major(dev)]->do_cdev_close(dev, flags, devtype, p));
}

int
tuntap_manager::cdev_read(dev_t dev, uio_t uio, int ioflag)
{
	return (mgr_map[major(dev)] == NULL ? EBADF
			: mgr_map[major(dev)]->do_cdev_read(dev, uio, ioflag));
}

int
tuntap_manager::cdev_write(dev_t dev, uio_t uio, int ioflag)
{
	return (mgr_map[major(dev)] == NULL ? EBADF
			: mgr_map[major(dev)]->do_cdev_write(dev, uio, ioflag));
}

int
tuntap_manager::cdev_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, proc_t p)
{
	return (mgr_map[major(dev)] == NULL ? EBADF
			: mgr_map[major(dev)]->do_cdev_ioctl(dev, cmd, data, fflag, p));
}

int
tuntap_manager::cdev_select(dev_t dev, int which, void *wql, proc_t p)
{
	return (mgr_map[major(dev)] == NULL ? EBADF
			: mgr_map[major(dev)]->do_cdev_select(dev, which, wql, p));
}

/* character device service methods */
int
tuntap_manager::do_cdev_open(dev_t dev, int flags, int devtype, proc_t p)
{
	int dmin = minor(dev);
	int error = ENOENT;

	cdev_gate.enter();

	if (dmin < (int) count && dmin >= 0 && tuntaps[dmin] != NULL)
		error = tuntaps[dmin]->cdev_open(flags, devtype, p);

	cdev_gate.exit();

	return error;
}

int
tuntap_manager::do_cdev_close(dev_t dev, int flags, int devtype, proc_t p)
{
	int dmin = minor(dev);
	int error = EBADF;

	cdev_gate.enter();

	if (dmin < (int) count && dmin >= 0 && tuntaps[dmin] != NULL)
		error = tuntaps[dmin]->cdev_close(flags, devtype, p);

	cdev_gate.exit();

	return error;
}

int
tuntap_manager::do_cdev_read(dev_t dev, uio_t uio, int ioflag)
{
	int dmin = minor(dev);
	int error = EBADF;

	cdev_gate.enter();

	if (dmin < (int) count && dmin >= 0 && tuntaps[dmin] != NULL)
		error = tuntaps[dmin]->cdev_read(uio, ioflag);

	cdev_gate.exit();
	
	return error;
}

int
tuntap_manager::do_cdev_write(dev_t dev, uio_t uio, int ioflag)
{
	int dmin = minor(dev);
	int error = EBADF;

	cdev_gate.enter();

	if (dmin < (int) count && dmin >= 0 && tuntaps[dmin] != NULL)
		error = tuntaps[dmin]->cdev_write(uio, ioflag);

	cdev_gate.exit();
	
	return error;
}

int
tuntap_manager::do_cdev_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, proc_t p)
{
	int dmin = minor(dev);
	int error = EBADF;

	cdev_gate.enter();

	if (dmin < (int) count && dmin >= 0 && tuntaps[dmin] != NULL)
		error = tuntaps[dmin]->cdev_ioctl(cmd, data, fflag, p);

	cdev_gate.exit();
	
	return error;
}

int
tuntap_manager::do_cdev_select(dev_t dev, int which, void *wql, proc_t p)
{
	int dmin = minor(dev);
	int error = EBADF;

	cdev_gate.enter();

	if (dmin < (int) count && dmin >= 0 && tuntaps[dmin] != NULL)
		error = tuntaps[dmin]->cdev_select(which, wql, p);

	cdev_gate.exit();
	
	return error;
}

