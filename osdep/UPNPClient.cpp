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

#ifdef ZT_USE_MINIUPNPC

// Uncomment to dump debug messages
//#define ZT_UPNP_TRACE 1

// Uncomment to build a main() for ad-hoc testing
//#define ZT_UPNP_TEST 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../node/Utils.hpp"
#include "UPNPClient.hpp"

#ifdef __WINDOWS__
#ifndef MINIUPNP_STATICLIB
#define MINIUPNP_STATICLIB
#endif
#endif

#include "../ext/bin/miniupnpc/include/miniupnpc/miniupnpc.h"
#include "../ext/bin/miniupnpc/include/miniupnpc/upnpcommands.h"

namespace ZeroTier {

class UPNPClientImpl
{
public:
	UPNPClientImpl(int localUdpPortToMap) :
		run(true),
		localPort(localUdpPortToMap)
	{
	}

	void threadMain()
		throw()
	{
		char lanaddr[4096];
		char externalip[4096]; // no range checking? so make these buffers larger than any UDP packet a uPnP server could send us as a precaution :P
		char inport[16];
		char outport[16];
		struct UPNPUrls urls;
		struct IGDdatas data;

#ifdef ZT_UPNP_TRACE
		fprintf(stderr,"UPNPClient: started for UDP port %d"ZT_EOL_S,localPort);
#endif

		unsigned int tryPortStart = 0;
		Utils::getSecureRandom(&tryPortStart,sizeof(tryPortStart));
		tryPortStart = (tryPortStart % (65535 - 1025)) + 1025;

		while (run) {
			{
				int upnpError = 0;
				UPNPDev *devlist = upnpDiscover(2000,(const char *)0,(const char *)0,0,0,&upnpError);
				if (devlist) {
#ifdef ZT_UPNP_TRACE
					{
						UPNPDev *dev = devlist;
						while (dev) {
							fprintf(stderr,"UPNPClient: found device at URL '%s': %s"ZT_EOL_S,dev->descURL,dev->st);
							dev = dev->pNext;
						}
					}
#endif

					memset(lanaddr,0,sizeof(lanaddr));
					memset(externalip,0,sizeof(externalip));
					memset(&urls,0,sizeof(urls));
					memset(&data,0,sizeof(data));
					Utils::snprintf(inport,sizeof(inport),"%d",localPort);

					if ((UPNP_GetValidIGD(devlist,&urls,&data,lanaddr,sizeof(lanaddr)))&&(lanaddr[0])) {
#ifdef ZT_UPNP_TRACE
						fprintf(stderr,"UPNPClient: my LAN IP address: %s"ZT_EOL_S,lanaddr);
#endif
						if ((UPNP_GetExternalIPAddress(urls.controlURL,data.first.servicetype,externalip) == UPNPCOMMAND_SUCCESS)&&(externalip[0])) {
#ifdef ZT_UPNP_TRACE
							fprintf(stderr,"UPNPClient: my external IP address: %s"ZT_EOL_S,externalip);
#endif

							for(int tries=0;tries<64;++tries) {
								int tryPort = (int)tryPortStart + tries;
								if (tryPort >= 65535)
									tryPort = (tryPort - 65535) + 1025;
								Utils::snprintf(outport,sizeof(outport),"%u",tryPort);

								int mapResult = 0;
								if ((mapResult = UPNP_AddPortMapping(urls.controlURL,data.first.servicetype,outport,inport,lanaddr,"ZeroTier","UDP",(const char *)0,"0")) == UPNPCOMMAND_SUCCESS) {
	#ifdef ZT_UPNP_TRACE
									fprintf(stderr,"UPNPClient: reserved external port: %s"ZT_EOL_S,outport);
	#endif
									{
										Mutex::Lock sl(surface_l);
										surface.clear();
										InetAddress tmp(externalip);
										tmp.setPort(tryPort);
										surface.push_back(tmp);
									}
									break;
								} else {
	#ifdef ZT_UPNP_TRACE
									fprintf(stderr,"UPNPClient: UPNP_AddAnyPortMapping(%s) failed: %d"ZT_EOL_S,outport,mapResult);
	#endif
									Thread::sleep(1000);
								}
							}
						} else {
#ifdef ZT_UPNP_TRACE
							fprintf(stderr,"UPNPClient: UPNP_GetExternalIPAddress failed"ZT_EOL_S);
#endif
						}
					} else {
#ifdef ZT_UPNP_TRACE
						fprintf(stderr,"UPNPClient: UPNP_GetValidIGD failed"ZT_EOL_S);
#endif
					}

					freeUPNPDevlist(devlist);
				} else {
#ifdef ZT_UPNP_TRACE
					fprintf(stderr,"UPNPClient: upnpDiscover error code: %d"ZT_EOL_S,upnpError);
#endif
				}
			}

#ifdef ZT_UPNP_TRACE
			fprintf(stderr,"UPNPClient: rescanning in %d ms"ZT_EOL_S,ZT_UPNP_CLIENT_REFRESH_DELAY);
#endif
			Thread::sleep(ZT_UPNP_CLIENT_REFRESH_DELAY);
		}
		delete this;
	}

	volatile bool run;
	int localPort;
	Mutex surface_l;
	std::vector<InetAddress> surface;
};

UPNPClient::UPNPClient(int localUdpPortToMap)
{
	_impl = new UPNPClientImpl(localUdpPortToMap);
	Thread::start(_impl);
}

UPNPClient::~UPNPClient()
{
	_impl->run = false;
}

std::vector<InetAddress> UPNPClient::get() const
{
	Mutex::Lock _l(_impl->surface_l);
	return _impl->surface;
}

} // namespace ZeroTier

#ifdef ZT_UPNP_TEST
int main(int argc,char **argv)
{
	ZeroTier::UPNPClient *client = new ZeroTier::UPNPClient(12345);
	ZeroTier::Thread::sleep(0xffffffff); // wait forever
	return 0;
}
#endif

#endif // ZT_USE_MINIUPNPC
