/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_INETADDRESS_HPP
#define ZT_INETADDRESS_HPP

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "Constants.hpp"
#include "../include/ZeroTierOne.h"
#include "Utils.hpp"
#include "MAC.hpp"
#include "Buffer.hpp"

namespace ZeroTier {

/**
 * Maximum integer value of enum IpScope
 */
#define ZT_INETADDRESS_MAX_SCOPE 7

/**
 * Extends sockaddr_storage with friendly C++ methods
 *
 * This is basically a "mixin" for sockaddr_storage. It adds methods and
 * operators, but does not modify the structure. This can be cast to/from
 * sockaddr_storage and used interchangeably. DO NOT change this by e.g.
 * adding non-static fields, since much code depends on this identity.
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
	 * Null address
	 */
	static const InetAddress NIL;

	/**
	 * IP address scope
	 *
	 * Note that these values are in ascending order of path preference and
	 * MUST remain that way or Path must be changed to reflect. Also be sure
	 * to change ZT_INETADDRESS_MAX_SCOPE if the max changes.
	 */
	enum IpScope
	{
		IP_SCOPE_NONE = 0,          // NULL or not an IP address
		IP_SCOPE_MULTICAST = 1,     // 224.0.0.0 and other V4/V6 multicast IPs
		IP_SCOPE_LOOPBACK = 2,      // 127.0.0.1, ::1, etc.
		IP_SCOPE_PSEUDOPRIVATE = 3, // 28.x.x.x, etc. -- unofficially unrouted IPv4 blocks often "bogarted"
		IP_SCOPE_GLOBAL = 4,        // globally routable IP address (all others)
		IP_SCOPE_LINK_LOCAL = 5,    // 169.254.x.x, IPv6 LL
		IP_SCOPE_SHARED = 6,        // currently unused, formerly used for carrier-grade NAT ranges
		IP_SCOPE_PRIVATE = 7        // 10.x.x.x, 192.168.x.x, etc.
	};

	// Can be used with the unordered maps and sets in c++11. We don't use C++11 in the core
	// but this is safe to put here.
	struct Hasher
	{
		inline std::size_t operator()(const InetAddress &a) const { return (std::size_t)a.hashCode(); }
	};

	inline InetAddress() { memset(this,0,sizeof(InetAddress)); }
	inline InetAddress(const InetAddress &a) { memcpy(this,&a,sizeof(InetAddress)); }
	inline InetAddress(const InetAddress *a) { memcpy(this,a,sizeof(InetAddress)); }
	inline InetAddress(const struct sockaddr_storage &ss) { *this = ss; }
	inline InetAddress(const struct sockaddr_storage *ss) { *this = ss; }
	inline InetAddress(const struct sockaddr &sa) { *this = sa; }
	inline InetAddress(const struct sockaddr *sa) { *this = sa; }
	inline InetAddress(const struct sockaddr_in &sa) { *this = sa; }
	inline InetAddress(const struct sockaddr_in *sa) { *this = sa; }
	inline InetAddress(const struct sockaddr_in6 &sa) { *this = sa; }
	inline InetAddress(const struct sockaddr_in6 *sa) { *this = sa; }
	inline InetAddress(const void *ipBytes,unsigned int ipLen,unsigned int port) { this->set(ipBytes,ipLen,port); }
	inline InetAddress(const uint32_t ipv4,unsigned int port) { this->set(&ipv4,4,port); }
	inline InetAddress(const char *ipSlashPort) { this->fromString(ipSlashPort); }

	inline void clear() { memset(this,0,sizeof(InetAddress)); }

	inline InetAddress &operator=(const InetAddress &a)
	{
		if (&a != this)
			memcpy(this,&a,sizeof(InetAddress));
		return *this;
	}

	inline InetAddress &operator=(const InetAddress *a)
	{
		if (a != this)
			memcpy(this,a,sizeof(InetAddress));
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr_storage &ss)
	{
		if (reinterpret_cast<const InetAddress *>(&ss) != this)
			memcpy(this,&ss,sizeof(InetAddress));
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr_storage *ss)
	{
		if (reinterpret_cast<const InetAddress *>(ss) != this)
			memcpy(this,ss,sizeof(InetAddress));
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr_in &sa)
	{
		if (reinterpret_cast<const InetAddress *>(&sa) != this) {
			memset(this,0,sizeof(InetAddress));
			memcpy(this,&sa,sizeof(struct sockaddr_in));
		}
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr_in *sa)
	{
		if (reinterpret_cast<const InetAddress *>(sa) != this) {
			memset(this,0,sizeof(InetAddress));
			memcpy(this,sa,sizeof(struct sockaddr_in));
		}
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr_in6 &sa)
	{
		if (reinterpret_cast<const InetAddress *>(&sa) != this) {
			memset(this,0,sizeof(InetAddress));
			memcpy(this,&sa,sizeof(struct sockaddr_in6));
		}
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr_in6 *sa)
	{
		if (reinterpret_cast<const InetAddress *>(sa) != this) {
			memset(this,0,sizeof(InetAddress));
			memcpy(this,sa,sizeof(struct sockaddr_in6));
		}
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr &sa)
	{
		if (reinterpret_cast<const InetAddress *>(&sa) != this) {
			memset(this,0,sizeof(InetAddress));
			switch(sa.sa_family) {
				case AF_INET:
					memcpy(this,&sa,sizeof(struct sockaddr_in));
					break;
				case AF_INET6:
					memcpy(this,&sa,sizeof(struct sockaddr_in6));
					break;
			}
		}
		return *this;
	}

	inline InetAddress &operator=(const struct sockaddr *sa)
	{
		if (reinterpret_cast<const InetAddress *>(sa) != this) {
			memset(this,0,sizeof(InetAddress));
			switch(sa->sa_family) {
				case AF_INET:
					memcpy(this,sa,sizeof(struct sockaddr_in));
					break;
				case AF_INET6:
					memcpy(this,sa,sizeof(struct sockaddr_in6));
					break;
			}
		}
		return *this;
	}

	/**
	 * @return IP scope classification (e.g. loopback, link-local, private, global)
	 */
	IpScope ipScope() const;

	/**
	 * Set from a raw IP and port number
	 *
	 * @param ipBytes Bytes of IP address in network byte order
	 * @param ipLen Length of IP address: 4 or 16
	 * @param port Port number or 0 for none
	 */
	void set(const void *ipBytes,unsigned int ipLen,unsigned int port);

	/**
	 * Set the port component
	 *
	 * @param port Port, 0 to 65535
	 */
	inline void setPort(unsigned int port)
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
	 * @return True if this network/netmask route describes a default route (e.g. 0.0.0.0/0)
	 */
	inline bool isDefaultRoute() const
	{
		switch(ss_family) {
			case AF_INET:
				return ( (reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr == 0) && (reinterpret_cast<const struct sockaddr_in *>(this)->sin_port == 0) );
			case AF_INET6:
				const uint8_t *ipb = reinterpret_cast<const uint8_t *>(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr);
				for(int i=0;i<16;++i) {
					if (ipb[i])
						return false;
				}
				return (reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_port == 0);
		}
		return false;
	}

	/**
	 * @return ASCII IP/port format representation
	 */
	char *toString(char buf[64]) const;

	/**
	 * @return IP portion only, in ASCII string format
	 */
	char *toIpString(char buf[64]) const;

	/**
	 * @param ipSlashPort IP/port (port is optional, will be 0 if not included)
	 * @return True if address appeared to be valid
	 */
	bool fromString(const char *ipSlashPort);

	/**
	 * @return Port or 0 if no port component defined
	 */
	inline unsigned int port() const
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
	inline unsigned int netmaskBits() const { return port(); }

	/**
	 * @return True if netmask bits is valid for the address type
	 */
	inline bool netmaskBitsValid() const
	{
		const unsigned int n = port();
		switch(ss_family) {
			case AF_INET: return (n <= 32);
			case AF_INET6: return (n <= 128);
		}
		return false;
	}

	/**
	 * Alias for port()
	 *
	 * This just aliases port() because for gateways we use this field to
	 * store the gateway metric.
	 *
	 * @return Gateway metric
	 */
	inline unsigned int metric() const { return port(); }

	/**
	 * Construct a full netmask as an InetAddress
	 *
	 * @return Netmask such as 255.255.255.0 if this address is /24 (port field will be unchanged)
	 */
	InetAddress netmask() const;

	/**
	 * Constructs a broadcast address from a network/netmask address
	 *
	 * This is only valid for IPv4 and will return a NULL InetAddress for other
	 * address families.
	 *
	 * @return Broadcast address (only IP portion is meaningful)
	 */
	InetAddress broadcast() const;

	/**
	 * Return the network -- a.k.a. the IP ANDed with the netmask
	 *
	 * @return Network e.g. 10.0.1.0/24 from 10.0.1.200/24
	 */
	InetAddress network() const;

	/**
	 * Test whether this IPv6 prefix matches the prefix of a given IPv6 address
	 *
	 * @param addr Address to check
	 * @return True if this IPv6 prefix matches the prefix of a given IPv6 address
	 */
	bool isEqualPrefix(const InetAddress &addr) const;

	/**
	 * Test whether this IP/netmask contains this address
	 *
	 * @param addr Address to check
	 * @return True if this IP/netmask (route) contains this address
	 */
	bool containsAddress(const InetAddress &addr) const;

	/**
	 * @return True if this is an IPv4 address
	 */
	inline bool isV4() const { return (ss_family == AF_INET); }

	/**
	 * @return True if this is an IPv6 address
	 */
	inline bool isV6() const { return (ss_family == AF_INET6); }

	/**
	 * @return pointer to raw address bytes or NULL if not available
	 */
	inline const void *rawIpData() const
	{
		switch(ss_family) {
			case AF_INET: return (const void *)&(reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr);
			case AF_INET6: return (const void *)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr);
			default: return 0;
		}
	}

	/**
	 * @return InetAddress containing only the IP portion of this address and a zero port, or NULL if not IPv4 or IPv6
	 */
	inline InetAddress ipOnly() const
	{
		InetAddress r;
		switch(ss_family) {
			case AF_INET:
				r.ss_family = AF_INET;
				reinterpret_cast<struct sockaddr_in *>(&r)->sin_addr.s_addr = reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr;
				break;
			case AF_INET6:
				r.ss_family = AF_INET6;
				memcpy(reinterpret_cast<struct sockaddr_in6 *>(&r)->sin6_addr.s6_addr,reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr,16);
				break;
		}
		return r;
	}

	/**
	 * Performs an IP-only comparison or, if that is impossible, a memcmp()
	 *
	 * @param a InetAddress to compare again
	 * @return True if only IP portions are equal (false for non-IP or null addresses)
	 */
	inline bool ipsEqual(const InetAddress &a) const
	{
		if (ss_family == a.ss_family) {
			if (ss_family == AF_INET)
				return (reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr == reinterpret_cast<const struct sockaddr_in *>(&a)->sin_addr.s_addr);
			if (ss_family == AF_INET6)
				return (memcmp(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr,reinterpret_cast<const struct sockaddr_in6 *>(&a)->sin6_addr.s6_addr,16) == 0);
			return (memcmp(this,&a,sizeof(InetAddress)) == 0);
		}
		return false;
	}

	/**
	 * Performs an IP-only comparison or, if that is impossible, a memcmp()
	 *
	 * This version compares only the first 64 bits of IPv6 addresses.
	 *
	 * @param a InetAddress to compare again
	 * @return True if only IP portions are equal (false for non-IP or null addresses)
	 */
	inline bool ipsEqual2(const InetAddress &a) const
	{
		if (ss_family == a.ss_family) {
			if (ss_family == AF_INET)
				return (reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr == reinterpret_cast<const struct sockaddr_in *>(&a)->sin_addr.s_addr);
			if (ss_family == AF_INET6)
				return (memcmp(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr,reinterpret_cast<const struct sockaddr_in6 *>(&a)->sin6_addr.s6_addr,8) == 0);
			return (memcmp(this,&a,sizeof(InetAddress)) == 0);
		}
		return false;
	}

	inline unsigned long hashCode() const
	{
		if (ss_family == AF_INET) {
			return ((unsigned long)reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr + (unsigned long)reinterpret_cast<const struct sockaddr_in *>(this)->sin_port);
		} else if (ss_family == AF_INET6) {
			unsigned long tmp = reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_port;
			const uint8_t *a = reinterpret_cast<const uint8_t *>(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr);
			for(long i=0;i<16;++i)
				reinterpret_cast<uint8_t *>(&tmp)[i % sizeof(tmp)] ^= a[i];
			return tmp;
		} else {
			unsigned long tmp = reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_port;
			const uint8_t *a = reinterpret_cast<const uint8_t *>(this);
			for(long i=0;i<(long)sizeof(InetAddress);++i)
				reinterpret_cast<uint8_t *>(&tmp)[i % sizeof(tmp)] ^= a[i];
			return tmp;
		}
	}

	/**
	 * Set to null/zero
	 */
	inline void zero() { memset(this,0,sizeof(InetAddress)); }

	/**
	 * Check whether this is a network/route rather than an IP assignment
	 *
	 * A network is an IP/netmask where everything after the netmask is
	 * zero e.g. 10.0.0.0/8.
	 *
	 * @return True if everything after netmask bits is zero
	 */
	bool isNetwork() const;

	/**
	 * @return 14-bit (0-16383) hash of this IP's first 24 or 48 bits (for V4 or V6) for rate limiting code, or 0 if non-IP
	 */
	inline unsigned long rateGateHash() const
	{
		unsigned long h = 0;
		switch(ss_family) {
			case AF_INET:
				h = (Utils::ntoh((uint32_t)reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr) & 0xffffff00) >> 8;
				h ^= (h >> 14);
				break;
			case AF_INET6: {
				const uint8_t *ip = reinterpret_cast<const uint8_t *>(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr);
				h = ((unsigned long)ip[0]); h <<= 1;
				h += ((unsigned long)ip[1]); h <<= 1;
				h += ((unsigned long)ip[2]); h <<= 1;
				h += ((unsigned long)ip[3]); h <<= 1;
				h += ((unsigned long)ip[4]); h <<= 1;
				h += ((unsigned long)ip[5]);
			}	break;
		}
		return (h & 0x3fff);
	}

	/**
	 * @return True if address family is non-zero
	 */
	inline operator bool() const { return (ss_family != 0); }

	// Marshal interface ///////////////////////////////////////////////////////
	static inline int marshalSizeMax() { return 19; }
	inline int marshal(uint8_t restrict data[19]) const
	{
		switch(ss_family) {
			case AF_INET:
				const unsigned int port = Utils::ntoh((uint16_t)reinterpret_cast<const sockaddr_in *>(this)->sin_port);
				data[0] = 4;
				data[1] = reinterpret_cast<const uint8_t *>(&(reinterpret_cast<const sockaddr_in *>(this)->sin_addr.s_addr))[0];
				data[2] = reinterpret_cast<const uint8_t *>(&(reinterpret_cast<const sockaddr_in *>(this)->sin_addr.s_addr))[1];
				data[3] = reinterpret_cast<const uint8_t *>(&(reinterpret_cast<const sockaddr_in *>(this)->sin_addr.s_addr))[2];
				data[4] = reinterpret_cast<const uint8_t *>(&(reinterpret_cast<const sockaddr_in *>(this)->sin_addr.s_addr))[3];
				data[5] = (uint8_t)((port >> 8) & 0xff);
				data[6] = (uint8_t)(port & 0xff);
				return 7;
			case AF_INET6:
				const unsigned int port = Utils::ntoh((uint16_t)reinterpret_cast<const sockaddr_in6 *>(this)->sin6_port);
				data[0] = 6;
				for(int i=0;i<16;++i)
					data[i+1] = reinterpret_cast<const sockaddr_in6 *>(this)->sin6_addr.s6_addr[i];
				data[17] = (uint8_t)((port >> 8) & 0xff);
				data[18] = (uint8_t)(port & 0xff);
				return 19;
			default:
				data[0] = 0;
				return 1;
		}
	}
	inline int unmarshal(const uint8_t *restrict data,const int len)
	{
		if (len <= 0)
			return -1;
		switch(data[0]) {
			case 0:
				return 1;
			case 4:
				if (len < 7)
					return -1;
				memset(this,0,sizeof(InetAddress));
				reinterpret_cast<sockaddr_in *>(this)->sin_family = AF_INET;
				reinterpret_cast<uint8_t *>(&(reinterpret_cast<sockaddr_in *>(this)->sin_addr.s_addr))[0] = data[1];
				reinterpret_cast<uint8_t *>(&(reinterpret_cast<sockaddr_in *>(this)->sin_addr.s_addr))[1] = data[2];
				reinterpret_cast<uint8_t *>(&(reinterpret_cast<sockaddr_in *>(this)->sin_addr.s_addr))[2] = data[3];
				reinterpret_cast<uint8_t *>(&(reinterpret_cast<sockaddr_in *>(this)->sin_addr.s_addr))[3] = data[4];
				reinterpret_cast<sockaddr_in *>(this)->sin_port = Utils::hton((((uint16_t)data[5]) << 8) | (uint16_t)data[6]);
				return 7;
			case 6:
				if (len < 19)
					return -1;
				memset(this,0,sizeof(InetAddress));
				reinterpret_cast<sockaddr_in6 *>(this)->sin6_family = AF_INET6;
				for(int i=0;i<16;i++)
					(reinterpret_cast<sockaddr_in6 *>(this)->sin6_addr.s6_addr)[i] = data[i+1];
				reinterpret_cast<sockaddr_in6 *>(this)->sin6_port = Utils::hton((((uint16_t)data[17]) << 8) | (uint16_t)data[18]);
				return 19;
			default:
				return -1;
		}
	}
	////////////////////////////////////////////////////////////////////////////

	template<unsigned int C>
	inline void serialize(Buffer<C> &b) const
	{
		// This is used in the protocol and must be the same as describe in places
		// like VERB_HELLO in Packet.hpp.
		switch(ss_family) {
			case AF_INET:
				b.append((uint8_t)0x04);
				b.append(&(reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr),4);
				b.append((uint16_t)port()); // just in case sin_port != uint16_t
				return;
			case AF_INET6:
				b.append((uint8_t)0x06);
				b.append(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr,16);
				b.append((uint16_t)port()); // just in case sin_port != uint16_t
				return;
			default:
				b.append((uint8_t)0);
				return;
		}
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		memset(this,0,sizeof(InetAddress));
		unsigned int p = startAt;
		switch(b[p++]) {
			case 0:
				return 1;
			case 0x01:
				// TODO: Ethernet address (but accept for forward compatibility)
				return 7;
			case 0x02:
				// TODO: Bluetooth address (but accept for forward compatibility)
				return 7;
			case 0x03:
				// TODO: Other address types (but accept for forward compatibility)
				// These could be extended/optional things like AF_UNIX, LTE Direct, shared memory, etc.
				return (unsigned int)(b.template at<uint16_t>(p) + 3); // other addresses begin with 16-bit non-inclusive length
			case 0x04:
				ss_family = AF_INET;
				memcpy(&(reinterpret_cast<struct sockaddr_in *>(this)->sin_addr.s_addr),b.field(p,4),4); p += 4;
				reinterpret_cast<struct sockaddr_in *>(this)->sin_port = Utils::hton(b.template at<uint16_t>(p)); p += 2;
				break;
			case 0x06:
				ss_family = AF_INET6;
				memcpy(reinterpret_cast<struct sockaddr_in6 *>(this)->sin6_addr.s6_addr,b.field(p,16),16); p += 16;
				reinterpret_cast<struct sockaddr_in *>(this)->sin_port = Utils::hton(b.template at<uint16_t>(p)); p += 2;
				break;
			default:
				throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_BAD_ENCODING;
		}
		return (p - startAt);
	}

	bool operator==(const InetAddress &a) const;
	bool operator<(const InetAddress &a) const;
	inline bool operator!=(const InetAddress &a) const { return !(*this == a); }
	inline bool operator>(const InetAddress &a) const { return (a < *this); }
	inline bool operator<=(const InetAddress &a) const { return !(a < *this); }
	inline bool operator>=(const InetAddress &a) const { return !(*this < a); }

	/**
	 * @param mac MAC address seed
	 * @return IPv6 link-local address
	 */
	static InetAddress makeIpv6LinkLocal(const MAC &mac);

	/**
	 * Compute private IPv6 unicast address from network ID and ZeroTier address
	 *
	 * This generates a private unicast IPv6 address that is mostly compliant
	 * with the letter of RFC4193 and certainly compliant in spirit.
	 *
	 * RFC4193 specifies a format of:
	 *
	 * | 7 bits |1|  40 bits   |  16 bits  |          64 bits           |
	 * | Prefix |L| Global ID  | Subnet ID |        Interface ID        |
	 *
	 * The 'L' bit is set to 1, yielding an address beginning with 0xfd. Then
	 * the network ID is filled into the global ID, subnet ID, and first byte
	 * of the "interface ID" field. Since the first 40 bits of the network ID
	 * is the unique ZeroTier address of its controller, this makes a very
	 * good random global ID. Since network IDs have 24 more bits, we let it
	 * overflow into the interface ID.
	 *
	 * After that we pad with two bytes: 0x99, 0x93, namely the default ZeroTier
	 * port in hex.
	 *
	 * Finally we fill the remaining 40 bits of the interface ID field with
	 * the 40-bit unique ZeroTier device ID of the network member.
	 *
	 * This yields a valid RFC4193 address with a random global ID, a
	 * meaningful subnet ID, and a unique interface ID, all mappable back onto
	 * ZeroTier space.
	 *
	 * This in turn could allow us, on networks numbered this way, to emulate
	 * IPv6 NDP and eliminate all multicast. This could be beneficial for
	 * small devices and huge networks, e.g. IoT applications.
	 *
	 * The returned address is given an odd prefix length of /88, since within
	 * a given network only the last 40 bits (device ID) are variable. This
	 * is a bit unusual but as far as we know should not cause any problems with
	 * any non-braindead IPv6 stack.
	 *
	 * @param nwid 64-bit network ID
	 * @param zeroTierAddress 40-bit device address (in least significant 40 bits, highest 24 bits ignored)
	 * @return IPv6 private unicast address with /88 netmask
	 */
	static InetAddress makeIpv6rfc4193(uint64_t nwid,uint64_t zeroTierAddress);

	/**
	 * Compute a private IPv6 "6plane" unicast address from network ID and ZeroTier address
	 */
	static InetAddress makeIpv66plane(uint64_t nwid,uint64_t zeroTierAddress);
};

} // namespace ZeroTier

#endif
