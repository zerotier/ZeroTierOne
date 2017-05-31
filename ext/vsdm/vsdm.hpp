/*
 * VSDM: Very Simple Distributed Map
 *
 * (c)2017 ZeroTier, Inc.
 * Written by Adam Ierymenko <adam.ierymenko@zerotier.com>
 * License: MIT
 */

#ifndef ZT_VSDM_HPP__
#define ZT_VSDM_HPP__

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

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

#define ZT_PHY_SOCKFD_TYPE int
#define ZT_PHY_SOCKFD_NULL (-1)
#define ZT_PHY_SOCKFD_VALID(s) ((s) > -1)
#define ZT_PHY_CLOSE_SOCKET(s) ::close(s)
#define ZT_PHY_MAX_SOCKETS (FD_SETSIZE)
#define ZT_PHY_MAX_INTERCEPTS ZT_PHY_MAX_SOCKETS
#define ZT_PHY_SOCKADDR_STORAGE_TYPE struct sockaddr_storage

#endif

#include <string>
#include <queue>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <list>
#include <stdexcept>
#include <vector>
#include <functional>

/*********************************************************************************************************/

namespace ztVsdmInternal {

/* This is the Phy<> adapter implementation for selected sockets from ZeroTier One.
 * It should build and run out of the box on Windows and most *nix systems. Parts
 * not used by VSDM have been removed. */

typedef void PhySocket;

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

	struct PhySocketImpl
	{
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
	static inline ZT_PHY_SOCKFD_TYPE getDescriptor(PhySocket *s) throw() { return reinterpret_cast<PhySocketImpl *>(s)->sock; }

	/**
	 * @param s Socket object
	 * @return Pointer to user object
	 */
	static inline void** getuptr(PhySocket *s) throw() { return &(reinterpret_cast<PhySocketImpl *>(s)->uptr); }

	/**
	 * Cause poll() to stop waiting immediately
	 *
	 * This can be used to reset the polling loop after changes that require
	 * attention, or to shut down a background thread that is waiting, etc.
	 */
	inline void whack()
	{
#if defined(_WIN32) || defined(_WIN64)
		::send(_whackSendSocket,(const char *)this,1,0);
#else
		(void)(::write(_whackSendSocket,(PhySocket *)this,1));
#endif
	}

	/**
	 * @return Number of open sockets
	 */
	inline unsigned long count() const throw() { return _socks.size(); }

	/**
	 * @return Maximum number of sockets allowed
	 */
	inline unsigned long maxCount() const throw() { return ZT_PHY_MAX_SOCKETS; }

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
	inline const void setNotifyWritable(PhySocket *sock,bool notifyWritable)
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
	inline const void setNotifyReadable(PhySocket *sock,bool notifyReadable)
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
					if (FD_ISSET(s->sock,&rfds)) {
						for(;;) {
							memset(&ss,0,sizeof(ss));
							socklen_t slen = sizeof(ss);
							long n = (long)::recvfrom(s->sock,buf,sizeof(buf),0,(struct sockaddr *)&ss,&slen);
							if (n > 0) {
								try {
									_handler->phyOnDatagram((PhySocket *)&(*s),&(s->uptr),(const struct sockaddr *)&(s->saddr),(const struct sockaddr *)&ss,(void *)buf,(unsigned long)n);
								} catch ( ... ) {}
							} else if (n < 0)
								break;
						}
					}
					break;

				case ZT_PHY_SOCKET_UNIX_IN: {
#ifdef __UNIX_LIKE__
					ZT_PHY_SOCKFD_TYPE sock = s->sock; // if closed, s->sock becomes invalid as s is no longer dereferencable
					if ((FD_ISSET(sock,&wfds))&&(FD_ISSET(sock,&_writefds))) {
						try {
							_handler->phyOnUnixWritable((PhySocket *)&(*s),&(s->uptr),false);
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

static inline uint64_t _swap64(const uint64_t n)
{
	return (
		((n & 0x00000000000000FFULL) << 56) |
		((n & 0x000000000000FF00ULL) << 40) |
		((n & 0x0000000000FF0000ULL) << 24) |
		((n & 0x00000000FF000000ULL) <<  8) |
		((n & 0x000000FF00000000ULL) >>  8) |
		((n & 0x0000FF0000000000ULL) >> 24) |
		((n & 0x00FF000000000000ULL) >> 40) |
		((n & 0xFF00000000000000ULL) >> 56)
	);
}

} // namespace ztVsdmInternal

/*********************************************************************************************************/

/**
 * No-op update watcher
 */
class vsdm_watcher_noop
{
public:
	template<typename K,typename V>
	inline void add(uint64_t,const K &k,const V &v,uint64_t) {}
	template<typename K,typename V>
	inline void update(uint64_t,const K &k,const V &v,uint64_t) {}
	template<typename K>
	inline void del(uint64_t,const K &k) {}
};

/**
 * No-op cryptor that adds no overhead and does no encryption
 */
class vsdm_cryptor_noop
{
public:
	static inline unsigned long overhead() { return 0; }
	inline void encrypt(void *d,unsigned long l) {}
	inline bool decrypt(void *d,unsigned long l) { return true; }
};

/**
 * Default serializer supporting std::string and stdint.h types
 */
class vsdm_default_serializer
{
public:
	static inline unsigned long objectSize(const std::string &o) { return o.length(); }
	static inline unsigned long objectSize(const uint8_t o) { return 1; }
	static inline unsigned long objectSize(const int8_t o) { return 1; }
	static inline unsigned long objectSize(const uint16_t o) { return 2; }
	static inline unsigned long objectSize(const int16_t o) { return 2; }
	static inline unsigned long objectSize(const uint32_t o) { return 4; }
	static inline unsigned long objectSize(const int32_t o) { return 4; }
	static inline unsigned long objectSize(const uint64_t o) { return 8; }
	static inline unsigned long objectSize(const int64_t o) { return 8; }

	static inline const char *objectData(const std::string &o) { return o.data(); }
	static inline const char *objectData(const uint8_t &o) { return reinterpret_cast<const char *>(&o); }
	static inline const char *objectData(const int8_t &o) { return reinterpret_cast<const char *>(&o); }
	static inline const char *objectData(const uint16_t &o) { return reinterpret_cast<const char *>(&o); }
	static inline const char *objectData(const int16_t &o) { return reinterpret_cast<const char *>(&o); }
	static inline const char *objectData(const uint32_t &o) { return reinterpret_cast<const char *>(&o); }
	static inline const char *objectData(const int32_t &o) { return reinterpret_cast<const char *>(&o); }
	static inline const char *objectData(const uint64_t &o) { return reinterpret_cast<const char *>(&o); }
	static inline const char *objectData(const int64_t &o) { return reinterpret_cast<const char *>(&o); }

	static inline bool objectDeserialize(const char *d,unsigned long l,std::string &o) { o.assign(d,l); return true; }
	static inline bool objectDeserialize(const char *d,unsigned long l,uint8_t &o) { if (l == 1) { memcpy(&o,d,1); return true; } else { return false; } }
	static inline bool objectDeserialize(const char *d,unsigned long l,int8_t &o) { if (l == 1) { memcpy(&o,d,1); return true; } else { return false; } }
	static inline bool objectDeserialize(const char *d,unsigned long l,uint16_t &o) { if (l == 2) { memcpy(&o,d,2); return true; } else { return false; } }
	static inline bool objectDeserialize(const char *d,unsigned long l,int16_t &o) { if (l == 2) { memcpy(&o,d,2); return true; } else { return false; } }
	static inline bool objectDeserialize(const char *d,unsigned long l,uint32_t &o) { if (l == 4) { memcpy(&o,d,4); return true; } else { return false; } }
	static inline bool objectDeserialize(const char *d,unsigned long l,int32_t &o) { if (l == 4) { memcpy(&o,d,4); return true; } else { return false; } }
	static inline bool objectDeserialize(const char *d,unsigned long l,uint64_t &o) { if (l == 8) { memcpy(&o,d,8); return true; } else { return false; } }
	static inline bool objectDeserialize(const char *d,unsigned long l,int64_t &o) { if (l == 8) { memcpy(&o,d,8); return true; } else { return false; } }
};

/**
 * VSDM: Very Simple Distributed Map
 *
 * See README.md for full docs.
 *
 * @tparam K Key type (must be supported by serializer)
 * @tparam V Value type (must be supported by serializer)
 * @tparam L Maximum message length (max allowed: UINT32_MAX - 1, default: 131072)
 * @tparam W Watcher function (default: vsdm_watcher_noop)
 * @tparam S Serializer class with static methods to serialize keys and values (default: vsdm_default_serializer)
 * @tparam C Cryptor to encrypt/decrypt and authenticate network traffic (default: vsdm_cryptor_noop)
 * @tparam M Map type for underlying data store (default: std::unordered_map)
 */
template<
	typename K,
	typename V,
	unsigned long L = 131072,
	typename W = vsdm_watcher_noop,
	typename S = vsdm_default_serializer,
	typename C = vsdm_cryptor_noop,
	template<typename,typename...> class M = std::unordered_map
>
class vsdm
{
	friend void vsdm_thread_main(void *parent);
	friend class ztVsdmInternal::Phy<vsdm *>;

private:
	struct vsdm_entry
	{
		vsdm_entry() : rev(0),deletedAt(0),v() {}
		uint64_t rev;
		uint64_t deletedAt;
		V v;
	};

	struct _connection
	{
		_connection() : outbuf(),inbuf(),gotHello(false),node(0),sock((ztVsdmInternal::PhySocket *)0) {}
		std::string outbuf;
		std::string inbuf;
		bool gotHello;
		uint64_t node;
		ztVsdmInternal::PhySocket *sock;
	};

public:
	typedef K key_type;
	typedef V value_type;

	/**
	 * @param id Cluster ID, must be the same on all nodes
	 * @param node Arbitrary unique node ID
	 * @param restrictInbound If true, restrict inbound connections to known peer IPs (added via link())
	 * @param cryptor Encryptor/decryptor instance (default: C())
	 * @param watcher Watcher function instance (default: W())
	 */
	vsdm(uint64_t id,uint64_t node,bool restrictInbound,const C &cryptor = C(),const W &watcher = W()) :
		_node(node),
		_id(id),
		_rev(0),
		_connections(),
		_m(),
		_lock(),
		_phy(this,false,false),
		_cryptor(cryptor),
		_watcher(watcher),
		_run(true),
		_restrictInbound(restrictInbound),
		_t(_threadMain,reinterpret_cast<void *>(this))
	{
	}

	~vsdm()
	{
		_run = false;
		_phy.whack();
		_t.join();
	}

	/**
	 * @param k Key to set
	 * @param v New value for key
	 * @return Revision of entry in map
	 */
	inline uint64_t set(const K &k,const V &v)
	{
		std::lock_guard<std::mutex> l(_lock);

		vsdm_entry &e = _m[k];
		e.rev = ++_rev;
		e.deletedAt = 0;
		e.v = v;

		std::vector<uint64_t> sentToNodes;
		for(typename std::unordered_map<ztVsdmInternal::PhySocket *,_connection>::iterator c2(_connections.begin());c2!=_connections.end();++c2) {
			if ((c2->second.gotHello)&&(std::find(sentToNodes.begin(),sentToNodes.end(),c2->second.node) == sentToNodes.end())) {
				sendUpdate(c2->second,k,e);
				sentToNodes.push_back(c2->second.node);
#ifdef VSDM_DEBUG
				fprintf(stderr,">> %lu: %s=%s\n",(unsigned long)c2->second.node,k.c_str(),v.c_str()); fflush(stderr);
#endif
			}
		}
		_phy.whack();

		return _rev;
	}

	/**
	 * @param k Key to check
	 * @return Revision of key that we have or 0 if not found
	 */
	inline uint64_t have(const K &k) const
	{
		std::lock_guard<std::mutex> l(_lock);
		typename std::unordered_map<std::string,vsdm_entry>::const_iterator i(_m.find(k));
		if ((i == _m.end())||(i->second.deletedAt))
			return 0;
		return i->second.rev;
	}

	/**
	 * @param k Key to get
	 * @param dfl Default value if key is not found
	 * @param have If non-NULL, set to revision of this key or 0 if not found
	 * @return Key value or dfl if not found
	 */
	inline V get(const K &k,const V &dfl = V(),uint64_t *have = (uint64_t *)0) const
	{
		std::lock_guard<std::mutex> l(_lock);
		typename std::unordered_map<std::string,vsdm_entry>::const_iterator i(_m.find(k));
		if ((i == _m.end())||(i->second.deletedAt)) {
			if (have)
				*have = 0;
			return dfl;
		}
		if (have)
			*have = i->second.rev;
		return i->second.v;
	}

	/**
	 * @param k Key to get
	 * @param have If non-NULL, set to revision of this key or 0 if not found
	 * @return Key's value or default/empty V() if not found
	 */
	inline V get(const K &k,uint64_t *have) const
	{
		return get(k,V(),have);
	}

	/**
	 * Erase a key
	 *
	 * Erased entries are not wholly purged from memory immediately. They
	 * are marked as erased and purged after sufficient time for propagation.
	 *
	 * @param k Key to erase
	 * @return Previous revision of this key in map or 0 if not found
	 */
	inline bool del(const K &k)
	{
		uint64_t prev = 0;
		std::lock_guard<std::mutex> l(_lock);

		typename std::unordered_map<std::string,vsdm_entry>::iterator i(_m.find(k));
		if (i == _m.end())
			return 0;
		prev = i->second.rev;
		i->second.rev = ++_rev;
		i->second.deletedAt = _rev;
		i->second.v.clear();

		std::vector<uint64_t> sentToNodes;
		for(typename std::unordered_map<ztVsdmInternal::PhySocket *,_connection>::iterator c2(_connections.begin());c2!=_connections.end();++c2) {
			if ((c2->second.gotHello)&&(std::find(sentToNodes.begin(),sentToNodes.end(),c2->second.node) == sentToNodes.end())) {
				sendUpdate(c2->second,k,i->second);
				sentToNodes.push_back(c2->second.node);
#ifdef VSDM_DEBUG
				fprintf(stderr,">> %lu: %s=<DEL>\n",(unsigned long)c2->second.node,k.c_str()); fflush(stderr);
#endif
			}
		}
		_phy.whack();

		return prev;
	}

	/**
	 * Listen for incoming node connections on an address
	 *
	 * This can be called more than once to listen on more than one address and port.
	 *
	 * @param sa Socket address
	 * @return True if bind succeeded
	 */
	inline bool listen(const struct sockaddr *sa)
	{
		std::lock_guard<std::mutex> l(_lock);
		return (_phy.tcpListen(sa) != (ztVsdmInternal::PhySocket *)0);
	}

	/**
	 * Add a remote node endpoint
	 *
	 * This can be called for an arbitrary number of other endpoints in the
	 * network to tell this node to attempt to maintain a link to them.
	 *
	 * @param node Node ID of remote
	 * @param sa Socket address of remote
	 * @param salen Length of socket address structure
	 */
	inline void link(uint64_t node,const struct sockaddr_in *sa,unsigned int salen)
	{
		std::lock_guard<std::mutex> l(_lock);
		if ((node != _node)&&(salen <= sizeof(struct sockaddr_storage)))
			memcpy(&(_peers[node]),sa,salen);
	}

	/**
	 * @return Node IDs of nodes that are currently connected
	 */
	inline std::vector<uint64_t> who() const
	{
		std::vector<uint64_t> w;
		std::lock_guard<std::mutex> l(_lock);
		for(typename std::unordered_map<ztVsdmInternal::PhySocket *,_connection>::const_iterator i(_connections.begin());i!=_connections.end();++i) {
			if ((i->gotHello)&&(std::find(w.begin(),w.end(),i->second.node) == w.end()))
				w.push_back(i->second.node);
		}
		return w;
	}

	/**
	 * @return True if we are currently connected to at least one other node
	 */
	inline bool connected() const
	{
		std::lock_guard<std::mutex> l(_lock);
		for(typename std::unordered_map<ztVsdmInternal::PhySocket *,_connection>::const_iterator i(_connections.begin());i!=_connections.end();++i) {
			if (i->gotHello)
				return true;
		}
		return false;
	}

	/**
	 * Iterate through all members of this map, with optional deletion
	 *
	 * The function is executed against all key/value pairs and returns a signed integer.
	 * A negative return value causes the entry to be deleted, while a positive return
	 * value means the key's value (which is passed into the function as a reference) has
	 * been modified and should be replicated. A return value of zero means no change.
	 *
	 * Other methods should not be called since doing so can result in a deadlock.
	 *
	 * @param func Function to execute against all members of map, returns integer (see description)
	 */
	template<typename F>
	inline void each(F func)
	{
		std::vector<uint64_t> sentToNodes;
		bool whack = false;
		std::lock_guard<std::mutex> l(_lock);
		for(typename M<K,vsdm_entry>::iterator i(_m.begin());i!=_m.end();++i) {
			if (!i->second.deletedAt) {
				try {
					const int result = func(i->first,i->second.v);
					if (result < 0) {
						i->second.rev = ++_rev;
						i->second.deletedAt = _rev;
						i->second.v.clear();
					} else if (result > 0) {
						i->second.rev = ++_rev;
						i->second.deletedAt = 0;
						// v will have been modified in place
					}
					if (result != 0) {
						sentToNodes.clear();
						for(typename std::unordered_map<ztVsdmInternal::PhySocket *,_connection>::iterator c2(_connections.begin());c2!=_connections.end();++c2) {
							if ((c2->second.gotHello)&&(std::find(sentToNodes.begin(),sentToNodes.end(),c2->second.node) == sentToNodes.end())) {
								sendUpdate(c2->second,i->first,i->second);
								sentToNodes.push_back(c2->second.node);
							}
						}
						whack = true;
					}
				} catch ( ... ) {}
			}
		}
		if (whack)
			_phy.whack();
	}

private:
	inline vsdm &operator=(const vsdm &v) { return *this; }

	static void _threadMain(void *p) { reinterpret_cast<vsdm *>(p)->threadMain(); }
	inline void threadMain()
	{
		std::vector<uint64_t> haveNodes;
		time_t lastcheck = 0;
		time_t lastclean = 0;
		while (_run) {
			_phy.poll(1000);

			time_t now = time(0);

			// Check connections with other nodes and try to establish them
			// if they're not present.
			if ((now - lastcheck) >= 2) {
				lastcheck = now;
				haveNodes.clear();

				std::lock_guard<std::mutex> l(_lock);

				for(typename std::unordered_map<ztVsdmInternal::PhySocket *,_connection>::const_iterator c(_connections.begin());c!=_connections.end();++c) {
					if (std::find(haveNodes.begin(),haveNodes.end(),c->second.node) == haveNodes.end())
						haveNodes.push_back(c->second.node);
				}

				for(std::unordered_map<uint64_t,struct sockaddr_storage>::iterator p(_peers.begin());p!=_peers.end();++p) {
					if (std::find(haveNodes.begin(),haveNodes.end(),p->first) == haveNodes.end()) {
						bool connected = false;
						ztVsdmInternal::PhySocket *ns = _phy.tcpConnect((const struct sockaddr *)&(p->second),connected,(void *)0,true);
						if (ns) {
							_connection &c = _connections[ns];
							c.gotHello = false;
							c.node = p->first;
							c.sock = ns;
						}
					}
				}
			}

			// Forget deleted entries if they've had ample time to propagate
			if ((now - lastclean) >= 120) {
				lastclean = now;
				uint64_t delHorizon = _m.size() * (2 + _peers.size());
				if (_rev > delHorizon) {
					delHorizon -= _rev;
					std::lock_guard<std::mutex> l(_lock);
					for(typename M<K,vsdm_entry>::iterator i(_m.begin());i!=_m.end();) {
						if ((i->second.deletedAt > 0)&&(i->second.deletedAt < delHorizon))
							_m.erase(i++);
						else ++i;
					}
				}
			}
		}
	}

	inline void sendUpdate(_connection &c,const std::string &k,const vsdm_entry &e)
	{
		// assumes lock is locked
		const uint32_t ks = (uint32_t)S::objectSize(k);
		uint32_t vs = 0;
		uint32_t hdr[4];
		hdr[0] = htonl((uint32_t)((e.rev >> 32) & 0xffffffff));
		hdr[1] = htonl((uint32_t)(e.rev & 0xffffffff));
		hdr[2] = htonl(ks);
		if (e.deletedAt) {
			hdr[3] = 0xffffffff;
		} else {
			vs = (uint32_t)S::objectSize(e.v);
			hdr[3] = htonl(vs);
		}

		const uint32_t s = htonl((uint32_t)(16 + C::overhead() + ks + vs));
		c.outbuf.append((const char *)&s,4);

		const unsigned long start = (unsigned long)c.outbuf.length();
		c.outbuf.append((const char *)hdr,16);
		c.outbuf.append(S::objectData(k),ks);
		if (!e.deletedAt)
			c.outbuf.append(S::objectData(e.v),vs);
		c.outbuf.append(C::overhead(),(char)0);
		const unsigned long end = (unsigned long)c.outbuf.length();

		_cryptor.encrypt(reinterpret_cast<void *>(const_cast<char *>(c.outbuf.data()) + start),end - start);

		_phy.setNotifyWritable(c.sock,true);
	}

	inline void sendUpdateToAll(ztVsdmInternal::PhySocket *receivedOnSock,const uint64_t receivedFromNode,const std::string &k,const vsdm_entry &e)
	{
		// assumes lock is locked
		std::vector<uint64_t> sentToNodes;
		for(typename std::unordered_map<ztVsdmInternal::PhySocket *,_connection>::iterator c2(_connections.begin());c2!=_connections.end();++c2) {
			if ((c2->first != receivedOnSock)&&(c2->second.gotHello)&&(c2->second.node != receivedFromNode)&&(std::find(sentToNodes.begin(),sentToNodes.end(),c2->second.node) == sentToNodes.end())) {
				sendUpdate(c2->second,k,e);
				sentToNodes.push_back(c2->second.node);
#ifdef VSDM_DEBUG
				fprintf(stderr,">> %lu: %s=%s\n",(unsigned long)c2->second.node,k.c_str(),(e.deletedAt) ? "<DEL>" : e.v.c_str()); fflush(stderr);
#endif
			}
		}
		_phy.whack();
	}

	inline void sendHello(ztVsdmInternal::PhySocket *sock)
	{
		uint64_t hdr[3];
		if (htonl(1) == 1) {
			hdr[0] = _node;
			hdr[1] = _id;
			hdr[2] = _rev;
		} else {
			hdr[0] = ztVsdmInternal::_swap64(_node);
			hdr[1] = ztVsdmInternal::_swap64(_id);
			hdr[2] = ztVsdmInternal::_swap64(_rev);
		}
		uint8_t tmp[24 + C::overhead()];
		memcpy(tmp,hdr,24);
		_cryptor.encrypt(reinterpret_cast<void *>(tmp),sizeof(tmp));
		_phy.streamSend(sock,reinterpret_cast<void *>(tmp),sizeof(tmp));
	}

	inline void phyOnTcpConnect(ztVsdmInternal::PhySocket *sock,void **uptr,bool success)
	{
		std::lock_guard<std::mutex> l(_lock);
		if (success) {
			_connection &c = _connections[sock];
			c.gotHello = false;
			c.sock = sock;
			*uptr = (void *)&c;
			sendHello(sock);
		} else {
			_connections.erase(sock);
		}
	}

	inline void phyOnTcpAccept(ztVsdmInternal::PhySocket *sockL,ztVsdmInternal::PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from)
	{
		std::lock_guard<std::mutex> l(_lock);

		if (_restrictInbound) {
			bool ok = false;
			for(typename std::unordered_map<uint64_t,struct sockaddr_storage>::const_iterator i(_peers.begin());i!=_peers.end();++i) {
				if (from->sa_family == i->second.ss_family) {
					if ( (from->sa_family == AF_INET) && (reinterpret_cast<const struct sockaddr_in *>(from)->sin_addr.s_addr == reinterpret_cast<const struct sockaddr_in *>(&(i->second))->sin_addr.s_addr) ) {
						ok = true;
						break;
					} else if ( (from->sa_family == AF_INET6) && (memcmp(reinterpret_cast<const struct sockaddr_in6 *>(from)->sin6_addr.s6_addr,reinterpret_cast<const struct sockaddr_in6 *>(&(i->second))->sin6_addr.s6_addr,16) == 0) )  {
						ok = true;
						break;
					}
				}
			}
			if (!ok) {
#ifdef VSDM_DEBUG
				fprintf(stderr," * dropped inbound connection: peer not from a known IP address\n"); fflush(stderr);
#endif
				_phy.close(sockN,false);
				return;
			}
		}

		_connection &c = _connections[sockN];
		c.gotHello = false;
		c.node = _node; // impossible value for a remote
		c.sock = sockN;
		*uptrN = (void *)&c;
		sendHello(sockN);
	}

	inline void phyOnTcpClose(ztVsdmInternal::PhySocket *sock,void **uptr)
	{
		std::lock_guard<std::mutex> l(_lock);
		_connections.erase(sock);
	}

	inline void phyOnTcpData(ztVsdmInternal::PhySocket *sock,void **uptr,void *data,unsigned long len)
	{
		_connection *const c = (_connection *)*uptr;
		if (!c) return;

		std::unique_lock<std::mutex> l(_lock);
		c->inbuf.append(reinterpret_cast<char *>(data),len);
		for(;;) {
			if (c->gotHello) {

				if (c->inbuf.length() >= 20) { // got message size and header
					uint32_t _totalLen;
					memcpy(&_totalLen,c->inbuf.data(),4);
					const unsigned long totalLen = ntohl(_totalLen);
					if ((totalLen > L)||(totalLen < 16)) { // message too small or too large
						_connections.erase(sock);
						_phy.close(sock,false);
						return;
					}

					if (c->inbuf.length() >= (4 + totalLen)) { // got full message

						if (!_cryptor.decrypt(reinterpret_cast<void *>(const_cast<char *>(c->inbuf.data()) + 4),totalLen)) {
							_connections.erase(sock);
							_phy.close(sock,false);
							return;
						}

						uint32_t hdr[4];
						memcpy(hdr,c->inbuf.data() + 4,16);

						const uint64_t objectRev = ((uint64_t)ntohl(hdr[0]) << 32) | (uint64_t)ntohl(hdr[1]);
						const unsigned long keyLen = (unsigned long)ntohl(hdr[2]);
						unsigned long valueLen = (unsigned long)ntohl(hdr[3]);

						if (objectRev > _rev)
							_rev = objectRev;

						uint64_t deletedAt = 0;
						if (valueLen == 0xffffffff) {
							valueLen = 0;
							deletedAt = _rev;
						}
						if ((keyLen + valueLen + 16 + C::overhead()) > totalLen) { // key and/or value length invalid
							_connections.erase(sock);
							_phy.close(sock,false);
							return;
						}

						K k;
						if (!S::objectDeserialize(c->inbuf.data() + 16 + 4,keyLen,k)) {
							_connections.erase(sock);
							_phy.close(sock,false);
							return;
						}

						vsdm_entry &e = _m[k];
						if (e.rev < objectRev) {
							const bool added = (e.rev == 0);
							e.rev = objectRev;
							e.deletedAt = deletedAt;
							if (e.deletedAt) {
								e.v = V();
							} else {
								if (!S::objectDeserialize(c->inbuf.data() + 16 + 4 + keyLen,valueLen,e.v)) {
									_connections.erase(sock);
									_phy.close(sock,false);
									return;
								}
							}
#ifdef VSDM_DEBUG
							fprintf(stderr,"<< %lu: %s=%s\n",(unsigned long)c->node,k.c_str(),(deletedAt) ? "<DEL>" : e.v.c_str()); fflush(stderr);
#endif
							sendUpdateToAll(sock,c->node,k,e);

							l.unlock();
							try {
								if (added) {
									_watcher.add(c->node,k,e.v,objectRev);
								} else if (deletedAt) {
									_watcher.del(c->node,k);
								} else {
									_watcher.update(c->node,k,e.v,objectRev);
								}
							} catch ( ... ) {}
							l.lock();
						}

						c->inbuf.erase(c->inbuf.begin(),c->inbuf.begin() + totalLen + 4);

						// continue and process more messages in queue, if any
					} else { // still waiting on full message
						break;
					}
				} else { // still waiting on message size and header
					break;
				}

			} else if (c->inbuf.length() >= (24 + C::overhead())) { // got hello header

				if (!_cryptor.decrypt(reinterpret_cast<void *>(const_cast<char *>(c->inbuf.data())),24 + C::overhead())) {
					_connections.erase(sock);
					_phy.close(sock,false);
					return;
				}

				uint64_t hdr[3];
				memcpy(hdr,c->inbuf.data(),24);
				c->inbuf.erase(c->inbuf.begin(),c->inbuf.begin() + 24 + C::overhead());

				if (htonl(1) != 1) {
					hdr[0] = ztVsdmInternal::_swap64(hdr[0]);
					hdr[1] = ztVsdmInternal::_swap64(hdr[1]);
					hdr[2] = ztVsdmInternal::_swap64(hdr[2]);
				}

				if ((hdr[0] == _node)||(hdr[1] != _id)) { // don't connect to self, and don't connect to other map IDs
					_connections.erase(sock);
					_phy.close(sock,false);
					break;
				} else {
					c->gotHello = true;
					c->node = hdr[0];

					if (hdr[2] > _rev)
						_rev = hdr[2];

					for(typename M<std::string,vsdm_entry>::const_iterator i(_m.begin());i!=_m.end();++i) {
						if (i->second.rev >= hdr[2]) {
							sendUpdate(*c,i->first,i->second);
#ifdef VSDM_DEBUG
							fprintf(stderr,">> %lu: %s=%s (new link)\n",(unsigned long)c->node,i->first.c_str(),i->second.v.c_str()); fflush(stderr);
#endif
						}
					}
					_phy.whack();
				}

				// continue and process more messages in queue, if any
			} else { // still waiting on hello header
				break;
			}
		}
	}

	inline void phyOnTcpWritable(ztVsdmInternal::PhySocket *sock,void **uptr)
	{
		std::lock_guard<std::mutex> l(_lock);
		_connection *c = (_connection *)*uptr;
		if (c) {
			if (c->outbuf.length() > 0) {
				long n = _phy.streamSend(sock,c->outbuf.data(),c->outbuf.length());
				if (n <= 0) {
					_connections.erase(sock);
					_phy.close(sock,false);
					return;
				} else if (n == (long)c->outbuf.length()) {
					c->outbuf.clear();
				} else {
					c->outbuf.erase(c->outbuf.begin(),c->outbuf.begin() + n);
				}
			}
			if (c->outbuf.length() == 0) {
				_phy.setNotifyWritable(c->sock,false);
			}
		}
	}

	inline void phyOnDatagram(ztVsdmInternal::PhySocket *sock,void **uptr,const struct sockaddr *localAddr,const struct sockaddr *from,void *data,unsigned long len) {}
	inline void phyOnFileDescriptorActivity(ztVsdmInternal::PhySocket *sock,void **uptr,bool readable,bool writable) {}
	inline void phyOnUnixAccept(ztVsdmInternal::PhySocket *sockL,ztVsdmInternal::PhySocket *sockN,void **uptrL,void **uptrN) {}
	inline void phyOnUnixClose(ztVsdmInternal::PhySocket *sock,void **uptr) {}
	inline void phyOnUnixData(ztVsdmInternal::PhySocket *sock,void **uptr,void *data,unsigned long len) {}
	inline void phyOnUnixWritable(ztVsdmInternal::PhySocket *sock,void **uptr) {}

	const uint64_t _node;
	const uint64_t _id;
	uint64_t _rev;
	std::unordered_map<uint64_t,struct sockaddr_storage> _peers;
	std::unordered_map<ztVsdmInternal::PhySocket *,_connection> _connections;
	M<K,vsdm_entry> _m;
	mutable std::mutex _lock;
	ztVsdmInternal::Phy<vsdm *> _phy;
	C _cryptor;
	W _watcher;
	volatile bool _run;
	bool _restrictInbound;
	std::thread _t;
};

#endif
