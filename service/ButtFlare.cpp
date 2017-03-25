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

#include "../osdep/Thread.hpp"
#include "ButtFlare.hpp"

namespace ZeroTier {

	typedef void PhySocket;

	ButtFlare::ButtFlare(
		void (*handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int), void *arg) 
		:
			_handler(handler),
			_phy(this,false,true),
			_enabled(true),
			_run(true)
	{
		// HTTP listen socket
		struct sockaddr_in in4;
		memset(&in4,0,sizeof(in4));
		in4.sin_family = AF_INET;
		in4.sin_addr.s_addr = Utils::hton((uint32_t)(0x7f000001)); // right now we just listen for TCP @127.0.0.1
		in4.sin_port = Utils::hton((uint16_t)http_listen_port);
		_tcpHttpListenSocket = _phy.tcpListen((const struct sockaddr *)&in4,this);
		// SSL listen socket
		in4.sin_port = Utils::hton((uint16_t)ssl_listen_port);
		_tcpSSLListenSocket = _phy.tcpListen((const struct sockaddr *)&in4,this);

		/*
		struct sockaddr_in6 in6;
		memset((void *)&in6,0,sizeof(in6));
		in6.sin6_family = AF_INET6;
		in6.sin6_port = in4.sin_port;
		if (_allowManagementFrom.size() == 0)
			in6.sin6_addr.s6_addr[15] = 1; // IPv6 localhost == ::1
		_v6TcpControlSocket = _phy.tcpListen((const struct sockaddr *)&in6,this);

		// We must bind one of IPv4 or IPv6 -- support either failing to support hosts that
		// have only IPv4 or only IPv6 stacks.
		if ((_v4TcpControlSocket)||(_v6TcpControlSocket)) {
			_ports[0] = _primaryPort;
			break;
		} else {
			if (_v4TcpControlSocket)
				_phy.close(_v4TcpControlSocket,false);
			if (_v6TcpControlSocket)
				_phy.close(_v6TcpControlSocket,false);
			_primaryPort = 0;
		}
		*/

		if(!_tcpHttpListenSocket)
			printf("Error binding on port %d for HTTP listen socket\n", http_listen_port);
		if(!_tcpSSLListenSocket)
			printf("Error binding on port %d for SSL listen socket\n", ssl_listen_port);
		_thread = Thread::start(this);
	} 

	ButtFlare::~ButtFlare()
	{
		_run = false;
		_phy.whack();
		_phy.whack(); // TODO: Rationale?
		Thread::join(_thread);
		_phy.close(_tcpHttpListenSocket,false);
		_phy.close(_tcpSSLListenSocket,false);
	}
	void ButtFlare::threadMain()
		throw()
	{
		while(_run) {
			_phy.poll(50); // in ms
		}
	}

	void ButtFlare::phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len)
	{
		TcpConnection *conn = cmap[sock];
		unsigned char *buf = (unsigned char*)data;
		std::string host = "";

		for(int i=0; i<len; i++) { printf("buf[%d] = %d, (char) = %c\n", i, buf[i], (char*)buf[i]); }
		printf("phyOnTcpData(len=%lu)\n", len);

		if(conn == NULL) {
			printf("No existing connection for this socket\n");
			return;
		}
		if(!conn->destination_sock) { // no connection yet
			
			// Determine if HTTP or TLS/SSL
			if(buf[0] == 22 && len > 100) // naive and incomplete way of checking for TLS, just for stub code
			{
				printf("TLS/SSL\n");
				host = "127.0.0.1";
			}
			else
			{
				printf("HTTP\n");
				host = "127.0.0.1";
			}

			if(host != "")
			{
				bool connected;
				struct sockaddr_in in4;
				memset(&in4,0,sizeof(in4));
				in4.sin_family = AF_INET;
				in4.sin_addr.s_addr = Utils::hton((uint32_t)(0x7f000001)); // right now we just listen for TCP @127.0.0.1
				in4.sin_port = Utils::hton((uint16_t)http_listen_port);
				conn->destination_sock = _phy.tcpConnect((const struct sockaddr *)&in4, connected, this);

				if(!connected) {
					printf("instant connect has occured\n");
				}
				if(!conn->destination_sock) {
					printf("there was an error connecting to the remote host\n");
				}
			}
		}
		else // connection already established, just forward the data
		{
			int n = _phy.streamSend(conn->destination_sock, buf, len);
			printf("wrote %d bytes (%p -> %p)\n", conn->origin_sock, conn->destination_sock);
		}
	}

	void ButtFlare::phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *localAddr,const struct sockaddr *from,void *data,unsigned long len)
	{
	}
	void ButtFlare::phyOnTcpWritable(PhySocket *sock,void **uptr)
	{
	}
	void ButtFlare::phyOnFileDescriptorActivity(PhySocket *sock,void **uptr,bool readable,bool writable)
	{
	}

	void ButtFlare::phyOnTcpConnect(PhySocket *sock,void **uptr,bool success)
	{
		printf("phyOnTcpConnect()\n");
	}

	void ButtFlare::phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from)
	{
		TcpConnection *conn = new TcpConnection();
		conn->origin_sock = sockN;
		cmap[sockN]=conn; // add new connection
	}

	void ButtFlare::phyOnUnixClose(PhySocket *sock,void **uptr) 
	{
	}
	void ButtFlare::phyOnUnixData(PhySocket *sock,void **uptr,void *data,ssize_t len)
	{
	}
	void ButtFlare::phyOnUnixWritable(PhySocket *sock,void **uptr,bool lwip_invoked)
	{
	}


	void ButtFlare::phyOnTcpClose(PhySocket *sock,void **uptr) 
	{
		TcpConnection *conn = cmap[sock];
		if(conn == NULL) {
			printf("No existing connection for this sock. Only closing origin sock\n");
			// _phy.close(sock);
			return;
		}
		else
		{
			// Close both ends
			// _phy.close(conn->destination_sock);
			// _phy.close(conn->origin_sock);
		}
	}
}

//#endif