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
#include <stdlib.h>
#include <string.h>

#include "Constants.hpp"
#include "Defaults.hpp"
#include "Utils.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#include <ShlObj.h>
#endif

namespace ZeroTier {

const Defaults ZT_DEFAULTS;

static inline std::map< Identity,std::vector< std::pair<InetAddress,bool> > > _mkSupernodeMap()
{
	std::map< Identity,std::vector< std::pair<InetAddress,bool> > > sn;
	Identity id;
	std::vector< std::pair<InetAddress,bool> > addrs;

	// Nothing special about a supernode... except that they are
	// designated as such and trusted to provide WHOIS lookup.

	// cthulhu.zerotier.com - New York, New York, USA
	addrs.clear();
	if (!id.fromString("8acf059fe3:0:482f6ee5dfe902319b419de5bdc765209c0ecda38c4d6e4fcf0d33658398b4527dcd22f93112fb9befd02fd78bf7261b333fc105d192a623ca9e50fc60b374a5"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("162.243.77.111",ZT_DEFAULT_UDP_PORT),false));
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("162.243.77.111",443),true));
	sn[id] = addrs;

	// nyarlathotep.zerotier.com - San Francisco, California, USA
	addrs.clear();
	if (!id.fromString("7e19876aba:0:2a6e2b2318930f60eb097f70d0f4b028b2cd6d3d0c63c014b9039ff35390e41181f216fb2e6fa8d95c1ee9667156411905c3dccfea78d8c6dfafba688170b3fa"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("198.199.97.220",ZT_DEFAULT_UDP_PORT),false));
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("198.199.97.220",443),true));
	sn[id] = addrs;

	// shub-niggurath.zerotier.com - Amsterdam, Netherlands
	addrs.clear();
	if (!id.fromString("36f63d6574:0:67a776487a1a99b32f413329f2b67c43fbf6152e42c6b66e89043e69d93e48314c7d709b58a83016bd2612dd89400b856e18c553da94892f7d3ca16bf2c92c24"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("198.211.127.172",ZT_DEFAULT_UDP_PORT),false));
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("198.211.127.172",443),true));
	sn[id] = addrs;

	// mi-go.zerotier.com - Singapore
	addrs.clear();
	if (!id.fromString("abbb7f4622:0:89d2c6b2062b10f4ce314dfcb914c082566247090a6f74c8ba1c15c63b205f540758f0abae85287397152c9d8cf463cfe51e7a480946cd6a31495b24ca13253c"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("128.199.254.204",ZT_DEFAULT_UDP_PORT),false));
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("128.199.254.204",443),true));
	sn[id] = addrs;

	// shoggoth.zerotier.com - Tokyo, Japan
	addrs.clear();
	if (!id.fromString("48e8f875cb:0:5ca54f55e1094f65589f3e6d74158b6964d418ddac3570757128f1c6a2498322d92fcdcd47de459f4d1f9b38df2afd0c7b3fc247ba3d773c38ba35288f24988e"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("108.61.200.101",ZT_DEFAULT_UDP_PORT),false));
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("108.61.200.101",443),true));
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

#else // not __UNIX_LIKE__

#ifdef __WINDOWS__
	char buf[16384];
	if (SUCCEEDED(SHGetFolderPathA(NULL,CSIDL_COMMON_APPDATA,NULL,0,buf)))
		return (std::string(buf) + "\\ZeroTier\\One");
	else return std::string("C:\\ZeroTier\\One");
#else
	// unknown platform
#endif

#endif // __UNIX_LIKE__ or not...
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
#ifdef ZT_TRACE_MULTICAST
	multicastTraceWatcher(ZT_TRACE_MULTICAST),
#endif
	defaultHomePath(_mkDefaultHomePath()),
	supernodes(_mkSupernodeMap()),
	updateAuthorities(_mkUpdateAuth()),
	updateLatestNfoURL(_mkUpdateUrl())
{
}

} // namespace ZeroTier
