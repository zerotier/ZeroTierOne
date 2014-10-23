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
	{
		Mutex::Lock _l(_inbox_m);
		while (!_inbox.empty()) {
			handler(_mySocket,arg,_inbox.front().first,_inbox.front().second);
			_inbox.pop();
		}
	}
	if (timeout)
		_waitCond.wait(timeout);
	else _waitCond.wait();
	{
		Mutex::Lock _l(_inbox_m);
		while (!_inbox.empty()) {
			handler(_mySocket,arg,_inbox.front().first,_inbox.front().second);
			_inbox.pop();
		}
	}
}

void SimNetSocketManager::whack()
{
	_waitCond.signal();
}

void SimNetSocketManager::closeTcpSockets()
{
}

} // namespace ZeroTier
