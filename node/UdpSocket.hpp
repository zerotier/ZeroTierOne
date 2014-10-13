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

#ifndef ZT_UDPSOCKET_HPP
#define ZT_UDPSOCKET_HPP

#include "Socket.hpp"

namespace ZeroTier {

class SocketManager;

/**
 * Locally bound UDP socket
 */
class UdpSocket : public Socket
{
	friend class SharedPtr<Socket>;
	friend class SocketManager;

public:
	virtual ~UdpSocket();
	virtual bool send(const InetAddress &to,const void *msg,unsigned int msglen);

protected:
#ifdef __WINDOWS__
	UdpSocket(Type t,SOCKET s) :
#else
	UdpSocket(Type t,int s) :
#endif
		Socket(t,s) {}

	virtual bool notifyAvailableForRead(const SharedPtr<Socket> &self,SocketManager *sm);
	virtual bool notifyAvailableForWrite(const SharedPtr<Socket> &self,SocketManager *sm);
};

} // namespace ZeroTier

#endif
