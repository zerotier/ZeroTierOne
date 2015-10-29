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
#include <algorithm>

#include "Constants.hpp"
#include "InetAddress.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Base class for paths
 *
 * The base Path class is an immutable value.
 */
class Path
{
public:
	/**
	 * Path trust category
	 *
	 * Note that this is NOT peer trust and has nothing to do with root server
	 * designations or other trust metrics. This indicates how much we trust
	 * this path to be secure and/or private. A trust level of normal means
	 * encrypt and authenticate all traffic. Privacy trust means we can send
	 * traffic in the clear. Ultimate trust means we don't even need
	 * authentication. Generally a private path would be a hard-wired local
	 * LAN, while an ultimate trust path would be a physically isolated private
	 * server backplane.
	 *
	 * Nearly all paths will be normal trust. The other levels are for high
	 * performance local SDN use only.
	 *
	 * These values MUST match ZT_LocalInterfaceAddressTrust in ZeroTierOne.h
	 */
	enum Trust // NOTE: max 255
	{
		TRUST_NORMAL = 0,
		TRUST_PRIVACY = 10,
		TRUST_ULTIMATE = 20
	};

	Path() :
		_lastSend(0),
		_lastReceived(0),
		_addr(),
		_localAddress(),
		_ipScope(InetAddress::IP_SCOPE_NONE),
		_flags(0)
	{
	}

	Path(const InetAddress &localAddress,const InetAddress &addr) :
		_lastSend(0),
		_lastReceived(0),
		_addr(addr),
		_localAddress(localAddress),
		_ipScope(addr.ipScope()),
		_flags(0)
	{
	}

	inline Path &operator=(const Path &p)
		throw()
	{
		if (this != &p)
			memcpy(this,&p,sizeof(Path));
		return *this;
	}

	/**
	 * Called when a packet is sent to this remote path
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
	 * @return True if this path appears active
	 */
	inline bool active(uint64_t now) const
		throw()
	{
		return ((now - _lastReceived) < ZT_PEER_ACTIVITY_TIMEOUT);
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
	bool send(const RuntimeEnvironment *RR,const void *data,unsigned int len,uint64_t now);

	/**
	 * @return Address of local side of this path or NULL if unspecified
	 */
	inline const InetAddress &localAddress() const throw() { return _localAddress; }

	/**
	 * @return Time of last send to this path
	 */
	inline uint64_t lastSend() const throw() { return _lastSend; }

	/**
	 * @return Time of last receive from this path
	 */
	inline uint64_t lastReceived() const throw() { return _lastReceived; }

	/**
	 * @return Physical address
	 */
	inline const InetAddress &address() const throw() { return _addr; }

	/**
	 * @return IP scope -- faster shortcut for address().ipScope()
	 */
	inline InetAddress::IpScope ipScope() const throw() { return _ipScope; }

	/**
	 * @return Preference rank, higher == better
	 */
	inline int preferenceRank() const throw()
	{
		// First, since the scope enum values in InetAddress.hpp are in order of
		// use preference rank, we take that. Then we multiple by two, yielding
		// a sequence like 0, 2, 4, 6, etc. Then if it's IPv6 we add one. This
		// makes IPv6 addresses of a given scope outrank IPv4 addresses of the
		// same scope -- e.g. 1 outranks 0. This makes us prefer IPv6, but not
		// if the address scope/class is of a fundamentally lower rank.
		return ( ((int)_ipScope * 2) + ((_addr.ss_family == AF_INET6) ? 1 : 0) );
	}

	/**
	 * @return True if path is considered reliable (no NAT keepalives etc. are needed)
	 */
	inline bool reliable() const throw()
	{
		if (_addr.ss_family == AF_INET)
			return ((_ipScope != InetAddress::IP_SCOPE_GLOBAL)&&(_ipScope != InetAddress::IP_SCOPE_PSEUDOPRIVATE));
		return true;
	}

	/**
	 * @return True if address is non-NULL
	 */
	inline operator bool() const throw() { return (_addr); }

	/**
	 * Check whether this address is valid for a ZeroTier path
	 *
	 * This checks the address type and scope against address types and scopes
	 * that we currently support for ZeroTier communication.
	 *
	 * @param a Address to check
	 * @return True if address is good for ZeroTier path use
	 */
	static inline bool isAddressValidForPath(const InetAddress &a)
		throw()
	{
		if ((a.ss_family == AF_INET)||(a.ss_family == AF_INET6)) {
			switch(a.ipScope()) {
				/* Note: we don't do link-local at the moment. Unfortunately these
				 * cause several issues. The first is that they usually require a
				 * device qualifier, which we don't handle yet and can't portably
				 * push in PUSH_DIRECT_PATHS. The second is that some OSes assign
				 * these very ephemerally or otherwise strangely. So we'll use
				 * private, pseudo-private, shared (e.g. carrier grade NAT), or
				 * global IP addresses. */
				case InetAddress::IP_SCOPE_PRIVATE:
				case InetAddress::IP_SCOPE_PSEUDOPRIVATE:
				case InetAddress::IP_SCOPE_SHARED:
				case InetAddress::IP_SCOPE_GLOBAL:
					return true;
				default:
					return false;
			}
		}
		return false;
	}

	template<unsigned int C>
	inline void serialize(Buffer<C> &b) const
	{
		b.append((uint8_t)1); // version
		b.append((uint64_t)_lastSend);
		b.append((uint64_t)_lastReceived);
		_addr.serialize(b);
		_localAddress.serialize(b);
		b.append((uint16_t)_flags);
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;
		if (b[p++] != 1)
			throw std::invalid_argument("invalid serialized Path");
		_lastSend = b.template at<uint64_t>(p); p += 8;
		_lastReceived = b.template at<uint64_t>(p); p += 8;
		p += _addr.deserialize(b,p);
		p += _localAddress.deserialize(b,p);
		_ipScope = _addr.ipScope();
		_flags = b.template at<uint16_t>(p); p += 2;
		return (p - startAt);
	}

private:
	uint64_t _lastSend;
	uint64_t _lastReceived;
	InetAddress _addr;
	InetAddress _localAddress;
	InetAddress::IpScope _ipScope; // memoize this since it's a computed value checked often
	uint16_t _flags;
};

} // namespace ZeroTier

#endif
