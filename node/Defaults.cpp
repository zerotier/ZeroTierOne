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

#ifdef ZT_USE_TESTNET

	// If ZT_USE_TESTNET is defined we talk to test rather than live supernode
	// instances. The testnet may not always be running, so this is probably not
	// of any interest to users. Testnet servers run on port 7773 (UDP) and
	// 773 (TCP).

	// cthulhu.zerotier.com - New York, New York, USA
	addrs.clear();
	if (!id.fromString("0bfa76f104:0:aff4d4604f2a2538d414a1d69fc722a28bea049d52192aded117c28b0f6c1052db9d36c488c5fe5e2071f2def8f86b6db64db09e819f90fdaedbfcb9f3bcdef9"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("162.243.77.111",7773),false));
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("162.243.77.111",773),true));
	sn[id] = addrs;

	// nyarlathotep.zerotier.com - San Francisco, California, USA
	addrs.clear();
	if (!id.fromString("9f2b042cdb:0:8993f9348bb9642afa9a60995a35ef19817894fd0b6859201c0e56e399288867c8f0d01ae2858f9dc6f95eee6d42e2f6d08c44551404906b25679aa6db1faee7"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("198.199.97.220",7773),false));
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("198.199.97.220",773),true));
	sn[id] = addrs;

	// shub-niggurath.zerotier.com - Amsterdam, Netherlands
	addrs.clear();
	if (!id.fromString("916a4ca17d:0:b679a8d6761096ba4958fea0036dc4dbb76cb8cbf1ce9bc352cc594c3c24987bb3b30b5448d1f494f5e90a6cdaac9d28317cb4088780278ef20bc7c366cb214a"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("198.211.127.172",7773),false));
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("198.211.127.172",773),true));
	sn[id] = addrs;

	// yig.zerotier.com - Sydney, Australia
	addrs.clear();
	if (!id.fromString("3b62c7a69a:0:d967595a3b96d780151764e6ffb47af2fa8865f8e344fba4a684c10dd2e70014e26312f5b8a1590c13bfeb909a1fd35b96a84a8a43e0704cd8d01d9c2b791359"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("108.61.212.61",7773),false));
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("108.61.212.61",773),true));
	sn[id] = addrs;

	// shoggoth.zerotier.com - Tokyo, Japan
	addrs.clear();
	if (!id.fromString("345ad16512:0:9e796aec6e083726f45fbfdc10bcf18c0dc7a7914c9ce29f5eb5abcf41bfcb6b3698b68131d347235ae488804317df9c6102e2753841b973037d1e4685dce9fc"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("108.61.200.101",7773),false));
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("108.61.200.101",773),true));
	sn[id] = addrs;

#else

	// Normally we use the live supernodes.

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

	// yig.zerotier.com - Sydney, Australia
	addrs.clear();
	if (!id.fromString("275f0151f6:0:58716258283f7e14a2f999875d9cc681c1f0ca8403dce38ec354ceaf284a555f36402e79a32d03b8c0963245b7f1af61a1ad3519d90e05bc3ce591034f6a1c9c"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("108.61.212.61",ZT_DEFAULT_UDP_PORT),false));
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("108.61.212.61",443),true));
	sn[id] = addrs;

	// shoggoth.zerotier.com - Tokyo, Japan
	addrs.clear();
	if (!id.fromString("48e8f875cb:0:5ca54f55e1094f65589f3e6d74158b6964d418ddac3570757128f1c6a2498322d92fcdcd47de459f4d1f9b38df2afd0c7b3fc247ba3d773c38ba35288f24988e"))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("108.61.200.101",ZT_DEFAULT_UDP_PORT),false));
	addrs.push_back(std::pair<InetAddress,bool>(InetAddress("108.61.200.101",443),true));
	sn[id] = addrs;

#endif

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
	updateLatestNfoURL(_mkUpdateUrl()),
	v4Broadcast(((uint32_t)0xffffffff),ZT_DEFAULT_UDP_PORT)
{
}

} // namespace ZeroTier
