/*
 * ip tunnel device for MacOSX.
 *
 * tun_interface class definition
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

#include "tun.h"

extern "C" {

#include <sys/syslog.h>
#include <sys/param.h>

#include <net/if_types.h>
#include <net/kpi_protocol.h>

#include <netinet/ip.h>

}

#if 0
#define dprintf(...)			log(LOG_INFO, __VA_ARGS__)
#else
#define dprintf(...)
#endif

/* members */
bool
tun_interface::initialize(unsigned short major, unsigned short unit)
{
	this->unit = unit;
	this->family_name = TUN_FAMILY_NAME;
	this->family = IFNET_FAMILY_TUN;
	this->type = IFT_OTHER;
	bzero(unique_id, UIDLEN);
	snprintf(unique_id, UIDLEN, "%s%d", family_name, unit);

	dprintf("tun: starting interface %s%d\n", family_name, unit);

	/* register character device */
	if (!tuntap_interface::register_chardev(major))
		return false;

	return true;
}

void
tun_interface::shutdown()
{
	dprintf("tun: shutting down interface %s%d\n", family_name, unit);

	unregister_chardev();
}

int
tun_interface::initialize_interface()
{
	prepend_af = false;

	/* register interface */
	if (!tuntap_interface::register_interface(NULL, NULL, 0))
		return EIO;
		
	/* set mtu */
	ifnet_set_mtu(ifp, TUN_MTU);
	/* set header length */
	ifnet_set_hdrlen(ifp, 0);
	/* add the pointopoint flag */
	ifnet_set_flags(ifp, IFF_POINTOPOINT, IFF_POINTOPOINT);

	/* we must call bpfattach(). Otherwise we deadlock BPF while unloading. Seems to be a bug in
	 * the kernel, see bpfdetach() in net/bpf.c, it will return without releasing the lock if
	 * the interface wasn't attached. I wonder what they were smoking while writing it ;-)
	 */
	bpfattach(ifp, DLT_NULL, sizeof(u_int32_t));

	return 0;
}

void
tun_interface::shutdown_interface()
{
	dprintf("tun: shutting down network interface of %s%d\n", family_name, unit);

	/* detach all protocols */
	for (unsigned int i = 0; i < MAX_ATTACHED_PROTOS; i++) {
		if (attached_protos[i].used) {
			errno_t err = ifnet_detach_protocol(ifp, attached_protos[i].proto);
			if (err)
				log(LOG_WARNING, "tun: could not detach protocol %d from %s%d\n",
						attached_protos[i].proto, family_name, unit);
		}
	}

	cleanup_interface();
	unregister_interface();
}

void
tun_interface::notify_bpf(mbuf_t mb, bool out)
{
	auto_lock l(&bpf_lock);

	if ((out && bpf_mode == BPF_MODE_OUTPUT)
			|| (!out && bpf_mode == BPF_MODE_INPUT)
			|| (bpf_mode == BPF_MODE_INPUT_OUTPUT)) {
		/* see wether AF is prepended */
		if (!prepend_af) {
			mbuf_t dummy_mb;
			struct ip *iphdr;
			u_int32_t af;
			u_int8_t ipv;
			errno_t err;

			/* see what we have: IPv4 or IPv6 */
			iphdr = (struct ip*) mbuf_data(mb);
#ifdef _IP_VHL
			ipv = IP_VHL_V(iphdr->ip_vhl);
#else
			ipv = iphdr->ip_v;
#endif
			if (ipv == 4)
				af = AF_INET;
			else if (ipv == 6)
				af = AF_INET6;
			else {
				/* what to do? */
				log(LOG_WARNING, "tun: unsupported IP version %d.\n", ipv);
				return;
			}

			/* prepend a dummy header */
			err = mbuf_get(MBUF_WAITOK, MBUF_TYPE_DATA, &dummy_mb);
			if (err) {
				log(LOG_WARNING, "tun: could not allocate temporary mbuf: %d\n",
						err);
				return;
			}

			mbuf_setnext(dummy_mb, mb);
			mbuf_setlen(dummy_mb, sizeof(u_int32_t));
			*((u_int32_t *) mbuf_data(dummy_mb)) = htonl(af);

			/* call bpf */
			(*bpf_callback)(ifp, dummy_mb);

			/* free the dummy mbuf */
			mbuf_free(dummy_mb);
		} else {
			/* just pass it through */
			(*bpf_callback)(ifp, mb);
		}
	}
}

int
tun_interface::cdev_ioctl(u_long cmd, caddr_t data, int fflag, proc_t p)
{
	int error;

	/* if the superclass handles it, we're done */
	error = tuntap_interface::cdev_ioctl(cmd, data, fflag, p);
	if (!error)
		return 0;

	switch (cmd) {
		case TUNSIFHEAD:
			prepend_af = *((int *) data) ? true : false;
			/* adjust header length. see tuntap_interface::cdev_write */
			ifnet_set_hdrlen(ifp, prepend_af ? sizeof(u_int32_t) : 0);
			return 0;
		case TUNGIFHEAD:
			*((int *) data) = prepend_af;
			return 0;
	}

	return ENOTTY;
}

errno_t
tun_interface::if_demux(mbuf_t m, char *header, protocol_family_t *proto)
{
	u_int32_t family;

	dprintf("tun: demux\n");

	/* size check */
	if (mbuf_len(m) < sizeof(u_int32_t))
		return ENOENT;

	/* if we are prepending AF for output, we expect to also have it at the beginning of
	 * incoming packets */
	if (!prepend_af) {
		struct ip *iphdr = (struct ip*) mbuf_data(m);
		u_int8_t ipv;

		dprintf("tun_demux: m: %p data: %p\n", m, mbuf_data(m));

#ifdef _IPVHL
		ipv = IP_VHL_V(iphdr->ip_vhl);
#else
		ipv = iphdr->ip_v;
#endif

		if (ipv == 4)
			family = AF_INET;
		else if (ipv == 6)
			family = AF_INET6;
		else {
			/* what to do? */
			log(LOG_WARNING, "tun: unsupported IP version %d\n", ipv);
			return ENOENT;
		}
	} else {
		family = ntohl(*((u_int32_t *) header));
	}

	/* find the protocol entry */
	for (unsigned int i = 0; i < MAX_ATTACHED_PROTOS; i++) {
		if (attached_protos[i].used && attached_protos[i].family == family) {
			*proto = attached_protos[i].proto;
			return 0;
		}
	}

	log(LOG_WARNING, "tun: no protocol found for family %d\n", family);

	return ENOENT;
}

errno_t
tun_interface::if_framer(mbuf_t *m, const struct sockaddr *dest, const char *dest_linkaddr,
		const char *frame_type)
{
	dprintf("tun: framer\n");

	/* check whether to prepend family field */
	if (prepend_af) {
		errno_t err;
		mbuf_t nm = *m;

		/* get space */
		err = mbuf_prepend(&nm, sizeof(u_int32_t), MBUF_WAITOK);
		if (err) {
			dprintf("tun: could not prepend data to mbuf: %d\n", err);
			return err;
		}
		*m = nm;

		*((u_int32_t *) mbuf_data(*m)) = htonl(dest->sa_family);
	}

	return 0;
}

errno_t
tun_interface::if_add_proto(protocol_family_t proto, const struct ifnet_demux_desc *desc,
		u_int32_t ndesc)
{
	errno_t err;

	dprintf("tun: if_add_proto proto %d\n", proto);

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
tun_interface::if_del_proto(protocol_family_t proto)
{
	dprintf("tun: if_del_proto proto %d\n", proto);

	/* delete all matching entries in attached_protos */
	for (unsigned int i = 0; i < MAX_ATTACHED_PROTOS; i++) {
		if (attached_protos[i].proto == proto)
			attached_protos[i].used = false;
	}

	return 0;
}

errno_t
tun_interface::if_check_multi(const struct sockaddr *maddr)
{
	dprintf("tun: check_multi family %d\n", maddr->sa_family);

	/* see whether it is an IPv4 multicast address */
	if (maddr->sa_family == AF_INET) {
		struct sockaddr_in *imaddr = (struct sockaddr_in *) maddr;

		if (IN_MULTICAST(ntohl(imaddr->sin_addr.s_addr)))
			return 0;
		else
			return EADDRNOTAVAIL;
	} else if (maddr->sa_family == AF_INET6) {
		struct sockaddr_in6 *imaddr = (struct sockaddr_in6 *) maddr;

		if (IN6_IS_ADDR_MULTICAST(&imaddr->sin6_addr))
			return 0;
		else
			return EADDRNOTAVAIL;
	}

	return EOPNOTSUPP;
}

errno_t
tun_interface::add_one_proto(protocol_family_t proto, const struct ifnet_demux_desc &dd)
{
	int free = -1;

	/* see if the protocol is already registered */
	for (unsigned int i = 0; i < MAX_ATTACHED_PROTOS; i++) {
		if (attached_protos[i].used) {
			if (dd.type == attached_protos[i].family) {
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

	dprintf("tun: adding proto family %d proto %d\n", dd.type, proto);

	/* ok, save information */
	attached_protos[free].used = true;
	attached_protos[free].family = dd.type;
	attached_protos[free].proto = proto;

	return 0;
}

/* tun_manager members */
tuntap_interface *
tun_manager::create_interface()
{
	return new tun_interface();
}

bool
tun_manager::shutdown()
{
	if (tuntap_inited) {
		if (tuntap_manager::shutdown())
			tuntap_inited = false;
		else
			return false;
	}

	/* unregister INET and INET6 protocol families */
	proto_unregister_plumber(PF_INET, IFNET_FAMILY_TUN);
	proto_unregister_plumber(PF_INET6, IFNET_FAMILY_TUN);

	return true;
}

bool
tun_manager::initialize(unsigned int count, char *family)
{
	errno_t err;

	tuntap_inited = false;

	/* register INET and INET6 protocol families */
	err = proto_register_plumber(PF_INET, IFNET_FAMILY_TUN, tun_inet_attach, tun_inet_detach);
	if (err) {
		log(LOG_ERR, "tun: could not register PF_INET protocol family: %d\n", err);
		return false;
	}

	err = proto_register_plumber(PF_INET6, IFNET_FAMILY_TUN, tun_inet6_attach,
			tun_inet6_detach);
	if (err) {
		log(LOG_ERR, "tun: could not register PF_INET6 protocol family: %d\n", err);
		return false;
	}

	tuntap_inited = true;

	/* have the superclass handle the rest */
	return tuntap_manager::initialize(count, family);
}

