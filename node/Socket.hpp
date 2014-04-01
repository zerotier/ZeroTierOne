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

#ifndef ZT_SOCKET_HPP
#define ZT_SOCKET_HPP

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "AtomicCounter.hpp"
#include "SharedPtr.hpp"
#include "NonCopyable.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#endif

/**
 * Maximum discrete message length supported by all socket types
 */
#define ZT_SOCKET_MAX_MESSAGE_LEN 2048

namespace ZeroTier {

class Socket;
class SocketManager;

/**
 * Base class of all socket types
 *
 * Socket implementations are tightly bound to SocketManager.
 */
class Socket : NonCopyable
{
	friend class SocketManager;
	friend class SharedPtr<Socket>;

public:
	enum Type
	{
		ZT_SOCKET_TYPE_UDP_V4,
		ZT_SOCKET_TYPE_UDP_V6,
		ZT_SOCKET_TYPE_TCP_IN, // incoming connection, not listen
		ZT_SOCKET_TYPE_TCP_OUT
	};

	virtual ~Socket() {}

	/**
	 * @return Socket type
	 */
	inline Type type() const
		throw()
	{
		return _type;
	}

	/**
	 * @return True if this is a TCP socket
	 */
	inline bool tcp() const
		throw()
	{
		return ((_type == ZT_SOCKET_TYPE_TCP_IN)||(_type == ZT_SOCKET_TYPE_TCP_OUT));
	}

	/**
	 * Send a ZeroTier message packet
	 *
	 * @param to Destination address (ignored in connected TCP sockets)
	 * @param msg Message data
	 * @param msglen Message length (max 16384 bytes)
	 * @return True if send appears successful on our end, false if e.g. address type unreachable from this socket
	 */
	virtual bool send(const InetAddress &to,const void *msg,unsigned int msglen) = 0;

protected:
#ifdef __WINDOWS__
	Socket(Type t,SOCKET s) :
#else
	Socket(Type t,int s) :
#endif
		_sock(s),
		_type(t) {}

	// Called only by SocketManager, should return false if socket is no longer open/valid (e.g. connection drop or other fatal error)
	virtual bool notifyAvailableForRead(const SharedPtr<Socket> &self,SocketManager *sm) = 0;
	virtual bool notifyAvailableForWrite(const SharedPtr<Socket> &self,SocketManager *sm) = 0;

#ifdef __WINDOWS__
	SOCKET _sock;
#else
	int _sock;
#endif
	Type _type;

	AtomicCounter __refCount;
};

} // namespace ZeroTier

#endif
