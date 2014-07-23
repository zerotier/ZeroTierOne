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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <ifaddrs.h>

#include <algorithm>
#include <utility>

#include "../Constants.hpp"
#include "BSDRoutingTable.hpp"

// All I wanted was the bloody rounting table. I didn't expect the Spanish inquisition.

#define ZT_BSD_ROUTE_CMD "/sbin/route"

namespace ZeroTier {

BSDRoutingTable::BSDRoutingTable()
{
}

BSDRoutingTable::~BSDRoutingTable()
{
}

std::vector<RoutingTable::Entry> BSDRoutingTable::get(bool includeLinkLocal,bool includeLoopback) const
{
	std::vector<RoutingTable::Entry> entries;
	int mib[6];
	size_t needed;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	if (!sysctl(mib,6,NULL,&needed,NULL,0)) {
		if (needed <= 0)
			return entries;

		char *buf = (char *)::malloc(needed);
		if (buf) {
			if (!sysctl(mib,6,buf,&needed,NULL,0)) {
		    struct rt_msghdr *rtm;
				for(char *next=buf,*end=buf+needed;next<end;) {
					rtm = (struct rt_msghdr *)next;
					char *saptr = (char *)(rtm + 1);
					char *saend = next + rtm->rtm_msglen;

					if (((rtm->rtm_flags & RTF_LLINFO) == 0)&&((rtm->rtm_flags & RTF_HOST) == 0)&&((rtm->rtm_flags & RTF_UP) != 0)&&((rtm->rtm_flags & RTF_MULTICAST) == 0)) {
						RoutingTable::Entry e;
						e.deviceIndex = -9999; // unset

						int which = 0;
						while (saptr < saend) {
							struct sockaddr *sa = (struct sockaddr *)saptr;
							unsigned int salen = sa->sa_len;
							if (!salen)
								break;

							// Skip missing fields in rtm_addrs bit field
							while ((rtm->rtm_addrs & 1) == 0) {
								rtm->rtm_addrs >>= 1;
								++which;
								if (which > 6)
									break;
							}
							if (which > 6)
								break;

							rtm->rtm_addrs >>= 1;
							switch(which++) {
								case 0:
									//printf("RTA_DST\n");
									if (sa->sa_family == AF_INET6) {
										struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
										// Nobody expects the Spanish inquisition!
										if ((sin6->sin6_addr.s6_addr[0] == 0xfe)&&((sin6->sin6_addr.s6_addr[1] & 0xc0) == 0x80)) {
											// Our chief weapon is... in-band signaling!
											// Seriously who in the living fuck thought this was a good idea and
											// then had the sadistic idea to not document it anywhere? Of course it's
											// not like there is any documentation on BSD sysctls anyway.
											unsigned int interfaceIndex = ((((unsigned int)sin6->sin6_addr.s6_addr[2]) << 8) & 0xff) | (((unsigned int)sin6->sin6_addr.s6_addr[3]) & 0xff);
											sin6->sin6_addr.s6_addr[2] = 0;
											sin6->sin6_addr.s6_addr[3] = 0;
											if (!sin6->sin6_scope_id)
												sin6->sin6_scope_id = interfaceIndex;
										}
									}
									e.destination.set(sa);
									break;
								case 1:
									//printf("RTA_GATEWAY\n");
									switch(sa->sa_family) {
										case AF_LINK:
											e.deviceIndex = (int)((const struct sockaddr_dl *)sa)->sdl_index;
											break;
										case AF_INET:
										case AF_INET6:
											e.gateway.set(sa);
											break;
									}
									break;
								case 2: {
									if (e.destination.isV6()) {
										salen = sizeof(struct sockaddr_in6); // Confess!
										unsigned int bits = 0;
										for(int i=0;i<16;++i) {
											unsigned char c = (unsigned char)((const struct sockaddr_in6 *)sa)->sin6_addr.s6_addr[i];
											if (c == 0xff)
												bits += 8;
											else break;
											/* must they be multiples of 8? Most of the BSD source I can find says yes..?
											else {
												while ((c & 0x80) == 0x80) {
													++bits;
													c <<= 1;
												}
												break;
											}
											*/
										}
										e.destination.setPort(bits);
									} else {
										salen = sizeof(struct sockaddr_in); // Confess!
										e.destination.setPort((unsigned int)Utils::countBits((uint32_t)((const struct sockaddr_in *)sa)->sin_addr.s_addr));
									}
									//printf("RTA_NETMASK\n");
								}	break;
								/*
								case 3:
									//printf("RTA_GENMASK\n");
									break;
								case 4:
									//printf("RTA_IFP\n");
									break;
								case 5:
									//printf("RTA_IFA\n");
									break;
								case 6:
									//printf("RTA_AUTHOR\n");
									break;
								*/
							}

							saptr += salen;
						}

						e.metric = (int)rtm->rtm_rmx.rmx_hopcount;
						if (e.metric < 0)
							e.metric = 0;

						if (((includeLinkLocal)||(!e.destination.isLinkLocal()))&&((includeLoopback)||((!e.destination.isLoopback())&&(!e.gateway.isLoopback()))))
							entries.push_back(e);
					}

					next = saend;
				}
			}

			::free(buf);
		}
	}

	for(std::vector<ZeroTier::RoutingTable::Entry>::iterator e1(entries.begin());e1!=entries.end();++e1) {
		if ((!e1->device[0])&&(e1->deviceIndex >= 0))
			if_indextoname(e1->deviceIndex,e1->device);
	}
	for(std::vector<ZeroTier::RoutingTable::Entry>::iterator e1(entries.begin());e1!=entries.end();++e1) {
		if ((!e1->device[0])&&(e1->gateway)) {
			int bestMetric = 9999999;
			for(std::vector<ZeroTier::RoutingTable::Entry>::iterator e2(entries.begin());e2!=entries.end();++e2) {
				if ((e1->gateway.within(e2->destination))&&(e2->metric <= bestMetric)) {
					bestMetric = e2->metric;
					Utils::scopy(e1->device,sizeof(e1->device),e2->device);
				}
			}
		}
	}

	std::sort(entries.begin(),entries.end());

	return entries;
}

RoutingTable::Entry BSDRoutingTable::set(const InetAddress &destination,const InetAddress &gateway,const char *device,int metric)
{
	if ((!gateway)&&((!device)||(!device[0])))
		return RoutingTable::Entry();

	std::vector<RoutingTable::Entry> rtab(get(true,true));

	for(std::vector<RoutingTable::Entry>::iterator e(rtab.begin());e!=rtab.end();++e) {
		if (e->destination == destination) {
			if (((!device)||(!device[0]))||(!strcmp(device,e->device))) {
				long p = (long)fork();
				if (p > 0) {
					int exitcode = -1;
					::waitpid(p,&exitcode,0);
				} else if (p == 0) {
					::close(STDOUT_FILENO);
					::close(STDERR_FILENO);
					::execl(ZT_BSD_ROUTE_CMD,ZT_BSD_ROUTE_CMD,"delete",(destination.isV6() ? "-inet6" : "-inet"),destination.toString().c_str(),(const char *)0);
					::_exit(-1);
				}
			}
		}
	}

	if (metric < 0)
		return RoutingTable::Entry();

	{
		char hcstr[64];
		Utils::snprintf(hcstr,sizeof(hcstr),"%d",metric);
		long p = (long)fork();
		if (p > 0) {
			int exitcode = -1;
			::waitpid(p,&exitcode,0);
		} else if (p == 0) {
			::close(STDOUT_FILENO);
			::close(STDERR_FILENO);
			if (gateway) {
				::execl(ZT_BSD_ROUTE_CMD,ZT_BSD_ROUTE_CMD,"add",(destination.isV6() ? "-inet6" : "-inet"),destination.toString().c_str(),gateway.toIpString().c_str(),"-hopcount",hcstr,(const char *)0);
			} else if ((device)&&(device[0])) {
				::execl(ZT_BSD_ROUTE_CMD,ZT_BSD_ROUTE_CMD,"add",(destination.isV6() ? "-inet6" : "-inet"),destination.toString().c_str(),"-interface",device,"-hopcount",hcstr,(const char *)0);
			}
			::_exit(-1);
		}
	}

	rtab = get(true,true);
	std::vector<RoutingTable::Entry>::iterator bestEntry(rtab.end());
	for(std::vector<RoutingTable::Entry>::iterator e(rtab.begin());e!=rtab.end();++e) {
		if ((e->destination == destination)&&(e->gateway.ipsEqual(gateway))) {
			if ((device)&&(device[0])) {
				if (!strcmp(device,e->device)) {
					if (metric == e->metric)
						bestEntry = e;
				}
			}
			if (bestEntry == rtab.end())
				bestEntry = e;
		}
	}
	if (bestEntry != rtab.end())
		return *bestEntry;

	return RoutingTable::Entry();
}

} // namespace ZeroTier

// Enable and build to test routing table interface
#if 0
using namespace ZeroTier;
int main(int argc,char **argv)
{
	BSDRoutingTable rt;

	printf("<destination> <gateway> <interface> <metric>\n");
	std::vector<RoutingTable::Entry> ents(rt.get());
	for(std::vector<RoutingTable::Entry>::iterator e(ents.begin());e!=ents.end();++e)
		printf("%s\n",e->toString().c_str());
	printf("\n");

	printf("adding 1.1.1.0 and 2.2.2.0...\n");
	rt.set(InetAddress("1.1.1.0",24),InetAddress("1.2.3.4",0),(const char *)0,1);
	rt.set(InetAddress("2.2.2.0",24),InetAddress(),"en0",1);
	printf("\n");

	printf("<destination> <gateway> <interface> <metric>\n");
	ents = rt.get();
	for(std::vector<RoutingTable::Entry>::iterator e(ents.begin());e!=ents.end();++e)
		printf("%s\n",e->toString().c_str());
	printf("\n");

	printf("deleting 1.1.1.0 and 2.2.2.0...\n");
	rt.set(InetAddress("1.1.1.0",24),InetAddress("1.2.3.4",0),(const char *)0,-1);
	rt.set(InetAddress("2.2.2.0",24),InetAddress(),"en0",-1);
	printf("\n");

	printf("<destination> <gateway> <interface> <metric>\n");
	ents = rt.get();
	for(std::vector<RoutingTable::Entry>::iterator e(ents.begin());e!=ents.end();++e)
		printf("%s\n",e->toString().c_str());
	printf("\n");

	return 0;
}
#endif
