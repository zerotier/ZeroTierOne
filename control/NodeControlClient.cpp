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

#include "NodeControlClient.hpp"
#include "../node/Constants.hpp"
#include "../node/Utils.hpp"
#include "../node/Defaults.hpp"
#include "IpcConnection.hpp"
#include "IpcListener.hpp"
#include "NodeControlService.hpp"

namespace ZeroTier {

struct _NodeControlClientImpl
{
	void (*resultHandler)(void *,const char *);
	void *arg;
	IpcConnection *ipcc;
	std::string err;
};

static void _CBipcResultHandler(void *arg,IpcConnection *ipcc,IpcConnection::EventType event,const char *result)
{
	if ((event == IpcConnection::IPC_EVENT_COMMAND)&&(result)) {
		if (strcmp(result,"200 auth OK"))
			((_NodeControlClientImpl *)arg)->resultHandler(((_NodeControlClientImpl *)arg)->arg,result);
	}
}

NodeControlClient::NodeControlClient(const char *hp,void (*resultHandler)(void *,const char *),void *arg,const char *authToken)
	throw() :
	_impl((void *)new _NodeControlClientImpl)
{
	_NodeControlClientImpl *impl = (_NodeControlClientImpl *)_impl;
	impl->ipcc = (IpcConnection *)0;

	if (!hp)
		hp = ZT_DEFAULTS.defaultHomePath.c_str();

	std::string at;
	if (authToken)
		at = authToken;
	else if (!Utils::readFile(authTokenDefaultSystemPath(),at)) {
		if (!Utils::readFile(authTokenDefaultUserPath(),at)) {
			impl->err = "no authentication token specified and authtoken.secret not readable";
			return;
		}
	}

	std::string myid;
	if (Utils::readFile((std::string(hp) + ZT_PATH_SEPARATOR_S + "identity.public").c_str(),myid)) {
		std::string myaddr(myid.substr(0,myid.find(':')));
		if (myaddr.length() != 10)
			impl->err = "invalid address extracted from identity.public";
		else {
			try {
				impl->resultHandler = resultHandler;
				impl->arg = arg;
				impl->ipcc = new IpcConnection((std::string(ZT_IPC_ENDPOINT_BASE) + myaddr).c_str(),&_CBipcResultHandler,_impl);
				impl->ipcc->printf("auth %s"ZT_EOL_S,at.c_str());
			} catch ( ... ) {
				impl->ipcc = (IpcConnection *)0;
				impl->err = "failure connecting to running ZeroTier One service";
			}
		}
	} else impl->err = "unable to read identity.public";
}

NodeControlClient::~NodeControlClient()
{
	if (_impl) {
		delete ((_NodeControlClientImpl *)_impl)->ipcc;
		delete (_NodeControlClientImpl *)_impl;
	}
}

const char *NodeControlClient::error() const
	throw()
{
	if (((_NodeControlClientImpl *)_impl)->err.length())
		return ((_NodeControlClientImpl *)_impl)->err.c_str();
	return (const char *)0;
}

void NodeControlClient::send(const char *command)
	throw()
{
	try {
		if (((_NodeControlClientImpl *)_impl)->ipcc)
			((_NodeControlClientImpl *)_impl)->ipcc->printf("%s"ZT_EOL_S,command);
	} catch ( ... ) {}
}

std::vector<std::string> NodeControlClient::splitLine(const char *line)
{
	return Utils::split(line," ","\\","\"");
}

const char *NodeControlClient::authTokenDefaultUserPath()
{
	static std::string dlp;
	static Mutex dlp_m;

	Mutex::Lock _l(dlp_m);

#ifdef __WINDOWS__

	if (!dlp.length()) {
		char buf[16384];
		if (SUCCEEDED(SHGetFolderPathA(NULL,CSIDL_APPDATA,NULL,0,buf)))
			dlp = (std::string(buf) + "\\ZeroTier\\One\\authtoken.secret");
	}

#else // not __WINDOWS__

	if (!dlp.length()) {
		const char *home = getenv("HOME");
		if (home) {
#ifdef __APPLE__
			dlp = (std::string(home) + "/Library/Application Support/ZeroTier/One/authtoken.secret");
#else
			dlp = (std::string(home) + "/.zeroTierOneAuthToken");
#endif
		}
	}

#endif // __WINDOWS__ or not __WINDOWS__

	return dlp.c_str();
}

const char *NodeControlClient::authTokenDefaultSystemPath()
{
	static std::string dsp;
	static Mutex dsp_m;

	Mutex::Lock _l(dsp_m);

	if (!dsp.length())
		dsp = (ZT_DEFAULTS.defaultHomePath + ZT_PATH_SEPARATOR_S"authtoken.secret");

	return dsp.c_str();
}

} // namespace ZeroTier
