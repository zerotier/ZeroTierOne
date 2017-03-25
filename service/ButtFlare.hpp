/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ZT_BUTTFLARE_HPP
#define ZT_BUTTFLARE_HPP

#include "../node/InetAddress.hpp"
#include "../osdep/Phy.hpp"

namespace ZeroTier {

	typedef void PhySocket;
	class ButtFlare;

	struct TcpConnection
	{
		enum {
			TCP_HTTP_INCOMING,
			TCP_HTTP_OUTGOING,
			TCP_TUNNEL_OUTGOING 
		} type;

		InetAddress from;
		InetAddress realhost;
		
		PhySocket *origin_sock;
		PhySocket *destination_sock;
	};

	class ButtFlare
	{
		friend class Phy<ButtFlare *>;

	public:
		ButtFlare(
			void (*handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
			void *arg);

		~ButtFlare();

		// Send incoming data to intended host
		void phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len);

		void phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *localAddr,const struct sockaddr *from,void *data,unsigned long len);
		void phyOnTcpWritable(PhySocket *sock,void **uptr);
		void phyOnFileDescriptorActivity(PhySocket *sock,void **uptr,bool readable,bool writable);

		// Establish outgoing connection to intended host
		void phyOnTcpConnect(PhySocket *sock,void **uptr,bool success);
		// Accept connection 
		void phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from);
		// Handle the closure of a Unix Domain socket
		void phyOnUnixClose(PhySocket *sock,void **uptr);
		void phyOnUnixData(PhySocket *sock,void **uptr,void *data,ssize_t len);
		void phyOnUnixWritable(PhySocket *sock,void **uptr,bool lwip_invoked);
		
		// Handle the closure of a TCP connection
		void phyOnTcpClose(PhySocket *sock,void **uptr);

		void threadMain()
			throw();

	  	void (*_handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int);

	private:
		int http_listen_port = 80;
		int ssl_listen_port = 8899;

		volatile bool _enabled;
		volatile bool _run;	

		Thread _thread;
		Phy<ButtFlare*> _phy;
		PhySocket *_tcpHttpListenSocket, *_tcpSSLListenSocket;
		std::map<PhySocket*, TcpConnection*> cmap;
	};
}

#endif