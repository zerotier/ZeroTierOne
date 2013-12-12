/*
 * ip tunnel/ethertap device for MacOSX.
 *
 * The class tuntaptap_interface contains the common functionality of tuntap_interface and
 * tap_interface.
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

#ifndef __TUNTAP_H__
#define __TUNTAP_H__

#include "util.h"
#include "lock.h"

extern "C" {

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/systm.h>
#include <sys/kpi_mbuf.h>

#include <kern/locks.h>

#include <net/if.h>
#include <net/bpf.h>
#include <net/kpi_interface.h>

}

extern "C" {

errno_t tuntap_if_output(ifnet_t ifp, mbuf_t m);
errno_t tuntap_if_ioctl(ifnet_t ifp, long unsigned int cmd, void *arg);
errno_t tuntap_if_set_bpf_tap(ifnet_t ifp, bpf_tap_mode mode, int (*cb)(ifnet_t, mbuf_t));
errno_t tuntap_if_demux(ifnet_t ifp, mbuf_t m, char *header, protocol_family_t *proto);
errno_t tuntap_if_framer(ifnet_t ifp, mbuf_t *m, const struct sockaddr *dest,
		const char *dest_linkaddr, const char *frame_type);
errno_t tuntap_if_add_proto(ifnet_t ifp, protocol_family_t proto,
		const struct ifnet_demux_desc *ddesc, u_int32_t ndesc);
errno_t tuntap_if_del_proto(ifnet_t ifp, protocol_family_t proto);
errno_t tuntap_if_check_multi(ifnet_t ifp, const struct sockaddr *maddr);
void tuntap_if_detached(ifnet_t ifp);

}

/* forward declaration */
class tuntap_interface;

/* both interface families have their manager object that will create, initialize, shutdown and
 * delete interfaces. This is (mostly) generic so it can be used both for tun and tap. The only
 * exception is the interface creation, therefore this class is abstract. tun and tap have their own
 * versions that simply fill in create_interface().
 */
class tuntap_manager {

	protected:
		/* manager cdev gate */
		tt_gate cdev_gate;
		/* interface count */
		unsigned int count;
		/* an array holding all the interface instances */
		tuntap_interface **tuntaps;
		/* the major device number */
		int dev_major;
		/* family name */
		char *family;

		/* wether static members are initialized */
		static bool statics_initialized;

		/* major-to-manager-map */
		static const int MAX_CDEV = 256;
		static tuntap_manager *mgr_map[MAX_CDEV];

		/* initializes static members */
		void initialize_statics();

	public:
		/* sets major device number, allocates the interface table. */
		bool initialize(unsigned int count, char *family);

		/* tries to shutdown the family. returns true if successful. the manager object may
		 * not be deleted if this wasn't called successfully.
		 */
		bool shutdown();

		/* the destructor deletes allocated memory and unregisters the character device
		 * switch */
		virtual ~tuntap_manager();

		/* here are the cdev routines for the class. They will figure out the manager object
		 * and call the service methods declared below.
		 */
		static int cdev_open(dev_t dev, int flags, int devtype, proc_t p);
		static int cdev_close(dev_t dev, int flags, int devtype, proc_t p);
		static int cdev_read(dev_t dev, uio_t uio, int ioflag);
		static int cdev_write(dev_t dev, uio_t uio, int ioflag);
		static int cdev_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag,
				proc_t p);
		static int cdev_select(dev_t dev, int which, void *wql, proc_t p);

	protected:
		/* Here are the actual service routines that will do the required things (creating
		 * interfaces and such) and forward to the interface's implementation.
		 */
		int do_cdev_open(dev_t dev, int flags, int devtype, proc_t p);
		int do_cdev_close(dev_t dev, int flags, int devtype, proc_t p);
		int do_cdev_read(dev_t dev, uio_t uio, int ioflag);
		int do_cdev_write(dev_t dev, uio_t uio, int ioflag);
		int do_cdev_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, proc_t p);
		int do_cdev_select(dev_t dev, int which, void *wql, proc_t p);

		/* abstract method that will create an interface. Implemented by tun and tap */
		virtual tuntap_interface *create_interface() = 0;
		
		/* makes sure there is one idle interface available (if nothing fails */
		void ensure_idle_device();

};

/* a class implementing a mbuf packet queue. On Darwin 7 we had struct ifqueue, but that is now
 * internal to the kernel for Darwin 8. So lets have our own.
 */
class tuntap_mbuf_queue {

	private:
		/* output end of the queue. dequeueing takes mbufs from here */
		mbuf_t head;
		/* input end. new mbufs are appended here. */
		mbuf_t tail;

		/* size */
		unsigned int size;

		/* maximum queue size */
		static const unsigned int QUEUE_SIZE = 128;

	public:
		/* initialize new empty queue */
		tuntap_mbuf_queue();
		~tuntap_mbuf_queue();

		/* is the queue full? */
		bool full() { return size == QUEUE_SIZE; }
		/* is it emtpy? */
		bool empty() { return size == 0; }

		/* enqueue an mbuf. returns true if there was space left, so the mbuf could be
		 * queued, false otherwise */
		bool enqueue(mbuf_t mb);

		/* tries to dequeue the next mbuf. If the queue is empty, NULL is returned */
		mbuf_t dequeue();

		/* makes the queue empty, discarding any queue packets */
		void clear();
};

class tuntap_interface {

	protected:
		/* interface number */
		unsigned int unit;
		/* family name */
		char *family_name;
		/* family identifier */
		ifnet_family_t family;
		/* interface type */
		u_int32_t type;
		/* id string */
		static const unsigned int UIDLEN = 20;
		char unique_id[UIDLEN];

		/* synchronization */
		tt_mutex lock;
		tt_mutex bpf_lock;
		tt_mutex detach_lock;

		/* the interface structure registered */
		ifnet_t ifp;
		/* whether the device has been opened */
		bool open;
		/* whether we are doing blocking i/o */
		bool block_io;
		/* whether the interface has properly been detached */
		bool interface_detached;
		/* handle to the devfs node for the character device */
		void *dev_handle;
		/* the pid of the process that opened the cdev, if any */
		pid_t pid;
		/* read select info */
		struct selinfo rsel;
		/* bpf mode, wether filtering is on or off */
		bpf_tap_mode bpf_mode;
		/* bpf callback. called when packet arrives/leaves */
		int (*bpf_callback)(ifnet_t, mbuf_t);
		/* pending packets queue (for output), must be accessed with the lock held */
		tuntap_mbuf_queue send_queue;
		/* whether an ioctl that we issued is currently being processed */
		bool in_ioctl;

		/* protected constructor. initializes most of the members */
		tuntap_interface();
		virtual ~tuntap_interface();

		/* initialize the device */
		virtual bool initialize(unsigned short major, unsigned short unit) = 0;

		/* character device management */
		virtual bool register_chardev(unsigned short major);
		virtual void unregister_chardev();

		/* network interface management */
		virtual bool register_interface(const struct sockaddr_dl *lladdr,
				void *bcaddr, u_int32_t bcaddrlen);
		virtual void unregister_interface();
		virtual void cleanup_interface();

		/* called when the character device is opened in order to intialize the network
		 * interface.
		 */
		virtual int initialize_interface() = 0;
		/* called when the character device is closed to shutdown the network interface */
		virtual void shutdown_interface() = 0;

		/* check wether the interface is idle (so it can be brought down) */
		virtual bool idle();

		/* shut it down */
		virtual void shutdown() = 0;

		/* notifies BPF of a packet coming through */
		virtual void notify_bpf(mbuf_t mb, bool out);

		/* executes a socket ioctl through a temporary socket */
		virtual void do_sock_ioctl(sa_family_t af, unsigned long cmd, void* arg);

		/* character device service methods. Called by the manager */
		virtual int cdev_open(int flags, int devtype, proc_t p);
		virtual int cdev_close(int flags, int devtype, proc_t p);
		virtual int cdev_read(uio_t uio, int ioflag);
		virtual int cdev_write(uio_t uio, int ioflag);
		virtual int cdev_ioctl(u_long cmd, caddr_t data, int fflag, proc_t p);
		virtual int cdev_select(int which, void *wql, proc_t p);

		/* interface functions. friends and implementation methods */
		friend errno_t tuntap_if_output(ifnet_t ifp, mbuf_t m);
		friend errno_t tuntap_if_ioctl(ifnet_t ifp, long unsigned int cmd, void *arg);
		friend errno_t tuntap_if_set_bpf_tap(ifnet_t ifp, bpf_tap_mode mode,
				int (*cb)(ifnet_t, mbuf_t));
		friend errno_t tuntap_if_demux(ifnet_t ifp, mbuf_t m, char *header,
				protocol_family_t *proto);
		friend errno_t tuntap_if_framer(ifnet_t ifp, mbuf_t *m, const struct sockaddr *dest,
				const char *dest_linkaddr, const char *frame_type);
		friend errno_t tuntap_if_add_proto(ifnet_t ifp, protocol_family_t proto,
				const struct ifnet_demux_desc *ddesc, u_int32_t ndesc);
		friend errno_t tuntap_if_del_proto(ifnet_t ifp, protocol_family_t proto);
		friend errno_t tuntap_if_check_multi(ifnet_t ifp, const struct sockaddr *maddr);
		friend void tuntap_if_detached(ifnet_t ifp);

		virtual errno_t if_output(mbuf_t m);
		virtual errno_t if_ioctl(u_int32_t cmd, void *arg);
		virtual errno_t if_set_bpf_tap(bpf_tap_mode mode, int (*cb)(ifnet_t, mbuf_t));
		virtual errno_t if_demux(mbuf_t m, char *header, protocol_family_t *proto) = 0;
		virtual errno_t if_framer(mbuf_t *m, const struct sockaddr *dest,
				const char *dest_linkaddr, const char *frame_type) = 0;
		virtual errno_t if_add_proto(protocol_family_t proto,
				const struct ifnet_demux_desc *ddesc, u_int32_t ndesc) = 0;
		virtual errno_t if_del_proto(protocol_family_t proto) = 0;
		virtual errno_t if_check_multi(const struct sockaddr *maddr);
		virtual void if_detached();

		/* tuntap_manager feeds us with cdev input, so it is our friend */
		friend class tuntap_manager;
};

#endif /* __TUNTAP_H__ */

