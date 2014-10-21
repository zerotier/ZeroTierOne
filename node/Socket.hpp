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

/**
 * Maximum discrete message length supported by all socket types
 */
#define ZT_SOCKET_MAX_MESSAGE_LEN 2048

namespace ZeroTier {

class Socket;

/**
 * Base class for transport-layer socket implementations
 */
class Socket : NonCopyable
{
	friend class SharedPtr<Socket>;

public:
	enum Type
	{
		ZT_SOCKET_TYPE_UDP_V4 = 1,
		ZT_SOCKET_TYPE_UDP_V6 = 2,
		ZT_SOCKET_TYPE_TCP_IN = 3,  // incoming connection accepted from listen socket
		ZT_SOCKET_TYPE_TCP_OUT = 4, // outgoing connection to remote endpoint
		ZT_SOCKET_TYPE_ETHERNET = 5 // unused, for future SDN backplane support
	};

	virtual ~Socket() {}

	/**
	 * @return Socket type
	 */
	inline Type type() const throw() { return _type; }

	/**
	 * @return True if this is a UDP socket
	 */
	inline bool udp() const throw() { return ((_type == ZT_SOCKET_TYPE_UDP_V4)||(_type == ZT_SOCKET_TYPE_UDP_V6)); }

	/**
	 * @return True if this is a TCP socket
	 */
	inline bool tcp() const throw() { return ((_type == ZT_SOCKET_TYPE_TCP_IN)||(_type == ZT_SOCKET_TYPE_TCP_OUT)); }

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
	Socket(const Type &t) : _type(t) {}

	Type _type;
	AtomicCounter __refCount;
};

} // namespace ZeroTier

#endif
