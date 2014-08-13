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

void InetAddress::set(const std::string &ip,unsigned int port)
	throw()
{
	memset(&_sa,0,sizeof(_sa));
	if (ip.find(':') != std::string::npos) {
		_sa.sin6.sin6_family = AF_INET6;
		_sa.sin6.sin6_port = Utils::hton((uint16_t)port);
		if (inet_pton(AF_INET6,ip.c_str(),(void *)&(_sa.sin6.sin6_addr.s6_addr)) <= 0)
			_sa.saddr.sa_family = 0;
	} else {
		_sa.sin.sin_family = AF_INET;
		_sa.sin.sin_port = Utils::hton((uint16_t)port);
		if (inet_pton(AF_INET,ip.c_str(),(void *)&(_sa.sin.sin_addr.s_addr)) <= 0)
			_sa.saddr.sa_family = 0;
	}
}

void InetAddress::set(const void *ipBytes,unsigned int ipLen,unsigned int port)
	throw()
{
	memset(&_sa,0,sizeof(_sa));
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

bool InetAddress::isLinkLocal() const
	throw()
{
	if (_sa.saddr.sa_family == AF_INET)
		return ((Utils::ntoh((uint32_t)_sa.sin.sin_addr.s_addr) & 0xffff0000) == 0xa9fe0000);
	else if (_sa.saddr.sa_family == AF_INET6) {
		if (_sa.sin6.sin6_addr.s6_addr[0] != 0xfe) return false;
		if (_sa.sin6.sin6_addr.s6_addr[1] != 0x80) return false;
		if (_sa.sin6.sin6_addr.s6_addr[2] != 0x00) return false;
		if (_sa.sin6.sin6_addr.s6_addr[3] != 0x00) return false;
		if (_sa.sin6.sin6_addr.s6_addr[4] != 0x00) return false;
		if (_sa.sin6.sin6_addr.s6_addr[5] != 0x00) return false;
		if (_sa.sin6.sin6_addr.s6_addr[6] != 0x00) return false;
		if (_sa.sin6.sin6_addr.s6_addr[7] != 0x00) return false;
		return true;
	}
	return false;
}

bool InetAddress::isDefaultRoute() const
	throw()
{
	if (_sa.saddr.sa_family == AF_INET)
		return ((_sa.sin.sin_addr.s_addr == 0)&&(_sa.sin.sin_port == 0));
	else if (_sa.saddr.sa_family == AF_INET6)
		return ((Utils::isZero(_sa.sin6.sin6_addr.s6_addr,16))&&(_sa.sin6.sin6_port == 0));
	return false;
}

std::string InetAddress::toString() const
{
	char buf[128],buf2[128];

	switch(_sa.saddr.sa_family) {
		case AF_INET:
#ifdef __WINDOWS__
			if (inet_ntop(AF_INET,(PVOID)&(_sa.sin.sin_addr.s_addr),buf,sizeof(buf))) {
#else
			if (inet_ntop(AF_INET,(const void *)&(_sa.sin.sin_addr.s_addr),buf,sizeof(buf))) {
#endif
				Utils::snprintf(buf2,sizeof(buf2),"%s/%u",buf,(unsigned int)ntohs(_sa.sin.sin_port));
				return std::string(buf2);
			}
			break;
		case AF_INET6:
#ifdef __WINDOWS__
			if (inet_ntop(AF_INET6,(PVOID)&(_sa.sin6.sin6_addr.s6_addr),buf,sizeof(buf))) {
#else
			if (inet_ntop(AF_INET6,(const void *)&(_sa.sin6.sin6_addr.s6_addr),buf,sizeof(buf))) {
#endif
				Utils::snprintf(buf2,sizeof(buf2),"%s/%u",buf,(unsigned int)ntohs(_sa.sin6.sin6_port));
				return std::string(buf2);
			}
			break;
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

std::string InetAddress::toIpString() const
{
	char buf[128];
	switch(_sa.saddr.sa_family) {
		case AF_INET:
#ifdef __WINDOWS__
			if (inet_ntop(AF_INET,(PVOID)&(_sa.sin.sin_addr.s_addr),buf,sizeof(buf)))
				return std::string(buf);
#else
			if (inet_ntop(AF_INET,(const void *)&(_sa.sin.sin_addr.s_addr),buf,sizeof(buf)))
				return std::string(buf);
#endif
			break;
		case AF_INET6:
#ifdef __WINDOWS__
			if (inet_ntop(AF_INET6,(PVOID)&(_sa.sin6.sin6_addr.s6_addr),buf,sizeof(buf)))
				return std::string(buf);
#else
			if (inet_ntop(AF_INET6,(const void *)&(_sa.sin6.sin6_addr.s6_addr),buf,sizeof(buf)))
				return std::string(buf);
#endif
			break;
	}
	return std::string();
}

InetAddress InetAddress::netmask() const
	throw()
{
	InetAddress r(*this);
	switch(_sa.saddr.sa_family) {
		case AF_INET:
			r._sa.sin.sin_addr.s_addr = Utils::hton((uint32_t)(0xffffffff << (32 - netmaskBits())));
			break;
		case AF_INET6: {
			unsigned char *bf = (unsigned char *)r._sa.sin6.sin6_addr.s6_addr;
			signed int bitsLeft = (signed int)netmaskBits();
			for(unsigned int i=0;i<16;++i) {
				if (bitsLeft > 0) {
					bf[i] = (unsigned char)((bitsLeft >= 8) ? 0xff : (0xff << (8 - bitsLeft)));
					bitsLeft -= 8;
				} else bf[i] = (unsigned char)0;
			}
		}	break;
	}
	return r;
}

bool InetAddress::sameNetworkAs(const InetAddress &ipnet) const
	throw()
{
	if (_sa.saddr.sa_family != ipnet._sa.saddr.sa_family)
		return false;

	unsigned int bits = netmaskBits();
	if (bits != ipnet.netmaskBits())
		return false;
	if (!bits)
		return true;
	switch(_sa.saddr.sa_family) {
		case AF_INET:
			if (bits >= 32) bits = 32;
			break;
		case AF_INET6:
			if (bits >= 128) bits = 128;
			break;
		default:
			return false;
	}

	const uint8_t *a = (const uint8_t *)rawIpData();
	const uint8_t *b = (const uint8_t *)ipnet.rawIpData();
	while (bits >= 8) {
		if (*(a++) != *(b++))
			return false;
		bits -= 8;
	}
	bits = 8 - bits;
	return ((*a >> bits) == (*b >> bits));
}

bool InetAddress::within(const InetAddress &ipnet) const
	throw()
{
	if (_sa.saddr.sa_family != ipnet._sa.saddr.sa_family)
		return false;

	unsigned int bits = ipnet.netmaskBits();
	switch(_sa.saddr.sa_family) {
		case AF_INET:
			if (bits > 32) return false;
			break;
		case AF_INET6:
			if (bits > 128) return false;
			break;
		default: return false;
	}

	const uint8_t *a = (const uint8_t *)rawIpData();
	const uint8_t *b = (const uint8_t *)ipnet.rawIpData();
	while (bits >= 8) {
		if (*(a++) != *(b++))
			return false;
		bits -= 8;
	}
	if (bits) {
		uint8_t mask = ((0xff << (8 - bits)) & 0xff);
		return ((*a & mask) == (*b & mask));
	} else return true;
}

bool InetAddress::operator==(const InetAddress &a) const
	throw()
{
	if (_sa.saddr.sa_family == AF_INET) {
		if (a._sa.saddr.sa_family == AF_INET)
			return ((_sa.sin.sin_addr.s_addr == a._sa.sin.sin_addr.s_addr)&&(_sa.sin.sin_port == a._sa.sin.sin_port));
		return false;
	} else if (_sa.saddr.sa_family == AF_INET6) {
		if (a._sa.saddr.sa_family == AF_INET6) {
			if (_sa.sin6.sin6_port == a._sa.sin6.sin6_port)
				return (!memcmp(_sa.sin6.sin6_addr.s6_addr,a._sa.sin6.sin6_addr.s6_addr,sizeof(_sa.sin6.sin6_addr.s6_addr)));
		}
		return false;
	} else return (memcmp(&_sa,&a._sa,sizeof(_sa)) == 0);
}

bool InetAddress::operator<(const InetAddress &a) const
	throw()
{
	if (_sa.saddr.sa_family < a._sa.saddr.sa_family)
		return true;
	else if (_sa.saddr.sa_family == a._sa.saddr.sa_family) {
		if (_sa.saddr.sa_family == AF_INET) {
			unsigned long x = Utils::ntoh((uint32_t)_sa.sin.sin_addr.s_addr);
			unsigned long y = Utils::ntoh((uint32_t)a._sa.sin.sin_addr.s_addr);
			if (x == y)
				return (Utils::ntoh((uint16_t)_sa.sin.sin_port) < Utils::ntoh((uint16_t)a._sa.sin.sin_port));
			else return (x < y);
		} else if (_sa.saddr.sa_family == AF_INET6) {
			int cmp = (int)memcmp(_sa.sin6.sin6_addr.s6_addr,a._sa.sin6.sin6_addr.s6_addr,16);
			if (cmp == 0)
				return (Utils::ntoh((uint16_t)_sa.sin6.sin6_port) < Utils::ntoh((uint16_t)a._sa.sin6.sin6_port));
			else return (cmp < 0);
		} else return (memcmp(&_sa,&a._sa,sizeof(_sa)) < 0);
	}
	return false;
}

InetAddress InetAddress::makeIpv6LinkLocal(const MAC &mac)
	throw()
{
	InetAddress ip;
	ip._sa.saddr.sa_family = AF_INET6;
	ip._sa.sin6.sin6_addr.s6_addr[0] = 0xfe;
	ip._sa.sin6.sin6_addr.s6_addr[1] = 0x80;
	ip._sa.sin6.sin6_addr.s6_addr[2] = 0x00;
	ip._sa.sin6.sin6_addr.s6_addr[3] = 0x00;
	ip._sa.sin6.sin6_addr.s6_addr[4] = 0x00;
	ip._sa.sin6.sin6_addr.s6_addr[5] = 0x00;
	ip._sa.sin6.sin6_addr.s6_addr[6] = 0x00;
	ip._sa.sin6.sin6_addr.s6_addr[7] = 0x00;
	ip._sa.sin6.sin6_addr.s6_addr[8] = mac[0] & 0xfd;
	ip._sa.sin6.sin6_addr.s6_addr[9] = mac[1];
	ip._sa.sin6.sin6_addr.s6_addr[10] = mac[2];
	ip._sa.sin6.sin6_addr.s6_addr[11] = 0xff;
	ip._sa.sin6.sin6_addr.s6_addr[12] = 0xfe;
	ip._sa.sin6.sin6_addr.s6_addr[13] = mac[3];
	ip._sa.sin6.sin6_addr.s6_addr[14] = mac[4];
	ip._sa.sin6.sin6_addr.s6_addr[15] = mac[5];
	ip._sa.sin6.sin6_port = Utils::hton((uint16_t)64);
	return ip;
}

} // namespace ZeroTier
