/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_PHY_HPP
#define ZT_PHY_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <list>
#include <stdexcept>

#if defined(_WIN32) || defined(_WIN64)

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#define ZT_PHY_SOCKFD_TYPE SOCKET
#define ZT_PHY_SOCKFD_NULL (INVALID_SOCKET)
#define ZT_PHY_SOCKFD_VALID(s) ((s) != INVALID_SOCKET)
#define ZT_PHY_CLOSE_SOCKET(s) ::closesocket(s)
#define ZT_PHY_MAX_SOCKETS (FD_SETSIZE)
#define ZT_PHY_MAX_INTERCEPTS ZT_PHY_MAX_SOCKETS
#define ZT_PHY_SOCKADDR_STORAGE_TYPE struct sockaddr_storage

#else // not Windows

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "../node/Metrics.hpp"

#if defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)
#ifndef IPV6_DONTFRAG
#define IPV6_DONTFRAG 62
#endif
#endif

#define ZT_PHY_SOCKFD_TYPE int
#define ZT_PHY_SOCKFD_NULL (-1)
#define ZT_PHY_SOCKFD_VALID(s) ((s) > -1)
#define ZT_PHY_CLOSE_SOCKET(s) ::close(s)
#define ZT_PHY_MAX_SOCKETS (FD_SETSIZE)
#define ZT_PHY_MAX_INTERCEPTS ZT_PHY_MAX_SOCKETS
#define ZT_PHY_SOCKADDR_STORAGE_TYPE struct sockaddr_storage

#endif // Windows or not

namespace ZeroTier {

/**
 * Opaque socket type
 */
typedef void PhySocket;

/**
 * Simple templated non-blocking sockets implementation
 *
 * Yes there is boost::asio and libuv, but I like small binaries and I hate
 * build dependencies. Both drag in a whole bunch of pasta with them.
 *
 * This class is templated on a pointer to a handler class which must
 * implement the following functions:
 *
 * For all platforms:
 *
 * phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *localAddr,const struct sockaddr *from,void *data,unsigned long len)
 * phyOnTcpConnect(PhySocket *sock,void **uptr,bool success)
 * phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from)
 * phyOnTcpClose(PhySocket *sock,void **uptr)
 * phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len)
 * phyOnTcpWritable(PhySocket *sock,void **uptr)
 * phyOnFileDescriptorActivity(PhySocket *sock,void **uptr,bool readable,bool writable)
 *
 * On Linux/OSX/Unix only (not required/used on Windows or elsewhere):
 *
 * phyOnUnixAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN)
 * phyOnUnixClose(PhySocket *sock,void **uptr)
 * phyOnUnixData(PhySocket *sock,void **uptr,void *data,unsigned long len)
 * phyOnUnixWritable(PhySocket *sock,void **uptr)
 *
 * These templates typically refer to function objects. Templates are used to
 * avoid the call overhead of indirection, which is surprisingly high for high
 * bandwidth applications pushing a lot of packets.
 *
 * The 'sock' pointer above is an opaque pointer to a socket. Each socket
 * has a 'uptr' user-settable/modifiable pointer associated with it, which
 * can be set on bind/connect calls and is passed as a void ** to permit
 * resetting at any time. The ACCEPT handler takes two sets of sock and
 * uptr: sockL and uptrL for the listen socket, and sockN and uptrN for
 * the new TCP connection socket that has just been created.
 *
 * Handlers are always called. On outgoing TCP connection, CONNECT is always
 * called on either success or failure followed by DATA and/or WRITABLE as
 * indicated. On socket close, handlers are called unless close() is told
 * explicitly not to call handlers. It is safe to close a socket within a
 * handler, and in that case close() can be told not to call handlers to
 * prevent recursion.
 *
 * This isn't thread-safe with the exception of whack(), which is safe to
 * call from another thread to abort poll().
 */
template <typename HANDLER_PTR_TYPE>
class Phy
{
private:
	HANDLER_PTR_TYPE _handler;

	enum PhySocketType
	{
		ZT_PHY_SOCKET_CLOSED = 0x00, // socket is closed, will be removed on next poll()
		ZT_PHY_SOCKET_TCP_OUT_PENDING = 0x01,
		ZT_PHY_SOCKET_TCP_OUT_CONNECTED = 0x02,
		ZT_PHY_SOCKET_TCP_IN = 0x03,
		ZT_PHY_SOCKET_TCP_LISTEN = 0x04,
		ZT_PHY_SOCKET_UDP = 0x05,
		ZT_PHY_SOCKET_FD = 0x06,
		ZT_PHY_SOCKET_UNIX_IN = 0x07,
		ZT_PHY_SOCKET_UNIX_LISTEN = 0x08
	};

	struct PhySocketImpl {
		PhySocketImpl() {}
		PhySocketType type;
		ZT_PHY_SOCKFD_TYPE sock;
		void *uptr; // user-settable pointer
		ZT_PHY_SOCKADDR_STORAGE_TYPE saddr; // remote for TCP_OUT and TCP_IN, local for TCP_LISTEN, RAW, and UDP
	};

	std::list<PhySocketImpl> _socks;
	fd_set _readfds;
	fd_set _writefds;
#if defined(_WIN32) || defined(_WIN64)
	fd_set _exceptfds;
#endif
	long _nfds;

	ZT_PHY_SOCKFD_TYPE _whackReceiveSocket;
	ZT_PHY_SOCKFD_TYPE _whackSendSocket;

	bool _noDelay;
	bool _noCheck;

public:
	/**
	 * @param handler Pointer of type HANDLER_PTR_TYPE to handler
	 * @param noDelay If true, disable TCP NAGLE algorithm on TCP sockets
	 * @param noCheck If true, attempt to set UDP SO_NO_CHECK option to disable sending checksums
	 */
	Phy(HANDLER_PTR_TYPE handler,bool noDelay,bool noCheck) :
		_handler(handler)
	{
		FD_ZERO(&_readfds);
		FD_ZERO(&_writefds);

#if defined(_WIN32) || defined(_WIN64)
		FD_ZERO(&_exceptfds);

		SOCKET pipes[2];
		{	// hack copied from StackOverflow, behaves a bit like pipe() on *nix systems
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
			pipes[0]=::socket(AF_INET, SOCK_STREAM,0);
			if (pipes[0] == INVALID_SOCKET)
				throw std::runtime_error("unable to create pipes for select() abort");
			connect(pipes[0],&addr,len);
			pipes[1]=accept(lst,0,0);
			closesocket(lst);
		}
#else // not Windows
		int pipes[2];
		if (::pipe(pipes))
			throw std::runtime_error("unable to create pipes for select() abort");
#endif // Windows or not

		_nfds = (pipes[0] > pipes[1]) ? (long)pipes[0] : (long)pipes[1];
		_whackReceiveSocket = pipes[0];
		_whackSendSocket = pipes[1];
		_noDelay = noDelay;
		_noCheck = noCheck;
	}

	~Phy()
	{
		for(typename std::list<PhySocketImpl>::const_iterator s(_socks.begin());s!=_socks.end();++s) {
			if (s->type != ZT_PHY_SOCKET_CLOSED)
				this->close((PhySocket *)&(*s),true);
		}
		ZT_PHY_CLOSE_SOCKET(_whackReceiveSocket);
		ZT_PHY_CLOSE_SOCKET(_whackSendSocket);
	}

	/**
	 * @param s Socket object
	 * @return Underlying OS-type (usually int or long) file descriptor associated with object
	 */
	static inline ZT_PHY_SOCKFD_TYPE getDescriptor(PhySocket* s) throw()
	{
		return reinterpret_cast<PhySocketImpl*>(s)->sock;
	}

	/**
	 * @param s Socket object
	 * @return Pointer to user object
	 */
	static inline void** getuptr(PhySocket* s) throw()
	{
		return &(reinterpret_cast<PhySocketImpl*>(s)->uptr);
	}

	/**
	 * Cause poll() to stop waiting immediately
	 *
	 * This can be used to reset the polling loop after changes that require
	 * attention, or to shut down a background thread that is waiting, etc.
	 */
	inline void whack()
	{
#if defined(_WIN32) || defined(_WIN64)
		::send(_whackSendSocket, (const char*)this, 1, 0);
#else
		(void)(::write(_whackSendSocket, (PhySocket*)this, 1));
#endif
	}

	/**
	 * @return Number of open sockets
	 */
	inline unsigned long count() const throw()
	{
		return _socks.size();
	}

	/**
	 * @return Maximum number of sockets allowed
	 */
	inline unsigned long maxCount() const throw()
	{
		return ZT_PHY_MAX_SOCKETS;
	}

	/**
	 * Wrap a raw file descriptor in a PhySocket structure
	 *
	 * This can be used to select/poll on a raw file descriptor as part of this
	 * class's I/O loop. By default the fd is set for read notification but
	 * this can be controlled with setNotifyReadable(). When any detected
	 * condition is present, the phyOnFileDescriptorActivity() callback is
	 * called with one or both of its arguments 'true'.
	 *
	 * The Phy<>::close() method *must* be called when you're done with this
	 * file descriptor to remove it from the select/poll set, but unlike other
	 * types of sockets Phy<> does not actually close the underlying fd or
	 * otherwise manage its life cycle. There is also no close notification
	 * callback for this fd, since Phy<> doesn't actually perform reading or
	 * writing or detect error conditions. This is only useful for adding a
	 * file descriptor to Phy<> to select/poll on it.
	 *
	 * @param fd Raw file descriptor
	 * @param uptr User pointer to supply to callbacks
	 * @return PhySocket wrapping fd or NULL on failure (out of memory or too many sockets)
	 */
	inline PhySocket *wrapSocket(ZT_PHY_SOCKFD_TYPE fd,void *uptr = (void *)0)
	{
		if (_socks.size() >= ZT_PHY_MAX_SOCKETS)
			return (PhySocket *)0;
		try {
			_socks.push_back(PhySocketImpl());
		} catch ( ... ) {
			return (PhySocket *)0;
		}
		PhySocketImpl &sws = _socks.back();
		if ((long)fd > _nfds)
			_nfds = (long)fd;
		FD_SET(fd,&_readfds);
		sws.type = ZT_PHY_SOCKET_UNIX_IN; /* TODO: Type was changed to allow for CBs with new RPC model */
		sws.sock = fd;
		sws.uptr = uptr;
		memset(&(sws.saddr),0,sizeof(struct sockaddr_storage));
		// no sockaddr for this socket type, leave saddr null
		return (PhySocket *)&sws;
	}

	/**
	 * Bind a UDP socket
	 *
	 * @param localAddress Local endpoint address and port
	 * @param uptr Initial value of user pointer associated with this socket (default: NULL)
	 * @param bufferSize Desired socket receive/send buffer size -- will set as close to this as possible (default: 0, leave alone)
	 * @return Socket or NULL on failure to bind
	 */
	inline PhySocket *udpBind(const struct sockaddr *localAddress,void *uptr = (void *)0,int bufferSize = 0)
	{
		if (_socks.size() >= ZT_PHY_MAX_SOCKETS)
			return (PhySocket *)0;

		ZT_PHY_SOCKFD_TYPE s = ::socket(localAddress->sa_family,SOCK_DGRAM,0);
		if (!ZT_PHY_SOCKFD_VALID(s))
			return (PhySocket *)0;

		if (bufferSize > 0) {
			int bs = bufferSize;
			while (bs >= 65536) {
				int tmpbs = bs;
				if (setsockopt(s,SOL_SOCKET,SO_RCVBUF,(const char *)&tmpbs,sizeof(tmpbs)) == 0)
					break;
				bs -= 4096;
			}
			bs = bufferSize;
			while (bs >= 65536) {
				int tmpbs = bs;
				if (setsockopt(s,SOL_SOCKET,SO_SNDBUF,(const char *)&tmpbs,sizeof(tmpbs)) == 0)
					break;
				bs -= 4096;
			}
		}

#if defined(_WIN32) || defined(_WIN64)
		{
			BOOL f;
			if (localAddress->sa_family == AF_INET6) {
				f = TRUE; setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(const char *)&f,sizeof(f));
				f = FALSE; setsockopt(s,IPPROTO_IPV6,IPV6_DONTFRAG,(const char *)&f,sizeof(f));
			}
			f = FALSE; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const char *)&f,sizeof(f));
			f = TRUE; setsockopt(s,SOL_SOCKET,SO_BROADCAST,(const char *)&f,sizeof(f));
		}
#else // not Windows
		{
			int f;
			if (localAddress->sa_family == AF_INET6) {
				f = 1; setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&f,sizeof(f));
#ifdef IPV6_MTU_DISCOVER
				f = 0; setsockopt(s,IPPROTO_IPV6,IPV6_MTU_DISCOVER,&f,sizeof(f));
#endif
#ifdef IPV6_DONTFRAG
				f = 0; setsockopt(s,IPPROTO_IPV6,IPV6_DONTFRAG,&f,sizeof(f));
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
#ifdef SO_NO_CHECK
			// For now at least we only set SO_NO_CHECK on IPv4 sockets since some
			// IPv6 stacks incorrectly discard zero checksum packets. May remove
			// this restriction later once broken stuff dies more.
			if ((localAddress->sa_family == AF_INET)&&(_noCheck)) {
				f = 1; setsockopt(s,SOL_SOCKET,SO_NO_CHECK,(void *)&f,sizeof(f));
			}
#endif
		}
#endif // Windows or not

		if (::bind(s,localAddress,(localAddress->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in))) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}

#if defined(_WIN32) || defined(_WIN64)
		{ u_long iMode=1; ioctlsocket(s,FIONBIO,&iMode); }
#else
		fcntl(s,F_SETFL,O_NONBLOCK);
#endif

		try {
			_socks.push_back(PhySocketImpl());
		} catch ( ... ) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}
		PhySocketImpl &sws = _socks.back();

		if ((long)s > _nfds)
			_nfds = (long)s;
		FD_SET(s,&_readfds);
		sws.type = ZT_PHY_SOCKET_UDP;
		sws.sock = s;
		sws.uptr = uptr;
		memset(&(sws.saddr),0,sizeof(struct sockaddr_storage));
		memcpy(&(sws.saddr),localAddress,(localAddress->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));

		return (PhySocket *)&sws;
	}

	/**
	 * Set the IP TTL for the next outgoing packet (for IPv4 UDP sockets only)
	 *
	 * @param ttl New TTL (0 or >255 will set it to 255)
	 * @return True on success
	 */
	inline bool setIp4UdpTtl(PhySocket *sock,unsigned int ttl)
	{
		PhySocketImpl &sws = *(reinterpret_cast<PhySocketImpl *>(sock));
#if defined(_WIN32) || defined(_WIN64)
		DWORD tmp = ((ttl == 0)||(ttl > 255)) ? 255 : (DWORD)ttl;
		return (::setsockopt(sws.sock,IPPROTO_IP,IP_TTL,(const char *)&tmp,sizeof(tmp)) == 0);
#else
		int tmp = ((ttl == 0)||(ttl > 255)) ? 255 : (int)ttl;
		return (::setsockopt(sws.sock,IPPROTO_IP,IP_TTL,(void *)&tmp,sizeof(tmp)) == 0);
#endif
	}

	/**
	 * Send a UDP packet
	 *
	 * @param sock UDP socket
	 * @param remoteAddress Destination address (must be correct type for socket)
	 * @param data Data to send
	 * @param len Length of packet
	 * @return True if packet appears to have been sent successfully
	 */
	inline bool udpSend(PhySocket *sock,const struct sockaddr *remoteAddress,const void *data,unsigned long len)
	{
		PhySocketImpl &sws = *(reinterpret_cast<PhySocketImpl *>(sock));
		bool sent = false;
#if defined(_WIN32) || defined(_WIN64)
		sent = ((long)::sendto(
				sws.sock,
				reinterpret_cast<const char *>(data),
				len,
				0,
				remoteAddress,
				(remoteAddress->sa_family == AF_INET6) ? 
					sizeof(struct sockaddr_in6) : 
					sizeof(struct sockaddr_in)) == (long)len);
#else
		sent = ((long)::sendto(
				sws.sock,
				data,
				len,
				0,
				remoteAddress,
				(remoteAddress->sa_family == AF_INET6) ? 
					sizeof(struct sockaddr_in6) : 
				 	sizeof(struct sockaddr_in)) == (long)len);
#endif
		if (sent) {
			Metrics::udp_send += len;
		}

		return sent;
	}

#ifdef __UNIX_LIKE__
	/**
	 * Listen for connections on a Unix domain socket
	 *
	 * @param path Path to Unix domain socket
	 * @param uptr Arbitrary pointer to associate
	 * @return PhySocket or NULL if cannot bind
	 */
	inline PhySocket *unixListen(const char *path,void *uptr = (void *)0)
	{
		struct sockaddr_un sun;

		if (_socks.size() >= ZT_PHY_MAX_SOCKETS)
			return (PhySocket *)0;

		memset(&sun,0,sizeof(sun));
		sun.sun_family = AF_UNIX;
		if (strlen(path) >= sizeof(sun.sun_path))
			return (PhySocket *)0;
		strcpy(sun.sun_path,path);

		ZT_PHY_SOCKFD_TYPE s = ::socket(PF_UNIX,SOCK_STREAM,0);
		if (!ZT_PHY_SOCKFD_VALID(s))
			return (PhySocket *)0;

		::fcntl(s,F_SETFL,O_NONBLOCK);

		::unlink(path);
		if (::bind(s,(struct sockaddr *)&sun,sizeof(struct sockaddr_un)) != 0) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}
		if (::listen(s,128) != 0) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}

		try {
			_socks.push_back(PhySocketImpl());
		} catch ( ... ) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}
		PhySocketImpl &sws = _socks.back();

		if ((long)s > _nfds)
			_nfds = (long)s;
		FD_SET(s,&_readfds);
		sws.type = ZT_PHY_SOCKET_UNIX_LISTEN;
		sws.sock = s;
		sws.uptr = uptr;
		memset(&(sws.saddr),0,sizeof(struct sockaddr_storage));
		memcpy(&(sws.saddr),&sun,sizeof(struct sockaddr_un));

		return (PhySocket *)&sws;
	}
#endif // __UNIX_LIKE__

	/**
	 * Bind a local listen socket to listen for new TCP connections
	 *
	 * @param localAddress Local address and port
	 * @param uptr Initial value of uptr for new socket (default: NULL)
	 * @return Socket or NULL on failure to bind
	 */
	inline PhySocket *tcpListen(const struct sockaddr *localAddress,void *uptr = (void *)0)
	{
		if (_socks.size() >= ZT_PHY_MAX_SOCKETS)
			return (PhySocket *)0;

		ZT_PHY_SOCKFD_TYPE s = ::socket(localAddress->sa_family,SOCK_STREAM,0);
		if (!ZT_PHY_SOCKFD_VALID(s))
			return (PhySocket *)0;

#if defined(_WIN32) || defined(_WIN64)
		{
			BOOL f;
			f = TRUE; ::setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(const char *)&f,sizeof(f));
			f = TRUE; ::setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const char *)&f,sizeof(f));
			f = (_noDelay ? TRUE : FALSE); setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(char *)&f,sizeof(f));
			u_long iMode=1;
			ioctlsocket(s,FIONBIO,&iMode);
		}
#else
		{
			int f;
			f = 1; ::setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&f,sizeof(f));
			f = 1; ::setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(void *)&f,sizeof(f));
			f = (_noDelay ? 1 : 0); setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(char *)&f,sizeof(f));
			fcntl(s,F_SETFL,O_NONBLOCK);
		}
#endif

		if (::bind(s,localAddress,(localAddress->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in))) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}

		if (::listen(s,1024)) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}

		try {
			_socks.push_back(PhySocketImpl());
		} catch ( ... ) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}
		PhySocketImpl &sws = _socks.back();

		if ((long)s > _nfds)
			_nfds = (long)s;
		FD_SET(s,&_readfds);
		sws.type = ZT_PHY_SOCKET_TCP_LISTEN;
		sws.sock = s;
		sws.uptr = uptr;
		memset(&(sws.saddr),0,sizeof(struct sockaddr_storage));
		memcpy(&(sws.saddr),localAddress,(localAddress->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));

		return (PhySocket *)&sws;
	}

	/**
	 * Start a non-blocking connect; CONNECT handler is called on success or failure
	 *
	 * A return value of NULL indicates a synchronous failure such as a
	 * failure to open a socket. The TCP connection handler is not called
	 * in this case.
	 *
	 * It is possible on some platforms for an "instant connect" to occur,
	 * such as when connecting to a loopback address. In this case, the
	 * 'connected' result parameter will be set to 'true' and if the
	 * 'callConnectHandler' flag is true (the default) the TCP connect
	 * handler will be called before the function returns.
	 *
	 * These semantics can be a bit confusing, but they're less so than
	 * the underlying semantics of asynchronous TCP connect.
	 *
	 * @param remoteAddress Remote address
	 * @param connected Result parameter: set to whether an "instant connect" has occurred (true if yes)
	 * @param uptr Initial value of uptr for new socket (default: NULL)
	 * @param callConnectHandler If true, call TCP connect handler even if result is known before function exit (default: true)
	 * @return New socket or NULL on failure
	 */
	inline PhySocket *tcpConnect(const struct sockaddr *remoteAddress,bool &connected,void *uptr = (void *)0,bool callConnectHandler = true)
	{
		if (_socks.size() >= ZT_PHY_MAX_SOCKETS)
			return (PhySocket *)0;

		ZT_PHY_SOCKFD_TYPE s = ::socket(remoteAddress->sa_family,SOCK_STREAM,0);
		if (!ZT_PHY_SOCKFD_VALID(s)) {
			connected = false;
			return (PhySocket *)0;
		}

#if defined(_WIN32) || defined(_WIN64)
		{
			BOOL f;
			if (remoteAddress->sa_family == AF_INET6) { f = TRUE; ::setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(const char *)&f,sizeof(f)); }
			f = TRUE; ::setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const char *)&f,sizeof(f));
			f = (_noDelay ? TRUE : FALSE); setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(char *)&f,sizeof(f));
			u_long iMode=1;
			ioctlsocket(s,FIONBIO,&iMode);
		}
#else
		{
			int f;
			if (remoteAddress->sa_family == AF_INET6) { f = 1; ::setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&f,sizeof(f)); }
			f = 1; ::setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(void *)&f,sizeof(f));
			f = (_noDelay ? 1 : 0); setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(char *)&f,sizeof(f));
			fcntl(s,F_SETFL,O_NONBLOCK);
		}
#endif

		connected = true;
		if (::connect(s,remoteAddress,(remoteAddress->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in))) {
			connected = false;
#if defined(_WIN32) || defined(_WIN64)
			if (WSAGetLastError() != WSAEWOULDBLOCK) {
#else
			if (errno != EINPROGRESS) {
#endif
				ZT_PHY_CLOSE_SOCKET(s);
				return (PhySocket *)0;
			} // else connection is proceeding asynchronously...
		}

		try {
			_socks.push_back(PhySocketImpl());
		} catch ( ... ) {
			ZT_PHY_CLOSE_SOCKET(s);
			return (PhySocket *)0;
		}
		PhySocketImpl &sws = _socks.back();

		if ((long)s > _nfds)
			_nfds = (long)s;
		if (connected) {
			FD_SET(s,&_readfds);
			sws.type = ZT_PHY_SOCKET_TCP_OUT_CONNECTED;
		} else {
			FD_SET(s,&_writefds);
#if defined(_WIN32) || defined(_WIN64)
			FD_SET(s,&_exceptfds);
#endif
			sws.type = ZT_PHY_SOCKET_TCP_OUT_PENDING;
		}
		sws.sock = s;
		sws.uptr = uptr;
		memset(&(sws.saddr),0,sizeof(struct sockaddr_storage));
		memcpy(&(sws.saddr),remoteAddress,(remoteAddress->sa_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in));

		if ((callConnectHandler)&&(connected)) {
			try {
				_handler->phyOnTcpConnect((PhySocket *)&sws,&(sws.uptr),true);
			} catch ( ... ) {}
		}

		return (PhySocket *)&sws;
	}

	/**
	 * Try to set buffer sizes as close to the given value as possible
	 *
	 * This will try the specified value and then lower values in 16K increments
	 * until one works.
	 *
	 * @param sock Socket
	 * @param receiveBufferSize Desired size of receive buffer
	 * @param sendBufferSize Desired size of send buffer
	 */
	inline void setBufferSizes(const PhySocket *sock,int receiveBufferSize,int sendBufferSize)
	{
		PhySocketImpl &sws = *(reinterpret_cast<PhySocketImpl *>(sock));
		if (receiveBufferSize > 0) {
			while (receiveBufferSize > 0) {
				int tmpbs = receiveBufferSize;
				if (::setsockopt(sws.sock,SOL_SOCKET,SO_RCVBUF,(const char *)&tmpbs,sizeof(tmpbs)) == 0)
					break;
				receiveBufferSize -= 16384;
			}
		}
		if (sendBufferSize > 0) {
			while (sendBufferSize > 0) {
				int tmpbs = sendBufferSize;
				if (::setsockopt(sws.sock,SOL_SOCKET,SO_SNDBUF,(const char *)&tmpbs,sizeof(tmpbs)) == 0)
					break;
				sendBufferSize -= 16384;
			}
		}
	}

	/**
	 * Attempt to send data to a stream socket (non-blocking)
	 *
	 * If -1 is returned, the socket should no longer be used as it is now
	 * destroyed. If callCloseHandler is true, the close handler will be
	 * called before the function returns.
	 *
	 * This can be used with TCP, Unix, or socket pair sockets.
	 *
	 * @param sock An open stream socket (other socket types will fail)
	 * @param data Data to send
	 * @param len Length of data
	 * @param callCloseHandler If true, call close handler on socket closing failure condition (default: true)
	 * @return Number of bytes actually sent or -1 on fatal error (socket closure)
	 */
	inline long streamSend(PhySocket *sock,const void *data,unsigned long len,bool callCloseHandler = true)
	{
		PhySocketImpl &sws = *(reinterpret_cast<PhySocketImpl *>(sock));
#if defined(_WIN32) || defined(_WIN64)
		long n = (long)::send(sws.sock,reinterpret_cast<const char *>(data),len,0);
		if (n == SOCKET_ERROR) {
				switch(WSAGetLastError()) {
					case WSAEINTR:
					case WSAEWOULDBLOCK:
						return 0;
					default:
						this->close(sock,callCloseHandler);
						return -1;
				}
		}
#else // not Windows
		long n = (long)::send(sws.sock,data,len,0);
		if (n < 0) {
			switch(errno) {
#ifdef EAGAIN
				case EAGAIN:
#endif
#if defined(EWOULDBLOCK) && ( !defined(EAGAIN) || (EWOULDBLOCK != EAGAIN) )
				case EWOULDBLOCK:
#endif
#ifdef EINTR
				case EINTR:
#endif
					return 0;
				default:
					this->close(sock,callCloseHandler);
					return -1;
			}
		}
#endif // Windows or not
		return n;
	}

#ifdef __UNIX_LIKE__
	/**
	 * Attempt to send data to a Unix domain socket connection (non-blocking)
	 *
	 * If -1 is returned, the socket should no longer be used as it is now
	 * destroyed. If callCloseHandler is true, the close handler will be
	 * called before the function returns.
	 *
	 * @param sock An open Unix socket (other socket types will fail)
	 * @param data Data to send
	 * @param len Length of data
	 * @param callCloseHandler If true, call close handler on socket closing failure condition (default: true)
	 * @return Number of bytes actually sent or -1 on fatal error (socket closure)
	 */
	inline long unixSend(PhySocket *sock,const void *data,unsigned long len,bool callCloseHandler = true)
	{
		PhySocketImpl &sws = *(reinterpret_cast<PhySocketImpl *>(sock));
		long n = (long)::write(sws.sock,data,len);
		if (n < 0) {
			switch(errno) {
#ifdef EAGAIN
				case EAGAIN:
#endif
#if defined(EWOULDBLOCK) && ( !defined(EAGAIN) || (EWOULDBLOCK != EAGAIN) )
				case EWOULDBLOCK:
#endif
#ifdef EINTR
				case EINTR:
#endif
					return 0;
				default:
					this->close(sock,callCloseHandler);
					return -1;
			}
		}
		return n;
	}
#endif // __UNIX_LIKE__

	/**
	 * For streams, sets whether we want to be notified that the socket is writable
	 *
	 * This can be used with TCP, Unix, or socket pair sockets.
	 *
	 * Call whack() if this is being done from another thread and you want
	 * it to take effect immediately. Otherwise it is only guaranteed to
	 * take effect on the next poll().
	 *
	 * @param sock Stream connection socket
	 * @param notifyWritable Want writable notifications?
	 */
	inline void setNotifyWritable(PhySocket *sock,bool notifyWritable)
	{
		PhySocketImpl &sws = *(reinterpret_cast<PhySocketImpl *>(sock));
		if (notifyWritable) {
			FD_SET(sws.sock,&_writefds);
		} else {
			FD_CLR(sws.sock,&_writefds);
		}
	}

	/**
	 * Set whether we want to be notified that a socket is readable
	 *
	 * This is primarily for raw sockets added with wrapSocket(). It could be
	 * used with others, but doing so would essentially lock them and prevent
	 * data from being read from them until this is set to 'true' again.
	 *
	 * @param sock Socket to modify
	 * @param notifyReadable True if socket should be monitored for readability
	 */
	inline void setNotifyReadable(PhySocket *sock,bool notifyReadable)
	{
		PhySocketImpl &sws = *(reinterpret_cast<PhySocketImpl *>(sock));
		if (notifyReadable) {
			FD_SET(sws.sock,&_readfds);
		} else {
			FD_CLR(sws.sock,&_readfds);
		}
	}

	/**
	 * Wait for activity and handle one or more events
	 *
	 * Note that this is not guaranteed to wait up to 'timeout' even
	 * if nothing happens, as whack() or other events such as signals
	 * may cause premature termination.
	 *
	 * @param timeout Timeout in milliseconds or 0 for none (forever)
	 */
	inline void poll(unsigned long timeout)
	{
		char buf[131072];
		struct sockaddr_storage ss;
		struct timeval tv;
		fd_set rfds,wfds,efds;

		memcpy(&rfds,&_readfds,sizeof(rfds));
		memcpy(&wfds,&_writefds,sizeof(wfds));
#if defined(_WIN32) || defined(_WIN64)
		memcpy(&efds,&_exceptfds,sizeof(efds));
#else
		FD_ZERO(&efds);
#endif

		tv.tv_sec = (long)(timeout / 1000);
		tv.tv_usec = (long)((timeout % 1000) * 1000);
		if (::select((int)_nfds + 1,&rfds,&wfds,&efds,(timeout > 0) ? &tv : (struct timeval *)0) <= 0)
			return;

		if (FD_ISSET(_whackReceiveSocket,&rfds)) {
			char tmp[16];
#if defined(_WIN32) || defined(_WIN64)
			::recv(_whackReceiveSocket,tmp,16,0);
#else
			::read(_whackReceiveSocket,tmp,16);
#endif
		}

		for(typename std::list<PhySocketImpl>::iterator s(_socks.begin());s!=_socks.end();) {
			switch (s->type) {

				case ZT_PHY_SOCKET_TCP_OUT_PENDING:
#if defined(_WIN32) || defined(_WIN64)
					if (FD_ISSET(s->sock,&efds)) {
						this->close((PhySocket *)&(*s),true);
					} else // ... if
#endif
					if (FD_ISSET(s->sock,&wfds)) {
						socklen_t slen = sizeof(ss);
						if (::getpeername(s->sock,(struct sockaddr *)&ss,&slen) != 0) {
							this->close((PhySocket *)&(*s),true);
						} else {
							s->type = ZT_PHY_SOCKET_TCP_OUT_CONNECTED;
							FD_SET(s->sock,&_readfds);
							FD_CLR(s->sock,&_writefds);
#if defined(_WIN32) || defined(_WIN64)
							FD_CLR(s->sock,&_exceptfds);
#endif
							try {
								_handler->phyOnTcpConnect((PhySocket *)&(*s),&(s->uptr),true);
							} catch ( ... ) {}
						}
					}
					break;

				case ZT_PHY_SOCKET_TCP_OUT_CONNECTED:
				case ZT_PHY_SOCKET_TCP_IN: {
					ZT_PHY_SOCKFD_TYPE sock = s->sock; // if closed, s->sock becomes invalid as s is no longer dereferencable
					if (FD_ISSET(sock,&rfds)) {
						long n = (long)::recv(sock,buf,sizeof(buf),0);
						if (n <= 0) {
							this->close((PhySocket *)&(*s),true);
						} else {
							try {
								_handler->phyOnTcpData((PhySocket *)&(*s),&(s->uptr),(void *)buf,(unsigned long)n);
							} catch ( ... ) {}
						}
					}
					if ((FD_ISSET(sock,&wfds))&&(FD_ISSET(sock,&_writefds))) {
						try {
							_handler->phyOnTcpWritable((PhySocket *)&(*s),&(s->uptr));
						} catch ( ... ) {}
					}
				}	break;

				case ZT_PHY_SOCKET_TCP_LISTEN:
					if (FD_ISSET(s->sock,&rfds)) {
						memset(&ss,0,sizeof(ss));
						socklen_t slen = sizeof(ss);
						ZT_PHY_SOCKFD_TYPE newSock = ::accept(s->sock,(struct sockaddr *)&ss,&slen);
						if (ZT_PHY_SOCKFD_VALID(newSock)) {
							if (_socks.size() >= ZT_PHY_MAX_SOCKETS) {
								ZT_PHY_CLOSE_SOCKET(newSock);
							} else {
#if defined(_WIN32) || defined(_WIN64)
								{ BOOL f = (_noDelay ? TRUE : FALSE); setsockopt(newSock,IPPROTO_TCP,TCP_NODELAY,(char *)&f,sizeof(f)); }
								{ u_long iMode=1; ioctlsocket(newSock,FIONBIO,&iMode); }
#else
								{ int f = (_noDelay ? 1 : 0); setsockopt(newSock,IPPROTO_TCP,TCP_NODELAY,(char *)&f,sizeof(f)); }
								fcntl(newSock,F_SETFL,O_NONBLOCK);
#endif
								_socks.push_back(PhySocketImpl());
								PhySocketImpl &sws = _socks.back();
								FD_SET(newSock,&_readfds);
								if ((long)newSock > _nfds)
									_nfds = (long)newSock;
								sws.type = ZT_PHY_SOCKET_TCP_IN;
								sws.sock = newSock;
								sws.uptr = (void *)0;
								memcpy(&(sws.saddr),&ss,sizeof(struct sockaddr_storage));
								try {
									_handler->phyOnTcpAccept((PhySocket *)&(*s),(PhySocket *)&(_socks.back()),&(s->uptr),&(sws.uptr),(const struct sockaddr *)&(sws.saddr));
								} catch ( ... ) {}
							}
						}
					}
					break;

				case ZT_PHY_SOCKET_UDP:
					if (FD_ISSET(s->sock, &rfds)) {
#if (defined(__linux__) || defined(linux) || defined(__linux)) && defined(MSG_WAITFORONE)
#define RECVMMSG_WINDOW_SIZE 128
#define RECVMMSG_BUF_SIZE	 1500
						iovec iovs[RECVMMSG_WINDOW_SIZE];
						uint8_t bufs[RECVMMSG_WINDOW_SIZE][RECVMMSG_BUF_SIZE];
						sockaddr_storage addrs[RECVMMSG_WINDOW_SIZE];
						memset(addrs, 0, sizeof(addrs));
						mmsghdr mm[RECVMMSG_WINDOW_SIZE];
						memset(mm, 0, sizeof(mm));
						for (int i = 0; i < RECVMMSG_WINDOW_SIZE; ++i) {
							iovs[i].iov_base = (void*)bufs[i];
							iovs[i].iov_len = RECVMMSG_BUF_SIZE;
							mm[i].msg_hdr.msg_name = (void*)&(addrs[i]);
							mm[i].msg_hdr.msg_iov = &(iovs[i]);
							mm[i].msg_hdr.msg_iovlen = 1;
						}
						for (int k = 0; k < 1024; ++k) {
							for (int i = 0; i < RECVMMSG_WINDOW_SIZE; ++i) {
								mm[i].msg_hdr.msg_namelen = sizeof(sockaddr_storage);
								mm[i].msg_len = 0;
							}
							int received_count = recvmmsg(s->sock, mm, RECVMMSG_WINDOW_SIZE, MSG_WAITFORONE, nullptr);
							if (received_count > 0) {
								for (int i = 0; i < received_count; ++i) {
									long n = (long)mm[i].msg_len;
									if (n > 0) {
										try {
											_handler->phyOnDatagram((PhySocket*)&(*s), &(s->uptr), (const struct sockaddr*)&(s->saddr), (const struct sockaddr*)&(addrs[i]), bufs[i], (unsigned long)n);
										}
										catch (...) {
										}
									}
								}
							}
							else {
								break;
							}
						}
#else
						for (int k = 0; k < 1024; ++k) {
							memset(&ss, 0, sizeof(ss));
							socklen_t slen = sizeof(ss);
							long n = (long)::recvfrom(s->sock, buf, sizeof(buf), 0, (struct sockaddr*)&ss, &slen);
							if (n > 0) {
								try {
									_handler->phyOnDatagram((PhySocket*)&(*s), &(s->uptr), (const struct sockaddr*)&(s->saddr), (const struct sockaddr*)&ss, (void*)buf, (unsigned long)n);
								}
								catch (...) {
								}
							}
							else if (n < 0)
								break;
						}
#endif
					}
					break;

				case ZT_PHY_SOCKET_UNIX_IN: {
#ifdef __UNIX_LIKE__
					ZT_PHY_SOCKFD_TYPE sock = s->sock; // if closed, s->sock becomes invalid as s is no longer dereferencable
					if ((FD_ISSET(sock,&wfds))&&(FD_ISSET(sock,&_writefds))) {
						try {
							_handler->phyOnUnixWritable((PhySocket *)&(*s),&(s->uptr));
						} catch ( ... ) {}
					}
					if (FD_ISSET(sock,&rfds)) {
						long n = (long)::read(sock,buf,sizeof(buf));
						if (n <= 0) {
							this->close((PhySocket *)&(*s),true);
						} else {
							try {
								_handler->phyOnUnixData((PhySocket *)&(*s),&(s->uptr),(void *)buf,(unsigned long)n);
							} catch ( ... ) {}
						}
					}
#endif // __UNIX_LIKE__
				}	break;

				case ZT_PHY_SOCKET_UNIX_LISTEN:
#ifdef __UNIX_LIKE__
					if (FD_ISSET(s->sock,&rfds)) {
						memset(&ss,0,sizeof(ss));
						socklen_t slen = sizeof(ss);
						ZT_PHY_SOCKFD_TYPE newSock = ::accept(s->sock,(struct sockaddr *)&ss,&slen);
						if (ZT_PHY_SOCKFD_VALID(newSock)) {
							if (_socks.size() >= ZT_PHY_MAX_SOCKETS) {
								ZT_PHY_CLOSE_SOCKET(newSock);
							} else {
								fcntl(newSock,F_SETFL,O_NONBLOCK);
								_socks.push_back(PhySocketImpl());
								PhySocketImpl &sws = _socks.back();
								FD_SET(newSock,&_readfds);
								if ((long)newSock > _nfds)
									_nfds = (long)newSock;
								sws.type = ZT_PHY_SOCKET_UNIX_IN;
								sws.sock = newSock;
								sws.uptr = (void *)0;
								memcpy(&(sws.saddr),&ss,sizeof(struct sockaddr_storage));
								try {
									//_handler->phyOnUnixAccept((PhySocket *)&(*s),(PhySocket *)&(_socks.back()),&(s->uptr),&(sws.uptr));
								} catch ( ... ) {}
							}
						}
					}
#endif // __UNIX_LIKE__
					break;

				case ZT_PHY_SOCKET_FD: {
					ZT_PHY_SOCKFD_TYPE sock = s->sock;
					const bool readable = ((FD_ISSET(sock,&rfds))&&(FD_ISSET(sock,&_readfds)));
					const bool writable = ((FD_ISSET(sock,&wfds))&&(FD_ISSET(sock,&_writefds)));
					if ((readable)||(writable)) {
						try {
							//_handler->phyOnFileDescriptorActivity((PhySocket *)&(*s),&(s->uptr),readable,writable);
						} catch ( ... ) {}
					}
				}	break;

				default:
					break;

			}

			if (s->type == ZT_PHY_SOCKET_CLOSED)
				_socks.erase(s++);
			else ++s;
		}
	}

	/**
	 * @param sock Socket to close
	 * @param callHandlers If true, call handlers for TCP connect (success: false) or close (default: true)
	 */
	inline void close(PhySocket *sock,bool callHandlers = true)
	{
		if (!sock)
			return;
		PhySocketImpl &sws = *(reinterpret_cast<PhySocketImpl *>(sock));
		if (sws.type == ZT_PHY_SOCKET_CLOSED)
			return;

		FD_CLR(sws.sock,&_readfds);
		FD_CLR(sws.sock,&_writefds);
#if defined(_WIN32) || defined(_WIN64)
		FD_CLR(sws.sock,&_exceptfds);
#endif

		if (sws.type != ZT_PHY_SOCKET_FD)
			ZT_PHY_CLOSE_SOCKET(sws.sock);

#ifdef __UNIX_LIKE__
		if (sws.type == ZT_PHY_SOCKET_UNIX_LISTEN)
			::unlink(((struct sockaddr_un *)(&(sws.saddr)))->sun_path);
#endif // __UNIX_LIKE__

		if (callHandlers) {
			switch(sws.type) {
				case ZT_PHY_SOCKET_TCP_OUT_PENDING:
					try {
						_handler->phyOnTcpConnect(sock,&(sws.uptr),false);
					} catch ( ... ) {}
					break;
				case ZT_PHY_SOCKET_TCP_OUT_CONNECTED:
				case ZT_PHY_SOCKET_TCP_IN:
					try {
						_handler->phyOnTcpClose(sock,&(sws.uptr));
					} catch ( ... ) {}
					break;
				case ZT_PHY_SOCKET_UNIX_IN:
#ifdef __UNIX_LIKE__
					try {
						_handler->phyOnUnixClose(sock,&(sws.uptr));
					} catch ( ... ) {}
#endif // __UNIX_LIKE__
					break;
				default:
					break;
			}
		}

		// Causes entry to be deleted from list in poll(), ignored elsewhere
		sws.type = ZT_PHY_SOCKET_CLOSED;

		if ((long)sws.sock >= (long)_nfds) {
			long nfds = (long)_whackSendSocket;
			if ((long)_whackReceiveSocket > nfds)
				nfds = (long)_whackReceiveSocket;
			for(typename std::list<PhySocketImpl>::iterator s(_socks.begin());s!=_socks.end();++s) {
				if ((s->type != ZT_PHY_SOCKET_CLOSED)&&((long)s->sock > nfds))
					nfds = (long)s->sock;
			}
			_nfds = nfds;
		}
	}
};

} // namespace ZeroTier

#endif
