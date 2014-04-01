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
#include "Buffer.hpp"

#define ZT_PATH_SERIALIZATION_VERSION 2

namespace ZeroTier {

/**
 * WAN address and protocol for reaching a peer
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
		_lastFirewallOpener(0),
		_lastPing(0),
		_addr(),
		_type(PATH_TYPE_NULL),
		_fixed(false) {}

	Path(const Path &p)
	{
		// InetAddress is memcpy'able
		memcpy(this,&p,sizeof(Path));
	}

	Path(const InetAddress &addr,Type t,bool fixed = false) :
		_lastSend(0),
		_lastReceived(0),
		_lastFirewallOpener(0),
		_lastPing(0),
		_addr(addr),
		_type(t),
		_fixed(fixed) {}

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
	inline uint64_t lastFirewallOpener() const throw() { return _lastFirewallOpener; }
	inline uint64_t lastPing() const throw() { return _lastPing; }

	inline bool fixed() const throw() { return _fixed; }
	inline void setFixed(bool f) throw() { _fixed = f; }

	inline void sent(uint64_t t) throw() { _lastSend = t; }
	inline void received(uint64_t t) throw() { _lastReceived = t; }
	inline void firewallOpenerSent(uint64_t t) throw() { _lastFirewallOpener = t; }
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
		Utils::snprintf(tmp,sizeof(tmp),"%s:%s:%lld;%lld;%lld;%lld;%s",
			t,
			_addr.toString().c_str(),
			(long long)((_lastSend != 0) ? (now - _lastSend) : -1),
			(long long)((_lastReceived != 0) ? (now - _lastReceived) : -1),
			(long long)((_lastFirewallOpener != 0) ? (now - _lastFirewallOpener) : -1),
			(long long)((_lastPing != 0) ? (now - _lastPing) : -1),
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

	template<unsigned int C>
	inline void serialize(Buffer<C> &b) const
	{
		b.append((unsigned char)ZT_PATH_SERIALIZATION_VERSION);
		b.append(_lastSend);
		b.append(_lastReceived);
		b.append(_lastFirewallOpener);
		b.append(_lastPing);
		b.append((unsigned char)_addr.type());
		switch(_addr.type()) {
			case InetAddress::TYPE_NULL:
				break;
			case InetAddress::TYPE_IPV4:
				b.append(_addr.rawIpData(),4);
				b.append((uint16_t)_addr.port());
				break;
			case InetAddress::TYPE_IPV6:
				b.append(_addr.rawIpData(),16);
				b.append((uint16_t)_addr.port());
				break;
		}
		b.append((unsigned char)_type);
		b.append(_fixed ? (unsigned char)1 : (unsigned char)0);
	}
	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;

		if (b[p++] != ZT_PATH_SERIALIZATION_VERSION)
			throw std::invalid_argument("Path: deserialize(): version mismatch");

		_lastSend = b.template at<uint64_t>(p); p += sizeof(uint64_t);
		_lastReceived = b.template at<uint64_t>(p); p += sizeof(uint64_t);
		_lastFirewallOpener = b.template at<uint64_t>(p); p += sizeof(uint64_t);
		_lastPing = b.template at<uint64_t>(p); p += sizeof(uint64_t);
		switch((InetAddress::AddressType)b[p++]) {
			case InetAddress::TYPE_IPV4:
				_addr.set(b.field(p,4),4,b.template at<uint16_t>(p + 4));
				p += 4 + sizeof(uint16_t);
				break;
			case InetAddress::TYPE_IPV6:
				_addr.set(b.field(p,16),16,b.template at<uint16_t>(p + 16));
				p += 16 + sizeof(uint16_t);
				break;
			default:
				_addr.zero();
				break;
		}
		_type = (Type)b[p++];
		_fixed = (b[p++] != 0);

		return (p - startAt);
	}

private:
	volatile uint64_t _lastSend;
	volatile uint64_t _lastReceived;
	volatile uint64_t _lastFirewallOpener;
	volatile uint64_t _lastPing;
	InetAddress _addr;
	Type _type;
	bool _fixed;
};

} // namespace ZeroTier

#endif
