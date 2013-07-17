/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <signal.h>
#endif

#include "UdpSocket.hpp"
#include "RuntimeEnvironment.hpp"
#include "Logger.hpp"
#include "Switch.hpp"

namespace ZeroTier {

UdpSocket::UdpSocket(
	bool localOnly,
	int localPort,
	bool ipv6,
	void (*packetHandler)(UdpSocket *,void *,const InetAddress &,const void *,unsigned int),
	void *arg)
	throw(std::runtime_error) :
	Thread(),
	_packetHandler(packetHandler),
	_arg(arg),
	_localPort(localPort),
	_sock(0),
	_v6(ipv6)
{
	int yes,no;

	if ((localPort <= 0)||(localPort > 0xffff))
		throw std::runtime_error("port is out of range");

	if (ipv6) {
		_sock = socket(AF_INET6,SOCK_DGRAM,0);
		if (_sock <= 0)
			throw std::runtime_error("unable to create IPv6 SOCK_DGRAM socket");

		yes = 1; setsockopt(_sock,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&yes,sizeof(yes));
		no = 0; setsockopt(_sock,SOL_SOCKET,SO_REUSEADDR,(void *)&no,sizeof(no));
#ifdef IP_DONTFRAG
		no = 0; setsockopt(_sock,IPPROTO_IP,IP_DONTFRAG,&no,sizeof(no));
#endif
#ifdef IP_MTU_DISCOVER
		no = 0; setsockopt(_sock,IPPROTO_IP,IP_MTU_DISCOVER,&no,sizeof(no));
#endif
#ifdef IPV6_MTU_DISCOVER
		no = 0; setsockopt(_sock,IPPROTO_IPV6,IPV6_MTU_DISCOVER,&no,sizeof(no));
#endif

		struct sockaddr_in6 sin6;
		memset(&sin6,0,sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_port = htons(localPort);
		if (localOnly)
			memcpy(&(sin6.sin6_addr.s6_addr),InetAddress::LO6.rawIpBytes(),16);
		else memcpy(&(sin6.sin6_addr),&in6addr_any,sizeof(struct in6_addr));
		if (::bind(_sock,(const struct sockaddr *)&sin6,sizeof(sin6))) {
			::close(_sock);
			throw std::runtime_error("unable to bind to port");
		}
	} else {
		_sock = socket(AF_INET,SOCK_DGRAM,0);
		if (_sock <= 0)
			throw std::runtime_error("unable to create IPv4 SOCK_DGRAM socket");

		no = 0; setsockopt(_sock,SOL_SOCKET,SO_REUSEADDR,(void *)&no,sizeof(no));
#ifdef IP_DONTFRAG
		no = 0; setsockopt(_sock,IPPROTO_IP,IP_DONTFRAG,&no,sizeof(no));
#endif
#ifdef IP_MTU_DISCOVER
		no = 0; setsockopt(_sock,IPPROTO_IP,IP_MTU_DISCOVER,&no,sizeof(no));
#endif

		struct sockaddr_in sin;
		memset(&sin,0,sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(localPort);
		if (localOnly)
			memcpy(&(sin.sin_addr.s_addr),InetAddress::LO4.rawIpBytes(),4);
		else sin.sin_addr.s_addr = INADDR_ANY;
		if (::bind(_sock,(const struct sockaddr *)&sin,sizeof(sin))) {
			::close(_sock);
			throw std::runtime_error("unable to bind to port");
		}
	}

	start();
}

UdpSocket::~UdpSocket()
{
	close(_sock);
}

bool UdpSocket::send(const InetAddress &to,const void *data,unsigned int len,int hopLimit)
	throw()
{
	Mutex::Lock _l(_sendLock);
	if (to.isV6()) {
		if (!_v6)
			return false;
		setsockopt(_sock,IPPROTO_IPV6,IPV6_UNICAST_HOPS,&hopLimit,sizeof(hopLimit));
		return ((int)sendto(_sock,data,len,0,to.saddr(),to.saddrLen()) == (int)len);
	} else {
		if (_v6)
			return false;
		setsockopt(_sock,IPPROTO_IP,IP_TTL,&hopLimit,sizeof(hopLimit));
		return ((int)sendto(_sock,data,len,0,to.saddr(),to.saddrLen()) == (int)len);
	}
}

void UdpSocket::main()
	throw()
{
	char buf[32768];
	InetAddress from;
	socklen_t salen;
	int n;

	for(;;) {
		salen = from.saddrSpaceLen();
		n = (int)recvfrom(_sock,buf,sizeof(buf),0,from.saddr(),&salen);
		if (n < 0) {
			if ((errno != EINTR)&&(errno != ETIMEDOUT))
				break;
		} else if (n > 0)
			_packetHandler(this,_arg,from,buf,(unsigned int)n);
	}
}

} // namespace ZeroTier
