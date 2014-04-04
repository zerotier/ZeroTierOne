/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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
#include <time.h>
#include <errno.h>
#include <sys/types.h>

#include "Constants.hpp"
#include "UdpSocket.hpp"
#include "SocketManager.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#endif

// Uncomment to intentionally break UDP in order to test TCP fallback
// This is here so I can commit it to the repo and drive myself insane.
//#define ZT_BREAK_UDP

namespace ZeroTier {

UdpSocket::~UdpSocket()
{
#ifdef __WINDOWS__
	::closesocket(_sock);
#else
	::close(_sock);
#endif
}

bool UdpSocket::send(const InetAddress &to,const void *msg,unsigned int msglen)
{
	return sendWithHopLimit(to,msg,msglen,0);
}

bool UdpSocket::sendWithHopLimit(const InetAddress &to,const void *msg,unsigned int msglen,int hopLimit)
{
#ifdef ZT_BREAK_UDP
	return true;
#else
	if (hopLimit <= 0)
		hopLimit = 255;
	if (to.isV6()) {
#ifdef __WINDOWS__
		DWORD hltmp = (DWORD)hopLimit;
		setsockopt(_sock,IPPROTO_IPV6,IPV6_UNICAST_HOPS,(const char *)&hltmp,sizeof(hltmp));
		return ((int)sendto(_sock,(const char *)msg,msglen,0,to.saddr(),to.saddrLen()) == (int)msglen);
#else
		setsockopt(_sock,IPPROTO_IPV6,IPV6_UNICAST_HOPS,&hopLimit,sizeof(hopLimit));
		return ((int)sendto(_sock,msg,msglen,0,to.saddr(),to.saddrLen()) == (int)msglen);
#endif
	} else {
#ifdef __WINDOWS__
		DWORD hltmp = (DWORD)hopLimit;
		setsockopt(_sock,IPPROTO_IP,IP_TTL,(const char *)&hltmp,sizeof(hltmp));
		return ((int)sendto(_sock,(const char *)msg,msglen,0,to.saddr(),to.saddrLen()) == (int)msglen);
#else
		setsockopt(_sock,IPPROTO_IP,IP_TTL,&hopLimit,sizeof(hopLimit));
		return ((int)sendto(_sock,msg,msglen,0,to.saddr(),to.saddrLen()) == (int)msglen);
#endif
	}
#endif
}

bool UdpSocket::notifyAvailableForRead(const SharedPtr<Socket> &self,SocketManager *sm)
{
	Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> buf;
	InetAddress from;
	socklen_t salen = from.saddrSpaceLen();
	int n = (int)recvfrom(_sock,(char *)(buf.data()),ZT_SOCKET_MAX_MESSAGE_LEN,0,from.saddr(),&salen);
	if (n > 0) {
		buf.setSize((unsigned int)n);
#ifndef ZT_BREAK_UDP
		sm->handleReceivedPacket(self,from,buf);
#endif
	}
	return true;
}

bool UdpSocket::notifyAvailableForWrite(const SharedPtr<Socket> &self,SocketManager *sm)
{
	return true;
}

} // namespace ZeroTier
