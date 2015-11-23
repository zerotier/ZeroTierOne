/*
 * ethertap device for MacOSX.
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

#ifndef __TAP_H__
#define __TAP_H__

#include "tuntap.h"

extern "C" {

#include <kern/thread.h>

}

#define TAP_FAMILY_NAME			((char *) "zt")
#define TAP_IF_COUNT			32	/* max number of tap interfaces */
#define TAP_MTU				2800
#define TAP_LLADDR			tap_lladdr

/* the mac address of our interfaces. note that the last byte will be replaced by the unit number */
extern u_char tap_lladdr[];

/* tap manager */
class tap_manager : public tuntap_manager {

	protected:
		/* just define the interface creation method */
		virtual tuntap_interface *create_interface();

};

/* the tap network interface */
class tap_interface : public tuntap_interface {
        public:
	   	tap_interface();

	protected:
		/* maximum number of protocols that can be attached */
		static const unsigned int MAX_ATTACHED_PROTOS = 8;

		/* information about attached protocols for demuxing is stored here */
		struct {
			/* whether this entry is used */
			bool used;
			/* type in the ethernet header */
			u_int16_t type;
			/* protocol passed to add_proto */
			protocol_family_t proto;
		} attached_protos[MAX_ATTACHED_PROTOS];

		/* The input thread for the network interface. */
		thread_t input_thread;

		/* initializes the interface */
		virtual bool initialize(unsigned short major, unsigned short unit);

		/* shuts the interface down */
		virtual void shutdown();

		/* called when the character device is opened in order to intialize the network
		 * interface.
		 */
		virtual int initialize_interface();
		/* called when the character device is closed to shutdown the network interface */
		virtual void shutdown_interface();

		/* override interface routines */
		virtual errno_t if_ioctl(u_int32_t cmd, void *arg);
		virtual errno_t if_demux(mbuf_t m, char *header, protocol_family_t *proto);
		virtual errno_t if_framer(mbuf_t *m, const struct sockaddr *dest,
				const char *dest_linkaddr, const char *frame_type);
		virtual errno_t if_add_proto(protocol_family_t proto,
				const struct ifnet_demux_desc *ddesc, u_int32_t ndesc);
		virtual errno_t if_del_proto(protocol_family_t proto);
		virtual errno_t if_check_multi(const struct sockaddr *maddr);

		/* if_add_proto helper */
		errno_t add_one_proto(protocol_family_t proto, const struct ifnet_demux_desc &dd);

		/* generates a kernel event */
		void generate_link_event(u_int32_t code);

		friend class tap_manager;
};

#endif /* __TAP_H__ */

