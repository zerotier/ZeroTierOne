/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_INETADDRESS_HPP
#define ZT_INETADDRESS_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "MAC.hpp"
#include "Containers.hpp"
#include "TriviallyCopyable.hpp"

namespace ZeroTier {

#define ZT_INETADDRESS_MARSHAL_SIZE_MAX 19
#define ZT_INETADDRESS_STRING_SIZE_MAX 64

/**
 * C++ class that overlaps in size with sockaddr_storage and adds convenience methods
 *
 * This is basically a "mixin" for sockaddr_storage. It adds methods and
 * operators, but does not modify the structure. This can be cast to/from
 * sockaddr_storage and used interchangeably. DO NOT change this by e.g.
 * adding non-static fields, since much code depends on this identity.
 */
struct InetAddress : public TriviallyCopyable
{
public:
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

	// Hasher for unordered sets and maps in C++11
	struct Hasher
	{
		ZT_INLINE std::size_t operator()(const InetAddress &a) const noexcept
		{ return (std::size_t) a.hashCode(); }
	};

	ZT_INLINE InetAddress() noexcept
	{ memoryZero(this); }

	explicit ZT_INLINE InetAddress(const sockaddr_storage &ss) noexcept
	{ *this = ss; }

	explicit ZT_INLINE InetAddress(const sockaddr_storage *const ss) noexcept
	{ *this = ss; }

	explicit ZT_INLINE InetAddress(const sockaddr &sa) noexcept
	{ *this = sa; }

	explicit ZT_INLINE InetAddress(const sockaddr *const sa) noexcept
	{ *this = sa; }

	explicit ZT_INLINE InetAddress(const sockaddr_in &sa) noexcept
	{ *this = sa; }

	explicit ZT_INLINE InetAddress(const sockaddr_in *const sa) noexcept
	{ *this = sa; }

	explicit ZT_INLINE InetAddress(const sockaddr_in6 &sa) noexcept
	{ *this = sa; }

	explicit ZT_INLINE InetAddress(const sockaddr_in6 *const sa) noexcept
	{ *this = sa; }

	ZT_INLINE InetAddress(const void *const ipBytes, const unsigned int ipLen, const unsigned int port) noexcept
	{ this->set(ipBytes, ipLen, port); }

	ZT_INLINE InetAddress(const uint32_t ipv4, const unsigned int port) noexcept
	{ this->set(&ipv4, 4, port); }

	explicit ZT_INLINE InetAddress(const char *const ipSlashPort) noexcept
	{ this->fromString(ipSlashPort); }

	ZT_INLINE InetAddress &operator=(const sockaddr_storage &ss) noexcept
	{
		as.ss = ss;
		return *this;
	}

	ZT_INLINE InetAddress &operator=(const sockaddr_storage *ss) noexcept
	{
		if (ss)
			as.ss = *ss;
		else memoryZero(this);
		return *this;
	}

	ZT_INLINE InetAddress &operator=(const sockaddr_in &sa) noexcept
	{
		as.sa_in = sa;
		return *this;
	}

	ZT_INLINE InetAddress &operator=(const sockaddr_in *sa) noexcept
	{
		if (sa)
			as.sa_in = *sa;
		else memoryZero(this);
		return *this;
	}

	ZT_INLINE InetAddress &operator=(const sockaddr_in6 &sa) noexcept
	{
		as.sa_in6 = sa;
		return *this;
	}

	ZT_INLINE InetAddress &operator=(const sockaddr_in6 *sa) noexcept
	{
		if (sa)
			as.sa_in6 = *sa;
		else memoryZero(this);
		return *this;
	}

	ZT_INLINE InetAddress &operator=(const sockaddr &sa) noexcept
	{
		if (sa.sa_family == AF_INET)
			as.sa_in = *reinterpret_cast<const sockaddr_in *>(&sa);
		else if (sa.sa_family == AF_INET6)
			as.sa_in6 = *reinterpret_cast<const sockaddr_in6 *>(&sa);
		else memoryZero(this);
		return *this;
	}

	ZT_INLINE InetAddress &operator=(const sockaddr *sa) noexcept
	{
		if (sa) {
			if (sa->sa_family == AF_INET)
				as.sa_in = *reinterpret_cast<const sockaddr_in *>(sa);
			else if (sa->sa_family == AF_INET6)
				as.sa_in6 = *reinterpret_cast<const sockaddr_in6 *>(sa);
			else memoryZero(this);
		} else {
			memoryZero(this);
		}
		return *this;
	}

	ZT_INLINE void clear() noexcept
	{ memoryZero(this); }

	/**
	 * @return Address family (ss_family in sockaddr_storage)
	 */
	ZT_INLINE uint8_t family() const noexcept
	{ return as.ss.ss_family; }

	/**
	 * @return IP scope classification (e.g. loopback, link-local, private, global)
	 */
	IpScope ipScope() const noexcept;

	/**
	 * Set from a raw IP and port number
	 *
	 * @param ipBytes Bytes of IP address in network byte order
	 * @param ipLen Length of IP address: 4 or 16
	 * @param port Port number or 0 for none
	 */
	void set(const void *ipBytes, unsigned int ipLen, unsigned int port) noexcept;

	/**
	 * Set the port component
	 *
	 * @param port Port, 0 to 65535
	 */
	ZT_INLINE void setPort(unsigned int port) noexcept
	{
		switch (as.ss.ss_family) {
			case AF_INET:
				as.sa_in.sin_port = Utils::hton((uint16_t) port);
				break;
			case AF_INET6:
				as.sa_in6.sin6_port = Utils::hton((uint16_t) port);
				break;
		}
	}

	/**
	 * @return True if this network/netmask route describes a default route (e.g. 0.0.0.0/0)
	 */
	bool isDefaultRoute() const noexcept;

	/**
	 * @return ASCII IP/port format representation
	 */
	char *toString(char buf[ZT_INETADDRESS_STRING_SIZE_MAX]) const noexcept;

	ZT_INLINE String toString() const
	{
		char buf[ZT_INETADDRESS_STRING_SIZE_MAX];
		toString(buf);
		return String(buf);
	}

	/**
	 * @return IP portion only, in ASCII string format
	 */
	char *toIpString(char buf[ZT_INETADDRESS_STRING_SIZE_MAX]) const noexcept;

	ZT_INLINE String toIpString() const
	{
		char buf[ZT_INETADDRESS_STRING_SIZE_MAX];
		toIpString(buf);
		return String(buf);
	}

	/**
	 * @param ipSlashPort IP/port (port is optional, will be 0 if not included)
	 * @return True if address appeared to be valid
	 */
	bool fromString(const char *ipSlashPort) noexcept;

	/**
	 * @return Port or 0 if no port component defined
	 */
	ZT_INLINE unsigned int port() const noexcept
	{
		switch (as.ss.ss_family) {
			case AF_INET:
				return Utils::ntoh((uint16_t) as.sa_in.sin_port);
			case AF_INET6:
				return Utils::ntoh((uint16_t) as.sa_in6.sin6_port);
			default:
				return 0;
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
	ZT_INLINE unsigned int netmaskBits() const noexcept
	{ return port(); }

	/**
	 * @return True if netmask bits is valid for the address type
	 */
	ZT_INLINE bool netmaskBitsValid() const noexcept
	{
		const unsigned int n = port();
		switch (as.ss.ss_family) {
			case AF_INET:
				return (n <= 32);
			case AF_INET6:
				return (n <= 128);
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
	ZT_INLINE unsigned int metric() const noexcept
	{ return port(); }

	/**
	 * Construct a full netmask as an InetAddress
	 *
	 * @return Netmask such as 255.255.255.0 if this address is /24 (port field will be unchanged)
	 */
	InetAddress netmask() const noexcept;

	/**
	 * Constructs a broadcast address from a network/netmask address
	 *
	 * This is only valid for IPv4 and will return a NULL InetAddress for other
	 * address families.
	 *
	 * @return Broadcast address (only IP portion is meaningful)
	 */
	InetAddress broadcast() const noexcept;

	/**
	 * Return the network -- a.k.a. the IP ANDed with the netmask
	 *
	 * @return Network e.g. 10.0.1.0/24 from 10.0.1.200/24
	 */
	InetAddress network() const noexcept;

	/**
	 * Test whether this IPv6 prefix matches the prefix of a given IPv6 address
	 *
	 * @param addr Address to check
	 * @return True if this IPv6 prefix matches the prefix of a given IPv6 address
	 */
	bool isEqualPrefix(const InetAddress &addr) const noexcept;

	/**
	 * Test whether this IP/netmask contains this address
	 *
	 * @param addr Address to check
	 * @return True if this IP/netmask (route) contains this address
	 */
	bool containsAddress(const InetAddress &addr) const noexcept;

	/**
	 * @return True if this is an IPv4 address
	 */
	ZT_INLINE bool isV4() const noexcept
	{ return (as.ss.ss_family == AF_INET); }

	/**
	 * @return True if this is an IPv6 address
	 */
	ZT_INLINE bool isV6() const noexcept
	{ return (as.ss.ss_family == AF_INET6); }

	/**
	 * @return pointer to raw address bytes or NULL if not available
	 */
	ZT_INLINE const void *rawIpData() const noexcept
	{
		switch (as.ss.ss_family) {
			case AF_INET:
				return reinterpret_cast<const void *>(&(as.sa_in.sin_addr.s_addr));
			case AF_INET6:
				return reinterpret_cast<const void *>(as.sa_in6.sin6_addr.s6_addr);
			default:
				return nullptr;
		}
	}

	/**
	 * @return InetAddress containing only the IP portion of this address and a zero port, or NULL if not IPv4 or IPv6
	 */
	ZT_INLINE InetAddress ipOnly() const noexcept
	{
		InetAddress r;
		switch (as.ss.ss_family) {
			case AF_INET:
				r.as.sa_in.sin_family = AF_INET;
				r.as.sa_in.sin_addr.s_addr = as.sa_in.sin_addr.s_addr;
				break;
			case AF_INET6:
				r.as.sa_in6.sin6_family = AF_INET6;
				Utils::copy<16>(r.as.sa_in6.sin6_addr.s6_addr, as.sa_in6.sin6_addr.s6_addr);
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
	ZT_INLINE bool ipsEqual(const InetAddress &a) const noexcept
	{
		const uint8_t f = as.ss.ss_family;
		if (f == a.as.ss.ss_family) {
			if (f == AF_INET)
				return as.sa_in.sin_addr.s_addr == a.as.sa_in.sin_addr.s_addr;
			if (f == AF_INET6)
				return memcmp(as.sa_in6.sin6_addr.s6_addr, a.as.sa_in6.sin6_addr.s6_addr, 16) == 0;
			return memcmp(this, &a, sizeof(InetAddress)) == 0;
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
	ZT_INLINE bool ipsEqual2(const InetAddress &a) const noexcept
	{
		const uint8_t f = as.ss.ss_family;
		if (f == a.as.ss.ss_family) {
			if (f == AF_INET)
				return as.sa_in.sin_addr.s_addr == a.as.sa_in.sin_addr.s_addr;
			if (f == AF_INET6)
				return memcmp(as.sa_in6.sin6_addr.s6_addr, a.as.sa_in6.sin6_addr.s6_addr, 8) == 0;
			return (memcmp(this, &a, sizeof(InetAddress)) == 0);
		}
		return false;
	}

	ZT_INLINE unsigned long hashCode() const noexcept
	{
		if (as.ss.ss_family == AF_INET) {
			return (unsigned long) Utils::hash32(((uint32_t) as.sa_in.sin_addr.s_addr + (uint32_t) as.sa_in.sin_port) ^ (uint32_t) Utils::s_mapNonce);
		} else if (as.ss.ss_family == AF_INET6) {
			return (unsigned long) Utils::hash64(
				(Utils::loadAsIsEndian<uint64_t>(as.sa_in6.sin6_addr.s6_addr) +
				 Utils::loadAsIsEndian<uint64_t>(as.sa_in6.sin6_addr.s6_addr + 8) +
				 (uint64_t) as.sa_in6.sin6_port) ^
				Utils::s_mapNonce);
		}
		return Utils::fnv1a32(this, sizeof(InetAddress));
	}

	/**
	 * Check whether this is a network/route rather than an IP assignment
	 *
	 * A network is an IP/netmask where everything after the netmask is
	 * zero e.g. 10.0.0.0/8.
	 *
	 * @return True if everything after netmask bits is zero
	 */
	bool isNetwork() const noexcept;

	/**
	 * @return True if address family is non-zero
	 */
	explicit ZT_INLINE operator bool() const noexcept
	{ return (as.ss.ss_family != 0); }

	static constexpr int marshalSizeMax() noexcept
	{ return ZT_INETADDRESS_MARSHAL_SIZE_MAX; }

	int marshal(uint8_t data[ZT_INETADDRESS_MARSHAL_SIZE_MAX]) const noexcept;

	int unmarshal(const uint8_t *restrict data, int len) noexcept;

	ZT_INLINE bool operator==(const InetAddress &a) const noexcept
	{
		if (as.ss.ss_family == a.as.ss.ss_family) {
			if (as.ss.ss_family == AF_INET)
				return ((as.sa_in.sin_port == a.as.sa_in.sin_port) && (as.sa_in.sin_addr.s_addr == a.as.sa_in.sin_addr.s_addr));
			if (as.ss.ss_family == AF_INET6)
				return ((as.sa_in6.sin6_port == a.as.sa_in6.sin6_port) && (memcmp(as.sa_in6.sin6_addr.s6_addr, a.as.sa_in6.sin6_addr.s6_addr, 16) == 0));
			return memcmp(this, &a, sizeof(InetAddress)) == 0;
		}
		return false;
	}

	ZT_INLINE bool operator<(const InetAddress &a) const noexcept
	{
		if (as.ss.ss_family == a.as.ss.ss_family) {
			if (as.ss.ss_family == AF_INET) {
				const uint16_t p0 = Utils::ntoh((uint16_t) as.sa_in.sin_port);
				const uint16_t p1 = Utils::ntoh((uint16_t) a.as.sa_in.sin_port);
				if (p0 == p1)
					return Utils::ntoh((uint32_t) as.sa_in.sin_addr.s_addr) < Utils::ntoh((uint32_t) a.as.sa_in.sin_addr.s_addr);
				return p0 < p1;
			}
			if (as.ss.ss_family == AF_INET6) {
				const uint16_t p0 = Utils::ntoh((uint16_t) as.sa_in6.sin6_port);
				const uint16_t p1 = Utils::ntoh((uint16_t) a.as.sa_in6.sin6_port);
				if (p0 == p1)
					return memcmp(as.sa_in6.sin6_addr.s6_addr, a.as.sa_in6.sin6_addr.s6_addr, 16) < 0;
				return p0 < p1;
			}
			return memcmp(this, &a, sizeof(InetAddress)) < 0;
		}
		return as.ss.ss_family < a.as.ss.ss_family;
	}

	ZT_INLINE bool operator!=(const InetAddress &a) const noexcept
	{ return !(*this == a); }

	ZT_INLINE bool operator>(const InetAddress &a) const noexcept
	{ return (a < *this); }

	ZT_INLINE bool operator<=(const InetAddress &a) const noexcept
	{ return !(a < *this); }

	ZT_INLINE bool operator>=(const InetAddress &a) const noexcept
	{ return !(*this < a); }

	/**
	 * Compute an IPv6 link-local address
	 *
	 * @param mac MAC address seed
	 * @return IPv6 link-local address
	 */
	static InetAddress makeIpv6LinkLocal(const MAC &mac) noexcept;

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
	static InetAddress makeIpv6rfc4193(uint64_t nwid, uint64_t zeroTierAddress) noexcept;

	/**
	 * Compute a private IPv6 "6plane" unicast address from network ID and ZeroTier address
	 */
	static InetAddress makeIpv66plane(uint64_t nwid, uint64_t zeroTierAddress) noexcept;

	/**
	 * Union allowing this to be accessed as a sockaddr of any supported type.
	 */
	union
	{
		sockaddr_storage ss;
		sockaddr sa;
		sockaddr_in sa_in;
		sockaddr_in6 sa_in6;
	} as;
};

static ZT_INLINE InetAddress *asInetAddress(sockaddr_in *const p) noexcept
{ return reinterpret_cast<InetAddress *>(p); }

static ZT_INLINE InetAddress *asInetAddress(sockaddr_in6 *const p) noexcept
{ return reinterpret_cast<InetAddress *>(p); }

static ZT_INLINE InetAddress *asInetAddress(sockaddr *const p) noexcept
{ return reinterpret_cast<InetAddress *>(p); }

static ZT_INLINE InetAddress *asInetAddress(sockaddr_storage *const p) noexcept
{ return reinterpret_cast<InetAddress *>(p); }

static ZT_INLINE const InetAddress *asInetAddress(const sockaddr_in *const p) noexcept
{ return reinterpret_cast<const InetAddress *>(p); }

static ZT_INLINE const InetAddress *asInetAddress(const sockaddr_in6 *const p) noexcept
{ return reinterpret_cast<const InetAddress *>(p); }

static ZT_INLINE const InetAddress *asInetAddress(const sockaddr *const p) noexcept
{ return reinterpret_cast<const InetAddress *>(p); }

static ZT_INLINE const InetAddress *asInetAddress(const sockaddr_storage *const p) noexcept
{ return reinterpret_cast<const InetAddress *>(p); }

static ZT_INLINE InetAddress &asInetAddress(sockaddr_in &p) noexcept
{ return *reinterpret_cast<InetAddress *>(&p); }

static ZT_INLINE InetAddress &asInetAddress(sockaddr_in6 &p) noexcept
{ return *reinterpret_cast<InetAddress *>(&p); }

static ZT_INLINE InetAddress &asInetAddress(sockaddr &p) noexcept
{ return *reinterpret_cast<InetAddress *>(&p); }

static ZT_INLINE InetAddress &asInetAddress(sockaddr_storage &p) noexcept
{ return *reinterpret_cast<InetAddress *>(&p); }

static ZT_INLINE const InetAddress &asInetAddress(const sockaddr_in &p) noexcept
{ return *reinterpret_cast<const InetAddress *>(&p); }

static ZT_INLINE const InetAddress &asInetAddress(const sockaddr_in6 &p) noexcept
{ return *reinterpret_cast<const InetAddress *>(&p); }

static ZT_INLINE const InetAddress &asInetAddress(const sockaddr &p) noexcept
{ return *reinterpret_cast<const InetAddress *>(&p); }

static ZT_INLINE const InetAddress &asInetAddress(const sockaddr_storage &p) noexcept
{ return *reinterpret_cast<const InetAddress *>(&p); }

} // namespace ZeroTier

#endif
