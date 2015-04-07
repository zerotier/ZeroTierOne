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

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <string>

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "Utils.hpp"

namespace ZeroTier {

const InetAddress InetAddress::LO4("127.0.0.1",0);
const InetAddress InetAddress::LO6("::1",0);
const InetAddress InetAddress::DEFAULT4((uint32_t)0,0);
const InetAddress InetAddress::DEFAULT6((const void *)0,16,0);

InetAddress::IpScope InetAddress::ipScope() const
	throw()
{
	switch(ss_family) {

		case AF_INET: {
			const uint32_t ip = Utils::ntoh((uint32_t)reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr);
			switch(ip >> 24) {
				case 0x00: return IP_SCOPE_NONE;                                      // 0.0.0.0/8 (reserved, never used)
				case 0x06: return IP_SCOPE_PSEUDOPRIVATE;                             // 6.0.0.0/8 (US Army)
				case 0x0a: return IP_SCOPE_PRIVATE;                                   // 10.0.0.0/8
				case 0x0b: return IP_SCOPE_PSEUDOPRIVATE;                             // 11.0.0.0/8 (US DoD)
				case 0x15: return IP_SCOPE_PSEUDOPRIVATE;                             // 21.0.0.0/8 (US DDN-RVN)
				case 0x16: return IP_SCOPE_PSEUDOPRIVATE;                             // 22.0.0.0/8 (US DISA)
				case 0x19: return IP_SCOPE_PSEUDOPRIVATE;                             // 25.0.0.0/8 (UK Ministry of Defense)
				case 0x1a: return IP_SCOPE_PSEUDOPRIVATE;                             // 26.0.0.0/8 (US DISA)
				case 0x1c: return IP_SCOPE_PSEUDOPRIVATE;                             // 28.0.0.0/8 (US DSI-North)
				case 0x1d: return IP_SCOPE_PSEUDOPRIVATE;                             // 29.0.0.0/8 (US DISA)
				case 0x1e: return IP_SCOPE_PSEUDOPRIVATE;                             // 30.0.0.0/8 (US DISA)
				case 0x2c: return IP_SCOPE_PSEUDOPRIVATE;                             // 44.0.0.0/8 (Amateur Radio)
				case 0x33: return IP_SCOPE_PSEUDOPRIVATE;                             // 51.0.0.0/8 (UK Department of Social Security)
				case 0x37: return IP_SCOPE_PSEUDOPRIVATE;                             // 55.0.0.0/8 (US DoD)
				case 0x38: return IP_SCOPE_PSEUDOPRIVATE;                             // 56.0.0.0/8 (US Postal Service)
				case 0x64:
					if ((ip & 0xffc00000) == 0x64400000) return IP_SCOPE_SHARED;        // 100.64.0.0/10
					break;
				case 0x7f: return IP_SCOPE_LOOPBACK;                                  // 127.0.0.0/8
				case 0xa9:
					if ((ip & 0xffff0000) == 0xa9fe0000) return IP_SCOPE_LINK_LOCAL;    // 169.254.0.0/16
					break;
				case 0xac:
					if ((ip & 0xfff00000) == 0xac100000) return IP_SCOPE_PRIVATE;       // 172.16.0.0/12
					break;
				case 0xc0:
					if ((ip & 0xffff0000) == 0xc9a80000) return IP_SCOPE_PRIVATE;				// 192.168.0.0/16
					break;
				case 0xff: return IP_SCOPE_NONE;                                      // 255.0.0.0/8 (broadcast, or unused/unusable)
				default:
					switch(ip >> 28) {
						case 0xe: return IP_SCOPE_MULTICAST;                              // 224.0.0.0/4
						case 0xf: return IP_SCOPE_PSEUDOPRIVATE;                          // 240.0.0.0/4 ("reserved," usually unusable)
						default: return IP_SCOPE_GLOBAL;                                  // everything else
					}
					break;
			}
		}	break;

		case AF_INET6: {
			const unsigned char *ip = reinterpret_cast<const unsigned char *>(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr);
			if ((ip[0] & 0xf0) == 0xf0) {
				if (ip[0] == 0xff) return IP_SCOPE_MULTICAST;                              // ff00::/8
				if ((ip[0] == 0xfe)&&((ip[1] & 0xc0) == 0x80)) return IP_SCOPE_LINK_LOCAL; // fe80::/10
				if ((ip[0] & 0xfe) == 0xfc) return IP_SCOPE_PRIVATE;                       // fc00::/7
			}
			unsigned int k = 0;
			while ((!ip[k])&&(k < 15)) ++k;
			if (k == 15) { // all 0's except last byte
				if (ip[15] == 0x01) return IP_SCOPE_LOOPBACK;                              // ::1/128
				if (ip[15] == 0x00) return IP_SCOPE_NONE;                                  // ::/128
			}
			return IP_SCOPE_GLOBAL;
		}	break;

	}

	return IP_SCOPE_NONE;
}

void InetAddress::set(const std::string &ip,unsigned int port)
	throw()
{
	if (ip.find(':') != std::string::npos) {
		struct sockaddr_in6 sin6;
		memset(&sin6,0,sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_port = Utils::hton((uint16_t)port);
		if (inet_pton(AF_INET6,ip.c_str(),(void *)&(sin6.sin6_addr.s6_addr)) <= 0)
			memset(this,0,sizeof(InetAddress));
		else *this = sin6;
	} else {
		struct sockaddr_in sin;
		memset(&sin,0,sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = Utils::hton((uint16_t)port);
		if (inet_pton(AF_INET,ip.c_str(),(void *)&(sin.sin_addr.s_addr)) <= 0)
			memset(this,0,sizeof(InetAddress));
		else *this = sin;
	}
}

void InetAddress::set(const void *ipBytes,unsigned int ipLen,unsigned int port)
	throw()
{
	memset(this,0,sizeof(InetAddress));
	if (ipLen == 4) {
		setV4();
		if (ipBytes)
			memcpy(rawIpData(),ipBytes,4);
		setPort(port);
	} else if (ipLen == 16) {
		setV6();
		if (ipBytes)
			memcpy(rawIpData(),ipBytes,16);
		setPort(port);
	}
}

std::string InetAddress::toString() const
{
	char buf[128];
	switch(ss_family) {
		case AF_INET:
			Utils::snprintf(buf,sizeof(buf),"%d.%d.%d.%d/%d",
					(int)(reinterpret_cast<const unsigned char *>(&(reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr)))[0],
					(int)(reinterpret_cast<const unsigned char *>(&(reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr)))[1],
					(int)(reinterpret_cast<const unsigned char *>(&(reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr)))[2],
					(int)(reinterpret_cast<const unsigned char *>(&(reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr)))[3],
					(int)Utils::ntoh((uint16_t)(reinterpret_cast<const struct sockaddr_in *>(this)->sin_port))
				);
			return std::string(buf);
		case AF_INET6:
			Utils::snprintf(buf,sizeof(buf),"%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x/%d",
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[0]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[1]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[2]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[3]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[4]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[5]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[6]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[7]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[8]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[9]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[10]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[11]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[12]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[13]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[14]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[15]),
					(int)Utils::ntoh((uint16_t)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_port))
				);
			return std::string(buf);
	}
	return std::string();
}

std::string InetAddress::toIpString() const
{
	char buf[128];
	switch(ss_family) {
		case AF_INET:
			Utils::snprintf(buf,sizeof(buf),"%d.%d.%d.%d",
					(int)(reinterpret_cast<const unsigned char *>(&(reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr)))[0],
					(int)(reinterpret_cast<const unsigned char *>(&(reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr)))[1],
					(int)(reinterpret_cast<const unsigned char *>(&(reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr)))[2],
					(int)(reinterpret_cast<const unsigned char *>(&(reinterpret_cast<const struct sockaddr_in *>(this)->sin_addr.s_addr)))[3]
				);
			return std::string(buf);
		case AF_INET6:
			Utils::snprintf(buf,sizeof(buf),"%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x",
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[0]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[1]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[2]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[3]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[4]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[5]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[6]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[7]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[8]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[9]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[10]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[11]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[12]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[13]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[14]),
					(int)(reinterpret_cast<const struct sockaddr_in6 *>(this)->sin6_addr.s6_addr[15])
				);
			return std::string(buf);
	}
	return std::string();
}

void InetAddress::fromString(const std::string &ipSlashPort)
{
	std::size_t slashAt = ipSlashPort.find('/');
	if ((slashAt == std::string::npos)||(slashAt >= ipSlashPort.length()))
		set(ipSlashPort,0);
	else {
		long p = strtol(ipSlashPort.substr(slashAt+1).c_str(),(char **)0,10);
		if ((p > 0)&&(p <= 0xffff))
			set(ipSlashPort.substr(0,slashAt),(unsigned int)p);
		else set(ipSlashPort.substr(0,slashAt),0);
	}
}

InetAddress InetAddress::netmask() const
	throw()
{
	InetAddress r(*this);
	switch(r.ss_family) {
		case AF_INET:
			reinterpret_cast<struct sockaddr_in *>(&r)->sin_addr.s_addr = Utils::hton((uint32_t)(0xffffffff << (32 - netmaskBits())));
			break;
		case AF_INET6: {
			unsigned char *bf = reinterpret_cast<unsigned char *>(reinterpret_cast<struct sockaddr_in6 *>(&r)->sin6_addr.s6_addr);
			signed int bitsLeft = (signed int)netmaskBits();
			for(unsigned int i=0;i<16;++i) {
				if (bitsLeft > 0) {
					bf[i] |= (unsigned char)((bitsLeft >= 8) ? 0x00 : (0xff >> bitsLeft));
					bitsLeft -= 8;
				}
			}
		}	break;
	}
	return r;
}

InetAddress InetAddress::broadcast() const
	throw()
{
	InetAddress r(*this);
	switch(r.ss_family) {
		case AF_INET:
			reinterpret_cast<struct sockaddr_in *>(&r)->sin_addr.s_addr |= Utils::hton((uint32_t)(0xffffffff >> netmaskBits()));
			break;
		case AF_INET6: {
			unsigned char *bf = reinterpret_cast<unsigned char *>(reinterpret_cast<struct sockaddr_in6 *>(&r)->sin6_addr.s6_addr);
			signed int bitsLeft = (signed int)netmaskBits();
			for(unsigned int i=0;i<16;++i) {
				if (bitsLeft > 0) {
					bf[i] |= (unsigned char)((bitsLeft >= 8) ? 0x00 : (0xff >> bitsLeft));
					bitsLeft -= 8;
				}
			}
		}	break;
	}
	return r;
}

InetAddress InetAddress::makeIpv6LinkLocal(const MAC &mac)
	throw()
{
	struct sockaddr_in6 sin6;
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr.s6_addr[0] = 0xfe;
	sin6.sin6_addr.s6_addr[1] = 0x80;
	sin6.sin6_addr.s6_addr[2] = 0x00;
	sin6.sin6_addr.s6_addr[3] = 0x00;
	sin6.sin6_addr.s6_addr[4] = 0x00;
	sin6.sin6_addr.s6_addr[5] = 0x00;
	sin6.sin6_addr.s6_addr[6] = 0x00;
	sin6.sin6_addr.s6_addr[7] = 0x00;
	sin6.sin6_addr.s6_addr[8] = mac[0] & 0xfd;
	sin6.sin6_addr.s6_addr[9] = mac[1];
	sin6.sin6_addr.s6_addr[10] = mac[2];
	sin6.sin6_addr.s6_addr[11] = 0xff;
	sin6.sin6_addr.s6_addr[12] = 0xfe;
	sin6.sin6_addr.s6_addr[13] = mac[3];
	sin6.sin6_addr.s6_addr[14] = mac[4];
	sin6.sin6_addr.s6_addr[15] = mac[5];
	sin6.sin6_port = Utils::hton((uint16_t)64);
	return InetAddress(sin6);
}

} // namespace ZeroTier
