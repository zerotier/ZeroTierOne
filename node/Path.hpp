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

#ifndef ZT_PATH_HPP
#define ZT_PATH_HPP

#include <stdint.h>
#include <string.h>

#include <stdexcept>
#include <string>

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "Utils.hpp"

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
	enum Type
	{
		PATH_TYPE_NULL = 0,
		PATH_TYPE_UDP = 1,
		PATH_TYPE_TCP_OUT = 2,
		PATH_TYPE_TCP_IN = 3
	};

	Path() :
		_lastSend(0),
		_lastReceived(0),
		_lastPing(0),
		_addr(),
		_type(PATH_TYPE_NULL),
		_fixed(false) {}

	Path(const Path &p)
	{
		memcpy(this,&p,sizeof(Path));
	}

	Path(const InetAddress &addr,Type t,bool fixed = false) :
		_lastSend(0),
		_lastReceived(0),
		_lastPing(0),
		_addr(addr),
		_type(t),
		_fixed(fixed) {}

	inline void init(const InetAddress &addr,Type t,bool fixed = false)
	{
		_lastSend = 0;
		_lastReceived = 0;
		_lastPing = 0;
		_addr = addr;
		_type = t;
		_fixed = fixed;
	}

	inline Path &operator=(const Path &p)
	{
		if (this != &p)
			memcpy(this,&p,sizeof(Path));
		return *this;
	}

	inline const InetAddress &address() const throw() { return _addr; }

	inline Type type() const throw() { return _type; }
	inline bool tcp() const throw() { return ((_type == PATH_TYPE_TCP_IN)||(_type == PATH_TYPE_TCP_OUT)); }

	inline uint64_t lastSend() const throw() { return _lastSend; }
	inline uint64_t lastReceived() const throw() { return _lastReceived; }
	inline uint64_t lastPing() const throw() { return _lastPing; }

	inline bool fixed() const throw() { return _fixed; }
	inline void setFixed(bool f) throw() { _fixed = f; }

	inline void sent(uint64_t t) throw() { _lastSend = t; }
	inline void received(uint64_t t) throw() { _lastReceived = t; }
	inline void pinged(uint64_t t) throw() { _lastPing = t; }

	/**
	 * @param now Current time
	 * @return True if this path is fixed or has received data in last ACTIVITY_TIMEOUT ms
	 */
	inline bool active(uint64_t now) const
		throw()
	{
		return ((_addr)&&((_fixed)||((now - _lastReceived) < ZT_PEER_PATH_ACTIVITY_TIMEOUT)));
	}

	/**
	 * @return Human-readable address and other information about this path, some computed as of current time
	 */
	inline std::string toString() const
	{
		uint64_t now = Utils::now();
		char tmp[1024];
		const char *t = "";
		switch(_type) {
			case PATH_TYPE_NULL: t = "null"; break;
			case PATH_TYPE_UDP: t = "udp"; break;
			case PATH_TYPE_TCP_OUT: t = "tcp_out"; break;
			case PATH_TYPE_TCP_IN: t = "tcp_in"; break;
		}
		Utils::snprintf(tmp,sizeof(tmp),"%s;%s;%lld;%lld;%lld;%s",
			t,
			_addr.toString().c_str(),
			(long long)((_lastSend != 0) ? ((now - _lastSend) / 1000LL) : -1),
			(long long)((_lastReceived != 0) ? ((now - _lastReceived) / 1000LL) : -1),
			(long long)((_lastPing != 0) ? ((now - _lastPing) / 1000LL) : -1),
			((_fixed) ? "fixed" : (active(now) ? "active" : "inactive"))
		);
		return std::string(tmp);
	}

	inline bool operator==(const Path &p) const throw() { return ((_addr == p._addr)&&(_type == p._type)); }
	inline bool operator!=(const Path &p) const throw() { return ((_addr != p._addr)||(_type != p._type)); }
	inline bool operator<(const Path &p) const
		throw()
	{
		if (_addr == p._addr)
			return ((int)_type < (int)p._type);
		else return (_addr < p._addr);
	}
	inline bool operator>(const Path &p) const throw() { return (p < *this); }
	inline bool operator<=(const Path &p) const throw() { return !(p < *this); }
	inline bool operator>=(const Path &p) const throw() { return !(*this < p); }

private:
	volatile uint64_t _lastSend;
	volatile uint64_t _lastReceived;
	volatile uint64_t _lastPing;
	InetAddress _addr;
	Type _type;
	bool _fixed;
};

} // namespace ZeroTier

#endif
