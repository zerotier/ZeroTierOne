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
	if (!id.fromString("d2ba4048c3:0:cfa02701eb69e2f2c64aa498151e57f82c172036967186c93f2afbe45a64d64306f88fa1225724f6f87beabd6a0feb18c746cf4691867542e18b894390692303"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(InetAddress("198.199.73.93",ZT_DEFAULT_UDP_PORT));
	sn[id] = addrs;

	// nyarlathotep.zerotier.com - San Francisco, California, USA
	addrs.clear();
	if (!id.fromString("80eb92f707:0:7f0209663d815438dead321ec78c65c27fec6feeb8ccd9acc152c59066740521e45d1a1cbc5186e3773178429c4b26ab0df2c78f3e822540d70456724797f23f"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(InetAddress("198.199.97.220",ZT_DEFAULT_UDP_PORT));
	sn[id] = addrs;

	// shub-niggurath.zerotier.com - Amsterdam, Netherlands
	addrs.clear();
	if (!id.fromString("34594c9086:0:2ac70c00cc03a078a2a8f889d61e72397f8cd684a6daecbe6350f24e0193790e118805a1673eaf57ce9f7b41f45525c995289c406461c739ccad2c93efa36746"))
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
