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
#include <sys/types.h>

#include "SocketManager.hpp"
#include "UdpSocket.hpp"
#include "TcpSocket.hpp"

#ifndef __WINDOWS__
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#endif

// Allow us to use the same value on Windows and *nix
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif

#ifdef __WINDOWS__
#define CLOSE_SOCKET(s) ::closesocket(s)
#else
#define CLOSE_SOCKET(s) ::close(s)
#endif

namespace ZeroTier {

#ifdef __WINDOWS__
// hack from StackOverflow, behaves a bit like pipe() on *nix systems
static inline void __winpipe(SOCKET fds[2])
{
	struct sockaddr_in inaddr;
	struct sockaddr addr;
	SOCKET lst=::socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
	memset(&inaddr, 0, sizeof(inaddr));
	memset(&addr, 0, sizeof(addr));
	inaddr.sin_family = AF_INET;
	inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	inaddr.sin_port = 0;
	int yes=1;
	setsockopt(lst,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(yes));
	bind(lst,(struct sockaddr *)&inaddr,sizeof(inaddr));
	listen(lst,1);
	int len=sizeof(inaddr);
	getsockname(lst, &addr,&len);
	fds[0]=::socket(AF_INET, SOCK_STREAM,0);
	connect(fds[0],&addr,len);
	fds[1]=accept(lst,0,0);
	closesocket(lst);
}
#endif

SocketManager::SocketManager(
	int localUdpPort,
	int localTcpPort,
	void (*packetHandler)(const SharedPtr<Socket> &,void *,const InetAddress &,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> &),
	void *arg) :
	_whackSendPipe(INVALID_SOCKET),
	_whackReceivePipe(INVALID_SOCKET),
	_tcpV4ListenSocket(INVALID_SOCKET),
	_tcpV6ListenSocket(INVALID_SOCKET),
	_nfds(0),
	_packetHandler(packetHandler),
	_arg(arg)
{
	FD_ZERO(&_readfds);
	FD_ZERO(&_writefds);

#ifdef __WINDOWS__
	{
		SOCKET tmps[2] = { INVALID_SOCKET,INVALID_SOCKET };
		__winpipe(tmps);
		_whackSendPipe = tmps[0];
		_whackReceivePipe = tmps[1];
		u_long iMode=1;
		ioctlsocket(tmps[1],FIONBIO,&iMode);
	}
#else
	{
		int tmpfds[2];
		if (::pipe(tmpfds))
			throw std::runtime_error("pipe() failed");
		_whackSendPipe = tmpfds[1];
		_whackReceivePipe = tmpfds[0];
		fcntl(_whackReceivePipe,F_SETFL,O_NONBLOCK);
	}
#endif
	FD_SET(_whackReceivePipe,&_readfds);

	if (localTcpPort > 0) {
		if (localTcpPort > 0xffff) {
			_closeSockets();
			throw std::runtime_error("invalid local TCP port number");
		}

		{ // bind TCP IPv6
			_tcpV6ListenSocket = ::socket(AF_INET6,SOCK_STREAM,0);
#ifdef __WINDOWS__
			if (_tcpV6ListenSocket == INVALID_SOCKET) {
				_closeSockets();
				throw std::runtime_error("unable to create IPv6 SOCK_STREAM socket");
			}
#else
			if (_tcpV6ListenSocket <= 0) {
				_closeSockets();
				throw std::runtime_error("unable to create IPv6 SOCK_STREAM socket");
			}
#endif

#ifdef __WINDOWS__
			{
				BOOL f;
				f = TRUE; ::setsockopt(_tcpV6ListenSocket,IPPROTO_IPV6,IPV6_V6ONLY,(const char *)&f,sizeof(f));
				f = TRUE; ::setsockopt(_tcpV6ListenSocket,SOL_SOCKET,SO_REUSEADDR,(const char *)&f,sizeof(f));
				u_long iMode=1;
				ioctlsocket(_tcpV6ListenSocket,FIONBIO,&iMode);
			}
#else
			{
				int f;
				f = 1; ::setsockopt(_tcpV6ListenSocket,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&f,sizeof(f));
				f = 1; ::setsockopt(_tcpV6ListenSocket,SOL_SOCKET,SO_REUSEADDR,(void *)&f,sizeof(f));
				fcntl(_tcpV6ListenSocket,F_SETFL,O_NONBLOCK);
			}
#endif

			struct sockaddr_in6 sin6;
			memset(&sin6,0,sizeof(sin6));
			sin6.sin6_family = AF_INET6;
			sin6.sin6_port = htons(localTcpPort);
			memcpy(&(sin6.sin6_addr),&in6addr_any,sizeof(struct in6_addr));
			if (::bind(_tcpV6ListenSocket,(const struct sockaddr *)&sin6,sizeof(sin6))) {
				_closeSockets();
				throw std::runtime_error("unable to bind to local TCP port");
			}

			if (::listen(_tcpV6ListenSocket,16)) {
				_closeSockets();
				throw std::runtime_error("listen() failed");
			}

			FD_SET(_tcpV6ListenSocket,&_readfds);
		}

		{ // bind TCP IPv4
			_tcpV4ListenSocket = ::socket(AF_INET,SOCK_STREAM,0);
#ifdef __WINDOWS__
			if (_tcpV4ListenSocket == INVALID_SOCKET) {
				_closeSockets();
				throw std::runtime_error("unable to create IPv4 SOCK_STREAM socket");
			}
#else
			if (_tcpV4ListenSocket <= 0) {
				_closeSockets();
				throw std::runtime_error("unable to create IPv4 SOCK_STREAM socket");
			}
#endif

#ifdef __WINDOWS__
			{
				BOOL f = TRUE; ::setsockopt(_tcpV4ListenSocket,SOL_SOCKET,SO_REUSEADDR,(const char *)&f,sizeof(f));
				u_long iMode=1;
				ioctlsocket(_tcpV4ListenSocket,FIONBIO,&iMode);
			}
#else
			{
				int f = 1; ::setsockopt(_tcpV4ListenSocket,SOL_SOCKET,SO_REUSEADDR,(void *)&f,sizeof(f));
				fcntl(_tcpV4ListenSocket,F_SETFL,O_NONBLOCK);
			}
#endif

			struct sockaddr_in sin4;
			memset(&sin4,0,sizeof(sin4));
			sin4.sin_family = AF_INET;
			sin4.sin_port = htons(localTcpPort);
			sin4.sin_addr.s_addr = INADDR_ANY;
			if (::bind(_tcpV4ListenSocket,(const struct sockaddr *)&sin4,sizeof(sin4))) {
				_closeSockets();
				throw std::runtime_error("unable to bind to local TCP port");
			}

			if (::listen(_tcpV4ListenSocket,16)) {
				_closeSockets();
				throw std::runtime_error("listen() failed");
			}

			FD_SET(_tcpV4ListenSocket,&_readfds);
		}
	}

	if (localUdpPort > 0) {
		if (localUdpPort > 0xffff) {
			_closeSockets();
			throw std::runtime_error("invalid local UDP port number");
		}

		{ // bind UDP IPv6
#ifdef __WINDOWS__
			SOCKET s = ::socket(AF_INET6,SOCK_DGRAM,0);
			if (s == INVALID_SOCKET) {
				_closeSockets();
				throw std::runtime_error("unable to create IPv6 SOCK_DGRAM socket");
			}
#else
			int s = ::socket(AF_INET6,SOCK_DGRAM,0);
			if (s <= 0) {
				_closeSockets();
				throw std::runtime_error("unable to create IPv6 SOCK_DGRAM socket");
			}
#endif

			{
#ifdef __WINDOWS__
				BOOL f;
				f = TRUE; setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(const char *)&f,sizeof(f));
				f = FALSE; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const char *)&f,sizeof(f));
				f = FALSE; setsockopt(s,IPPROTO_IPV6,IPV6_DONTFRAG,(const char *)&f,sizeof(f));
#else
				int f;
				f = 1; setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&f,sizeof(f));
				f = 0; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(void *)&f,sizeof(f));
#ifdef IP_DONTFRAG
				f = 0; setsockopt(s,IPPROTO_IP,IP_DONTFRAG,&f,sizeof(f));
#endif
#ifdef IP_MTU_DISCOVER
				f = 0; setsockopt(s,IPPROTO_IP,IP_MTU_DISCOVER,&f,sizeof(f));
#endif
#ifdef IPV6_MTU_DISCOVER
				f = 0; setsockopt(s,IPPROTO_IPV6,IPV6_MTU_DISCOVER,&f,sizeof(f));
#endif
#endif
			}

			struct sockaddr_in6 sin6;
			memset(&sin6,0,sizeof(sin6));
			sin6.sin6_family = AF_INET6;
			sin6.sin6_port = htons(localUdpPort);
			memcpy(&(sin6.sin6_addr),&in6addr_any,sizeof(struct in6_addr));
			if (::bind(s,(const struct sockaddr *)&sin6,sizeof(sin6))) {
				CLOSE_SOCKET(s);
				_closeSockets();
				throw std::runtime_error("unable to bind to port");
			}

			_udpV6Socket = SharedPtr<Socket>(new UdpSocket(Socket::ZT_SOCKET_TYPE_UDP_V6,s));
#ifdef __WINDOWS__
			u_long iMode=1;
			ioctlsocket(s,FIONBIO,&iMode);
#else
			fcntl(s,F_SETFL,O_NONBLOCK);
#endif
			FD_SET(s,&_readfds);
		}

		{ // bind UDP IPv4
#ifdef __WINDOWS__
			SOCKET s = ::socket(AF_INET,SOCK_DGRAM,0);
			if (s == INVALID_SOCKET) {
				_closeSockets();
				throw std::runtime_error("unable to create IPv4 SOCK_DGRAM socket");
			}
#else
			int s = ::socket(AF_INET,SOCK_DGRAM,0);
			if (s <= 0) {
				_closeSockets();
				throw std::runtime_error("unable to create IPv4 SOCK_DGRAM socket");
			}
#endif

			{
#ifdef __WINDOWS__
				BOOL f;
				f = FALSE; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const char *)&f,sizeof(f));
				f = FALSE; setsockopt(s,IPPROTO_IP,IP_DONTFRAGMENT,(const char *)&f,sizeof(f));
#else
				int f;
				f = 0; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(void *)&f,sizeof(f));
#ifdef IP_DONTFRAG
				f = 0; setsockopt(s,IPPROTO_IP,IP_DONTFRAG,&f,sizeof(f));
#endif
#ifdef IP_MTU_DISCOVER
				f = 0; setsockopt(s,IPPROTO_IP,IP_MTU_DISCOVER,&f,sizeof(f));
#endif
#endif
			}

			struct sockaddr_in sin4;
			memset(&sin4,0,sizeof(sin4));
			sin4.sin_family = AF_INET;
			sin4.sin_port = htons(localUdpPort);
			sin4.sin_addr.s_addr = INADDR_ANY;
			if (::bind(s,(const struct sockaddr *)&sin4,sizeof(sin4))) {
				CLOSE_SOCKET(s);
				_closeSockets();
				throw std::runtime_error("unable to bind to port");
			}

			_udpV4Socket = SharedPtr<Socket>(new UdpSocket(Socket::ZT_SOCKET_TYPE_UDP_V4,s));
#ifdef __WINDOWS__
			u_long iMode=1;
			ioctlsocket(s,FIONBIO,&iMode);
#else
			fcntl(s,F_SETFL,O_NONBLOCK);
#endif
			FD_SET(s,&_readfds);
		}
	}

	_updateNfds();
}

SocketManager::~SocketManager()
{
	Mutex::Lock _l(_pollLock);
	_closeSockets();
}

bool SocketManager::send(const InetAddress &to,bool tcp,const void *msg,unsigned int msglen)
{
	if (tcp) {
		SharedPtr<Socket> ts;
		{
			Mutex::Lock _l(_tcpSockets_m);
			std::map< InetAddress,SharedPtr<Socket> >::iterator opents(_tcpSockets.find(to));
			if (opents != _tcpSockets.end())
				ts = opents->second;
		}
		if (ts)
			return ts->send(to,msg,msglen);

#ifdef __WINDOWS__
		SOCKET s = ::socket(to.isV4() ? AF_INET : AF_INET6,SOCK_STREAM,0);
		if (s == INVALID_SOCKET)
			return false;
		if (s >= FD_SETSIZE) {
			::closesocket(s);
			return false;
		}
		{
			u_long iMode=1;
			ioctlsocket(s,FIONBIO,&iMode);
		}
#else
		int s = ::socket(to.isV4() ? AF_INET : AF_INET6,SOCK_STREAM,0);
		if (s <= 0)
			return false;
		if (s >= FD_SETSIZE) {
			::close(s);
			return false;
		}
		fcntl(s,F_SETFL,O_NONBLOCK);
#endif

		bool connecting = false;
		if (connect(s,to.saddr(),to.saddrLen())) {
			if (errno != EINPROGRESS) {
				CLOSE_SOCKET(s);
				return false;
			} else connecting = true;
		}

		ts = SharedPtr<Socket>(new TcpSocket(this,s,connecting,to));
		if (!ts->send(to,msg,msglen))
			return false;

		_fdSetLock.lock();
		FD_SET(s,&_readfds);
		if (connecting)
			FD_SET(s,&_writefds);
		_fdSetLock.unlock();

		{
			Mutex::Lock _l(_tcpSockets_m);
			_tcpSockets[to] = ts;
		}

		return true;
	} else if (to.isV4()) {
		if (_udpV4Socket)
			return _udpV4Socket->send(to,msg,msglen);
	} else if (to.isV6()) {
		if (_udpV6Socket)
			return _udpV6Socket->send(to,msg,msglen);
	}
	return false;
}

bool SocketManager::sendFirewallOpener(const InetAddress &to,int hopLimit)
{
	if (to.isV4()) {
		if (_udpV4Socket)
			return ((UdpSocket *)_udpV4Socket.ptr())->sendWithHopLimit(to,"",1,hopLimit);
	} else if (to.isV6()) {
		if (_udpV6Socket)
			return ((UdpSocket *)_udpV6Socket.ptr())->sendWithHopLimit(to,"",1,hopLimit);
	}
	return false;
}

void SocketManager::poll(unsigned long timeout)
{
	fd_set rfds,wfds,efds;
	struct timeval tv;
	std::vector< SharedPtr<Socket> > ts;
#ifdef __WINDOWS__
	SOCKET sockfd;
#else
	int sockfd;
#endif

	Mutex::Lock _l(_pollLock);

	_fdSetLock.lock();
	memcpy(&rfds,&_readfds,sizeof(rfds));
	memcpy(&wfds,&_writefds,sizeof(wfds));
	_fdSetLock.unlock();
	FD_ZERO(&efds);

	tv.tv_sec = (long)(timeout / 1000);
	tv.tv_usec = (long)((timeout % 1000) * 1000);
	select(_nfds + 1,&rfds,&wfds,&efds,(timeout > 0) ? &tv : (struct timeval *)0);

	if (FD_ISSET(_whackReceivePipe,&rfds)) {
		char tmp;
#ifdef __WINDOWS__
		::recv(_whackReceivePipe,&tmp,1,0);
#else
		::read(_whackReceivePipe,&tmp,1);
#endif
	}

	if ((_tcpV4ListenSocket != INVALID_SOCKET)&&(FD_ISSET(_tcpV4ListenSocket,&rfds))) {
		struct sockaddr_in from;
		socklen_t fromlen = sizeof(from);
		sockfd = accept(_tcpV4ListenSocket,(struct sockaddr *)&from,&fromlen);
#ifdef __WINDOWS__
		if (sockfd != INVALID_SOCKET) {
#else
		if (sockfd > 0) {
#endif
			if (sockfd < FD_SETSIZE) {
				InetAddress fromia((const struct sockaddr *)&from);
				Mutex::Lock _l2(_tcpSockets_m);
				try {
					_tcpSockets[fromia] = SharedPtr<Socket>(new TcpSocket(this,sockfd,false,fromia));
#ifdef __WINDOWS__
					u_long iMode=1;
					ioctlsocket(sockfd,FIONBIO,&iMode);
#else
					fcntl(sockfd,F_SETFL,O_NONBLOCK);
#endif
					_fdSetLock.lock();
					FD_SET(sockfd,&_readfds);
					_fdSetLock.unlock();
					if ((int)sockfd > (int)_nfds)
						_nfds = (int)sockfd;
				} catch ( ... ) {
					CLOSE_SOCKET(sockfd);
				}
			} else {
				CLOSE_SOCKET(sockfd);
			}
		}
	}
	if ((_tcpV6ListenSocket != INVALID_SOCKET)&&(FD_ISSET(_tcpV6ListenSocket,&rfds))) {
		struct sockaddr_in6 from;
		socklen_t fromlen = sizeof(from);
		sockfd = accept(_tcpV6ListenSocket,(struct sockaddr *)&from,&fromlen);
#ifdef __WINDOWS__
		if (sockfd != INVALID_SOCKET) {
#else
		if (sockfd > 0) {
#endif
			if (sockfd < FD_SETSIZE) {
				InetAddress fromia((const struct sockaddr *)&from);
				Mutex::Lock _l2(_tcpSockets_m);
				try {
					_tcpSockets[fromia] = SharedPtr<Socket>(new TcpSocket(this,sockfd,false,fromia));
#ifdef __WINDOWS__
					u_long iMode=1;
					ioctlsocket(sockfd,FIONBIO,&iMode);
#else
					fcntl(sockfd,F_SETFL,O_NONBLOCK);
#endif
					_fdSetLock.lock();
					FD_SET(sockfd,&_readfds);
					_fdSetLock.unlock();
					if ((int)sockfd > (int)_nfds)
						_nfds = (int)sockfd;
				} catch ( ... ) {
					CLOSE_SOCKET(sockfd);
				}
			} else {
				CLOSE_SOCKET(sockfd);
			}
		}
	}

	if ((_udpV4Socket)&&(FD_ISSET(_udpV4Socket->_sock,&rfds))) {
		_udpV4Socket->notifyAvailableForRead(_udpV4Socket,this);
	}
	if ((_udpV6Socket)&&(FD_ISSET(_udpV6Socket->_sock,&rfds))) {
		_udpV6Socket->notifyAvailableForRead(_udpV6Socket,this);
	}

	bool closedSockets = false;
	{ // grab copy of TCP sockets list because _tcpSockets[] might be changed in a handler
		Mutex::Lock _l2(_tcpSockets_m);
		if (_tcpSockets.size()) {
			ts.reserve(_tcpSockets.size());
			uint64_t now = Utils::now();
			for(std::map< InetAddress,SharedPtr<Socket> >::iterator s(_tcpSockets.begin());s!=_tcpSockets.end();) {
				if ((now - ((TcpSocket *)s->second.ptr())->_lastActivity) < ZT_TCP_TUNNEL_ACTIVITY_TIMEOUT) {
					ts.push_back(s->second);
					++s;
				} else {
					_fdSetLock.lock();
					FD_CLR(s->second->_sock,&_readfds);
					FD_CLR(s->second->_sock,&_writefds);
					_fdSetLock.unlock();
					_tcpSockets.erase(s++);
					closedSockets = true;
				}
			}
		}
	}
	for(std::vector< SharedPtr<Socket> >::iterator s(ts.begin());s!=ts.end();++s) {
		if (FD_ISSET((*s)->_sock,&wfds)) {
			if (!(*s)->notifyAvailableForWrite(*s,this)) {
				{
					Mutex::Lock _l2(_tcpSockets_m);
					_tcpSockets.erase(((TcpSocket *)s->ptr())->_remote);
				}
				_fdSetLock.lock();
				FD_CLR((*s)->_sock,&_readfds);
				FD_CLR((*s)->_sock,&_writefds);
				_fdSetLock.unlock();
				closedSockets = true;
				continue;
			}
		}
		if (FD_ISSET((*s)->_sock,&rfds)) {
			if (!(*s)->notifyAvailableForRead(*s,this)) {
				{
					Mutex::Lock _l2(_tcpSockets_m);
					_tcpSockets.erase(((TcpSocket *)s->ptr())->_remote);
				}
				_fdSetLock.lock();
				FD_CLR((*s)->_sock,&_readfds);
				FD_CLR((*s)->_sock,&_writefds);
				_fdSetLock.unlock();
				closedSockets = true;
				continue;
			}
		}
	}
	if (closedSockets)
		_updateNfds();
}

void SocketManager::whack()
{
	_whackSendPipe_m.lock();
#ifdef __WINDOWS__
	::send(_whackSendPipe,(const char *)this,1,0);
#else
	::write(_whackSendPipe,(const void *)this,1); // data is arbitrary, just send a byte
#endif
	_whackSendPipe_m.unlock();
}

void SocketManager::closeTcpSockets()
{
	{
		Mutex::Lock _l2(_tcpSockets_m);
		_fdSetLock.lock();
		for(std::map< InetAddress,SharedPtr<Socket> >::iterator s(_tcpSockets.begin());s!=_tcpSockets.end();++s) {
			FD_CLR(s->second->_sock,&_readfds);
			FD_CLR(s->second->_sock,&_writefds);
		}
		_fdSetLock.unlock();
		_tcpSockets.clear();
	}
	_updateNfds();
}

void SocketManager::_closeSockets()
	throw()
{
#ifdef __WINDOWS__
	if (_whackSendPipe != INVALID_SOCKET)
		::closesocket(_whackSendPipe);
	if (_whackReceivePipe != INVALID_SOCKET)
		::closesocket(_whackReceivePipe);
	if (_tcpV4ListenSocket != INVALID_SOCKET)
		::closesocket(_tcpV4ListenSocket);
	if (_tcpV6ListenSocket != INVALID_SOCKET)
		::closesocket(_tcpV6ListenSocket);
#else
	if (_whackSendPipe > 0)
		::close(_whackSendPipe);
	if (_whackReceivePipe > 0)
		::close(_whackReceivePipe);
	if (_tcpV4ListenSocket > 0)
		::close(_tcpV4ListenSocket);
	if (_tcpV4ListenSocket > 0)
		::close(_tcpV6ListenSocket);
#endif
}

void SocketManager::_updateNfds()
{
#ifdef __WINDOWS__
	SOCKET nfds = _whackSendPipe;
#else
	int nfds = _whackSendPipe;
#endif
	if (_whackReceivePipe > nfds)
		nfds = _whackReceivePipe;
	if (_tcpV4ListenSocket > nfds)
		nfds = _tcpV4ListenSocket;
	if (_tcpV6ListenSocket > nfds)
		nfds = _tcpV6ListenSocket;
	if ((_udpV4Socket)&&(_udpV4Socket->_sock > nfds))
		nfds = _udpV4Socket->_sock;
	if ((_udpV6Socket)&&(_udpV6Socket->_sock > nfds))
		nfds = _udpV6Socket->_sock;
	Mutex::Lock _l(_tcpSockets_m);
	for(std::map< InetAddress,SharedPtr<Socket> >::const_iterator s(_tcpSockets.begin());s!=_tcpSockets.end();++s) {
		if (s->second->_sock > nfds)
			nfds = s->second->_sock;
	}
	_nfds = (int)nfds;
}

} // namespace ZeroTier
