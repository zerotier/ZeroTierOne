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

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "Utils.hpp"

#include <string>

namespace ZeroTier {

/**
 * WAN address and protocol for reaching a peer
 */
class Path
{
public:
	Path() :
		_lastSent(0),
		_lastReceived(0),
		_lastFirewallOpener(0),
		_lastPing(0),
		_addr(),
		_tcp(false),
		_fixed(false) {}

	Path(const InetAddress &addr,bool tcp,bool fixed) :
		_lastSent(0),
		_lastReceived(0),
		_lastFirewallOpener(0),
		_lastPing(0),
		_addr(addr),
		_tcp(tcp),
		_fixed(fixed) {}

	inline const InetAddress &address() const throw() { return _addr; }
	inline bool tcp() const throw() { return _tcp; }
	inline uint64_t lastSent() const throw() { return _lastSent; }
	inline uint64_t lastReceived() const throw() { return _lastReceived; }
	inline uint64_t lastFirewallOpener() const throw() { return _lastFirewallOpener; }
	inline uint64_t lastPing() const throw() { return _lastPing; }
	inline bool fixed() const throw() { return _fixed; }

	inline void sent(uint64_t t) throw() { _lastSent = t; }
	inline void received(uint64_t t) throw() { _lastReceived = t; }
	inline void firewallOpenerSent(uint64_t t) throw() { _lastFirewallOpener = t; }
	inline void pinged(uint64_t t) throw() { _lastPing = t; }

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
		char lsago[32],lrago[32],lfoago[32],lpago[32];
		Utils::snprintf(lsago,sizeof(lsago),"%lld",(long long)((_lastSent != 0) ? (now - _lastSent) : -1));
		Utils::snprintf(lrago,sizeof(lrago),"%lld",(long long)((_lastReceived != 0) ? (now - _lastReceived) : -1));
		Utils::snprintf(lfoago,sizeof(lfoago),"%lld",(long long)((_lastFirewallOpener != 0) ? (now - _lastFirewallOpener) : -1));
		Utils::snprintf(lpago,sizeof(lfoago),"%lld",(long long)((_lastPing != 0) ? (now - _lastPing) : -1));
		return (std::string(_tcp ? "tcp:" : "udp:") + _addr.toString() + "[" + lsago + "," lrago + "," + lpago + "," + lfoago + "," + (active(now) ? "active" : "inactive") + "," + (_fixed ? "fixed" : "learned") + "]");
	}

	inline operator==(const Path &p) const throw() { return ((_addr == p._addr)&&(_tcp == p._tcp)); }
	inline operator!=(const Path &p) const throw() { return ((_addr != p._addr)||(_tcp != p._tcp)); }
	inline operator<(const Path &p) const
		throw()
	{
		if (_addr == p._addr) {
			if (!_tcp) // UDP < TCP
				return p._tcp;
			return false;
		} else return (_addr < p._addr);
	}
	inline bool operator>(const Path &p) const throw() { return (p < *this); }
	inline bool operator<=(const Path &p) const throw() { return !(p < *this); }
	inline bool operator>=(const Path &p) const throw() { return !(*this < p); }

private:
	volatile uint64_t _lastSent;
	volatile uint64_t _lastReceived;
	volatile uint64_t _lastFirewallOpener;
	volatile uint64_t _lastPing;
	InetAddress _addr;
	bool _tcp;
	bool _fixed;
};

} // namespace ZeroTier

#endif
