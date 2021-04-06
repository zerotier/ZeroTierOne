/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#define _WIN32_WINNT 0x06010000

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "Utils.hpp"

namespace ZeroTier {

static_assert(ZT_SOCKADDR_STORAGE_SIZE == sizeof(sockaddr_storage), "ZT_SOCKADDR_STORAGE_SIZE is incorrect on this platform, must be size of sockaddr_storage");
static_assert(ZT_SOCKADDR_STORAGE_SIZE == sizeof(InetAddress), "ZT_SOCKADDR_STORAGE_SIZE should equal InetAddress, which should equal size of sockaddr_storage");
static_assert(ZT_SOCKADDR_STORAGE_SIZE == sizeof(ZT_InetAddress), "ZT_SOCKADDR_STORAGE_SIZE should equal ZT_InetAddress, which should equal size of sockaddr_storage");

const InetAddress InetAddress::LO4((const void *) ("\x7f\x00\x00\x01"), 4, 0);
const InetAddress InetAddress::LO6((const void *) ("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01"), 16, 0);
const InetAddress InetAddress::NIL;

ZT_MAYBE_UNUSED InetAddress::IpScope InetAddress::ipScope() const noexcept
{
	switch (as.ss.ss_family) {

		case AF_INET: {
			const uint32_t ip = Utils::ntoh((uint32_t)as.sa_in.sin_addr.s_addr);
			switch (ip >> 24U) {
				case 0x00:
					return ZT_IP_SCOPE_NONE;                                            // 0.0.0.0/8 (reserved, never used)
				case 0x06:
					return ZT_IP_SCOPE_PSEUDOPRIVATE;                                   // 6.0.0.0/8 (US Army)
				case 0x0a:
					return ZT_IP_SCOPE_PRIVATE;                                         // 10.0.0.0/8
				case 0x0b: //return IP_SCOPE_PSEUDOPRIVATE;                           // 11.0.0.0/8 (US DoD)
				case 0x15: //return IP_SCOPE_PSEUDOPRIVATE;                           // 21.0.0.0/8 (US DDN-RVN)
				case 0x16: //return IP_SCOPE_PSEUDOPRIVATE;                           // 22.0.0.0/8 (US DISA)
				case 0x19: //return IP_SCOPE_PSEUDOPRIVATE;                           // 25.0.0.0/8 (UK Ministry of Defense)
				case 0x1a: //return IP_SCOPE_PSEUDOPRIVATE;                           // 26.0.0.0/8 (US DISA)
				case 0x1c: //return IP_SCOPE_PSEUDOPRIVATE;                           // 28.0.0.0/8 (US DSI-North)
				case 0x1d: //return IP_SCOPE_PSEUDOPRIVATE;                           // 29.0.0.0/8 (US DISA)
				case 0x1e: //return IP_SCOPE_PSEUDOPRIVATE;                           // 30.0.0.0/8 (US DISA)
				case 0x33: //return IP_SCOPE_PSEUDOPRIVATE;                           // 51.0.0.0/8 (UK Department of Social Security)
				case 0x37: //return IP_SCOPE_PSEUDOPRIVATE;                           // 55.0.0.0/8 (US DoD)
				case 0x38:                                                            // 56.0.0.0/8 (US Postal Service)
					return ZT_IP_SCOPE_PSEUDOPRIVATE;
				case 0x64:
					if ((ip & 0xffc00000) == 0x64400000) return ZT_IP_SCOPE_PRIVATE;    // 100.64.0.0/10
					break;
				case 0x7f:
					return ZT_IP_SCOPE_LOOPBACK;                                        // 127.0.0.0/8
				case 0xa9:
					if ((ip & 0xffff0000) == 0xa9fe0000) return ZT_IP_SCOPE_LINK_LOCAL; // 169.254.0.0/16
					break;
				case 0xac:
					if ((ip & 0xfff00000) == 0xac100000) return ZT_IP_SCOPE_PRIVATE;    // 172.16.0.0/12
					break;
				case 0xc0:
					if ((ip & 0xffff0000) == 0xc0a80000) return ZT_IP_SCOPE_PRIVATE;    // 192.168.0.0/16
					if ((ip & 0xffffff00) == 0xc0000200) return ZT_IP_SCOPE_PRIVATE;    // 192.0.2.0/24
					break;
				case 0xc6:
					if ((ip & 0xfffe0000) == 0xc6120000) return ZT_IP_SCOPE_PRIVATE;    // 198.18.0.0/15
					if ((ip & 0xffffff00) == 0xc6336400) return ZT_IP_SCOPE_PRIVATE;    // 198.51.100.0/24
					break;
				case 0xcb:
					if ((ip & 0xffffff00) == 0xcb007100) return ZT_IP_SCOPE_PRIVATE;    // 203.0.113.0/24
					break;
				case 0xff:
					return ZT_IP_SCOPE_NONE;                                            // 255.0.0.0/8 (broadcast, or unused/unusable)
			}
			switch (ip >> 28U) {
				case 0xe:
					return ZT_IP_SCOPE_MULTICAST;                                       // 224.0.0.0/4
				case 0xf:
					return ZT_IP_SCOPE_PSEUDOPRIVATE;                                   // 240.0.0.0/4 ("reserved," usually unusable)
			}
			return ZT_IP_SCOPE_GLOBAL;
		}

		case AF_INET6: {
			const uint8_t *const ip = as.sa_in6.sin6_addr.s6_addr;
			if ((ip[0] & 0xf0U) == 0xf0) {
				if (ip[0] == 0xff) return ZT_IP_SCOPE_MULTICAST;                      // ff00::/8
				if ((ip[0] == 0xfe) && ((ip[1] & 0xc0U) == 0x80)) {
					unsigned int k = 2;
					while ((!ip[k]) && (k < 15)) ++k;
					if ((k == 15) && (ip[15] == 0x01))
						return ZT_IP_SCOPE_LOOPBACK;                                      // fe80::1/128
					else return ZT_IP_SCOPE_LINK_LOCAL;                                 // fe80::/10
				}
				if ((ip[0] & 0xfeU) == 0xfc) return ZT_IP_SCOPE_PRIVATE;              // fc00::/7
			}
			unsigned int k = 0;
			while ((!ip[k]) && (k < 15)) ++k;
			if (k == 15) { // all 0's except last byte
				if (ip[15] == 0x01) return ZT_IP_SCOPE_LOOPBACK;                      // ::1/128
				if (ip[15] == 0x00) return ZT_IP_SCOPE_NONE;                          // ::/128
			}
			return ZT_IP_SCOPE_GLOBAL;
		}

	}
	return ZT_IP_SCOPE_NONE;
}

ZT_MAYBE_UNUSED void InetAddress::set(const void *ipBytes, unsigned int ipLen, unsigned int port) noexcept
{
	memoryZero(this);
	if (ipLen == 4) {
		as.sa_in.sin_family = AF_INET;
		as.sa_in.sin_port = Utils::hton((uint16_t) port);
		as.sa_in.sin_addr.s_addr = Utils::loadMachineEndian< uint32_t >(ipBytes);
	} else if (ipLen == 16) {
		as.sa_in6.sin6_family = AF_INET6;
		as.sa_in6.sin6_port = Utils::hton((uint16_t) port);
		Utils::copy<16>(as.sa_in6.sin6_addr.s6_addr, ipBytes);
	}
}

ZT_MAYBE_UNUSED bool InetAddress::isDefaultRoute() const noexcept
{
	switch (as.ss.ss_family) {
		case AF_INET:
			return ((as.sa_in.sin_port == 0) && (as.sa_in.sin_addr.s_addr == 0));
		case AF_INET6:
			if (as.sa_in6.sin6_port == 0) {
				for (unsigned int i = 0;i < 16;++i) {
					if (as.sa_in6.sin6_addr.s6_addr[i])
						return false;
				}
				return true;
			}
			return false;
		default:
			return false;
	}
}

ZT_MAYBE_UNUSED char *InetAddress::toString(char buf[ZT_INETADDRESS_STRING_SIZE_MAX]) const noexcept
{
	char *p = toIpString(buf);
	if (*p) {
		while (*p) ++p;
		*(p++) = '/';
		Utils::decimal(port(), p);
	}
	return buf;
}

ZT_MAYBE_UNUSED char *InetAddress::toIpString(char buf[ZT_INETADDRESS_STRING_SIZE_MAX]) const noexcept
{
	buf[0] = (char) 0;
	switch (as.ss.ss_family) {
		case AF_INET:
			inet_ntop(AF_INET, &as.sa_in.sin_addr.s_addr, buf, INET_ADDRSTRLEN);
			break;
		case AF_INET6:
			inet_ntop(AF_INET6, as.sa_in6.sin6_addr.s6_addr, buf, INET6_ADDRSTRLEN);
			break;
	}
	return buf;
}

ZT_MAYBE_UNUSED bool InetAddress::fromString(const char *ipSlashPort) noexcept
{
	char buf[64];

	memoryZero(this);

	if (!*ipSlashPort)
		return true;
	if (!Utils::scopy(buf, sizeof(buf), ipSlashPort))
		return false;

	char *portAt = buf;
	while ((*portAt) && (*portAt != '/'))
		++portAt;
	unsigned int port = 0;
	if (*portAt) {
		*(portAt++) = (char) 0;
		port = Utils::strToUInt(portAt) & 0xffffU;
	}

	if (strchr(buf, ':')) {
		as.sa_in6.sin6_family = AF_INET6;
		as.sa_in6.sin6_port = Utils::hton((uint16_t) port);
		inet_pton(AF_INET6, buf, as.sa_in6.sin6_addr.s6_addr);
		return true;
	} else if (strchr(buf, '.')) {
		as.sa_in.sin_family = AF_INET;
		as.sa_in.sin_port = Utils::hton((uint16_t) port);
		inet_pton(AF_INET, buf, &as.sa_in.sin_addr.s_addr);
		return true;
	}

	return false;
}

ZT_MAYBE_UNUSED InetAddress InetAddress::netmask() const noexcept
{
	InetAddress r(*this);
	switch (r.as.ss.ss_family) {
		case AF_INET:
			r.as.sa_in.sin_addr.s_addr = Utils::hton((uint32_t) (0xffffffffU << (32 - netmaskBits())));
			break;
		case AF_INET6: {
			uint64_t nm[2];
			const unsigned int bits = netmaskBits();
			if (bits) {
				nm[0] = Utils::hton((uint64_t) ((bits >= 64) ? 0xffffffffffffffffULL : (0xffffffffffffffffULL << (64 - bits))));
				nm[1] = Utils::hton((uint64_t) ((bits <= 64) ? 0ULL : (0xffffffffffffffffULL << (128 - bits))));
			} else {
				nm[0] = 0;
				nm[1] = 0;
			}
			Utils::copy<16>(r.as.sa_in6.sin6_addr.s6_addr, nm);
		}
			break;
	}
	return r;
}

ZT_MAYBE_UNUSED InetAddress InetAddress::broadcast() const noexcept
{
	if (as.ss.ss_family == AF_INET) {
		InetAddress r(*this);
		reinterpret_cast<sockaddr_in *>(&r)->sin_addr.s_addr |= Utils::hton((uint32_t) (0xffffffffU >> netmaskBits()));
		return r;
	}
	return InetAddress();
}

ZT_MAYBE_UNUSED InetAddress InetAddress::network() const noexcept
{
	InetAddress r(*this);
	switch (r.as.ss.ss_family) {
		case AF_INET:
			r.as.sa_in.sin_addr.s_addr &= Utils::hton((uint32_t) (0xffffffffU << (32 - netmaskBits())));
			break;
		case AF_INET6: {
			uint64_t nm[2];
			const unsigned int bits = netmaskBits();
			Utils::copy<16>(nm, reinterpret_cast<sockaddr_in6 *>(&r)->sin6_addr.s6_addr);
			nm[0] &= Utils::hton((uint64_t) ((bits >= 64) ? 0xffffffffffffffffULL : (0xffffffffffffffffULL << (64 - bits))));
			nm[1] &= Utils::hton((uint64_t) ((bits <= 64) ? 0ULL : (0xffffffffffffffffULL << (128 - bits))));
			Utils::copy<16>(r.as.sa_in6.sin6_addr.s6_addr, nm);
		}
			break;
	}
	return r;
}

ZT_MAYBE_UNUSED bool InetAddress::isEqualPrefix(const InetAddress &addr) const noexcept
{
	if (addr.as.ss.ss_family == as.ss.ss_family) {
		switch (as.ss.ss_family) {
			case AF_INET6: {
				const InetAddress mask(netmask());
				InetAddress addr_mask(addr.netmask());
				const uint8_t *const n = addr_mask.as.sa_in6.sin6_addr.s6_addr;
				const uint8_t *const m = mask.as.sa_in6.sin6_addr.s6_addr;
				const uint8_t *const a = addr.as.sa_in6.sin6_addr.s6_addr;
				const uint8_t *const b = as.sa_in6.sin6_addr.s6_addr;
				for (unsigned int i = 0;i < 16;++i) {
					if ((a[i] & m[i]) != (b[i] & n[i]))
						return false;
				}
				return true;
			}
		}
	}
	return false;
}

ZT_MAYBE_UNUSED bool InetAddress::containsAddress(const InetAddress &addr) const noexcept
{
	if (addr.as.ss.ss_family == as.ss.ss_family) {
		switch (as.ss.ss_family) {
			case AF_INET: {
				const unsigned int bits = netmaskBits();
				if (bits == 0)
					return true;
				return (
					(Utils::ntoh((uint32_t) addr.as.sa_in.sin_addr.s_addr) >> (32 - bits)) ==
					(Utils::ntoh((uint32_t) as.sa_in.sin_addr.s_addr) >> (32 - bits))
				);
			}
			case AF_INET6: {
				const InetAddress mask(netmask());
				const uint8_t *const m = mask.as.sa_in6.sin6_addr.s6_addr;
				const uint8_t *const a = addr.as.sa_in6.sin6_addr.s6_addr;
				const uint8_t *const b = as.sa_in6.sin6_addr.s6_addr;
				for (unsigned int i = 0;i < 16;++i) {
					if ((a[i] & m[i]) != b[i])
						return false;
				}
				return true;
			}
		}
	}
	return false;
}

ZT_MAYBE_UNUSED bool InetAddress::isNetwork() const noexcept
{
	switch (as.ss.ss_family) {
		case AF_INET: {
			unsigned int bits = netmaskBits();
			if (bits <= 0)
				return false;
			if (bits >= 32)
				return false;
			const uint32_t ip = Utils::ntoh((uint32_t) as.sa_in.sin_addr.s_addr);
			return ((ip & (0xffffffffU >> bits)) == 0);
		}
		case AF_INET6: {
			unsigned int bits = netmaskBits();
			if (bits <= 0)
				return false;
			if (bits >= 128)
				return false;
			const uint8_t *const ip = as.sa_in6.sin6_addr.s6_addr;
			unsigned int p = bits / 8;
			if ((ip[p++] & (0xffU >> (bits % 8))) != 0)
				return false;
			while (p < 16) {
				if (ip[p++])
					return false;
			}
			return true;
		}
	}
	return false;
}

ZT_MAYBE_UNUSED int InetAddress::marshal(uint8_t data[ZT_INETADDRESS_MARSHAL_SIZE_MAX]) const noexcept
{
	unsigned int port;
	switch (as.ss.ss_family) {
		case AF_INET:
			port = Utils::ntoh((uint16_t) reinterpret_cast<const sockaddr_in *>(this)->sin_port);
			data[0] = 4;
			data[1] = reinterpret_cast<const uint8_t *>(&as.sa_in.sin_addr.s_addr)[0];
			data[2] = reinterpret_cast<const uint8_t *>(&as.sa_in.sin_addr.s_addr)[1];
			data[3] = reinterpret_cast<const uint8_t *>(&as.sa_in.sin_addr.s_addr)[2];
			data[4] = reinterpret_cast<const uint8_t *>(&as.sa_in.sin_addr.s_addr)[3];
			data[5] = (uint8_t) (port >> 8U);
			data[6] = (uint8_t) port;
			return 7;
		case AF_INET6:
			port = Utils::ntoh((uint16_t) as.sa_in6.sin6_port);
			data[0] = 6;
			Utils::copy<16>(data + 1, as.sa_in6.sin6_addr.s6_addr);
			data[17] = (uint8_t) (port >> 8U);
			data[18] = (uint8_t) port;
			return 19;
		default:
			data[0] = 0;
			return 1;
	}
}

ZT_MAYBE_UNUSED int InetAddress::unmarshal(const uint8_t *restrict data, const int len) noexcept
{
	memoryZero(this);
	if (unlikely(len <= 0))
		return -1;
	switch (data[0]) {
		case 0:
			return 1;
		case 4:
			if (unlikely(len < 7))
				return -1;
			as.sa_in.sin_family = AF_INET;
			as.sa_in.sin_port = Utils::loadMachineEndian< uint16_t >(data + 5);
			as.sa_in.sin_addr.s_addr = Utils::loadMachineEndian< uint32_t >(data + 1);
			return 7;
		case 6:
			if (unlikely(len < 19))
				return -1;
			as.sa_in6.sin6_family = AF_INET6;
			as.sa_in6.sin6_port = Utils::loadMachineEndian< uint16_t >(data + 17);
			Utils::copy<16>(as.sa_in6.sin6_addr.s6_addr, data + 1);
			return 19;
		default:
			return -1;
	}
}

ZT_MAYBE_UNUSED InetAddress InetAddress::makeIpv6LinkLocal(const MAC &mac) noexcept
{
	InetAddress r;
	r.as.sa_in6.sin6_family = AF_INET6;
	r.as.sa_in6.sin6_port = ZT_CONST_TO_BE_UINT16(64);
	r.as.sa_in6.sin6_addr.s6_addr[0] = 0xfe;
	r.as.sa_in6.sin6_addr.s6_addr[1] = 0x80;
	r.as.sa_in6.sin6_addr.s6_addr[2] = 0x00;
	r.as.sa_in6.sin6_addr.s6_addr[3] = 0x00;
	r.as.sa_in6.sin6_addr.s6_addr[4] = 0x00;
	r.as.sa_in6.sin6_addr.s6_addr[5] = 0x00;
	r.as.sa_in6.sin6_addr.s6_addr[6] = 0x00;
	r.as.sa_in6.sin6_addr.s6_addr[7] = 0x00;
	r.as.sa_in6.sin6_addr.s6_addr[8] = mac[0] & 0xfdU;
	r.as.sa_in6.sin6_addr.s6_addr[9] = mac[1];
	r.as.sa_in6.sin6_addr.s6_addr[10] = mac[2];
	r.as.sa_in6.sin6_addr.s6_addr[11] = 0xff;
	r.as.sa_in6.sin6_addr.s6_addr[12] = 0xfe;
	r.as.sa_in6.sin6_addr.s6_addr[13] = mac[3];
	r.as.sa_in6.sin6_addr.s6_addr[14] = mac[4];
	r.as.sa_in6.sin6_addr.s6_addr[15] = mac[5];
	return r;
}

ZT_MAYBE_UNUSED InetAddress InetAddress::makeIpv6rfc4193(uint64_t nwid, uint64_t zeroTierAddress) noexcept
{
	InetAddress r;
	r.as.sa_in6.sin6_family = AF_INET6;
	r.as.sa_in6.sin6_port = ZT_CONST_TO_BE_UINT16(88); // /88 includes 0xfd + network ID, discriminating by device ID below that
	r.as.sa_in6.sin6_addr.s6_addr[0] = 0xfd;
	r.as.sa_in6.sin6_addr.s6_addr[1] = (uint8_t) (nwid >> 56U);
	r.as.sa_in6.sin6_addr.s6_addr[2] = (uint8_t) (nwid >> 48U);
	r.as.sa_in6.sin6_addr.s6_addr[3] = (uint8_t) (nwid >> 40U);
	r.as.sa_in6.sin6_addr.s6_addr[4] = (uint8_t) (nwid >> 32U);
	r.as.sa_in6.sin6_addr.s6_addr[5] = (uint8_t) (nwid >> 24U);
	r.as.sa_in6.sin6_addr.s6_addr[6] = (uint8_t) (nwid >> 16U);
	r.as.sa_in6.sin6_addr.s6_addr[7] = (uint8_t) (nwid >> 8U);
	r.as.sa_in6.sin6_addr.s6_addr[8] = (uint8_t) nwid;
	r.as.sa_in6.sin6_addr.s6_addr[9] = 0x99;
	r.as.sa_in6.sin6_addr.s6_addr[10] = 0x93;
	r.as.sa_in6.sin6_addr.s6_addr[11] = (uint8_t) (zeroTierAddress >> 32U);
	r.as.sa_in6.sin6_addr.s6_addr[12] = (uint8_t) (zeroTierAddress >> 24U);
	r.as.sa_in6.sin6_addr.s6_addr[13] = (uint8_t) (zeroTierAddress >> 16U);
	r.as.sa_in6.sin6_addr.s6_addr[14] = (uint8_t) (zeroTierAddress >> 8U);
	r.as.sa_in6.sin6_addr.s6_addr[15] = (uint8_t) zeroTierAddress;
	return r;
}

ZT_MAYBE_UNUSED InetAddress InetAddress::makeIpv66plane(uint64_t nwid, uint64_t zeroTierAddress) noexcept
{
	nwid ^= (nwid >> 32U);
	InetAddress r;
	r.as.sa_in6.sin6_family = AF_INET6;
	r.as.sa_in6.sin6_port = ZT_CONST_TO_BE_UINT16(40);
	r.as.sa_in6.sin6_addr.s6_addr[0] = 0xfc;
	r.as.sa_in6.sin6_addr.s6_addr[1] = (uint8_t) (nwid >> 24U);
	r.as.sa_in6.sin6_addr.s6_addr[2] = (uint8_t) (nwid >> 16U);
	r.as.sa_in6.sin6_addr.s6_addr[3] = (uint8_t) (nwid >> 8U);
	r.as.sa_in6.sin6_addr.s6_addr[4] = (uint8_t) nwid;
	r.as.sa_in6.sin6_addr.s6_addr[5] = (uint8_t) (zeroTierAddress >> 32U);
	r.as.sa_in6.sin6_addr.s6_addr[6] = (uint8_t) (zeroTierAddress >> 24U);
	r.as.sa_in6.sin6_addr.s6_addr[7] = (uint8_t) (zeroTierAddress >> 16U);
	r.as.sa_in6.sin6_addr.s6_addr[8] = (uint8_t) (zeroTierAddress >> 8U);
	r.as.sa_in6.sin6_addr.s6_addr[9] = (uint8_t) zeroTierAddress;
	r.as.sa_in6.sin6_addr.s6_addr[15] = 0x01;
	return r;
}

extern "C" {
ZT_MAYBE_UNUSED extern const int ZT_AF_INET = (int)AF_INET;
ZT_MAYBE_UNUSED extern const int ZT_AF_INET6 = (int)AF_INET6;
}

} // namespace ZeroTier
