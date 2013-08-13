/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#ifndef _ZT_INETADDRESS_HPP
#define _ZT_INETADDRESS_HPP

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <string>

#include "Constants.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace ZeroTier {

/**
 * Wrapper for sockaddr structures for IPV4 and IPV6
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

	InetAddress()
		throw()
	{
		memset(&_sa,0,sizeof(_sa));
	}

	InetAddress(const InetAddress &a)
		throw()
	{
		memcpy(&_sa,&a._sa,sizeof(_sa));
	}

	InetAddress(const struct sockaddr *sa)
		throw()
	{
		this->set(sa);
	}

	InetAddress(const void *ipBytes,unsigned int ipLen,unsigned int port)
		throw()
	{
		this->set(ipBytes,ipLen,port);
	}

	InetAddress(const std::string &ip,unsigned int port)
		throw()
	{
		this->set(ip,port);
	}

	InetAddress(const std::string &ipSlashPort)
		throw()
	{
		this->fromString(ipSlashPort);
	}

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
			case AF_INET:
				memcpy(&_sa.sin,sa,sizeof(struct sockaddr_in));
				break;
			case AF_INET6:
				memcpy(&_sa.sin6,sa,sizeof(struct sockaddr_in6));
				break;
			default:
				_sa.saddr.sa_family = 0;
				break;
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
	inline void set(const void *ipBytes,unsigned int ipLen,unsigned int port)
		throw()
	{
		_sa.saddr.sa_family = 0;
		if (ipLen == 4) {
			setV4();
			memcpy(rawIpData(),ipBytes,4);
			setPort(port);
		} else if (ipLen == 16) {
			setV6();
			memcpy(rawIpData(),ipBytes,16);
			setPort(port);
		}
	}

	/**
	 * Set the port component
	 *
	 * @param port Port, 0 to 65535
	 */
	inline void setPort(unsigned int port)
		throw()
	{
		if (_sa.saddr.sa_family == AF_INET)
			_sa.sin.sin_port = htons((uint16_t)port);
		else if (_sa.saddr.sa_family == AF_INET6)
			_sa.sin6.sin6_port = htons((uint16_t)port);
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
			case AF_INET:
				return ntohs(_sa.sin.sin_port);
			case AF_INET6:
				return ntohs(_sa.sin6.sin6_port);
		}
		return 0;
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
	inline unsigned int netmaskBits() const
		throw()
	{
		return port();
	}

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
		return (isV4() ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
	}

	/**
	 * @return Combined length of internal structure, room for either V4 or V6
	 */
	inline unsigned int saddrSpaceLen() const
		throw()
	{
		return sizeof(_sa);
	}

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
	 * Compare only the IP portions of addresses, ignoring port
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

	bool operator==(const InetAddress &a) const throw();
	inline bool operator!=(const InetAddress &a) const throw() { return !(*this == a); }
	bool operator<(const InetAddress &a) const throw();
	inline bool operator>(const InetAddress &a) const throw() { return (a < *this); }
	inline bool operator<=(const InetAddress &a) const throw() { return !(a < *this); }
	inline bool operator>=(const InetAddress &a) const throw() { return !(*this < a); }

	/**
	 * @return True if address family is non-zero
	 */
	inline operator bool() const throw() { return ((_sa.saddr.sa_family == AF_INET)||(_sa.saddr.sa_family == AF_INET6)); }

	/**
	 * Set to null/zero
	 */
	inline void zero()
		throw()
	{
		_sa.saddr.sa_family = 0;
	}

private:
	union {
		struct sockaddr saddr;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} _sa;
};

} // namespace ZeroTier

#endif
