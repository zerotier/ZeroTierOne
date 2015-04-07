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

#ifndef ZT_INETADDRESS_HPP
#define ZT_INETADDRESS_HPP

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <string>

#include "Constants.hpp"
#include "../include/ZeroTierOne.h"
#include "Utils.hpp"
#include "MAC.hpp"

namespace ZeroTier {

/**
 * Extends sockaddr_storage with friendly C++ methods
 *
 * This is basically a "mixin" for sockaddr_storage. It adds methods and
 * operators, but does not modify the structure. This can be cast to/from
 * sockaddr_storage and used interchangeably. Don't change this as it's
 * used in a few places.
 */
struct InetAddress : public sockaddr_storage
{
	/**
	 * Loopback IPv4 address (no port)
	 */
	static const InetAddress LO4;

	/**
	 * Loopback IPV6 address (no port)
	 */
	static const InetAddress LO6;

	/**
	 * 0.0.0.0/0
	 */
	static const InetAddress DEFAULT4;

	/**
	 * ::/0
	 */
	static const InetAddress DEFAULT6;

	/**
	 * IP address scope
	 *
	 * Be sure the integer values of these start at 0 and increment
	 * monotonically without gaps, as they're used as an array index.
	 * The NONE entry must be the last, since it's the count. It's
	 * okay to change these since they are not exported via the API.
	 */
	enum IpScope
	{
		IP_SCOPE_LOOPBACK = 0,      // 127.0.0.1
		IP_SCOPE_MULTICAST = 1,     // 224.0.0.0 and other multicast IPs
		IP_SCOPE_LINK_LOCAL = 2,    // 169.254.x.x, IPv6 LL
		IP_SCOPE_PRIVATE = 3,       // 10.x.x.x, etc.
		IP_SCOPE_PSEUDOPRIVATE = 4, // 28.x.x.x, etc. -- unofficially unrouted IP blocks often "bogarted"
		IP_SCOPE_SHARED = 5,        // 100.64.0.0/10, shared space for e.g. carrier-grade NAT
		IP_SCOPE_GLOBAL = 6,        // globally routable IP address (all others)
		IP_SCOPE_NONE = 7           // not an IP address -- also the number of classes, must be last entry
	};

	InetAddress() throw() { memset(this,0,sizeof(InetAddress)); }
	InetAddress(const InetAddress &a) throw() { memcpy(this,&a,sizeof(InetAddress)); }
	InetAddress(const struct sockaddr_storage &ss) throw() { *this = ss; }
	InetAddress(const struct sockaddr_storage *ss) throw() { *this = ss; }
	InetAddress(const struct sockaddr &sa) throw() { *this = sa; }
	InetAddress(const struct sockaddr *sa) throw() { *this = sa; }
	InetAddress(const struct sockaddr_in &sa) throw() { *this = sa; }
	InetAddress(const struct sockaddr_in *sa) throw() { *this = sa; }
	InetAddress(const struct sockaddr_in6 &sa) throw() { *this = sa; }
	InetAddress(const struct sockaddr_in6 *sa) throw() { *this = sa; }
	InetAddress(const void *ipBytes,unsigned int ipLen,unsigned int port) throw() { this->set(ipBytes,ipLen,port); }
	InetAddress(const uint32_t ipv4,unsigned int port) throw() { this->set(&ipv4,4,port); }
	InetAddress(const std::string &ip,unsigned int port) throw() { this->set(ip,port); }
	InetAddress(const std::string &ipSlashPort) throw() { this->fromString(ipSlashPort); }
	InetAddress(const char *ipSlashPort) throw() { this->fromString(std::string(ipSlashPort)); }

	inline InetAddress &operator=(const InetAddress &a)
		throw()
	{
		memcpy(this,&a,sizeof(InetAddress));
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr_storage &ss)
		throw()
	{
		memcpy(this,&ss,sizeof(InetAddress));
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr_storage *ss)
		throw()
	{
		memcpy(this,ss,sizeof(InetAddress));
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr_in &sa)
		throw()
	{
		memset(this,0,sizeof(InetAddress));
		memcpy(this,&sa,sizeof(struct sockaddr_in));
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr_in *sa)
		throw()
	{
		memset(this,0,sizeof(InetAddress));
		memcpy(this,sa,sizeof(struct sockaddr_in));
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr_in6 &sa)
		throw()
	{
		memset(this,0,sizeof(InetAddress));
		memcpy(this,&sa,sizeof(struct sockaddr_in6));
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr_in6 *sa)
		throw()
	{
		memset(this,0,sizeof(InetAddress));
		memcpy(this,sa,sizeof(struct sockaddr_in6));
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr &sa)
		throw()
	{
		memset(this,0,sizeof(InetAddress));
		switch(sa.sa_family) {
			case AF_INET:
				memcpy(this,&sa,sizeof(struct sockaddr_in));
				break;
			case AF_INET6:
				memcpy(this,&sa,sizeof(struct sockaddr_in6));
				break;
		}
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr *sa)
		throw()
	{
		memset(this,0,sizeof(InetAddress));
		switch(sa->sa_family) {
			case AF_INET:
				memcpy(this,sa,sizeof(struct sockaddr_in));
				break;
			case AF_INET6:
				memcpy(this,sa,sizeof(struct sockaddr_in6));
				break;
		}
		return *this;
	}

	/**
	 * @return IP scope classification (e.g. loopback, link-local, private, global)
	 */
	IpScope ipScope() const
		throw();

	/**
	 * Set from a string-format IP and a port
	 *
	 * @param ip IP address in V4 or V6 ASCII notation
	 * @param port Port or 0 for none
	 */
	void set(const std::string &ip,unsigned int port)
		throw();

	/**
	 * Set from a raw IP and port number
	 *
	 * @param ipBytes Bytes of IP address in network byte order
	 * @param ipLen Length of IP address: 4 or 16
	 * @param port Port number or 0 for none
	 */
	void set(const void *ipBytes,unsigned int ipLen,unsigned int port)
		throw();

	/**
	 * Set the port component
	 *
	 * @param port Port, 0 to 65535
	 */
	inline void setPort(unsigned int port)
		throw()
	{
		switch(ss_family) {
			case AF_INET:
				reinterpret_cast<struct sockaddr_in *>(this)->sin_port = Utils::hton((uint16_t)port);
				break;
			case AF_INET6:
				reinterpret_cast<struct sockaddr_in6 *>(this)->sin6_port = Utils::hton((uint16_t)port);
				break;
		}
	}

	/**
	 * @return ASCII IP/port format representation
	 */
	std::string toString() const;

	/**
	 * @return IP portion only, in ASCII string format
	 */
	std::string toIpString() const;

	/**
	 * @param ipSlashPort ASCII IP/port format notation
	 */
	void fromString(const std::string &ipSlashPort);

	/**
	 * @return Port or 0 if no port component defined
	 */
	inline unsigned int port() const
		throw()
	{
		switch(ss_family) {
			case AF_INET: return Utils::ntoh((uint16_t)(reinterpret_cast<const struct sockaddr_in *>(this)->sin_port));
			case AF_INET6: return Utils::ntoh((uint16_t)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_port));
			default: return 0;
		}
	}

	/**
	 * Alias for port()
	 *
	 * This just aliases port() to make code more readable when netmask bits
	 * are stuffed there, as they are in Network, EthernetTap, and a few other
	 * spots.
	 *
	 * @return Netmask bits
	 */
	inline unsigned int netmaskBits() const throw() { return port(); }

	/**
	 * Construct a full netmask as an InetAddress
	 */
	InetAddress netmask() const
		throw();

	/**
	 * Constructs a broadcast address from a network/netmask address
	 *
	 * @return Broadcast address (only IP portion is meaningful)
	 */
	InetAddress broadcast() const
		throw();

	/**
	 * @return True if this is an IPv4 address
	 */
	inline bool isV4() const throw() { return (ss_family == AF_INET); }

	/**
	 * @return True if this is an IPv6 address
	 */
	inline bool isV6() const throw() { return (ss_family == AF_INET6); }

	/**
	 * Force type to IPv4
	 */
	inline void setV4() throw() { ss_family = AF_INET; }

	/**
	 * Force type to IPv6
	 */
	inline void setV6() throw() { ss_family = AF_INET6; }

	/**
	 * @return Length of sockaddr_in if IPv4, sockaddr_in6 if IPv6
	 */
	inline unsigned int saddrLen() const
		throw()
	{
		switch(ss_family) {
			case AF_INET: return sizeof(struct sockaddr_in);
			case AF_INET6: return sizeof(struct sockaddr_in6);
			default: return 0;
		}
	}

	/**
	 * @return Raw sockaddr_in structure (valid if IPv4)
	 */
	inline const struct sockaddr_in *saddr4() const throw() { return reinterpret_cast<const struct sockaddr_in *>(this); }

	/**
	 * @return Raw sockaddr_in6 structure (valid if IPv6)
	 */
	inline const struct sockaddr_in6 *saddr6() const throw() { return reinterpret_cast<const struct sockaddr_in6 *>(this); }

	/**
	 * @return pointer to raw IP address bytes
	 */
	inline const void *rawIpData() const
		throw()
	{
		switch(ss_family) {
			case AF_INET: return (const void *)&(reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr);
			case AF_INET6: return (const void *)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr);
			default: return 0;
		}
	}

	/**
	 * @return pointer to raw IP address bytes
	 */
	inline void *rawIpData()
		throw()
	{
		switch(ss_family) {
			case AF_INET: return (void *)&(reinterpret_cast<struct sockaddr_in *>(this)->sin_addr.s_addr);
			case AF_INET6: return (void *)(reinterpret_cast<struct sockaddr_in6 *>(this)->sin6_addr.s6_addr);
			default: return 0;
		}
	}

	/**
	 * Set to null/zero
	 */
	inline void zero() throw() { memset(this,0,sizeof(InetAddress)); }

	/**
	 * @return True if address family is non-zero
	 */
	inline operator bool() const throw() { return (ss_family != 0); }

	inline bool operator==(const InetAddress &a) const throw() { return (memcmp(this,&a,sizeof(InetAddress)) == 0); }
	inline bool operator!=(const InetAddress &a) const throw() { return !(*this == a); }
	inline bool operator<(const InetAddress &a) const throw() { return (memcmp(this,&a,sizeof(InetAddress)) < 0); }
	inline bool operator>(const InetAddress &a) const throw() { return (a < *this); }
	inline bool operator<=(const InetAddress &a) const throw() { return !(a < *this); }
	inline bool operator>=(const InetAddress &a) const throw() { return !(*this < a); }

	/**
	 * @param mac MAC address seed
	 * @return IPv6 link-local address
	 */
	static InetAddress makeIpv6LinkLocal(const MAC &mac)
		throw();
};

} // namespace ZeroTier

#endif
