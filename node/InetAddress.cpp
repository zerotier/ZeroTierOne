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

void InetAddress::set(const std::string &ip,unsigned int port)
	throw()
{
	memset(&_sa,0,sizeof(_sa));
	if (ip.find(':') != std::string::npos) {
		_sa.sin6.sin6_family = AF_INET6;
		_sa.sin6.sin6_port = htons((uint16_t)port);
		if (inet_pton(AF_INET6,ip.c_str(),(void *)&(_sa.sin6.sin6_addr.s6_addr)) <= 0)
			_sa.saddr.sa_family = 0;
	} else {
		_sa.sin.sin_family = AF_INET;
		_sa.sin.sin_port = htons((uint16_t)port);
		if (inet_pton(AF_INET,ip.c_str(),(void *)&(_sa.sin.sin_addr.s_addr)) <= 0)
			_sa.saddr.sa_family = 0;
	}
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
	} else if (!_sa.saddr.sa_family)
		return (!a._sa.saddr.sa_family);
	return (!memcmp(&_sa,&a._sa,sizeof(_sa)));
}

bool InetAddress::operator<(const InetAddress &a) const
	throw()
{
	if (_sa.saddr.sa_family == AF_INET) {
		if (a._sa.saddr.sa_family == AF_INET)
			return ((ntohl(_sa.sin.sin_addr.s_addr < ntohl(a._sa.sin.sin_addr.s_addr)))||((_sa.sin.sin_addr.s_addr == a._sa.sin.sin_addr.s_addr)&&(ntohs(_sa.sin.sin_port) < ntohs(a._sa.sin.sin_port))));
		else if (a._sa.saddr.sa_family == AF_INET6)
			return true;
	} else if (_sa.saddr.sa_family == AF_INET6) {
		if (a._sa.saddr.sa_family == AF_INET6) {
			int cmp = memcmp(_sa.sin6.sin6_addr.s6_addr,a._sa.sin6.sin6_addr.s6_addr,16);
			return ((cmp < 0)||((!cmp)&&(ntohs(_sa.sin6.sin6_port) < ntohs(a._sa.sin6.sin6_port))));
		} else if (a._sa.saddr.sa_family == AF_INET)
			return false;
	}
	return (_sa.saddr.sa_family < a._sa.saddr.sa_family);
}

} // namespace ZeroTier
