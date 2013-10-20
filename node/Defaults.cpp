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
#include <stdlib.h>
#include <string.h>

#include "Constants.hpp"
#include "Defaults.hpp"
#include "Utils.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#endif

namespace ZeroTier {

const Defaults ZT_DEFAULTS;

static inline std::map< Identity,std::vector<InetAddress> > _mkSupernodeMap()
{
	std::map< Identity,std::vector<InetAddress> > sn;
	Identity id;
	std::vector<InetAddress> addrs;

	// Nothing special about a supernode... except that they are
	// designated as such and trusted to provide WHOIS lookup.

	// cthulhu.zerotier.com - New York, New York, USA
	addrs.clear();
	if (!id.fromString("21e93d708a:0:f6bfe55675a3e66f6169eefc73187a64693a994ede68eba75c9ea65818730a2b03da67a41ce7023a68e5fcd44087529c123e802f510ee00dc9162244ccd63a88"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(InetAddress("198.199.73.93",ZT_DEFAULT_UDP_PORT));
	sn[id] = addrs;

	// nyarlathotep.zerotier.com - San Francisco, California, USA
	addrs.clear();
	if (!id.fromString("d383d43143:0:7f6fe9386bf67de895869a8a949dad651c1003d3ffb382e9971f9a1ab1513f451ecfe00c6a1c45998ab9fc9a4b4f88f06651d9323752d75eaa2a44a10125e79d"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(InetAddress("198.199.97.220",ZT_DEFAULT_UDP_PORT));
	sn[id] = addrs;

	// shub-niggurath.zerotier.com - Amsterdam, Netherlands
	addrs.clear();
	if (!id.fromString("b6179d3e54:0:e4c7ac071b0ed0820da9cec34a5269398e7fa304bf7ded8faef8f929b247be1a6e0acc0f1ff8e6159f53fc94a797b05e27357891db854f693025e94582d8206d"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(InetAddress("198.211.127.172",ZT_DEFAULT_UDP_PORT));
	sn[id] = addrs;

	return sn;
}

static inline std::string _mkDefaultHomePath()
{
#ifdef __UNIX_LIKE__
#ifdef __APPLE__
	return std::string("/Library/Application Support/ZeroTier/One");
#else
	return std::string("/var/lib/zerotier-one");
#endif
#else
#ifdef __WINDOWS__
	OSVERSIONINFO vi;
	memset (&vi,0,sizeof(vi));
	vi.dwOSVersionInfoSize = sizeof(vi);
	GetVersionEx(&vi);
	if (vi.dwMajorVersion < 6)
		return std::string("C:\\Documents and Settings\\All Users\\Application Data\\ZeroTier\\One");
	return std::string("C:\\ProgramData\\ZeroTier\\One");
#else
	// unknown platform
#endif
#endif
}

Defaults::Defaults() :
#ifdef ZT_TRACE_MULTICAST
	multicastTraceWatcher(ZT_TRACE_MULTICAST),
#endif
	defaultHomePath(_mkDefaultHomePath()),
	supernodes(_mkSupernodeMap())
{
}

} // namespace ZeroTier
