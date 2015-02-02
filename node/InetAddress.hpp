/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2015  ZeroTier Networks
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
#include "Utils.hpp"
#include "MAC.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

namespace ZeroTier {

/**
 * Wrapper for sockaddr structures for IPV4 and IPV6
 *
 * Note: this class is raw memcpy'able, which is used in a couple places.
 */
class InetAddress
{
public:
	/**
	 * Address type
	 */
	enum AddressType
	{
		TYPE_NULL = 0,
		TYPE_IPV4 = AF_INET,
		TYPE_IPV6 = AF_INET6
	};

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

	InetAddress() throw() { memset(&_sa,0,sizeof(_sa)); }
	InetAddress(const InetAddress &a) throw() { memcpy(&_sa,&a._sa,sizeof(_sa)); }
	InetAddress(const struct sockaddr *sa) throw() { this->set(sa); }
	InetAddress(const void *ipBytes,unsigned int ipLen,unsigned int port) throw() { this->set(ipBytes,ipLen,port); }
	InetAddress(const uint32_t ipv4,unsigned int port) throw() { this->set(&ipv4,4,port); }
	InetAddress(const std::string &ip,unsigned int port) throw() { this->set(ip,port); }
	InetAddress(const std::string &ipSlashPort) throw() { this->fromString(ipSlashPort); }
	InetAddress(const char *ipSlashPort) throw() { this->fromString(std::string(ipSlashPort)); }

	inline InetAddress &operator=(const InetAddress &a)
		throw()
	{
		memcpy(&_sa,&a._sa,sizeof(_sa));
		return *this;
	}

	/**
	 * Set from an OS-level sockaddr structure
	 *
	 * @param sa Socket address (V4 or V6)
	 */
	inline void set(const struct sockaddr *sa)
		throw()
	{
		switch(sa->sa_family) {
			case AF_INET: memcpy(&_sa.sin,sa,sizeof(struct sockaddr_in)); break;
			case AF_INET6: memcpy(&_sa.sin6,sa,sizeof(struct sockaddr_in6)); break;
			default: memset(&_sa,0,sizeof(_sa)); break;
		}
	}

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
		if (_sa.saddr.sa_family == AF_INET)
			_sa.sin.sin_port = Utils::hton((uint16_t)port);
		else if (_sa.saddr.sa_family == AF_INET6)
			_sa.sin6.sin6_port = Utils::hton((uint16_t)port);
	}

	/**
	 * @return True if this is a link-local IP address
	 */
	bool isLinkLocal() const
		throw();

	/**
	 * @return True if this ip/netmask would represent a default route (e.g. 0.0.0.0/0)
	 */
	bool isDefaultRoute() const
		throw();

	/**
	 * @return True if this is a loopback address
	 */
	inline bool isLoopback() const
		throw()
	{
		return ((*this == LO4)||(*this == LO6));
	}

	/**
	 * @return ASCII IP/port format representation
	 */
	std::string toString() const;

	/**
	 * @param ipSlashPort ASCII IP/port format notation
	 */
	void fromString(const std::string &ipSlashPort);

	/**
	 * @return IP portion only, in ASCII string format
	 */
	std::string toIpString() const;

	/**
	 * @return Port or 0 if no port component defined
	 */
	inline unsigned int port() const
		throw()
	{
		switch(_sa.saddr.sa_family) {
			case AF_INET: return Utils::ntoh((uint16_t)_sa.sin.sin_port);
			case AF_INET6: return Utils::ntoh((uint16_t)_sa.sin6.sin6_port);
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
	inline bool isV4() const throw() { return (_sa.saddr.sa_family == AF_INET); }

	/**
	 * @return True if this is an IPv6 address
	 */
	inline bool isV6() const throw() { return (_sa.saddr.sa_family == AF_INET6); }

	/**
	 * @return Address type or TYPE_NULL if not defined
	 */
	inline AddressType type() const throw() { return (AddressType)_sa.saddr.sa_family; }

	/**
	 * Force type to IPv4
	 */
	inline void setV4() throw() { _sa.saddr.sa_family = AF_INET; }

	/**
	 * Force type to IPv6
	 */
	inline void setV6() throw() { _sa.saddr.sa_family = AF_INET6; }

	/**
	 * @return Raw sockaddr structure
	 */
	inline struct sockaddr *saddr() throw() { return &(_sa.saddr); }
	inline const struct sockaddr *saddr() const throw() { return &(_sa.saddr); }

	/**
	 * @return Length of sockaddr_in if IPv4, sockaddr_in6 if IPv6
	 */
	inline unsigned int saddrLen() const
		throw()
	{
		switch(_sa.saddr.sa_family) {
			case AF_INET: return sizeof(struct sockaddr_in);
			case AF_INET6: return sizeof(struct sockaddr_in6);
			default: return 0;
		}
	}

	/**
	 * @return Checksum of this address (not portable, so don't use for long-term storage purposes)
	 */
	inline uint64_t hashCode() const
	{
		switch(_sa.saddr.sa_family) {
			case AF_INET:
				return ((uint64_t)_sa.sin.sin_port + (uint64_t)(_sa.sin.sin_addr.s_addr));
			case AF_INET6:
				return ((uint64_t)_sa.sin6.sin6_port + ( ((const uint64_t *)_sa.sin6.sin6_addr.s6_addr)[0] ^ ((const uint64_t *)_sa.sin6.sin6_addr.s6_addr)[1] ));
		}
		return 0;
	}

	/**
	 * @return Combined length of internal structure, room for either V4 or V6
	 */
	inline unsigned int saddrSpaceLen() const throw() { return sizeof(_sa); }

	/**
	 * @return Raw sockaddr_in structure (valid if IPv4)
	 */
	inline const struct sockaddr_in *saddr4() const throw() { return &(_sa.sin); }

	/**
	 * @return Raw sockaddr_in6 structure (valid if IPv6)
	 */
	inline const struct sockaddr_in6 *saddr6() const throw() { return &(_sa.sin6); }

	/**
	 * @return Raw IP address (4 bytes for IPv4, 16 bytes for IPv6)
	 */
	inline void *rawIpData() throw() { return ((_sa.saddr.sa_family == AF_INET) ? (void *)(&(_sa.sin.sin_addr.s_addr)) : (void *)_sa.sin6.sin6_addr.s6_addr); }
	inline const void *rawIpData() const throw() { return ((_sa.saddr.sa_family == AF_INET) ? (void *)(&(_sa.sin.sin_addr.s_addr)) : (void *)_sa.sin6.sin6_addr.s6_addr); }

	/**
	 * Compare only the IP portions of addresses, ignoring port/netmask
	 *
	 * @param a Address to compare
	 * @return True if both addresses are of the same (valid) type and their IPs match
	 */
	inline bool ipsEqual(const InetAddress &a) const
		throw()
	{
		if (_sa.saddr.sa_family == a._sa.saddr.sa_family) {
			switch(_sa.saddr.sa_family) {
				case AF_INET:
					return (_sa.sin.sin_addr.s_addr == a._sa.sin.sin_addr.s_addr);
				case AF_INET6:
					return (!memcmp(_sa.sin6.sin6_addr.s6_addr,a._sa.sin6.sin6_addr.s6_addr,16));
			}
		}
		return false;
	}

	/**
	 * Compare IP/netmask with another IP/netmask
	 *
	 * @param ipnet IP/netmask to compare with
	 * @return True if [netmask] bits match
	 */
	bool sameNetworkAs(const InetAddress &ipnet) const
		throw();

	/**
	 * Determine whether this address is within an ip/netmask
	 *
	 * @param ipnet IP/netmask
	 * @return True if this address is within this network
	 */
	bool within(const InetAddress &ipnet) const
		throw();

	/**
	 * Set to null/zero
	 */
	inline void zero() throw() { memset(&_sa,0,sizeof(_sa)); }

	/**
	 * @return True if address family is non-zero
	 */
	inline operator bool() const throw() { return ((_sa.saddr.sa_family == AF_INET)||(_sa.saddr.sa_family == AF_INET6)); }

	bool operator==(const InetAddress &a) const throw();
	inline bool operator!=(const InetAddress &a) const throw() { return !(*this == a); }
	bool operator<(const InetAddress &a) const throw();
	inline bool operator>(const InetAddress &a) const throw() { return (a < *this); }
	inline bool operator<=(const InetAddress &a) const throw() { return !(a < *this); }
	inline bool operator>=(const InetAddress &a) const throw() { return !(*this < a); }

	/**
	 * @param mac MAC address seed
	 * @return IPv6 link-local address
	 */
	static InetAddress makeIpv6LinkLocal(const MAC &mac)
		throw();

private:
	union {
		struct sockaddr saddr;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} _sa;
};

} // namespace ZeroTier

#endif
