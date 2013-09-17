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
	throw(std::runtime_error)
{
	std::map< Identity,std::vector<InetAddress> > sn;
	Identity id;
	std::vector<InetAddress> addrs;

	// Nothing special about a supernode... except that they are
	// designated as such and trusted to provide WHOIS lookup.

	// cthulhu.zerotier.com - New York, New York, USA
	addrs.clear();
	if (!id.fromString("a0fa79d81c:2:0bb348bb38883a29054659a37c204f2c0b082985cb51b36fad31366dfedd616c20aacc5e33ceee2b054670639563238c4fe50bb8716c1ac7996762c0eaefbb23:b7e91f4c77815327c59ff0979f33861e665d002a357448572954c85919be61f768ee6a4d4e42318ffd9cfcc08cadedcd0277a33a950e316a1d7b5bf082919400c44cad1e725fc2035e2d7087d0c8bf51adc5875b643d759a475f899cfbf3e1a4"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(InetAddress("198.199.73.93",ZT_DEFAULT_UDP_PORT));
	sn[id] = addrs;

	// nyarlathotep.zerotier.com - San Francisco, California, USA
	addrs.clear();
	if (!id.fromString("1521e171ab:2:43bcdc31f2d75667163f3384bc8866e95ce39b4735999e7760494f6480e0fb70f45675f887f8fdfe50e47b082f3fcfc589381f78b3d3bd1dcbf888ccf14d7935:5026836a5732ed890e778f46ded38410dda51c448f82ab76dd0d2c0152bddd5f05fee2fedf8c9f4ccf1f6181f2cdc1f723c59a143a9928c560b2da652f656507f490acfe70e8f5b2a2bba0eca4ea85b03ce00480afd00d49fc756a03bb740592"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(InetAddress("198.199.97.220",ZT_DEFAULT_UDP_PORT));
	sn[id] = addrs;

	// shub-niggurath.zerotier.com - Amsterdam, Netherlands
	addrs.clear();
	if (!id.fromString("11c3bddb9a:2:27e1c10a937dde0d6013e7a93755040ff93a98f5bcad809722a6dcde0b255f07da523f9eae818079be0deccbd4572d2e746fe7b8ba8ae6a7a15bdf0456062c37:5f0a7ea9615388a5532c8ce58f9352ba8950c8b3db261d60c02e1ed5a1a42a5e79bc757b38d8a94d00d8e738a6a33cd9b1586022bdff77c9c49ae16609cf5d03f0f60e36a67467c01870ccf26f61793853b93fb6eab53f65f20f623898e9d28d"))
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

Defaults::Defaults()
	throw(std::runtime_error) :
	defaultHomePath(_mkDefaultHomePath()),
	supernodes(_mkSupernodeMap())
{
}

} // namespace ZeroTier
