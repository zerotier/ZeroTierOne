/*
 * ip tunnel/ethertap device for MacOSX. Common functionality of tap_interface and tun_interface.
 *
 * tuntap_interface class definition
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

#if 0
#define dprintf(...)			log(LOG_INFO, __VA_ARGS__)
#else
#define dprintf(...)
#endif

extern "C" {

#include <sys/conf.h>
#include <sys/syslog.h>
#include <sys/param.h>
#include <sys/filio.h>
#include <sys/sockio.h>
#include <sys/fcntl.h>
#include <sys/kpi_socket.h>

#include <vm/vm_kern.h>

#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_arp.h>

#include <miscfs/devfs/devfs.h>

}

extern "C" {

/* interface service functions that delegate to the appropriate tuntap_interface instance */
errno_t
tuntap_if_output(ifnet_t ifp, mbuf_t m)
{
	if (ifp != NULL) {
		tuntap_interface *ttif = (tuntap_interface *) ifnet_softc(ifp);
		if (ttif != NULL)
			return ttif->if_output(m);
	}

	if (m != NULL)
		mbuf_freem_list(m);

	return ENODEV;
}

errno_t
tuntap_if_ioctl(ifnet_t ifp, long unsigned int cmd, void *arg)
{
	if (ifp != NULL) {
		tuntap_interface *ttif = (tuntap_interface *) ifnet_softc(ifp);
		if (ttif != NULL)
			return ttif->if_ioctl(cmd, arg);
	}

	return ENODEV;
}

errno_t
tuntap_if_set_bpf_tap(ifnet_t ifp, bpf_tap_mode mode, int (*cb)(ifnet_t, mbuf_t))
{
	if (ifp != NULL) {
		tuntap_interface *ttif = (tuntap_interface *) ifnet_softc(ifp);
		if (ttif != NULL)
			return ttif->if_set_bpf_tap(mode, cb);
	}

	return ENODEV;
}

errno_t
tuntap_if_demux(ifnet_t ifp, mbuf_t m, char *header, protocol_family_t *proto)
{
	if (ifp != NULL) {
		tuntap_interface *ttif = (tuntap_interface *) ifnet_softc(ifp);
		if (ttif != NULL)
			return ttif->if_demux(m, header, proto);
	}

	return ENODEV;
}

errno_t
tuntap_if_framer(ifnet_t ifp, mbuf_t *m, const struct sockaddr *dest, const char *dest_linkaddr,
		const char *frame_type)
{
	if (ifp != NULL) {
		tuntap_interface *ttif = (tuntap_interface *) ifnet_softc(ifp);
		if (ttif != NULL)
			return ttif->if_framer(m, dest, dest_linkaddr, frame_type);
	}

	return ENODEV;
}

errno_t
tuntap_if_add_proto(ifnet_t ifp, protocol_family_t proto, const struct ifnet_demux_desc *ddesc,
		u_int32_t ndesc)
{
	if (ifp != NULL) {
		tuntap_interface *ttif = (tuntap_interface *) ifnet_softc(ifp);
		if (ttif != NULL)
			return ttif->if_add_proto(proto, ddesc, ndesc);
	}

	return ENODEV;
}

errno_t
tuntap_if_del_proto(ifnet_t ifp, protocol_family_t proto)
{
	if (ifp != NULL) {
		tuntap_interface *ttif = (tuntap_interface *) ifnet_softc(ifp);
		if (ttif != NULL)
			return ttif->if_del_proto(proto);
	}

	return ENODEV;
}

errno_t
tuntap_if_check_multi(ifnet_t ifp, const struct sockaddr* maddr)
{
	if (ifp != NULL)
	{
		tuntap_interface *ttif = (tuntap_interface *) ifnet_softc(ifp);
		if (ttif != NULL)
			return ttif->if_check_multi(maddr);
	}

	return ENODEV;
}

void
tuntap_if_detached(ifnet_t ifp)
{
	if (ifp != NULL) {
		tuntap_interface *ttif = (tuntap_interface *) ifnet_softc(ifp);
		if (ttif != NULL)
			ttif->if_detached();
	}
}

errno_t
tuntap_if_noop_output(ifnet_t, mbuf_t)
{
	return ENODEV;
}

errno_t
tuntap_if_noop_demux(ifnet_t, mbuf_t, char*, protocol_family_t*)
{
	return ENODEV;
}

errno_t
tuntap_if_noop_add_proto(ifnet_t, protocol_family_t, const struct ifnet_demux_desc*, u_int32_t)
{
	return ENODEV;
}

errno_t
tuntap_if_noop_del_proto(ifnet_t, protocol_family_t)
{
	return ENODEV;
}

} /* extern "C" */

/* tuntap_mbuf_queue */
tuntap_mbuf_queue::tuntap_mbuf_queue()
{
	head = tail = NULL;
	size = 0;
}

tuntap_mbuf_queue::~tuntap_mbuf_queue()
{
	clear();
}

bool
tuntap_mbuf_queue::enqueue(mbuf_t mb)
{
	if (size == QUEUE_SIZE)
		return false;

	mbuf_setnextpkt(mb, NULL);

	if (head == NULL)
		head = tail = mb;
	else {
		mbuf_setnextpkt(tail, mb);
		tail = mb;
	}
	size++;

	return true;
}

mbuf_t
tuntap_mbuf_queue::dequeue()
{
	mbuf_t ret;

	/* check wether there is a packet in the queue */
	if (head == NULL)
		return NULL;

	/* fetch it */
	ret = head;
	head = mbuf_nextpkt(head);
	mbuf_setnextpkt(ret, NULL);
	size--;

	return ret;
}

void
tuntap_mbuf_queue::clear()
{
	/* free mbufs that are in the queue */
	if (head != NULL)
		mbuf_freem_list(head);

	head = NULL;
	tail = NULL;
	size = 0;
}

/* tuntap_interface members */
tuntap_interface::tuntap_interface()
{
	/* initialize the members */
	ifp = NULL;
	open = false;
	block_io = true;
	dev_handle = NULL;
	pid = 0;
	selthreadclear(&rsel);
	bpf_mode = BPF_MODE_DISABLED;
	bpf_callback = NULL;
	bzero(unique_id, UIDLEN);
	in_ioctl = false;
}

tuntap_interface::~tuntap_interface()
{
}

bool
tuntap_interface::register_chardev(unsigned short major)
{
	/* register character device */
	dev_handle = devfs_make_node(makedev(major, unit), DEVFS_CHAR, 0, 0, 0660, "%s%d",
			family_name, (int) unit);

	if (dev_handle == NULL) {
		log(LOG_ERR, "tuntap: could not make /dev/%s%d\n", family_name, (int) unit);
		return false;
	}

	return true;
}

void
tuntap_interface::unregister_chardev()
{
	dprintf("unregistering character device\n");

	/* unregister character device */
	if (dev_handle != NULL)
		devfs_remove(dev_handle);
	dev_handle = NULL;
}

bool
tuntap_interface::register_interface(const struct sockaddr_dl* lladdr, void *bcaddr,
		u_int32_t bcaddrlen)
{
	struct ifnet_init_params ip;
	errno_t err;

	dprintf("register_interface\n");

	/* initialize an initialization info struct */
	ip.uniqueid_len = UIDLEN;
	ip.uniqueid = unique_id;
	ip.name = family_name;
	ip.unit = unit;
	ip.family = family;
	ip.type = type;
	ip.output = tuntap_if_output;
	ip.demux = tuntap_if_demux;
	ip.add_proto = tuntap_if_add_proto;
	ip.del_proto = tuntap_if_del_proto;
	ip.check_multi = tuntap_if_check_multi;
	ip.framer = tuntap_if_framer;
	ip.softc = this;
	ip.ioctl = tuntap_if_ioctl;
	ip.set_bpf_tap = tuntap_if_set_bpf_tap;
	ip.detach = tuntap_if_detached;
	ip.event = NULL;
	ip.broadcast_addr = bcaddr;
	ip.broadcast_len = bcaddrlen;

	dprintf("tuntap: tuntap_if_check_multi is at 0x%08x\n", (void*) tuntap_if_check_multi);

	/* allocate the interface */
	err = ifnet_allocate(&ip, &ifp);
	if (err) {
		log(LOG_ERR, "tuntap: could not allocate interface for %s%d: %d\n", family_name,
				(int) unit, err);
		ifp = NULL;
		return false;
	}

	/* activate the interface */
	err = ifnet_attach(ifp, lladdr);
	if (err) {
		log(LOG_ERR, "tuntap: could not attach interface %s%d: %d\n", family_name,
				(int) unit, err);
		ifnet_release(ifp);
		ifp = NULL;
		return false;
	}

	dprintf("setting interface flags\n");

	/* set interface flags */
	ifnet_set_flags(ifp, IFF_RUNNING | IFF_MULTICAST | IFF_SIMPLEX, (u_int16_t) ~0UL);

	dprintf("flags: %x\n", ifnet_flags(ifp));
	
	return true;
}

void
tuntap_interface::unregister_interface()
{
	errno_t err;

	dprintf("unregistering network interface\n");

	if (ifp != NULL) {
		interface_detached = false;

		/* detach interface */
		err = ifnet_detach(ifp);
		if (err)
			log(LOG_ERR, "tuntap: error detaching interface %s%d: %d\n",
					family_name, unit, err);

		dprintf("interface detaching\n");

		/* Wait until the interface has completely been detached. */
		detach_lock.lock();
		while (!interface_detached)
			detach_lock.sleep(&interface_detached);
		detach_lock.unlock();

		dprintf("interface detached\n");

		/* release the interface */
		ifnet_release(ifp);

		ifp = NULL;
	}

	dprintf("network interface unregistered\n");
}

void
tuntap_interface::cleanup_interface()
{
	errno_t err;
	ifaddr_t *addrs;
	ifaddr_t *a;
	struct ifreq ifr;

	/* mark the interface down */
	ifnet_set_flags(ifp, 0, IFF_UP | IFF_RUNNING);

	/* Unregister all interface addresses. This works around a deficiency in the Darwin kernel.
	 * If we don't remove all IP addresses that are attached to the interface it can happen that
	 * the IP code fails to clean them up itself. When the interface is recycled, the IP code
	 * might then think some addresses are still attached to the interface...
	 */

	err = ifnet_get_address_list(ifp, &addrs);
	if (!err) {

		/* Execute a SIOCDIFADDR ioctl for each address. For technical reasons, we can only
		 * do that with a socket of the appropriate family. So try to create a dummy socket.
		 * I know this is a little expensive, but better than crashing...
		 *
		 * This really sucks.
		 */
		for (a = addrs; *a != NULL; a++) {
			/* initialize the request parameters */
			snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s%d",
				ifnet_name(ifp), ifnet_unit(ifp));
			ifaddr_address(*a, &(ifr.ifr_addr), sizeof(ifr.ifr_addr));
			if (ifr.ifr_addr.sa_family != AF_INET)
				continue;

			dprintf("trying to delete address of family %d\n", ifr.ifr_addr.sa_family);

			do_sock_ioctl(ifr.ifr_addr.sa_family, SIOCDIFADDR, &ifr);
		}

		/* release the address list */
		ifnet_free_address_list(addrs);
	}
}

bool
tuntap_interface::idle()
{
	return !(open);
}

void
tuntap_interface::notify_bpf(mbuf_t mb, bool out)
{
	auto_lock l(&bpf_lock);

	if ((out && bpf_mode == BPF_MODE_OUTPUT)
			|| (!out && bpf_mode == BPF_MODE_INPUT)
			|| (bpf_mode == BPF_MODE_INPUT_OUTPUT))
		(*bpf_callback)(ifp, mb);
}

void
tuntap_interface::do_sock_ioctl(sa_family_t af, unsigned long cmd, void* arg) {
	if (in_ioctl) {
		log(LOG_ERR, "tuntap: ioctl recursion detected, aborting.\n");
		return;
	}

	socket_t sock;
	errno_t err = sock_socket(af, SOCK_RAW, 0, NULL, NULL, &sock);
	if (err) {
		log(LOG_ERR, "tuntap: failed to create socket: %d\n", err);
		return;
	}

	in_ioctl = true;

	/* issue the ioctl */
	err = sock_ioctl(sock, cmd, arg);
	if (err)
		log(LOG_ERR, "tuntap: socket ioctl %d failed: %d\n", cmd, err);

	in_ioctl = false;

	/* get rid of the socket */
	sock_close(sock);
}

/* character device service methods */
int
tuntap_interface::cdev_open(int flags, int devtype, proc_t p)
{
	dprintf("tuntap: cdev_open()\n");

	/* grab the lock so that there can only be one thread inside */
	auto_lock l(&lock);

	/* check wether it is already open */
	if (open)
		return EBUSY;

	/* bring the network interface up */
	int error = initialize_interface();
	if (error)
		return error;

	open = true;
	pid = proc_pid(p);

	return 0;
}

int
tuntap_interface::cdev_close(int flags, int devtype, proc_t p)
{
	dprintf("tuntap: cdev_close()\n");

	auto_lock l(&lock);

	if (open) {
		open = false;

		/* shut down the network interface */
		shutdown_interface();

		/* clear the queue */
		send_queue.clear();

		/* wakeup the cdev thread and notify selects */
		wakeup(this);
		selwakeup(&rsel);

		return 0;
	}

	return EBADF;
}

int
tuntap_interface::cdev_read(uio_t uio, int ioflag)
{
	auto_lock l(&lock);

	unsigned int nb = 0;
	int error;

	dprintf("tuntap: cdev read\n");

	if (!open || ifp == NULL || !(ifnet_flags(ifp) & IFF_UP))
		return EIO;

	/* fetch a new mbuf from the queue if necessary */
	mbuf_t cur_mbuf = NULL;
	while (cur_mbuf == NULL) {
		dprintf("tuntap: fetching new mbuf\n");

		cur_mbuf = send_queue.dequeue();
		if (cur_mbuf == NULL) {
			/* nothing in queue, block or return */
			if (!block_io) {
				dprintf("tuntap: aborting (nbio)\n");
				return EWOULDBLOCK;
			} else {
				/* block */
				dprintf("tuntap: waiting\n");
				/* release the lock while waiting */
				l.unlock();
				error = msleep(this, NULL, PZERO | PCATCH, "tuntap", NULL);

				l.lock();

				if (error)
					return error;

				/* see whether the device was closed in the meantime */
				if (!open || ifp == NULL || !(ifnet_flags(ifp) & IFF_UP))
					return EIO;

			}
		}
	}

	/* notify bpf */
	notify_bpf(cur_mbuf, true);

	/* output what we have */
	do {
		dprintf("tuntap: got new mbuf: %p uio_resid: %d\n", cur_mbuf, uio_resid(uio));

		/* now we have an mbuf */
		int chunk_len = min(mbuf_len(cur_mbuf), uio_resid(uio));
		error = uiomove((char *) mbuf_data(cur_mbuf), chunk_len, uio);
		if (error) {
			mbuf_freem(cur_mbuf);
			return error;
		}
		nb += chunk_len;

		dprintf("tuntap: moved %d bytes to userspace uio_resid: %d\n", chunk_len,
				uio_resid(uio));

		/* update cur_mbuf */
		cur_mbuf = mbuf_free(cur_mbuf);

	} while (uio_resid(uio) > 0 && cur_mbuf != NULL);

	/* update statistics */
	ifnet_stat_increment_out(ifp, 1, nb, 0);

	/* still data left? forget about that ;-) */
	if (cur_mbuf != NULL)
		mbuf_freem(cur_mbuf);

	dprintf("tuntap: read done\n");

	return 0;
}

int
tuntap_interface::cdev_write(uio_t uio, int ioflag)
{
	auto_lock l(&lock);

	if (!open || ifp == NULL || !(ifnet_flags(ifp) & IFF_UP))
		return EIO;

	dprintf("tuntap: cdev write. uio_resid: %d\n", uio_resid(uio));

	/* pack the data into an mbuf chain */
	mbuf_t first, mb;

	/* first we need an mbuf having a header */
	mbuf_gethdr(MBUF_WAITOK, MBUF_TYPE_DATA, &first);
	if (first == NULL) {
		log(LOG_ERR, "tuntap: could not get mbuf.\n");
		return ENOMEM;
	}
	mbuf_setlen(first, 0);

	unsigned int mlen = mbuf_maxlen(first);
	unsigned int chunk_len;
	unsigned int copied = 0;
	int error;

	/* stuff the data into the mbuf(s) */
	mb = first;
	while (uio_resid(uio) > 0) {
		/* copy a chunk. enforce mtu (don't know if this is correct behaviour) */
		chunk_len = min(ifnet_mtu(ifp), min(uio_resid(uio), mlen));
		error = uiomove((caddr_t) mbuf_data(mb), chunk_len, uio);
		if (error) {
			log(LOG_ERR, "tuntap: could not copy data from userspace: %d\n", error);
			mbuf_freem(first);
			return error;
		}

		dprintf("tuntap: copied %d bytes, uio_resid %d\n", chunk_len,
				uio_resid(uio));

		mlen -= chunk_len;
		mbuf_setlen(mb, mbuf_len(mb) + chunk_len);
		copied += chunk_len;

		/* if done, break the loop */
		if (uio_resid(uio) <= 0 || copied >= ifnet_mtu(ifp))
			break;

		/* allocate a new mbuf if the current is filled */
		if (mlen == 0) {
			mbuf_t next;
			mbuf_get(MBUF_WAITOK, MBUF_TYPE_DATA, &next);
			if (next == NULL) {
				log(LOG_ERR, "tuntap: could not get mbuf.\n");
				mbuf_freem(first);
				return ENOMEM;
			}
			mbuf_setnext(mb, next);
			mb = next;
			mbuf_setlen(mb, 0);
			mlen = mbuf_maxlen(mb);
		}
	}

	/* fill in header info */
	mbuf_pkthdr_setrcvif(first, ifp);
	mbuf_pkthdr_setlen(first, copied);
	mbuf_pkthdr_setheader(first, mbuf_data(first));
	mbuf_set_csum_performed(first, 0, 0);

	/* update statistics */
	ifnet_stat_increment_in(ifp, 1, copied, 0);

	dprintf("tuntap: mbuf chain constructed. first: %p mb: %p len: %d data: %p\n",
			first, mb, mbuf_len(first), mbuf_data(first));

	/* notify bpf */
	notify_bpf(first, false);

	/* need to adjust the data pointer to point directly behind the linklevel header. The header
	 * itself is later accessed via m_pkthdr.header. Well, if something is ugly, here is it.
	 */
	mbuf_adj(first, ifnet_hdrlen(ifp));

	/* pass the packet over to the network stack */
	error = ifnet_input(ifp, first, NULL);

	if (error) {
		log(LOG_ERR, "tuntap: could not input packet into network stack.\n");
		mbuf_freem(first);
		return error;
	}

	return 0;
}

int
tuntap_interface::cdev_ioctl(u_long cmd, caddr_t data, int fflag, proc_t p)
{
	auto_lock l(&lock);

	dprintf("tuntap: cdev ioctl: %d\n", (int) (cmd & 0xff));

	switch (cmd) {
		case FIONBIO:
			/* set i/o mode */
			block_io = *((int *) data) ? false : true;
			return 0;
		case FIOASYNC:
			/* don't allow switching it on */
			if (*((int *) data))
				return ENOTTY;
			return 0;
	}

	return ENOTTY;
}

int
tuntap_interface::cdev_select(int which, void *wql, proc_t p)
{
	auto_lock l(&lock);

	int ret = 0;

	dprintf("tuntap: select. which: %d\n", which);

	switch (which) {
		case FREAD:
			/* check wether data is available */
			{
				if (!send_queue.empty())
					ret = 1;
				else {
					dprintf("tuntap: select: waiting\n");
					selrecord(p, &rsel, wql);
				}
			}
			break;
		case FWRITE:
			/* we are always writeable */
			ret = 1;
	}

	return ret;
}

/* interface service methods */
errno_t
tuntap_interface::if_output(mbuf_t m)
{
	mbuf_t pkt;

	dprintf("tuntap: if output\n");

	/* just to be sure */
	if (m == NULL)
		return 0;

	if (!open || ifp == NULL || !(ifnet_flags(ifp) & IFF_UP)) {
		mbuf_freem_list(m);
		return EHOSTDOWN;
	}

	/* check whether packet has a header */
	if ((mbuf_flags(m) & MBUF_PKTHDR) == 0) {
		log(LOG_ERR, "tuntap: packet to be output has no mbuf header.\n");
		mbuf_freem_list(m);
		return EINVAL;
	}

	/* put the packet(s) into the output queue */
	while (m != NULL) {
		/* keep pointer, iterate */
		pkt = m;
		m = mbuf_nextpkt(m);
		mbuf_setnextpkt(pkt, NULL);

		auto_lock l(&lock);

		if (!send_queue.enqueue(pkt)) {
			mbuf_freem(pkt);
			mbuf_freem_list(m);
			return ENOBUFS;
		}
	}

	/* protect the wakeup calls with the lock, not sure they are safe. */
	{
		auto_lock l(&lock);

		/* wakeup the cdev thread and notify selects */
		wakeup(this);
		selwakeup(&rsel);
	}

	return 0;
}

errno_t
tuntap_interface::if_ioctl(u_int32_t cmd, void *arg)
{
	dprintf("tuntap: if ioctl: %d\n", (int) (cmd & 0xff));

	switch (cmd) {
		case SIOCSIFADDR:
			{
				dprintf("tuntap: if_ioctl: SIOCSIFADDR\n");

				/* Unfortunately, ifconfig sets the address family field of an INET
				 * netmask to zero, which makes early mDNSresponder versions ignore
				 * the interface. Fix that here. This one is of the category "ugly
				 * workaround". Dumb Darwin...
				 *
				 * Meanwhile, Apple has fixed mDNSResponder, and recent versions of
				 * Leopard don't need this hack anymore. However, Tiger still has a
				 * broken version so we leave the hack in for now.
				 *
				 * TODO: Revisit when dropping Tiger support.
				 *
				 * Btw. If you configure other network interfaces using ifconfig,
				 * you run into the same problem. I still don't know how to make the
				 * tap devices show up in the network configuration panel...
				 */
				ifaddr_t ifa = (ifaddr_t) arg;
				if (ifa == NULL)
					return 0;

				sa_family_t af = ifaddr_address_family(ifa);
				if (af != AF_INET)
					return 0;

				struct ifaliasreq ifra;
				int sa_size = sizeof(struct sockaddr);
				if (ifaddr_address(ifa, &ifra.ifra_addr, sa_size)
					|| ifaddr_dstaddress(ifa, &ifra.ifra_broadaddr, sa_size)
					|| ifaddr_netmask(ifa, &ifra.ifra_mask, sa_size)) {
					log(LOG_WARNING,
						"tuntap: failed to parse interface address.\n");
					return 0;
				}

				// Check that the address family fields match. If not, issue another
				// SIOCAIFADDR to fix the entry.
				if (ifra.ifra_addr.sa_family != af
					|| ifra.ifra_broadaddr.sa_family != af
					|| ifra.ifra_mask.sa_family != af) {
					log(LOG_INFO, "tuntap: Fixing address family for %s%d\n",
						family_name, unit);

					snprintf(ifra.ifra_name, sizeof(ifra.ifra_name), "%s%d",
						family_name, unit);
					ifra.ifra_addr.sa_family = af;
					ifra.ifra_broadaddr.sa_family = af;
					ifra.ifra_mask.sa_family = af;

					do_sock_ioctl(af, SIOCAIFADDR, &ifra);
				}

				return 0;
			}

		case SIOCSIFFLAGS:
			return 0;

		case SIOCGIFSTATUS:
			{
				struct ifstat *stat = (struct ifstat *) arg;
				int len;
				char *p;

				if (stat == NULL)
					return EINVAL;

				/* print status */
				len = strlen(stat->ascii);
				p = stat->ascii + len;
				if (open) {
					snprintf(p, IFSTATMAX - len, "\topen (pid %u)\n", pid);
				} else {
					snprintf(p, IFSTATMAX - len, "\tclosed\n");
				}

				return 0;
			}

		case SIOCSIFMTU:
			{
				struct ifreq *ifr = (struct ifreq *) arg;

				if (ifr == NULL)
					return EINVAL;

				ifnet_set_mtu(ifp, ifr->ifr_mtu);

				return 0;
			}

		case SIOCDIFADDR:
			return 0;

	}

	return EOPNOTSUPP;
}

errno_t
tuntap_interface::if_set_bpf_tap(bpf_tap_mode mode, int (*cb)(ifnet_t, mbuf_t))
{
	dprintf("tuntap: mode %d\n", mode);

	auto_lock l(&bpf_lock);

	bpf_callback = cb;
	bpf_mode = mode;

	return 0;
}

errno_t
tuntap_interface::if_check_multi(const struct sockaddr *maddr)
{
	dprintf("tuntap: if_check_multi\n");

	return EOPNOTSUPP;
}

void
tuntap_interface::if_detached()
{
	dprintf("tuntap: if_detached\n");

	/* wake unregister_interface() */
	detach_lock.lock();
	interface_detached = true;
	detach_lock.wakeup(&interface_detached);
	detach_lock.unlock();

	dprintf("if_detached done\n");
}

