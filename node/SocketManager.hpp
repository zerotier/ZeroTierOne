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

#ifndef ZT_SOCKETMANAGER_HPP
#define ZT_SOCKETMANAGER_HPP

#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <stdexcept>

#include "Constants.hpp"
#include "SharedPtr.hpp"
#include "InetAddress.hpp"
#include "Socket.hpp"
#include "Mutex.hpp"
#include "NonCopyable.hpp"
#include "Buffer.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#endif

namespace ZeroTier {

/**
 * Socket I/O multiplexer
 *
 * This wraps select(), epoll(), etc. and handles creation of Sockets.
 */
class SocketManager : NonCopyable
{
	friend class Socket;
	friend class UdpSocket;
	friend class TcpSocket;

public:
	/**
	 * @param localUdpPort Local UDP port to bind or 0 for no UDP support
	 * @param localTcpPort Local TCP port to listen to or 0 for no incoming TCP connect support
	 * @param packetHandler Function to call when packets are received by a socket
	 * @param arg Second argument to packetHandler()
	 * @throws std::runtime_error Could not bind local port(s) or open socket(s)
	 */
	SocketManager(
		int localUdpPort,
		int localTcpPort,
		void (*packetHandler)(const SharedPtr<Socket> &,void *,const InetAddress &,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> &),
		void *arg);

	~SocketManager();

	/**
	 * Send a message to a remote peer
	 *
	 * @param to Destination address
	 * @param tcp Use TCP?
	 * @param autoConnectTcp If true, automatically initiate TCP connection if there is none
	 * @param msg Message to send
	 * @param msglen Length of message
	 */
	bool send(const InetAddress &to,bool tcp,bool autoConnectTcp,const void *msg,unsigned int msglen);

	/**
	 * Send a message to a remote peer via UDP (shortcut for setting both TCP params to false in send)
	 *
	 * @param to Destination address
	 * @param msg Message to send
	 * @param msglen Length of message
	 */
	inline bool sendUdp(const InetAddress &to,const void *msg,unsigned int msglen) { return send(to,false,false,msg,msglen); }

	/**
	 * Send a UDP packet with a limited IP TTL
	 *
	 * @param to Destination address
	 * @param hopLimit IP TTL
	 */
	bool sendFirewallOpener(const InetAddress &to,int hopLimit);

	/**
	 * Perform I/O polling operation (e.g. select())
	 *
	 * If called concurrently, one will block until the other completes.
	 *
	 * @param timeout Timeout in milliseconds, may return sooner if whack() is called
	 */
	void poll(unsigned long timeout);

	/**
	 * Cause current or next blocking poll() operation to timeout immediately
	 */
	void whack();

	/**
	 * Close TCP sockets
	 */
	void closeTcpSockets();

private:
	// Called by socket implementations when a packet is received
	inline void handleReceivedPacket(const SharedPtr<Socket> &sock,const InetAddress &from,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> &data)
		throw()
	{
		try {
			_packetHandler(sock,_arg,from,data);
		} catch ( ... ) {} // handlers shouldn't throw
	}

	// Used by TcpSocket to register/unregister for write availability notification
	inline void startNotifyWrite(const Socket *sock)
		throw()
	{
		_fdSetLock.lock();
		FD_SET(sock->_sock,&_writefds);
		_fdSetLock.unlock();
	}
	inline void stopNotifyWrite(const Socket *sock)
		throw()
	{
		_fdSetLock.lock();
		FD_CLR(sock->_sock,&_writefds);
		_fdSetLock.unlock();
	}

	// Called in SocketManager destructor or in constructor cleanup before exception throwing
	void _closeSockets()
		throw();

	// Called in SocketManager to recompute _nfds for select() based implementation
	void _updateNfds();

#ifdef __WINDOWS__
	SOCKET _whackSendPipe;
	SOCKET _whackReceivePipe;
	SOCKET _tcpV4ListenSocket;
	SOCKET _tcpV6ListenSocket;
#else
	int _whackSendPipe;
	int _whackReceivePipe;
	int _tcpV4ListenSocket;
	int _tcpV6ListenSocket;
#endif
	Mutex _whackSendPipe_m;

	SharedPtr<Socket> _udpV4Socket;
	SharedPtr<Socket> _udpV6Socket;

	fd_set _readfds;
	fd_set _writefds;
	volatile int _nfds;
	Mutex _fdSetLock;

	std::map< InetAddress,SharedPtr<Socket> > _tcpSockets;
	Mutex _tcpSockets_m;

	void (*_packetHandler)(const SharedPtr<Socket> &,void *,const InetAddress &,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> &);
	void *_arg;

	Mutex _pollLock;
};

} // namespace ZeroTier

#endif
