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

#ifndef ZT_SOCKETMANAGER_HPP
#define ZT_SOCKETMANAGER_HPP

#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <stdexcept>

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "Buffer.hpp"
#include "NonCopyable.hpp"
#include "Socket.hpp"

namespace ZeroTier {

/**
 * Socket I/O implementation
 */
class SocketManager : NonCopyable
{
public:
	SocketManager() {}
	virtual ~SocketManager() {}

	/**
	 * Send a message to a remote peer
	 *
	 * @param to Destination address
	 * @param tcp Use TCP?
	 * @param autoConnectTcp If true, automatically initiate TCP connection if there is none
	 * @param msg Message to send
	 * @param msglen Length of message
	 */
	virtual bool send(
		const InetAddress &to,
		bool tcp,
		bool autoConnectTcp,
		const void *msg,
		unsigned int msglen) = 0;

	/**
	 * Send a message to a remote peer via UDP (shortcut for setting both TCP params to false in send)
	 *
	 * @param to Destination address
	 * @param msg Message to send
	 * @param msglen Length of message
	 */
	inline bool sendUdp(
		const InetAddress &to,
		const void *msg,
		unsigned int msglen) { return send(to,false,false,msg,msglen); }

	/**
	 * Perform I/O polling operation (e.g. select())
	 *
	 * If called concurrently, one will block until the other completes.
	 *
	 * @param timeout Timeout in milliseconds, may return sooner if whack() is called
	 * @param handler Packet data handler
	 * @param arg Void argument to packet data handler
	 */
	virtual void poll(
		unsigned long timeout,
		void (*handler)(const SharedPtr<Socket> &,void *,const InetAddress &,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> &),
		void *arg) = 0;

	/**
	 * Cause current or next blocking poll() operation to timeout immediately
	 */
	virtual void whack() = 0;

	/**
	 * Close TCP sockets
	 */
	virtual void closeTcpSockets() = 0;
};

} // namespace ZeroTier

#endif
