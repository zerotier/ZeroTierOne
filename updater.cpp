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
#include <stdint.h>
#include <time.h>

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <stdexcept>

#include "version.h"
#include "include/ZeroTierOne.h"
#include "node/Constants.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#include <tchar.h>
#include <wchar.h>
#include <lmcons.h>
#include <newdev.h>
#include <atlbase.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#endif

#include "node/Utils.hpp"
#include "node/Address.hpp"
#include "node/Dictionary.hpp"
#include "node/Identity.hpp"
#include "osdep/OSUtils.hpp"
#include "osdep/Http.hpp"

using namespace ZeroTier;

namespace {

static std::map< Address,Identity > updateAuthorities()
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

static bool validateUpdate(
	const void *data,
	unsigned int len,
	const Address &signedBy,
	const std::string &signature)
{
	std::map< Address,Identity > ua(updateAuthorities());
	std::map< Address,Identity >::const_iterator updateAuthority = ua.find(signedBy);
	if (updateAuthority == ua.end())
		return false;
	return updateAuthority->second.verify(data,len,signature.data(),(unsigned int)signature.length());
}

/*
static inline const char *updateUrl()
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
*/

static const char *parseUpdateNfo(
	const char *nfoText,
	unsigned int &vMajor,
	unsigned int &vMinor,
	unsigned int &vRevision,
	Address &signedBy,
	std::string &signature,
	std::string &url)
{
	try {
		Dictionary nfo(nfoText);

		vMajor = Utils::strToUInt(nfo.get("vMajor").c_str());
		vMinor = Utils::strToUInt(nfo.get("vMinor").c_str());
		vRevision = Utils::strToUInt(nfo.get("vRevision").c_str());
		signedBy = nfo.get("signedBy");
		signature = Utils::unhex(nfo.get("ed25519"));
		url = nfo.get("url");

		if (signature.length() != ZT_C25519_SIGNATURE_LEN)
			return "bad ed25519 signature, invalid length";
		if ((url.length() <= 7)||(url.substr(0,7) != "http://"))
			return "invalid URL, must begin with http://";

		return (const char *)0;
	} catch ( ... ) {
		return "invalid NFO file format or one or more required fields missing";
	}
}

} // anonymous namespace

#ifdef __WINDOWS__
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc,char **argv)
#endif
{
#ifdef __WINDOWS__
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2),&wsaData);
#endif

	return 0;
}
