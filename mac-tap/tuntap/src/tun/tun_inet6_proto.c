/*
 * ip tunnel device for MacOSX. This is the protocol module for PF_INET.
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

#include <sys/kpi_mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/param.h>

#include <net/kpi_protocol.h>
#include <net/kpi_interface.h>

static errno_t
tun_inet6_input(ifnet_t ifp, protocol_family_t protocol, mbuf_t m, char *header)
{
	/* input the packet */
	return proto_input(PF_INET6, m);
}

static errno_t
tun_inet6_pre_output(ifnet_t ifp, protocol_family_t proto, mbuf_t *packet,
		const struct sockaddr *dest, void *route, char *frame_type, char *dst_addr)
{

	/* check wether the destination address is an inet address */
	if (dest->sa_family != AF_INET6)
		return EAFNOSUPPORT;

	/* place the address family as frame type */
	*((uint32_t *) frame_type) = htonl(AF_INET6);

	return 0;
}

errno_t
tun_inet6_attach(ifnet_t ifp, protocol_family_t proto)
{
	struct ifnet_attach_proto_param pr;
	struct ifnet_demux_desc ddesc[1];

	/* fill out pr and attach the protocol */
	ddesc[0].type = AF_INET6;
	ddesc[0].data = NULL;
	ddesc[0].datalen = 0;
	pr.demux_array = ddesc;
	pr.demux_count = 1;
	pr.input = tun_inet6_input;
	pr.pre_output = tun_inet6_pre_output;
	pr.event = NULL;
	pr.ioctl = NULL;
	pr.detached = NULL;
	pr.resolve = NULL;
	pr.send_arp = NULL;

	return ifnet_attach_protocol(ifp, proto, &pr);
}

void
tun_inet6_detach(ifnet_t ifp, protocol_family_t proto)
{
	/* just detach the protocol */
	ifnet_detach_protocol(ifp, proto);
}

