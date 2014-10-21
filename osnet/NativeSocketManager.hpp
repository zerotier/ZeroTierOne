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

#ifndef ZT_NATIVESOCKETMANAGER_HPP
#define ZT_NATIVESOCKETMANAGER_HPP

#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <stdexcept>

#include "../node/Constants.hpp"
#include "../node/SharedPtr.hpp"
#include "../node/Mutex.hpp"
#include "../node/SocketManager.hpp"
#include "../node/Socket.hpp"

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

class NativeSocket;
class NativeUdpSocket;
class NativeTcpSocket;

/**
 * Native socket manager for Unix and Windows
 */
class NativeSocketManager : public SocketManager
{
	friend class NativeUdpSocket;
	friend class NativeTcpSocket;

public:
	NativeSocketManager(int localUdpPort,int localTcpPort);
	virtual ~NativeSocketManager();

	virtual bool send(const InetAddress &to,bool tcp,bool autoConnectTcp,const void *msg,unsigned int msglen);
	virtual void poll(unsigned long timeout,void (*handler)(const SharedPtr<Socket> &,void *,const InetAddress &,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> &),void *arg);
	virtual void whack();
	virtual void closeTcpSockets();

private:
	// Used by TcpSocket to register/unregister for write availability notification
	void _startNotifyWrite(const NativeSocket *sock);
	void _stopNotifyWrite(const NativeSocket *sock);

	// Called in SocketManager destructor or in constructor cleanup before exception throwing
	void _closeSockets();

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

	Mutex _pollLock;
};

} // namespace ZeroTier

#endif
