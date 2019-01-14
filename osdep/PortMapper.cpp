/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifdef ZT_USE_MINIUPNPC

// Uncomment to dump debug messages
//#define ZT_PORTMAPPER_TRACE 1

#ifdef __ANDROID__
#include <android/log.h>
#define PM_TRACE(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, "PortMapper", __VA_ARGS__))
#else
#define PM_TRACE(...) fprintf(stderr, __VA_ARGS__)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "../node/Utils.hpp"
#include "OSUtils.hpp"
#include "PortMapper.hpp"

// These must be defined to get rid of dynamic export stuff in libminiupnpc and libnatpmp
#ifdef __WINDOWS__
#ifndef MINIUPNP_STATICLIB
#define MINIUPNP_STATICLIB
#endif
#ifndef STATICLIB
#define STATICLIB
#endif
#endif

#ifdef ZT_USE_SYSTEM_MINIUPNPC
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#else
#ifdef __ANDROID__
#include "miniupnpc.h"
#include "upnpcommands.h"
#else
#include "../ext/miniupnpc/miniupnpc.h"
#include "../ext/miniupnpc/upnpcommands.h"
#endif
#endif

#ifdef ZT_USE_SYSTEM_NATPMP
#include <natpmp.h>
#else
#ifdef __ANDROID__
#include "natpmp.h"
#else
#include "../ext/libnatpmp/natpmp.h"
#endif
#endif

namespace ZeroTier {

class PortMapperImpl
{
public:
	PortMapperImpl(int localUdpPortToMap,const char *un) :
		run(true),
		localPort(localUdpPortToMap),
		uniqueName(un)
	{
	}

	~PortMapperImpl() {}

	void threadMain()
		throw()
	{
		int mode = 0; // 0 == NAT-PMP, 1 == UPnP

#ifdef ZT_PORTMAPPER_TRACE
		fprintf(stderr,"PortMapper: started for UDP port %d" ZT_EOL_S,localPort);
#endif

		while (run) {

			// ---------------------------------------------------------------------
			// NAT-PMP mode (preferred)
			// ---------------------------------------------------------------------
			if (mode == 0) {
			  natpmp_t natpmp;
			  natpmpresp_t response;
				int r = 0;

				bool natPmpSuccess = false;
				for(int tries=0;tries<60;++tries) {
					int tryPort = (int)localPort + tries;
					if (tryPort >= 65535)
						tryPort = (tryPort - 65535) + 1025;

					memset(&natpmp,0,sizeof(natpmp));
					memset(&response,0,sizeof(response));

					if (initnatpmp(&natpmp,0,0) != 0) {
						mode = 1;
						closenatpmp(&natpmp);
#ifdef ZT_PORTMAPPER_TRACE
                        PM_TRACE("PortMapper: NAT-PMP: init failed, switching to UPnP mode" ZT_EOL_S);
#endif
						break;
					}

					InetAddress publicAddress;
					sendpublicaddressrequest(&natpmp);
					int64_t myTimeout = OSUtils::now() + 5000;
					do {
						fd_set fds;
						struct timeval timeout;
						FD_ZERO(&fds);
						FD_SET(natpmp.s, &fds);
						getnatpmprequesttimeout(&natpmp, &timeout);
						select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
						r = readnatpmpresponseorretry(&natpmp, &response);
						if (OSUtils::now() >= myTimeout)
							break;
					} while (r == NATPMP_TRYAGAIN);
					if (r == 0) {
						publicAddress = InetAddress((uint32_t)response.pnu.publicaddress.addr.s_addr,0);
					} else {
#ifdef ZT_PORTMAPPER_TRACE
                        PM_TRACE("PortMapper: NAT-PMP: request for external address failed, aborting..." ZT_EOL_S);
#endif
						closenatpmp(&natpmp);
						break;
					}

				  sendnewportmappingrequest(&natpmp,NATPMP_PROTOCOL_UDP,localPort,tryPort,(ZT_PORTMAPPER_REFRESH_DELAY * 2) / 1000);
					myTimeout = OSUtils::now() + 10000;
					do {
				    fd_set fds;
				    struct timeval timeout;
				    FD_ZERO(&fds);
				    FD_SET(natpmp.s, &fds);
				    getnatpmprequesttimeout(&natpmp, &timeout);
				    select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
				    r = readnatpmpresponseorretry(&natpmp, &response);
						if (OSUtils::now() >= myTimeout)
							break;
				  } while (r == NATPMP_TRYAGAIN);
					if (r == 0) {
						publicAddress.setPort(response.pnu.newportmapping.mappedpublicport);
#ifdef ZT_PORTMAPPER_TRACE
                        char paddr[128];
                        PM_TRACE("PortMapper: NAT-PMP: mapped %u to %s" ZT_EOL_S,(unsigned int)localPort,publicAddress.toString(paddr));
#endif
						Mutex::Lock sl(surface_l);
						surface.clear();
						surface.push_back(publicAddress);
						natPmpSuccess = true;
						closenatpmp(&natpmp);
						break;
					} else {
						closenatpmp(&natpmp);
						// continue
					}
				}

				if (!natPmpSuccess) {
					mode = 1;
#ifdef ZT_PORTMAPPER_TRACE
                    PM_TRACE("PortMapper: NAT-PMP: request failed, switching to UPnP mode" ZT_EOL_S);
#endif
				}
			}
			// ---------------------------------------------------------------------

			// ---------------------------------------------------------------------
			// UPnP mode
			// ---------------------------------------------------------------------
			if (mode == 1) {
				char lanaddr[4096];
				char externalip[4096]; // no range checking? so make these buffers larger than any UDP packet a uPnP server could send us as a precaution :P
				char inport[16];
				char outport[16];
				struct UPNPUrls urls;
				struct IGDdatas data;

				int upnpError = 0;
				UPNPDev *devlist = upnpDiscoverAll(5000,(const char *)0,(const char *)0,0,0,2,&upnpError);
				if (devlist) {

#ifdef ZT_PORTMAPPER_TRACE
					{
						UPNPDev *dev = devlist;
						while (dev) {
                            PM_TRACE("PortMapper: found UPnP device at URL '%s': %s" ZT_EOL_S,dev->descURL,dev->st);
							dev = dev->pNext;
						}
					}
#endif

					memset(lanaddr,0,sizeof(lanaddr));
					memset(externalip,0,sizeof(externalip));
					memset(&urls,0,sizeof(urls));
					memset(&data,0,sizeof(data));
					OSUtils::ztsnprintf(inport,sizeof(inport),"%d",localPort);

					if ((UPNP_GetValidIGD(devlist,&urls,&data,lanaddr,sizeof(lanaddr)))&&(lanaddr[0])) {
#ifdef ZT_PORTMAPPER_TRACE
                        PM_TRACE("PortMapper: UPnP: my LAN IP address: %s" ZT_EOL_S,lanaddr);
#endif
						if ((UPNP_GetExternalIPAddress(urls.controlURL,data.first.servicetype,externalip) == UPNPCOMMAND_SUCCESS)&&(externalip[0])) {
#ifdef ZT_PORTMAPPER_TRACE
                            PM_TRACE("PortMapper: UPnP: my external IP address: %s" ZT_EOL_S,externalip);
#endif

							for(int tries=0;tries<60;++tries) {
								int tryPort = (int)localPort + tries;
								if (tryPort >= 65535)
									tryPort = (tryPort - 65535) + 1025;
								OSUtils::ztsnprintf(outport,sizeof(outport),"%u",tryPort);

								// First check and see if this port is already mapped to the
								// same unique name. If so, keep this mapping and don't try
								// to map again since this can break buggy routers. But don't
								// fail if this command fails since not all routers support it.
								{
									char haveIntClient[128]; // 128 == big enough for all these as per miniupnpc "documentation"
									char haveIntPort[128];
									char haveDesc[128];
									char haveEnabled[128];
									char haveLeaseDuration[128];
									memset(haveIntClient,0,sizeof(haveIntClient));
									memset(haveIntPort,0,sizeof(haveIntPort));
									memset(haveDesc,0,sizeof(haveDesc));
									memset(haveEnabled,0,sizeof(haveEnabled));
									memset(haveLeaseDuration,0,sizeof(haveLeaseDuration));
									if ((UPNP_GetSpecificPortMappingEntry(urls.controlURL,data.first.servicetype,outport,"UDP",(const char *)0,haveIntClient,haveIntPort,haveDesc,haveEnabled,haveLeaseDuration) == UPNPCOMMAND_SUCCESS)&&(uniqueName == haveDesc)) {
#ifdef ZT_PORTMAPPER_TRACE
                                        PM_TRACE("PortMapper: UPnP: reusing previously reserved external port: %s" ZT_EOL_S,outport);
#endif
										Mutex::Lock sl(surface_l);
										surface.clear();
										InetAddress tmp(externalip);
										tmp.setPort(tryPort);
										surface.push_back(tmp);
										break;
									}
								}

								// Try to map this port
								int mapResult = 0;
								if ((mapResult = UPNP_AddPortMapping(urls.controlURL,data.first.servicetype,outport,inport,lanaddr,uniqueName.c_str(),"UDP",(const char *)0,"0")) == UPNPCOMMAND_SUCCESS) {
#ifdef ZT_PORTMAPPER_TRACE
                                    PM_TRACE("PortMapper: UPnP: reserved external port: %s" ZT_EOL_S,outport);
#endif
									Mutex::Lock sl(surface_l);
									surface.clear();
									InetAddress tmp(externalip);
									tmp.setPort(tryPort);
									surface.push_back(tmp);
									break;
								} else {
#ifdef ZT_PORTMAPPER_TRACE
                                    PM_TRACE("PortMapper: UPnP: UPNP_AddPortMapping(%s) failed: %d" ZT_EOL_S,outport,mapResult);
#endif
									Thread::sleep(1000);
								}
							}

						} else {
							mode = 0;
#ifdef ZT_PORTMAPPER_TRACE
                            PM_TRACE("PortMapper: UPnP: UPNP_GetExternalIPAddress failed, returning to NAT-PMP mode" ZT_EOL_S);
#endif
						}
					} else {
						mode = 0;
#ifdef ZT_PORTMAPPER_TRACE
                        PM_TRACE("PortMapper: UPnP: UPNP_GetValidIGD failed, returning to NAT-PMP mode" ZT_EOL_S);
#endif
					}

					freeUPNPDevlist(devlist);

				} else {
					mode = 0;
#ifdef ZT_PORTMAPPER_TRACE
                    PM_TRACE("PortMapper: upnpDiscover failed, returning to NAT-PMP mode: %d" ZT_EOL_S,upnpError);
#endif
				}
			}
			// ---------------------------------------------------------------------

#ifdef ZT_PORTMAPPER_TRACE
            PM_TRACE("UPNPClient: rescanning in %d ms" ZT_EOL_S,ZT_PORTMAPPER_REFRESH_DELAY);
#endif
			Thread::sleep(ZT_PORTMAPPER_REFRESH_DELAY);
		}

		delete this;
	}

	volatile bool run;
	int localPort;
	std::string uniqueName;

	Mutex surface_l;
	std::vector<InetAddress> surface;
};

PortMapper::PortMapper(int localUdpPortToMap,const char *uniqueName)
{
	_impl = new PortMapperImpl(localUdpPortToMap,uniqueName);
	Thread::start(_impl);
}

PortMapper::~PortMapper()
{
	_impl->run = false;
}

std::vector<InetAddress> PortMapper::get() const
{
	Mutex::Lock _l(_impl->surface_l);
	return _impl->surface;
}

} // namespace ZeroTier

#endif // ZT_USE_MINIUPNPC
