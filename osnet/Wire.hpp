/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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

#ifndef ZT_WIRE_HPP
#define ZT_WIRE_HPP

#include <stdio.h>
#include <stdlib.h>

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#else
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#define ZT_SELECTWIRE_SOCKFD_TYPE SOCKET
#define ZT_SELECTWIRE_SOCKFD_NULL (INVALID_SOCKET)
#define ZT_SELECTWIRE_SOCKFD_VALID(s) ((s) != INVALID_SOCKET)
#define ZT_SELECTWIRE_CLOSE_SOCKET(s) ::closesocket(s)
#define ZT_SELECTWIRE_MAX_SOCKETS (FD_SETSIZE)
#define ZT_SELECTWIRE_SOCKADDR_STORAGE_TYPE struct sockaddr_storage
#else
#define ZT_SELECTWIRE_SOCKFD_TYPE int
#define ZT_SELECTWIRE_SOCKFD_NULL (-1)
#define ZT_SELECTWIRE_SOCKFD_VALID(s) ((s) > -1)
#define ZT_SELECTWIRE_CLOSE_SOCKET(s) ::close(s)
#define ZT_SELECTWIRE_MAX_SOCKETS (FD_SETSIZE)
#define ZT_SELECTWIRE_SOCKADDR_STORAGE_TYPE struct sockaddr_storage
#endif

namespace ZeroTier {

/**
 * Wire implementation using select() for *nix or Windows
 *
 * This implementation takes four functions or function objects as template
 * paramters:
 *
 * ON_DATAGRAM_FUNCTION(const void *sock,void **uptr,const struct sockaddr *from,void *data,unsigned long len)
 * ON_TCP_CONNECT_FUNCTION(const void *sock,void **uptr,bool success)
 * ON_TCP_ACCEPT_FUNCTION(const void *sockL,const void *sockN,void **uptrL,void **uptrN,const struct sockaddr *from)
 * ON_TCP_CLOSE_FUNCTION(const void *sock,void *uptr)
 * ON_TCP_DATA_FUNCTION(const void *sock,void **uptr,void *data,unsigned long len)
 * ON_TCP_WRITABLE_FUNCTION(const void *sock,void **uptr)
 *
 * These templates typically refer to function objects. Templates are used to
 * avoid the call overhead of indirection.
 */
template
<
	typename ON_DATAGRAM_FUNCTION,
	typename ON_TCP_CONNECT_FUNCTION,
	typename ON_TCP_ACCEPT_FUNCTION,
	typename ON_TCP_CLOSE_FUNCTION,
	typename ON_TCP_DATA_FUNCTION,
	typename ON_TCP_WRITABLE_FUNCTION
>
class Wire
{
public:
	Wire(
		ON_DATAGRAM_FUNCTION dgHandler,
		ON_TCP_CONNECT_FUNCTION tcpConnectHandler,
		ON_TCP_ACCEPT_FUNCTION tcpAcceptHandler,
		ON_TCP_CLOSE_FUNCTION tcpCloseHandler,
		ON_TCP_DATA_FUNCTION tcpDataHandler,
		ON_TCP_WRITABLE_FUNCTION tcpWritableHandler) :
		_dgHandler(dgHandler),
		_tcpConnectHandler(tcpConnectHandler),
		_tcpAcceptHandler(tcpAcceptHandler),
		_tcpCloseHandler(tcpCloseHandler),
		_tcpDataHandler(tcpDataHandler),
		_tcpWritableHandler(tcpWritableHandler)
	{
		for(unsigned lont i=0;i<ZT_SELECTWIRE_MAX_SOCKETS;++i)
			_socks[i].type = ZT_WIRE_SOCKET_NULL;

		FD_ZERO(&_readfds);
		FD_ZERO(&_writefds);
		FD_ZERO(&_exceptfds);

#if defined(_WIN32) || defined(_WIN64)
		SOCKET pipes[2];
		_winPipeHack(pipes);
#else
		int pipes[2];
		if (::pipe(pipes))
			throw std::runtime_error("unable to create pipes for select() abort");
#endif
		_nfds = (pipes[0] > pipes[1]) ? (long)pipes[0] : (long)pipes[1];
		_whackReceiveSocket = pipes[0];
		_whackSendSocket = pipes[1];
	}

	~Wire()
	{
		for(unsigned long i=0;i<_nsocks;++i) {
			if (_socks[i].type != ZT_WIRE_SOCKET_NULL)
				this->close(_socks[i],true);
		}
		ZT_SELECTWIRE_CLOSE_SOCKET(_whackReceiveSocket);
		ZT_SELECTWIRE_CLOSE_SOCKET(_whackSendSocket);
	}

	inline void whack()
	{
#ifdef __WINDOWS__
		::send(_whackSendSocket,(const char *)this,1,0);
#else
		::write(_whackSendSocket,(const void *)this,1);
#endif
	}

	inline const void *udpBind(const struct sockaddr *localAddress,void *uptr)
	{
		ZT_SELECTWIRE_SOCKFD_TYPE s = ::socket(AF_INET6,SOCK_DGRAM,0);
		if (!ZT_SELECTWIRE_SOCKFD_VALID(s))
			return (const void *)0;

		int bs = 262144;
		while (bs >= 65536) {
			int tmpbs = bs;
			if (setsockopt(s,SOL_SOCKET,SO_RCVBUF,(const char *)&tmpbs,sizeof(tmpbs)) == 0)
				break;
			bs -= 16384;
		}
		bs = 262144;
		while (bs >= 65536) {
			int tmpbs = bs;
			if (setsockopt(s,SOL_SOCKET,SO_SNDBUF,(const char *)&tmpbs,sizeof(tmpbs)) == 0)
				break;
			bs -= 16384;
		}
#ifdef __WINDOWS__
		BOOL f;
		if (localAddress->ss_family == AF_INET6) {
			f = TRUE; setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(const char *)&f,sizeof(f));
			f = FALSE; setsockopt(s,IPPROTO_IPV6,IPV6_DONTFRAG,(const char *)&f,sizeof(f));
		}
		f = FALSE; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const char *)&f,sizeof(f));
		f = TRUE; setsockopt(s,SOL_SOCKET,SO_BROADCAST,(const char *)&f,sizeof(f));
#else
		int f;
		if (localAddress->ss_family == AF_INET6) {
			f = 1; setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&f,sizeof(f));
#ifdef IPV6_MTU_DISCOVER
			f = 0; setsockopt(s,IPPROTO_IPV6,IPV6_MTU_DISCOVER,&f,sizeof(f));
#endif
		}
		f = 0; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(void *)&f,sizeof(f));
		f = 1; setsockopt(s,SOL_SOCKET,SO_BROADCAST,(void *)&f,sizeof(f));
#ifdef IP_DONTFRAG
		f = 0; setsockopt(s,IPPROTO_IP,IP_DONTFRAG,&f,sizeof(f));
#endif
#ifdef IP_MTU_DISCOVER
		f = 0; setsockopt(s,IPPROTO_IP,IP_MTU_DISCOVER,&f,sizeof(f));
#endif
#endif

		if (::bind(s,localAddress,(localAddress->ss_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in))) {
			ZT_SELECTWIRE_CLOSE_SOCKET(s);
			return (const void *)0;
		}

#ifdef __WINDOWS__
		u_long iMode=1;
		ioctlsocket(s,FIONBIO,&iMode);
#else
		fcntl(s,F_SETFL,O_NONBLOCK);
#endif

		for(unsigned long i=0;i<ZT_SELECTWIRE_MAX_SOCKETS;++i) {
			if (_socks[i].type == ZT_WIRE_SOCKET_NULL) {
				if ((long)s > _nfds)
					_nfds = (long)s;
				FD_SET(s,&_readfds);
				_socks[i].type = ZT_WIRE_SOCKET_UDP;
				_socks[i].sock = s;
				_socks[i].uptr = uptr;
				memcpy(&(_socks[i].saddr),localAddress,sizeof(struct sockaddr_storage));
				return (const void *)&(_socks[i]);
			}
		}

		ZT_SELECTWIRE_CLOSE_SOCKET(s);
		return (const void *)0;
	}

	inline bool udpSend(const void *sock,const struct sockaddr *addr,unsigned int addrlen,const void *data,unsigned long len)
	{
		WireSocket &sws = *(const_cast <WireSocket *>(reinterpret_cast<const WireSocket *>(sock)));
		return ((long)::sendto(sws.sock,data,len,0,addr,(socklen_t)addrlen) == (long)len);
	}

	inline const void *tcpListen(const struct sockaddr *localAddress,void *uptr)
	{
	}

	inline const void *tcpConnect(const struct sockaddr *remoteAddress,void *uptr)
	{
	}

	inline unsigned long tcpSend(const void *sock,const void *data,unsigned long len)
	{
		WireSocket &sws = *(const_cast <WireSocket *>(reinterpret_cast<const WireSocket *>(sock)));
		long n = ::send(sws.sock,data,len,0);
		return ((n > 0) ? (unsigned long)n : 0);
	}

	inline const void tcpSetNotifyWritable(const void *sock,bool notifyWritable)
	{
		WireSocket &sws = *(const_cast <WireSocket *>(reinterpret_cast<const WireSocket *>(sock)));
		if (notifyWritable) {
			FD_SET(sws.sock,&_writefds);
		} else {
			FD_CLR(sws.sock,&_writefds);
		}
		this->whack();
	}

	inline void poll(unsigned long timeout)
	{
		char buf[131072];
		struct sockaddr_storage ss;
		struct timeval tv;
		fd_set rfds,wfds,efds;

		memcpy(&rfds,&_readfds,sizeof(rfds));
		memcpy(&wfds,&_writefds,sizeof(wfds));
		memcpy(&efds,&_exceptfds,sizeof(efds));

		tv.tv_sec = (long)(timeout / 1000);
		tv.tv_usec = (long)((timeout % 1000) * 1000);
		select((int)_nfds + 1,&rfds,&wfds,&efds,(timeout > 0) ? &tv : (struct timeval *)0);

		if (FD_ISSET(_whackReceiveSocket,&rfds)) {
			char tmp[16];
#ifdef __WINDOWS__
			::recv(_whackReceiveSocket,tmp,16,0);
#else
			::read(_whackReceiveSocket,tmp,16);
#endif
		}

		for(unsigned long i=0;i<ZT_SELECTWIRE_MAX_SOCKETS;++i) {
			switch (_socks[i].type) {
				case ZT_WIRE_SOCKET_TCP_OUT_PENDING:
					if (FD_ISSET(_socks[i].sock,&efds))
						this->close((const void *)&(_socks[i]),true);
					else if (FD_ISSET(_socks[i].sock,&wfds)) {
						socklen_t slen = sizeof(ss);
						if (::getpeername(_socks[i].sock,(strut sockaddr *)&ss,&slen) != 0)
							this->close((const void *)&(_socks[i]),true);
						else {
							_socks[i].type = ZT_WIRE_SOCKET_TCP_OUT_CONNECTED;
							FD_SET(_socks[i].sock,&_readfds);
							FD_CLR(_socks[i].sock,&_writefds);
							FD_CLR(_socks[i].sock,&_exceptfds);
							try {
								_tcpConnectHandler((const void *)&(_socks[i]),&(_socks[i].uptr),true);
							} catch ( ... ) {}
						}
					}
					break;
				case ZT_WIRE_SOCKET_TCP_OUT_CONNECTED:
				case ZT_WIRE_SOCKET_TCP_IN:
					if (FD_ISSET(_socks[i].sock,&rfds)) {
						long n = (long)::recv(_socks[i].sock,buf,sizeof(buf),0);
						if (n <= 0) {
							this->close((const void *)&(_socks[i]),true);
						} else {
							try {
								_tcpDataHandler((const void *)&(_socks[i]),&(_socks[i].uptr),(void *)buf,(unsigned long)n);
							} catch ( ... ) {}
						}
					}
					if ((FD_ISSET(_socks[i].sock,&wfds))&&(FD_ISSET(_socks[i].sock,&_writefds))) {
						try {
							_tcpWritableHandler((const void *)&(_socks[i]),&(_socks[i].uptr));
						} catch ( ... ) {}
					}
					break;
				case ZT_WIRE_SOCKET_TCP_LISTEN:
					if (FD_ISSET(_socks[i].sock,&rfds)) {
						memset(&ss,0,sizeof(ss));
						socklen_t slen = sizeof(ss);
						ZT_SELECTWIRE_SOCKFD_TYPE s = ::accept(_socks[i].sock,(struct sockaddr *)&ss,&slen);
						if (ZT_SELECTWIRE_SOCKFD_VALID(s)) {
#ifdef __WINDOWS__
							u_long iMode=1;
							ioctlsocket(s,FIONBIO,&iMode);
#else
							fcntl(s,F_SETFL,O_NONBLOCK);
#endif
							bool haveSlot = false;
							for(unsigned long k=0;k<ZT_SELECTWIRE_MAX_SOCKETS;++k) {
								if (_socks[k].type == ZT_WIRE_SOCKET_NULL) {
									FD_SET(s,&_readfds);
									haveSlot = true;
									if ((long)s > _nfds)
										_nfds = (long)s;
									FD_SET(s,&_readfds);
									_socks[k].type = ZT_WIRE_SOCKET_UDP;
									_socks[k].sock = s;
									_socks[k].uptr = (void *)0;
									memcpy(&(_socks[k].saddr),&ss,sizeof(struct sockaddr_storage));
									try {
										_tcpAcceptHandler((const void *)&(_socks[i]),(const void *)&(_socks[k]),&(_socks[i].uptr),&(_socks[k].uptr),(const struct sockaddr *)&(_socks[k].saddr));
									} catch ( ... ) {}
									break;
								}
							}
							if (!haveSlot)
								ZT_SELECTWIRE_CLOSE_SOCKET(s);
						}
					}
					break;
				case ZT_WIRE_SOCKET_UDP:
					if (FD_ISSET(_socks[i].sock,&rfds)) {
						memset(&ss,0,sizeof(ss));
						socklen_t slen = sizeof(ss);
						long n = (long)::recvfrom(_socks[i].sock,buf,sizeof(buf),0,(struct sockaddr *)&ss,&slen);
						if (n > 0) {
							try {
								_dgHandler((const void *)&(_socks[i]),&(_socks[i].uptr),(const struct sockaddr *)&ss,(void *)buf,(unsigned long)n);
							} catch ( ... ) {}
						}
					}
					break;
				default:
					break;
			}
		}
	}

	inline void close(const void *sock,bool callHandlers)
	{
		WireSocket &sws = *(const_cast <WireSocket *>(reinterpret_cast<const WireSocket *>(sock)));

		const WireSocketType oldType = sws.type;
		sws.type = ZT_WIRE_SOCKET_NULL;

		FD_CLR(sws.sock,&_readfds);
		FD_CLR(sws.sock,&_writefds);
		FD_CLR(sws.sock,&_exceptfds);

		ZT_SELECTWIRE_CLOSE_SOCKET(sws.sock);

		switch(oldType) {
			case ZT_WIRE_SOCKET_TCP_OUT_PENDING:
				if (callHandlers) {
					try {
						_tcpConnectHandler((const void *)&sws,&(sws.uptr),false);
					} catch ( ... ) {}
				}
				break;
			case ZT_WIRE_SOCKET_TCP_OUT_CONNECTED:
			case ZT_WIRE_SOCKET_TCP_IN:
				if (callHandlers) {
					try {
						_tcpCloseHandler((const void *)&sws,sws.uptr);
					} catch ( ... ) {}
				}
				break;
			default:
				break;
		}

		if ((long)sws.sock >= _nfds) {
			long nfds = (long)_whackSendSocket;
			for(unsigned long i=0;i<ZT_SELECTWIRE_MAX_SOCKETS;++i) {
				if ((_socks[i].type != ZT_WIRE_SOCKET_NULL)&&((long)_socks[i].sock > nfds))
					nfds = (long)_socks[i].sock;
			}
			_nfds = nfds;
		}
	}

private:
#if defined(_WIN32) || defined(_WIN64)
	// hack copied from StackOverflow, behaves a bit like pipe() on *nix systems
	inline void _winPipeHack(SOCKET fds[2]) const
	{
		struct sockaddr_in inaddr;
		struct sockaddr addr;
		SOCKET lst=::socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
		if (lst == INVALID_SOCKET)
			throw std::runtime_error("unable to create pipes for select() abort");
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
		if (fds[0] == INVALID_SOCKET)
			throw std::runtime_error("unable to create pipes for select() abort");
		connect(fds[0],&addr,len);
		fds[1]=accept(lst,0,0);
		closesocket(lst);
	}
#endif

	enum WireSocketType
	{
		ZT_WIRE_SOCKET_TCP_OUT_PENDING = 0x00,
		ZT_WIRE_SOCKET_TCP_OUT_CONNECTED = 0x01,
		ZT_WIRE_SOCKET_TCP_IN = 0x02,
		ZT_WIRE_SOCKET_TCP_LISTEN = 0x03, // isTCP() == ((type & 0x03) != 0)
		ZT_WIRE_SOCKET_RAW = 0x04,
		ZT_WIRE_SOCKET_UDP = 0x05,
		ZT_WIRE_SOCKET_NULL = 0x06
	};

	struct WireSocket
	{
		WireSocketType type;
		ZT_SELECTWIRE_SOCKFD_TYPE sock;
		void *uptr; // user-settable pointer
		ZT_SELECTWIRE_SOCKADDR_STORAGE_TYPE saddr; // from address for TCP_IN, local address otherwise
	};

	inline bool _isTCP(const WireSocket &sws) const throw() { return ((((unsigned int)sws.type) & 0x03) != 0); }

	ON_DATAGRAM_FUNCTION _dgHandler;
	ON_TCP_CONNECT_FUNCTION _tcpConnectHandler;
	ON_TCP_ACCEPT_FUNCTION _tcpAcceptHandler;
	ON_TCP_CLOSE_FUNCTION _tcpCloseHandler;
	ON_TCP_DATA_FUNCTION _tcpDataHandler;
	ON_TCP_WRITABLE_FUNCTION _tcpWritableHandler;
	WireSocket _socks[ZT_SELECTWIRE_MAX_SOCKETS];
	fd_set _readfds,_writefds,_exceptfds;
	long _nfds;
	ZT_SELECTWIRE_SOCKFD_TYPE _whackReceiveSocket;
	ZT_SELECTWIRE_SOCKFD_TYPE _whackSendSocket;
};

} // namespace ZeroTier

#endif
