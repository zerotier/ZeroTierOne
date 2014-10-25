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

#ifndef ZT_SIMNETSOCKETMANAGER_HPP
#define ZT_SIMNETSOCKETMANAGER_HPP

#include <map>
#include <utility>
#include <vector>

#include "../node/Constants.hpp"
#include "../node/SocketManager.hpp"
#include "../node/Mutex.hpp"
#include "../node/Condition.hpp"

namespace ZeroTier {

class SimNet;

/**
 * Socket manager for an IP endpoint in a simulated network
 */
class SimNetSocketManager : public SocketManager
{
	friend class SimNet;

public:
	struct TransferStats
	{
		TransferStats() : received(0),sent(0) {}
		unsigned long long received;
		unsigned long long sent;
	};

	SimNetSocketManager();
	virtual ~SimNetSocketManager();

	/**
	 * @return IP address of this simulated endpoint
	 */
	inline const InetAddress &address() const { return _address; }

	/**
	 * @return Local endpoint stats
	 */
	inline const TransferStats &totals() const { return _totals; }

	/**
	 * @param peer Peer IP address
	 * @return Transfer stats for this peer
	 */
	inline TransferStats stats(const InetAddress &peer) const
	{
		Mutex::Lock _l(_stats_m);
		std::map< InetAddress,TransferStats >::const_iterator s(_stats.find(peer));
		if (s == _stats.end())
			return TransferStats();
		return s->second;
	}

	/**
	 * @return Network to which this endpoint belongs
	 */
	inline SimNet *net() const { return _sn; }

	/**
	 * Enqueue data from another endpoint to be picked up on next poll()
	 *
	 * @param from Originating endpoint address
	 * @param data Data
	 * @param len Length of data in bytes
	 */
	inline void enqueue(const InetAddress &from,const void *data,unsigned int len)
	{
		{
			Mutex::Lock _l(_inbox_m);
			_inbox.push_back(std::pair< InetAddress,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> >(from,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN>(data,len)));
		}
		_waitCond.signal();
	}

	virtual bool send(const InetAddress &to,bool tcp,bool autoConnectTcp,const void *msg,unsigned int msglen);
	virtual void poll(unsigned long timeout,void (*handler)(const SharedPtr<Socket> &,void *,const InetAddress &,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> &),void *arg);
	virtual void whack();
	virtual void closeTcpSockets();

private:
	// These are set by SimNet after object creation
	SimNet *_sn;
	InetAddress _address;

	SharedPtr<Socket> _mySocket;
	TransferStats _totals;

	std::vector< std::pair< InetAddress,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> > > _inbox;
	Mutex _inbox_m;

	std::map< InetAddress,TransferStats > _stats;
	Mutex _stats_m;

	Condition _waitCond;
};

} // namespace ZeroTier

#endif
