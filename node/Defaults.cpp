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
#include <stdlib.h>
#include <string.h>

#include "Constants.hpp"
#include "Defaults.hpp"
#include "Utils.hpp"

// bin2c'd signed default root topology dictionary
#include "../root-topology/ZT_DEFAULT_ROOT_TOPOLOGY.c"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#include <ShlObj.h>
#endif

namespace ZeroTier {

const Defaults ZT_DEFAULTS;

static inline std::string _mkDefaultHomePath()
{
#ifdef __UNIX_LIKE__

#ifdef __APPLE__
	// /Library/... on Apple
	return std::string("/Library/Application Support/ZeroTier/One");
#else

#ifdef __FreeBSD__
	// FreeBSD likes /var/db instead of /var/lib
	return std::string("/var/db/zerotier-one");
#else
	// Use /var/lib for Linux and other *nix
	return std::string("/var/lib/zerotier-one");
#endif

#endif

#else // not __UNIX_LIKE__

#ifdef __WINDOWS__
	// Look up app data folder on Windows, e.g. C:\ProgramData\...
	char buf[16384];
	if (SUCCEEDED(SHGetFolderPathA(NULL,CSIDL_COMMON_APPDATA,NULL,0,buf)))
		return (std::string(buf) + "\\ZeroTier\\One");
	else return std::string("C:\\ZeroTier\\One");
#else
#error Unknown platform, please define a default home path!
#endif

#endif // __UNIX_LIKE__ or not...
}

static inline std::map< Address,Identity > _mkRootTopologyAuth()
{
	std::map< Address,Identity > ua;

	{ // 0001
		Identity id("77792b1c02:0:b5c361e8e9c2154e82c3e902fdfc337468b092a7c4d8dc685c37eb10ee4f3c17cc0bb1d024167e8cb0824d12263428373582da3d0a9a14b36e4546c317e811e6");
		ua[id.address()] = id;
	}
	{ // 0002
		Identity id("86921e6de1:0:9ba04f9f12ed54ef567f548cb69d31e404537d7b0ee000c63f3d7c8d490a1a47a5a5b2af0cbe12d23f9194270593f298d936d7c872612ea509ef1c67ce2c7fc1");
		ua[id.address()] = id;
	}
	{ // 0003
		Identity id("90302b7025:0:358154a57af1b7afa07d0d91b69b92eaad2f11ade7f02343861f0c1b757d15626e8cb7f08fc52993d2202a39cbf5128c5647ee8c63d27d92db5a1d0fbe1eba19");
		ua[id.address()] = id;
	}
	{ // 0004
		Identity id("e5174078ee:0:c3f90daa834a74ee47105f5726ae2e29fc8ae0e939c9326788b52b16d847354de8de3b13a81896bbb509b91e1da21763073a30bbfb2b8e994550798d30a2d709");
		ua[id.address()] = id;
	}

	return ua;
}

static inline std::map< Address,Identity > _mkUpdateAuth()
{
	std::map< Address,Identity > ua;

	{ // 0001
		Identity id("e9bc3707b5:0:c4cef17bde99eadf9748c4fd11b9b06dc5cd8eb429227811d2c336e6b96a8d329e8abd0a4f45e47fe1bcebf878c004c822d952ff77fc2833af4c74e65985c435");
		ua[id.address()] = id;
	}
	{ // 0002
		Identity id("56520eaf93:0:7d858b47988b34399a9a31136de07b46104d7edb4a98fa1d6da3e583d3a33e48be531532b886f0b12cd16794a66ab9220749ec5112cbe96296b18fe0cc79ca05");
		ua[id.address()] = id;
	}
	{ // 0003
		Identity id("7c195de2e0:0:9f659071c960f9b0f0b96f9f9ecdaa27c7295feed9c79b7db6eedcc11feb705e6dd85c70fa21655204d24c897865b99eb946b753a2bbcf2be5f5e006ae618c54");
		ua[id.address()] = id;
	}
	{ // 0004
		Identity id("415f4cfde7:0:54118e87777b0ea5d922c10b337c4f4bd1db7141845bd54004b3255551a6e356ba6b9e1e85357dbfafc45630b8faa2ebf992f31479e9005f0472685f2d8cbd6e");
		ua[id.address()] = id;
	}

	return ua;
}

static inline const char *_mkUpdateUrl()
{
#if defined(__LINUX__) && ( defined(__i386__) || defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__i386) )
	if (sizeof(void *) == 8)
		return "http://download.zerotier.com/ZeroTierOneInstaller-linux-x64-LATEST.nfo";
	else return "http://download.zerotier.com/ZeroTierOneInstaller-linux-x86-LATEST.nfo";
#define GOT_UPDATE_URL
#endif

#ifdef __APPLE__
	return "http://download.zerotier.com/ZeroTierOneInstaller-mac-combined-LATEST.nfo";
#define GOT_UPDATE_URL
#endif

#ifdef __WINDOWS__
	return "http://download.zerotier.com/ZeroTierOneInstaller-windows-intel-LATEST.nfo";
#define GOT_UPDATE_URL
#endif

#ifndef GOT_UPDATE_URL
	return "";
#endif
}

Defaults::Defaults() :
	defaultHomePath(_mkDefaultHomePath()),
	defaultRootTopology((const char *)ZT_DEFAULT_ROOT_TOPOLOGY,ZT_DEFAULT_ROOT_TOPOLOGY_LEN),
	rootTopologyAuthorities(_mkRootTopologyAuth()),
	updateAuthorities(_mkUpdateAuth()),
	updateLatestNfoURL(_mkUpdateUrl()),
	v4Broadcast(((uint32_t)0xffffffff),ZT_DEFAULT_UDP_PORT)
{
}

} // namespace ZeroTier
