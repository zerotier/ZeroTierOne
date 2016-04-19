/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#ifndef ZT_PATH_HPP
#define ZT_PATH_HPP

#include <stdint.h>
#include <string.h>

#include <stdexcept>
#include <algorithm>

#include "Constants.hpp"
#include "InetAddress.hpp"

// Note: if you change these flags check the logic below. Some of it depends
// on these bits being what they are.

/**
 * Flag indicating that this path is suboptimal
 *
 * Clusters set this flag on remote paths if GeoIP or other routing decisions
 * indicate that a peer should be handed off to another cluster member.
 */
#define ZT_PATH_FLAG_CLUSTER_SUBOPTIMAL 0x0001

/**
 * Flag indicating that this path is optimal
 *
 * Peers set this flag on paths that are pushed by a cluster and indicated as
 * optimal. A second flag is needed since we want to prioritize cluster optimal
 * paths and de-prioritize sub-optimal paths and for new paths we don't know
 * which one they are. So we want a trinary state: optimal, suboptimal, unknown.
 */
#define ZT_PATH_FLAG_CLUSTER_OPTIMAL 0x0002

/**
 * Maximum return value of preferenceRank()
 */
#define ZT_PATH_MAX_PREFERENCE_RANK ((ZT_INETADDRESS_MAX_SCOPE << 1) | 1)

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
	Path() :
		_lastSend(0),
		_lastPing(0),
		_lastKeepalive(0),
		_lastReceived(0),
		_addr(),
		_localAddress(),
		_flags(0),
		_ipScope(InetAddress::IP_SCOPE_NONE)
	{
	}

	Path(const InetAddress &localAddress,const InetAddress &addr) :
		_lastSend(0),
		_lastPing(0),
		_lastKeepalive(0),
		_lastReceived(0),
		_addr(addr),
		_localAddress(localAddress),
		_flags(0),
		_ipScope(addr.ipScope())
	{
	}

	inline Path &operator=(const Path &p)
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
	inline void sent(uint64_t t) { _lastSend = t; }

	/**
	 * Called when we've sent a ping or echo
	 *
	 * @param t Time of send
	 */
	inline void pinged(uint64_t t) { _lastPing = t; }

	/**
	 * Called when we send a NAT keepalive
	 *
	 * @param t Time of send
	 */
	inline void sentKeepalive(uint64_t t) { _lastKeepalive = t; }

	/**
	 * Called when a packet is received from this remote path
	 *
	 * @param t Time of receive
	 */
	inline void received(uint64_t t)
	{
		_lastReceived = t;
		_probation = 0;
	}

	/**
	 * @param now Current time
	 * @return True if this path appears active
	 */
	inline bool active(uint64_t now) const
	{
		return ( ((now - _lastReceived) < ZT_PATH_ACTIVITY_TIMEOUT) && (_probation < ZT_PEER_DEAD_PATH_DETECTION_MAX_PROBATION) );
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
	 * @return Time we last pinged or dead path checked this link
	 */
	inline uint64_t lastPing() const throw() { return _lastPing; }

	/**
	 * @return Time of last keepalive
	 */
	inline uint64_t lastKeepalive() const throw() { return _lastKeepalive; }

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
	 * @param f Valuve of ZT_PATH_FLAG_CLUSTER_SUBOPTIMAL and inverse of ZT_PATH_FLAG_CLUSTER_OPTIMAL (both are changed)
	 */
	inline void setClusterSuboptimal(bool f)
	{
		if (f) {
			_flags = (_flags | ZT_PATH_FLAG_CLUSTER_SUBOPTIMAL) & ~ZT_PATH_FLAG_CLUSTER_OPTIMAL;
		} else {
			_flags = (_flags | ZT_PATH_FLAG_CLUSTER_OPTIMAL) & ~ZT_PATH_FLAG_CLUSTER_SUBOPTIMAL;
		}
	}

	/**
	 * @return True if ZT_PATH_FLAG_CLUSTER_SUBOPTIMAL is set
	 */
	inline bool isClusterSuboptimal() const { return ((_flags & ZT_PATH_FLAG_CLUSTER_SUBOPTIMAL) != 0); }

	/**
	 * @return True if ZT_PATH_FLAG_CLUSTER_OPTIMAL is set
	 */
	inline bool isClusterOptimal() const { return ((_flags & ZT_PATH_FLAG_CLUSTER_OPTIMAL) != 0); }

	/**
	 * @return Preference rank, higher == better (will be less than 255)
	 */
	inline unsigned int preferenceRank() const throw()
	{
		/* First, since the scope enum values in InetAddress.hpp are in order of
		 * use preference rank, we take that. Then we multiple by two, yielding
		 * a sequence like 0, 2, 4, 6, etc. Then if it's IPv6 we add one. This
		 * makes IPv6 addresses of a given scope outrank IPv4 addresses of the
		 * same scope -- e.g. 1 outranks 0. This makes us prefer IPv6, but not
		 * if the address scope/class is of a fundamentally lower rank. */
		return ( ((unsigned int)_ipScope << 1) | (unsigned int)(_addr.ss_family == AF_INET6) );
	}

	/**
	 * @return This path's overall quality score (higher is better)
	 */
	inline uint64_t score() const throw()
	{
		// This is a little bit convoluted because we try to be branch-free, using multiplication instead of branches for boolean flags

		// Start with the last time this path was active, and add a fudge factor to prevent integer underflow if _lastReceived is 0
		uint64_t score = _lastReceived + (ZT_PEER_DIRECT_PING_DELAY * (ZT_PEER_DEAD_PATH_DETECTION_MAX_PROBATION + 1));

		// Increase score based on path preference rank, which is based on IP scope and address family
		score += preferenceRank() * (ZT_PEER_DIRECT_PING_DELAY / ZT_PATH_MAX_PREFERENCE_RANK);

		// Increase score if this is known to be an optimal path to a cluster
		score += (uint64_t)(_flags & ZT_PATH_FLAG_CLUSTER_OPTIMAL) * (ZT_PEER_DIRECT_PING_DELAY / 2); // /2 because CLUSTER_OPTIMAL is flag 0x0002

		// Decrease score if this is known to be a sub-optimal path to a cluster
		score -= (uint64_t)(_flags & ZT_PATH_FLAG_CLUSTER_SUBOPTIMAL) * ZT_PEER_DIRECT_PING_DELAY;

		// Penalize for missed ECHO tests in dead path detection
		score -= (uint64_t)((ZT_PEER_DIRECT_PING_DELAY / 2) * _probation);

		return score;
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
					if (a.ss_family == AF_INET6) {
						// TEMPORARY HACK: for now, we are going to blacklist he.net IPv6
						// tunnels due to very spotty performance and low MTU issues over
						// these IPv6 tunnel links.
						const uint8_t *ipd = reinterpret_cast<const uint8_t *>(reinterpret_cast<const struct sockaddr_in6 *>(&a)->sin6_addr.s6_addr);
						if ((ipd[0] == 0x20)&&(ipd[1] == 0x01)&&(ipd[2] == 0x04)&&(ipd[3] == 0x70))
							return false;
					}
					return true;
				default:
					return false;
			}
		}
		return false;
	}

	/**
	 * @return Current path probation count (for dead path detect)
	 */
	inline unsigned int probation() const { return _probation; }

	/**
	 * Increase this path's probation violation count (for dead path detect)
	 */
	inline void increaseProbation() { ++_probation; }

	template<unsigned int C>
	inline void serialize(Buffer<C> &b) const
	{
		b.append((uint8_t)2); // version
		b.append((uint64_t)_lastSend);
		b.append((uint64_t)_lastPing);
		b.append((uint64_t)_lastKeepalive);
		b.append((uint64_t)_lastReceived);
		_addr.serialize(b);
		_localAddress.serialize(b);
		b.append((uint16_t)_flags);
		b.append((uint16_t)_probation);
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;
		if (b[p++] != 2)
			throw std::invalid_argument("invalid serialized Path");
		_lastSend = b.template at<uint64_t>(p); p += 8;
		_lastPing = b.template at<uint64_t>(p); p += 8;
		_lastKeepalive = b.template at<uint64_t>(p); p += 8;
		_lastReceived = b.template at<uint64_t>(p); p += 8;
		p += _addr.deserialize(b,p);
		p += _localAddress.deserialize(b,p);
		_flags = b.template at<uint16_t>(p); p += 2;
		_probation = b.template at<uint16_t>(p); p += 2;
		_ipScope = _addr.ipScope();
		return (p - startAt);
	}

	inline bool operator==(const Path &p) const { return ((p._addr == _addr)&&(p._localAddress == _localAddress)); }
	inline bool operator!=(const Path &p) const { return ((p._addr != _addr)||(p._localAddress != _localAddress)); }

private:
	uint64_t _lastSend;
	uint64_t _lastPing;
	uint64_t _lastKeepalive;
	uint64_t _lastReceived;
	InetAddress _addr;
	InetAddress _localAddress;
	unsigned int _flags;
	unsigned int _probation;
	InetAddress::IpScope _ipScope; // memoize this since it's a computed value checked often
};

} // namespace ZeroTier

#endif
