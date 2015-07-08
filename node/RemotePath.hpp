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

#ifndef ZT_REMOTEPATH_HPP
#define ZT_REMOTEPATH_HPP

#include <stdint.h>
#include <string.h>

#include <stdexcept>
#include <algorithm>

#include "Path.hpp"
#include "Node.hpp"
#include "AntiRecursion.hpp"
#include "RuntimeEnvironment.hpp"

namespace ZeroTier {

/**
 * Path to a remote peer
 *
 * This extends Path to include status information about path activity.
 */
class RemotePath : public Path
{
public:
	RemotePath() :
		Path(),
		_lastSend(0),
		_lastReceived(0),
		_fixed(false) {}

	RemotePath(const InetAddress &addr,bool fixed) :
		Path(addr,0,TRUST_NORMAL,false),
		_lastSend(0),
		_lastReceived(0),
		_fixed(fixed) {}

	inline uint64_t lastSend() const throw() { return _lastSend; }
	inline uint64_t lastReceived() const throw() { return _lastReceived; }

	/**
	 * @return Is this a fixed path?
	 */
	inline bool fixed() const throw() { return _fixed; }

	/**
	 * @param f New value of fixed flag
	 */
	inline void setFixed(const bool f)
		throw()
	{
		_fixed = f;
	}

	/**
	 * Called when a packet is sent to this remote path
	 *
	 * This is called automatically by RemotePath::send().
	 *
	 * @param t Time of send
	 */
	inline void sent(uint64_t t)
		throw()
	{
		_lastSend = t;
	}

	/**
	 * Called when a packet is received from this remote path
	 *
	 * @param t Time of receive
	 */
	inline void received(uint64_t t)
		throw()
	{
		_lastReceived = t;
	}

	/**
	 * @param now Current time
	 * @return True if this path is fixed or has received data in last ACTIVITY_TIMEOUT ms
	 */
	inline bool active(uint64_t now) const
		throw()
	{
		return ( (_fixed) || ((now - _lastReceived) < ZT_PEER_ACTIVITY_TIMEOUT) );
	}

	/**
	 * Send a packet via this path
	 *
	 * @param RR Runtime environment
	 * @param data Packet data
	 * @param len Packet length
	 * @param now Current time
	 * @return True if transport reported success
	 */
	inline bool send(const RuntimeEnvironment *RR,const void *data,unsigned int len,uint64_t now)
	{
		if (RR->node->putPacket(_addr,data,len)) {
			sent(now);
			RR->antiRec->logOutgoingZT(data,len);
			return true;
		}
		return false;
	}

private:
	uint64_t _lastSend;
	uint64_t _lastReceived;
	bool _fixed;
};

} // namespace ZeroTier

#endif
