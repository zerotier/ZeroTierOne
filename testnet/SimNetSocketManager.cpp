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

#include "SimNetSocketManager.hpp"
#include "SimNet.hpp"

#include "../node/Constants.hpp"
#include "../node/Socket.hpp"

namespace ZeroTier {

class SimNetSocket : public Socket
{
public:
	SimNetSocket(SimNetSocketManager *sm) :
		Socket(ZT_SOCKET_TYPE_UDP_V4),
		_parent(sm) {}

	virtual bool send(const InetAddress &to,const void *msg,unsigned int msglen)
	{
		SimNetSocketManager *dest = _parent->net()->get(to);
		if (dest)
			dest->enqueue(_parent->address(),msg,msglen);
		return true; // we emulate UDP, which has no delivery guarantee semantics
	}

	SimNetSocketManager *_parent;
};

SimNetSocketManager::SimNetSocketManager() :
	_sn((SimNet *)0), // initialized by SimNet
	_mySocket(new SimNetSocket(this))
{
}

SimNetSocketManager::~SimNetSocketManager()
{
}

bool SimNetSocketManager::send(const InetAddress &to,bool tcp,bool autoConnectTcp,const void *msg,unsigned int msglen)
{
	if (tcp)
		return false; // we emulate UDP
	SimNetSocketManager *dest = _sn->get(to);
	if (dest)
		dest->enqueue(_address,msg,msglen);
	return true; // we emulate UDP, which has no delivery guarantee semantics
}

void SimNetSocketManager::poll(unsigned long timeout,void (*handler)(const SharedPtr<Socket> &,void *,const InetAddress &,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> &),void *arg)
{
	std::pair< InetAddress,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> > msg;
	if ((_inbox.pop(msg,timeout))&&(msg.second.size()))
		handler(_mySocket,arg,msg.first,msg.second);
}

void SimNetSocketManager::whack()
{
	_inbox.push(std::pair< InetAddress,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> >());
}

void SimNetSocketManager::closeTcpSockets()
{
}

} // namespace ZeroTier
