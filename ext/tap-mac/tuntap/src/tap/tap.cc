/*
 * ethertap device for macosx.
 *
 * tap_interface class definition
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

#include "tap.h"

extern "C" {

#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/random.h>
#include <sys/kern_event.h>

#include <mach/thread_policy.h>

#include <net/if_types.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/dlil.h>
#include <net/ethernet.h>

}

#if 0
#define dprintf(...)			log(LOG_INFO, __VA_ARGS__)
#else
#define dprintf(...)
#endif

// These declarations are missing in the Kernel.framework headers, put present in userspace :-/
#pragma pack(4)
struct ifmediareq {
	char	ifm_name[IFNAMSIZ];	/* if name, e.g. "en0" */
	int	ifm_current;		/* current media options */
	int	ifm_mask;		/* don't care mask */
	int	ifm_status;		/* media status */
	int	ifm_active;		/* active options */
	int	ifm_count;		/* # entries in ifm_ulist array */
	int	*ifm_ulist;		/* media words */
};

struct ifmediareq64 {
	char	ifm_name[IFNAMSIZ];	/* if name, e.g. "en0" */
	int	ifm_current;		/* current media options */
	int	ifm_mask;		/* don't care mask */
	int	ifm_status;		/* media status */
	int	ifm_active;		/* active options */
	int	ifm_count;		/* # entries in ifm_ulist array */
	user64_addr_t ifmu_ulist __attribute__((aligned(8)));
};

struct ifmediareq32 {
	char	ifm_name[IFNAMSIZ];	/* if name, e.g. "en0" */
	int	ifm_current;		/* current media options */
	int	ifm_mask;		/* don't care mask */
	int	ifm_status;		/* media status */
	int	ifm_active;		/* active options */
	int	ifm_count;		/* # entries in ifm_ulist array */
	user32_addr_t ifmu_ulist;	/* 32-bit pointer */
};
#pragma pack()

#define	SIOCGIFMEDIA32	_IOWR('i', 56, struct ifmediareq32) /* get net media */
#define	SIOCGIFMEDIA64	_IOWR('i', 56, struct ifmediareq64) /* get net media (64-bit) */

/* thread_policy_set is exported in Mach.kext, but commented in mach/thread_policy.h in the
 * Kernel.Framework headers (why?). Add a local declaration to work around that.
 */
extern "C" {
kern_return_t thread_policy_set(
	thread_t thread,
	thread_policy_flavor_t flavor,
	thread_policy_t policy_info,
	mach_msg_type_number_t count);
}

static unsigned char ETHER_BROADCAST_ADDR[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* members */
tap_interface::tap_interface() {
	bzero(attached_protos, sizeof(attached_protos));
	input_thread = THREAD_NULL;
}

bool
tap_interface::initialize(unsigned short major, unsigned short unit)
{
	this->unit = unit;
	this->family_name = TAP_FAMILY_NAME;
	this->family = IFNET_FAMILY_ETHERNET;
	this->type = IFT_ETHER;
	bzero(unique_id, UIDLEN);
	snprintf(unique_id, UIDLEN, "%s%d", family_name, unit);

	dprintf("tap: starting interface %s%d\n", TAP_FAMILY_NAME, unit);

	/* register character device */
	if (!tuntap_interface::register_chardev(major))
		return false;

	return true;
}

void
tap_interface::shutdown()
{
	dprintf("tap: shutting down tap interface %s%d\n", TAP_FAMILY_NAME, unit);

	unregister_chardev();
}

int
tap_interface::initialize_interface()
{
	struct sockaddr_dl lladdr;
	lladdr.sdl_len = sizeof(lladdr);
	lladdr.sdl_family = AF_LINK;
	lladdr.sdl_alen = ETHER_ADDR_LEN;
	lladdr.sdl_nlen = lladdr.sdl_slen = 0;

	/* generate a random MAC address */
	read_random(LLADDR(&lladdr), ETHER_ADDR_LEN);

	/* clear multicast bit and set local assignment bit (see IEEE 802) */
	(LLADDR(&lladdr))[0] &= 0xfe;
	(LLADDR(&lladdr))[0] |= 0x02;

	dprintf("tap: random tap address: %02x:%02x:%02x:%02x:%02x:%02x\n",
			(LLADDR(&lladdr))[0] & 0xff,
			(LLADDR(&lladdr))[1] & 0xff,
			(LLADDR(&lladdr))[2] & 0xff,
			(LLADDR(&lladdr))[3] & 0xff,
			(LLADDR(&lladdr))[4] & 0xff,
			(LLADDR(&lladdr))[5] & 0xff);

	/* register interface */
	if (!tuntap_interface::register_interface(&lladdr, ETHER_BROADCAST_ADDR, ETHER_ADDR_LEN))
		return EIO;

	/* Set link level address. Yes, we need to do that again. Darwin sucks. */
	errno_t err = ifnet_set_lladdr(ifp, LLADDR(&lladdr), ETHER_ADDR_LEN);
	if (err)
		dprintf("tap: failed to set lladdr on %s%d: %d\n", family_name, unit, err);

	/* set mtu */
	ifnet_set_mtu(ifp, TAP_MTU);
	/* set header length */
	ifnet_set_hdrlen(ifp, sizeof(struct ether_header));
	/* add the broadcast flag */
	ifnet_set_flags(ifp, IFF_BROADCAST, IFF_BROADCAST);

	/* we must call bpfattach(). Otherwise we deadlock BPF while unloading. Seems to be a bug in
	 * the kernel, see bpfdetach() in net/bpf.c, it will return without releasing the lock if
	 * the interface wasn't attached. I wonder what they were smoking while writing it ;-)
	 */
	bpfattach(ifp, DLT_EN10MB, ifnet_hdrlen(ifp));

	/* Inject an empty packet to trigger the input thread calling demux(), which will unblock
	 * thread_sync_lock. This is part of a hack to avoid a kernel crash on re-attaching
	 * interfaces, see comment in shutdown_interface for more information.
	 */
	mbuf_t empty_mbuf;
	mbuf_gethdr(MBUF_WAITOK, MBUF_TYPE_DATA, &empty_mbuf);
	if (empty_mbuf != NULL) {
		mbuf_pkthdr_setrcvif(empty_mbuf, ifp);
		mbuf_pkthdr_setlen(empty_mbuf, 0);
		mbuf_pkthdr_setheader(empty_mbuf, mbuf_data(empty_mbuf));
		mbuf_set_csum_performed(empty_mbuf, 0, 0);
		if (ifnet_input(ifp, empty_mbuf, NULL) == 0) {
			auto_lock l(&thread_sync_lock);
			for (int i = 0; i < 100 && input_thread == THREAD_NULL; ++i) {
				dprintf("input thread not found, waiting...\n");
				thread_sync_lock.sleep(&input_thread, 10000000);
			}
		} else {
			mbuf_freem(empty_mbuf);
		}
	}
	if (input_thread == THREAD_NULL)
		dprintf("Failed to determine input thread!\n");

	return 0;
}

void
tap_interface::shutdown_interface()
{
	dprintf("tap: shutting down network interface of device %s%d\n", TAP_FAMILY_NAME, unit);

	/* detach all protocols */
	for (unsigned int i = 0; i < MAX_ATTACHED_PROTOS; i++) {
		if (attached_protos[i].used) {
			errno_t err = ifnet_detach_protocol(ifp, attached_protos[i].proto);
			if (err)
				log(LOG_WARNING, "tap: could not detach protocol %d from %s%d\n",
						attached_protos[i].proto, TAP_FAMILY_NAME, unit);
		}
	}

	cleanup_interface();
	unregister_interface();

	/* There's a race condition in the kernel that may cause crashes when quickly re-attaching
	 * interfaces. The crash happens when the interface gets re-attached before the input thread
	 * for the interface managed to terminate, in which case an assert on the input_waiting flag
	 * to be clear triggers in ifnet_attach. The bug is really that there's no synchronization
	 * for terminating the input thread. To work around this, the following code does add the
	 * missing synchronization to wait for the input thread to terminate. Of course, threading
	 * primitives available to kexts are few, and I'm not aware of a way to wait for a thread to
	 * terminate. Hence, the code calls thread_policy_set (passing bogus parameters) in a loop,
	 * until it returns KERN_TERMINATED. Since this is all rather fragile, there's an upper
	 * limit on the loop iteratations we're willing to make, so this terminates eventually even
	 * if things change on the kernel side eventually.
	 */
	if (input_thread != THREAD_NULL) {
		dprintf("Waiting for input thread...\n");
		kern_return_t result = 0;
		for (int i = 0; i < 100; ++i) {
			result = thread_policy_set(input_thread, -1, NULL, 0);
			dprintf("thread_policy_set result: %d\n", result);
			if (result == KERN_TERMINATED) {
				dprintf("Input thread terminated.\n");
				thread_deallocate(input_thread);
				input_thread = THREAD_NULL;
				break;
			}

			auto_lock l(&thread_sync_lock);
			thread_sync_lock.sleep(&input_thread, 10000000);
		}
	}
}

errno_t
tap_interface::if_ioctl(u_int32_t cmd, void *arg)
{
	dprintf("tap: if_ioctl cmd: %d (%x)\n", cmd & 0xff, cmd);

	switch (cmd) {
		case SIOCSIFLLADDR:
			{
				/* set ethernet address */
				struct sockaddr *ea = &(((struct ifreq *) arg)->ifr_addr);

				dprintf("tap: SIOCSIFLLADDR family %d len %d\n",
						ea->sa_family, ea->sa_len);

				/* check if it is really an ethernet address */
				if (ea->sa_family != AF_LINK || ea->sa_len != ETHER_ADDR_LEN)
					return EINVAL;

				/* ok, copy */
				errno_t err = ifnet_set_lladdr(ifp, ea->sa_data, ETHER_ADDR_LEN);
				if (err) {
					dprintf("tap: failed to set lladdr on %s%d: %d\n",
							family_name, unit, err);
					return err;
				}

				/* Generate a LINK_ON event. This necessary for configd to re-read
				 * the interface data and refresh the MAC address. Not doing so
				 * would result in the DHCP client using a stale MAC address...
				 */
				generate_link_event(KEV_DL_LINK_ON);

				return 0;
			}

		case SIOCGIFMEDIA32:
		case SIOCGIFMEDIA64:
			{
				struct ifmediareq *ifmr = (struct ifmediareq*) arg;
				user_addr_t list = USER_ADDR_NULL;

				ifmr->ifm_current = IFM_ETHER;
				ifmr->ifm_mask = 0;
				ifmr->ifm_status = IFM_AVALID | IFM_ACTIVE;
				ifmr->ifm_active = IFM_ETHER;
				ifmr->ifm_count = 1;

				if (cmd == SIOCGIFMEDIA64)
					list = ((struct ifmediareq64*) ifmr)->ifmu_ulist;
				else
					list = CAST_USER_ADDR_T(
						((struct ifmediareq32*) ifmr)->ifmu_ulist);
				
				if (list != USER_ADDR_NULL)
					return copyout(&ifmr->ifm_current, list, sizeof(int));
				
				return 0;
			}

		default:
			/* let our superclass handle it */
			return tuntap_interface::if_ioctl(cmd, arg);
	}
			
	return EOPNOTSUPP;
}

errno_t
tap_interface::if_demux(mbuf_t m, char *header, protocol_family_t *proto)
{
	struct ether_header *eh = (struct ether_header *) header;
	unsigned char lladdr[ETHER_ADDR_LEN];

	dprintf("tap: if_demux\n");

	/* Make note of what input thread this interface is running on. This is part of a hack to
	 * avoid a crash on re-attaching interfaces, see comment in shutdown_interface for details.
	 */
	if (input_thread == THREAD_NULL) {
		auto_lock l(&thread_sync_lock);
		input_thread = current_thread();
		thread_reference(input_thread);
		thread_sync_lock.wakeup(&input_thread);
	}

	/* size check */
	if (mbuf_len(m) < sizeof(struct ether_header))
		return ENOENT;

	/* catch broadcast and multicast (stolen from bsd/net/ether_if_module.c) */
	if (eh->ether_dhost[0] & 1) {
		if (memcmp(ETHER_BROADCAST_ADDR, eh->ether_dhost, ETHER_ADDR_LEN) == 0) {
			/* broadcast */
			dprintf("tap: broadcast packet.\n");
			mbuf_setflags_mask(m, MBUF_BCAST, MBUF_BCAST);
		} else {
			/* multicast */
			dprintf("tap: multicast packet.\n");
			mbuf_setflags_mask(m, MBUF_MCAST, MBUF_MCAST);
		}
	} else {
		/* check wether the packet has our address */
		ifnet_lladdr_copy_bytes(ifp, lladdr, ETHER_ADDR_LEN);
		if (memcmp(lladdr, eh->ether_dhost, ETHER_ADDR_LEN) != 0)
			mbuf_setflags_mask(m, MBUF_PROMISC, MBUF_PROMISC);
	}

	/* find the protocol */
	for (unsigned int i = 0; i < MAX_ATTACHED_PROTOS; i++) {
		if (attached_protos[i].used && attached_protos[i].type == eh->ether_type) {
			*proto = attached_protos[i].proto;
			return 0;
		}
	}

	dprintf("tap: if_demux() failed to find proto.\n");

	/* no matching proto found */
	return ENOENT;
}

errno_t
tap_interface::if_framer(mbuf_t *m, const struct sockaddr *dest, const char *dest_linkaddr,
		const char *frame_type)
{
	struct ether_header *eh;
	mbuf_t nm = *m;
	errno_t err;

	dprintf("tap: if_framer\n");

	/* prepend the ethernet header */
	err = mbuf_prepend(&nm, sizeof (struct ether_header), MBUF_WAITOK);
	if (err) {
		dprintf("tap: could not prepend data to mbuf: %d\n", err);
		return err;
	}
	*m = nm;

	/* fill the header */
	eh = (struct ether_header *) mbuf_data(*m);
	memcpy(eh->ether_dhost, dest_linkaddr, ETHER_ADDR_LEN);
	ifnet_lladdr_copy_bytes(ifp, eh->ether_shost, ETHER_ADDR_LEN);
	eh->ether_type = *((u_int16_t *) frame_type);

	return 0;
}

errno_t
tap_interface::if_add_proto(protocol_family_t proto, const struct ifnet_demux_desc *desc,
		u_int32_t ndesc)
{
	errno_t err;

	dprintf("tap: if_add_proto proto %d\n", proto);

	for (unsigned int i = 0; i < ndesc; i++) {
		/* try to add the protocol */
		err = add_one_proto(proto, desc[i]);
		if (err != 0) {
			/* if that fails, remove everything stored so far */
			if_del_proto(proto);
			return err;
		}
	}

	return 0;
}

errno_t
tap_interface::if_del_proto(protocol_family_t proto)
{
	dprintf("tap: if_del_proto proto %d\n", proto);

	/* delete all matching entries in attached_protos */
	for (unsigned int i = 0; i < MAX_ATTACHED_PROTOS; i++) {
		if (attached_protos[i].proto == proto)
			attached_protos[i].used = false;
	}

	return 0;
}

errno_t
tap_interface::if_check_multi(const struct sockaddr *maddr)
{
	dprintf("tap: if_check_multi family %d\n", maddr->sa_family);

	/* see whether it is a ethernet address with the multicast bit set */
	if (maddr->sa_family == AF_LINK) {
		struct sockaddr_dl *dlmaddr = (struct sockaddr_dl *) maddr;
		if (LLADDR(dlmaddr)[0] & 0x01)
			return 0;
		else
			return EADDRNOTAVAIL;
	}

	return EOPNOTSUPP;
}

errno_t
tap_interface::add_one_proto(protocol_family_t proto, const struct ifnet_demux_desc &dd)
{
	int free = -1;
	u_int16_t dt;

	/* we only support DLIL_DESC_ETYPE2 */
	if (dd.type != DLIL_DESC_ETYPE2 || dd.datalen != 2) {
		log(LOG_WARNING, "tap: tap only supports DLIL_DESC_ETYPE2 protocols.\n");
		return EINVAL;
	}

	dt = *((u_int16_t *) (dd.data));

	/* see if the protocol is already registered */
	for (unsigned int i = 0; i < MAX_ATTACHED_PROTOS; i++) {
		if (attached_protos[i].used) {
			if (dt == attached_protos[i].type) {
				/* already registered */
				if (attached_protos[i].proto == proto) {
					/* matches the old entry */
					return 0;
				} else
					return EEXIST;
			}
		} else if (free == -1)
			free = i;
	}

	/* did we find a free entry? */
	if (free == -1)
		/* is ENOBUFS correct? */
		return ENOBUFS;

	/* ok, save information */
	attached_protos[free].used = true;
	attached_protos[free].type = dt;
	attached_protos[free].proto = proto;

	return 0;
}

/* This code is shamelessly stolen from if_bond.c */
void
tap_interface::generate_link_event(u_int32_t code)
{
	struct {
		struct kern_event_msg header;
		u_int32_t unit;
		char if_name[IFNAMSIZ];
	} event;

	bzero(&event, sizeof(event));
	event.header.total_size = sizeof(event);
	event.header.vendor_code = KEV_VENDOR_APPLE;
	event.header.kev_class = KEV_NETWORK_CLASS;
	event.header.kev_subclass = KEV_DL_SUBCLASS;
	event.header.event_code = code;
	event.header.event_data[0] = family;
	event.unit = (u_int32_t) unit;
	strncpy(event.if_name, ifnet_name(ifp), IFNAMSIZ);

	ifnet_event(ifp, &event.header);
}

/* tap_manager members */
tuntap_interface *
tap_manager::create_interface()
{
	return new tap_interface();
}

