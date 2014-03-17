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

#ifndef ZT_TCPSOCKET_HPP
#define ZT_TCPSOCKET_HPP

#include <stdint.h>

#include "InetAddress.hpp"
#include "Mutex.hpp"
#include "Utils.hpp"
#include "Socket.hpp"

#define ZT_TCP_SENDQ_LENGTH 4096
#define ZT_TCP_MAX_MESSAGE_LENGTH 2048

namespace ZeroTier {

class SocketManager;

/**
 * A TCP socket encapsulating ZeroTier packets over a TCP stream connection
 *
 * This implements a simple packet encapsulation that is designed to look like
 * a TLS connection. It's not a TLS connection, but it sends TLS format record
 * headers. It could be extended in the future to implement a fake TLS
 * handshake.
 *
 * At the moment, each packet is just made to look like TLS application data:
 *   <[1] TLS content type> - currently 0x17 for "application data"
 *   <[1] TLS major version> - currently 0x03 for TLS 1.2
 *   <[1] TLS minor version> - currently 0x03 for TLS 1.2
 *   <[2] payload length> - 16-bit length of payload in bytes
 *   <[...] payload> - Message payload
 *
 * The primary purpose of TCP sockets is to work over ports like HTTPS(443),
 * allowing users behind particularly fascist firewalls to at least reach
 * ZeroTier's supernodes. UDP is the preferred method of communication as
 * encapsulating L2 and L3 protocols over TCP is inherently inefficient
 * due to double-ACKs. So TCP is only used as a fallback.
 */
class TcpSocket : public Socket
{
	friend class SharedPtr<Socket>;
	friend class SocketManager;

public:
	virtual ~TcpSocket();
	virtual bool send(const InetAddress &to,const void *msg,unsigned int msglen);

protected:
#ifdef __WINDOWS__
	TcpSocket(SOCKET s,bool c,const InetAddress &r) :
#else
	TcpSocket(int s,bool c,const InetAddress &r) :
#endif
		Socket(Socket::ZT_SOCKET_TYPE_TCP,s),
		_lastReceivedData(Utils::now()),
		_inptr(0),
		_outptr(0),
		_connecting(c),
		_remote(r),
		_lock() {}

	virtual bool notifyAvailableForRead(const SharedPtr<Socket> &self,SocketManager *sm);
	virtual bool notifyAvailableForWrite(const SharedPtr<Socket> &self,SocketManager *sm);

private:
	unsigned char _outbuf[ZT_TCP_SENDQ_LENGTH];
	unsigned char _inbuf[ZT_TCP_MAX_MESSAGE_LENGTH];
	uint64_t _lastReceivedData; // updated whenever data is received, checked directly by SocketManager for stale TCP cleanup
	unsigned int _inptr;
	unsigned int _outptr;
	bool _connecting; // manipulated directly by SocketManager, true if connect() is in progress
	InetAddress _remote;
	Mutex _lock;
};

} // namespace ZeroTier

#endif
