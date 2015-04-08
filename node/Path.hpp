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

#ifndef ZT_PATH_HPP
#define ZT_PATH_HPP

#include <stdint.h>
#include <string.h>

#include <stdexcept>
#include <string>
#include <algorithm>

#include "Constants.hpp"
#include "Node.hpp"
#include "InetAddress.hpp"
#include "Utils.hpp"
#include "AntiRecursion.hpp"
#include "RuntimeEnvironment.hpp"

namespace ZeroTier {

/**
 * WAN address and protocol for reaching a peer
 *
 * This structure is volatile and memcpy-able, and depends on
 * InetAddress being similarly safe.
 */
class Path
{
public:
	Path() :
		_addr(),
		_lastSend(0),
		_lastReceived(0),
		_lastReceiveDesperation(0),
		_fixed(false) {}

	Path(const Path &p) throw() { memcpy(this,&p,sizeof(Path)); }

	Path(const InetAddress &addr,bool fixed) :
		_addr(addr),
		_lastSend(0),
		_lastReceived(0),
		_lastReceiveDesperation(0),
		_fixed(fixed) {}

	inline void init(const InetAddress &addr,bool fixed)
	{
		_addr = addr;
		_lastSend = 0;
		_lastReceived = 0;
		_lastReceiveDesperation = 0;
		_fixed = fixed;
	}

	inline Path &operator=(const Path &p)
	{
		if (this != &p)
			memcpy(this,&p,sizeof(Path));
		return *this;
	}

	inline const InetAddress &address() const throw() { return _addr; }

	inline uint64_t lastSend() const throw() { return _lastSend; }
	inline uint64_t lastReceived() const throw() { return _lastReceived; }

	/**
	 * Called when a packet is sent to this path
	 *
	 * This is called automatically by Path::send().
	 *
	 * @param t Time of send
	 */
	inline void sent(uint64_t t)
		throw()
	{
		_lastSend = t;
	}

	/**
	 * Called when a packet is received from this path
	 *
	 * @param t Time of receive
	 * @param d Link desperation of receive
	 */
	inline void received(uint64_t t,unsigned int d)
		throw()
	{
		_lastReceived = t;
		_lastReceiveDesperation = d;
	}

	/**
	 * @return Is this a fixed path?
	 */
	inline bool fixed() const throw() { return _fixed; }

	/**
	 * @param f New value of fixed path flag
	 */
	inline void setFixed(bool f) throw() { _fixed = f; }

	/**
	 * @return Last desperation reported via incoming link
	 */
	inline unsigned int lastReceiveDesperation() const throw() { return _lastReceiveDesperation; }

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
		if (RR->node->putPacket(_addr,data,len,std::max(RR->node->coreDesperation(),_lastReceiveDesperation))) {
			sent(now);
			RR->antiRec->logOutgoingZT(data,len);
			return true;
		}
		return false;
	}

	/**
	 * @param now Current time
	 * @return Human-readable address and other information about this path
	 */
	inline std::string toString(uint64_t now) const
	{
		char tmp[1024];
		Utils::snprintf(tmp,sizeof(tmp),"%s(%s)",
			_addr.toString().c_str(),
			((_fixed) ? "fixed" : (active(now) ? "active" : "inactive"))
		);
		return std::string(tmp);
	}

	inline operator bool() const throw() { return (_addr); }

	inline bool operator==(const Path &p) const throw() { return (_addr == p._addr); }
	inline bool operator!=(const Path &p) const throw() { return (_addr != p._addr); }
	inline bool operator<(const Path &p) const throw() { return (_addr < p._addr); }
	inline bool operator>(const Path &p) const throw() { return (_addr > p._addr); }
	inline bool operator<=(const Path &p) const throw() { return (_addr <= p._addr); }
	inline bool operator>=(const Path &p) const throw() { return (_addr >= p._addr); }

private:
	InetAddress _addr;
	uint64_t _lastSend;
	uint64_t _lastReceived;
	unsigned int _lastReceiveDesperation;
	bool _fixed;
};

} // namespace ZeroTier

#endif
