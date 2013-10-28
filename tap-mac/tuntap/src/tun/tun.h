/*
 * ip tunnel device for MacOSX.
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

#ifndef __TUN_H__
#define __TUN_H__

#include "tuntap.h"

#define TUN_FAMILY_NAME			((char *) "tun")
#define TUN_IF_COUNT			16	/* max number of tun interfaces */
#define TUN_MTU				1500

#include "tun_ioctls.h"

extern "C" {

errno_t tun_inet_attach(ifnet_t ifp, protocol_family_t proto);
void tun_inet_detach(ifnet_t ifp, protocol_family_t proto);
errno_t tun_inet6_attach(ifnet_t ifp, protocol_family_t proto);
void tun_inet6_detach(ifnet_t ifp, protocol_family_t proto);

}

/* tun_manager */
class tun_manager : public tuntap_manager {

	protected:
		/* create an interface */
		virtual tuntap_interface *create_interface();

		/* whether we need to call tuntap_manager::shutdown() */
		bool tuntap_inited;

	public:
		/* special initalize */
		virtual bool initialize(unsigned int count, char *family);

		/* special shutdown */
		virtual bool shutdown();

};

/* the tun network interface */
class tun_interface : public tuntap_interface {

	protected:
		/* maximum number of protocols that can be attached */
		static const unsigned int MAX_ATTACHED_PROTOS = 8;

		/* information about attached protocols for demuxing is stored here */
		struct {
			/* whether this entry is used */
			bool used;
			/* protocol family (this is equal to proto, but keep it seperated from
			 * Apple's KPI stuff...) */
			u_int32_t family;
			/* protocol passed to add_proto */
			protocol_family_t proto;
		} attached_protos[MAX_ATTACHED_PROTOS];

		/* whether the address family field is prepended to each packet */
		bool prepend_af;

		/* intializes the interface */
		virtual bool initialize(unsigned short major, unsigned short int unit);

		/* shutdown the interface */
		virtual void shutdown();

		/* called when the character device is opened in order to intialize the network
		 * interface.
		 */
		virtual int initialize_interface();
		/* called when the character device is closed to shutdown the network interface */
		virtual void shutdown_interface();

		/* override interface routines */
		virtual errno_t if_demux(mbuf_t m, char *header, protocol_family_t *proto);
		virtual errno_t if_framer(mbuf_t *m, const struct sockaddr *dest,
				const char *dest_linkaddr, const char *frame_type);
		virtual errno_t if_add_proto(protocol_family_t proto,
				const struct ifnet_demux_desc *desc, u_int32_t ndesc);
		virtual errno_t if_del_proto(protocol_family_t proto);
		virtual errno_t if_check_multi(const struct sockaddr *maddr);

		/* helper to if_add_proto */
		virtual errno_t add_one_proto(protocol_family_t proto,
				const struct ifnet_demux_desc &dd);

		/* override notify_bpf because we might need to prepend an address header */
		virtual void notify_bpf(mbuf_t mb, bool out);

		/* need to override cdev_ioctl to get our special ioctls */
		virtual int cdev_ioctl(u_long cmd, caddr_t data, int fflag, proc_t p);

};

#endif /* __TUN_H__ */

