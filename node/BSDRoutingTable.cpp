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
#include <unistd.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <net/if_dl.h>

#include <algorithm>
#include <utility>

#include "Constants.hpp"
#include "BSDRoutingTable.hpp"

// All I wanted was the bloody rounting table. I didn't expect the Spanish inquisition.

namespace ZeroTier {

BSDRoutingTable::BSDRoutingTable()
{
}

BSDRoutingTable::~BSDRoutingTable()
{
}

std::vector<RoutingTable::Entry> BSDRoutingTable::get() const
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
									e.gateway.set(sa);
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
							}

							saptr += salen;
						}

						e.metric = (int)rtm->rtm_rmx.rmx_hopcount;

						entries.push_back(e);
						printf("%s\n",e.toString().c_str());
					}

					next = saend;
				}
			}

			::free(buf);
		}
	}

	std::sort(entries.begin(),entries.end());
	return entries;
}

bool BSDRoutingTable::set(const RoutingTable::Entry &re)
{
	return true;
}

} // namespace ZeroTier

// Enable and build to test routing table interface
///*
int main(int argc,char **argv)
{
	ZeroTier::BSDRoutingTable rt;
	std::vector<ZeroTier::RoutingTable::Entry> ents(rt.get());
	return 0;
}
//*/
