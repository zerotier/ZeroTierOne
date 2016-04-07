/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#include "../node/Constants.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#include <netioapi.h>
#include <IPHlpApi.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __UNIX_LIKE__
#include <unistd.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <ifaddrs.h>
#endif

#include <vector>
#include <algorithm>
#include <utility>

#include "RoutingTable.hpp"

#define ZT_BSD_ROUTE_CMD "/sbin/route"
#define ZT_LINUX_IP_COMMAND "/sbin/ip"

namespace ZeroTier {

// ---------------------------------------------------------------------------

#ifdef __LINUX__

std::vector<RoutingTable::Entry> RoutingTable::get(bool includeLinkLocal,bool includeLoopback)
{
	char buf[131072];
	char *stmp,*stmp2;
	std::vector<RoutingTable::Entry> entries;

	{
		int fd = ::open("/proc/net/route",O_RDONLY);
		if (fd <= 0)
			buf[0] = (char)0;
		else {
			int n = (int)::read(fd,buf,sizeof(buf) - 1);
			::close(fd);
			if (n < 0) n = 0;
			buf[n] = (char)0;
		}
	}

	int lineno = 0;
	for(char *line=Utils::stok(buf,"\r\n",&stmp);(line);line=Utils::stok((char *)0,"\r\n",&stmp)) {
		if (lineno == 0) {
			++lineno;
			continue; // skip header
		}

		char *iface = (char *)0;
		uint32_t destination = 0;
		uint32_t gateway = 0;
		int metric = 0;
		uint32_t mask = 0;

		int fno = 0;
		for(char *f=Utils::stok(line,"\t \r\n",&stmp2);(f);f=Utils::stok((char *)0,"\t \r\n",&stmp2)) {
			switch(fno) {
				case 0: iface = f; break;
				case 1: destination = (uint32_t)Utils::hexStrToULong(f); break;
				case 2: gateway = (uint32_t)Utils::hexStrToULong(f); break;
				case 6: metric = (int)Utils::strToInt(f); break;
				case 7: mask = (uint32_t)Utils::hexStrToULong(f); break;
			}
			++fno;
		}

		if ((iface)&&(destination)) {
			RoutingTable::Entry e;
			if (destination)
				e.destination.set(&destination,4,Utils::countBits(mask));
			e.gateway.set(&gateway,4,0);
			e.deviceIndex = 0; // not used on Linux
			e.metric = metric;
			Utils::scopy(e.device,sizeof(e.device),iface);
			if ((e.destination)&&((includeLinkLocal)||(!e.destination.isLinkLocal()))&&((includeLoopback)||((!e.destination.isLoopback())&&(!e.gateway.isLoopback())&&(strcmp(iface,"lo")))))
				entries.push_back(e);
		}

		++lineno;
	}

	{
		int fd = ::open("/proc/net/ipv6_route",O_RDONLY);
		if (fd <= 0)
			buf[0] = (char)0;
		else {
			int n = (int)::read(fd,buf,sizeof(buf) - 1);
			::close(fd);
			if (n < 0) n = 0;
			buf[n] = (char)0;
		}
	}

	for(char *line=Utils::stok(buf,"\r\n",&stmp);(line);line=Utils::stok((char *)0,"\r\n",&stmp)) {
		char *destination = (char *)0;
		unsigned int destPrefixLen = 0;
		char *gateway = (char *)0; // next hop in ipv6 terminology
		int metric = 0;
		char *device = (char *)0;

		int fno = 0;
		for(char *f=Utils::stok(line,"\t \r\n",&stmp2);(f);f=Utils::stok((char *)0,"\t \r\n",&stmp2)) {
			switch(fno) {
				case 0: destination = f; break;
				case 1: destPrefixLen = (unsigned int)Utils::hexStrToULong(f); break;
				case 4: gateway = f; break;
				case 5: metric = (int)Utils::hexStrToLong(f); break;
				case 9: device = f; break;
			}
			++fno;
		}

		if ((device)&&(destination)) {
			unsigned char tmp[16];
			RoutingTable::Entry e;
			Utils::unhex(destination,tmp,16);
			if ((!Utils::isZero(tmp,16))&&(tmp[0] != 0xff))
				e.destination.set(tmp,16,destPrefixLen);
			Utils::unhex(gateway,tmp,16);
			e.gateway.set(tmp,16,0);
			e.deviceIndex = 0; // not used on Linux
			e.metric = metric;
			Utils::scopy(e.device,sizeof(e.device),device);
			if ((e.destination)&&((includeLinkLocal)||(!e.destination.isLinkLocal()))&&((includeLoopback)||((!e.destination.isLoopback())&&(!e.gateway.isLoopback())&&(strcmp(device,"lo")))))
				entries.push_back(e);
		}
	}

	std::sort(entries.begin(),entries.end());
	return entries;
}

RoutingTable::Entry RoutingTable::set(const InetAddress &destination,const InetAddress &gateway,const char *device,int metric,bool ifscope)
{
	char metstr[128];

	if ((!gateway)&&((!device)||(!device[0])))
		return RoutingTable::Entry();

	Utils::snprintf(metstr,sizeof(metstr),"%d",metric);

	if (metric < 0) {
		long pid = (long)vfork();
		if (pid == 0) {
			if (gateway) {
				if ((device)&&(device[0])) {
					::execl(ZT_LINUX_IP_COMMAND,ZT_LINUX_IP_COMMAND,"route","del",destination.toString().c_str(),"via",gateway.toIpString().c_str(),"dev",device,(const char *)0);
				} else {
					::execl(ZT_LINUX_IP_COMMAND,ZT_LINUX_IP_COMMAND,"route","del",destination.toString().c_str(),"via",gateway.toIpString().c_str(),(const char *)0);
				}
			} else {
				::execl(ZT_LINUX_IP_COMMAND,ZT_LINUX_IP_COMMAND,"route","del",destination.toString().c_str(),"dev",device,(const char *)0);
			}
			::_exit(-1);
		} else if (pid > 0) {
			int exitcode = -1;
			::waitpid(pid,&exitcode,0);
		}
	} else {
		long pid = (long)vfork();
		if (pid == 0) {
			if (gateway) {
				if ((device)&&(device[0])) {
					::execl(ZT_LINUX_IP_COMMAND,ZT_LINUX_IP_COMMAND,"route","replace",destination.toString().c_str(),"metric",metstr,"via",gateway.toIpString().c_str(),"dev",device,(const char *)0);
				} else {
					::execl(ZT_LINUX_IP_COMMAND,ZT_LINUX_IP_COMMAND,"route","replace",destination.toString().c_str(),"metric",metstr,"via",gateway.toIpString().c_str(),(const char *)0);
				}
			} else {
				::execl(ZT_LINUX_IP_COMMAND,ZT_LINUX_IP_COMMAND,"route","replace",destination.toString().c_str(),"metric",metstr,"dev",device,(const char *)0);
			}
			::_exit(-1);
		} else if (pid > 0) {
			int exitcode = -1;
			::waitpid(pid,&exitcode,0);
		}
	}

	std::vector<RoutingTable::Entry> rtab(get(true,true));
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

#endif // __LINUX__

// ---------------------------------------------------------------------------

#ifdef __BSD__

std::vector<RoutingTable::Entry> RoutingTable::get(bool includeLinkLocal,bool includeLoopback)
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
									e.destination = *sa;
									break;
								case 1:
									//printf("RTA_GATEWAY\n");
									switch(sa->sa_family) {
										case AF_LINK:
											e.deviceIndex = (int)((const struct sockaddr_dl *)sa)->sdl_index;
											break;
										case AF_INET:
										case AF_INET6:
											e.gateway = *sa;
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

						InetAddress::IpScope dscope = e.destination.ipScope();
						if ( ((includeLinkLocal)||(dscope != InetAddress::IP_SCOPE_LINK_LOCAL)) && ((includeLoopback)||((dscope != InetAddress::IP_SCOPE_LOOPBACK) && (e.gateway.ipScope() != InetAddress::IP_SCOPE_LOOPBACK) )))
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
				if ((e2->destination.containsAddress(e1->gateway))&&(e2->metric <= bestMetric)) {
					bestMetric = e2->metric;
					Utils::scopy(e1->device,sizeof(e1->device),e2->device);
				}
			}
		}
	}

	std::sort(entries.begin(),entries.end());

	return entries;
}

RoutingTable::Entry RoutingTable::set(const InetAddress &destination,const InetAddress &gateway,const char *device,int metric,bool ifscope)
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

#endif // __BSD__

// ---------------------------------------------------------------------------

#ifdef __WINDOWS__

static void _copyInetAddressToSockaddrInet(const InetAddress &a,SOCKADDR_INET &sinet)
{
	memset(&sinet,0,sizeof(sinet));
	if (a.isV4()) {
		sinet.Ipv4.sin_addr.S_un.S_addr = *((const uint32_t *)a.rawIpData());
		sinet.Ipv4.sin_family = AF_INET;
		sinet.Ipv4.sin_port = htons(a.port());
	} else if (a.isV6()) {
		memcpy(sinet.Ipv6.sin6_addr.u.Byte,a.rawIpData(),16);
		sinet.Ipv6.sin6_family = AF_INET6;
		sinet.Ipv6.sin6_port = htons(a.port());
	}
}

std::vector<RoutingTable::Entry> RoutingTable::get(bool includeLinkLocal,bool includeLoopback) const
{
	std::vector<RoutingTable::Entry> entries;
	PMIB_IPFORWARD_TABLE2 rtbl = NULL;

	if (GetIpForwardTable2(AF_UNSPEC,&rtbl) != NO_ERROR)
		return entries;
	if (!rtbl)
		return entries;

	for(ULONG r=0;r<rtbl->NumEntries;++r) {
		RoutingTable::Entry e;
		switch(rtbl->Table[r].DestinationPrefix.Prefix.si_family) {
			case AF_INET:
				e.destination.set(&(rtbl->Table[r].DestinationPrefix.Prefix.Ipv4.sin_addr.S_un.S_addr),4,rtbl->Table[r].DestinationPrefix.PrefixLength);
				break;
			case AF_INET6:
				e.destination.set(rtbl->Table[r].DestinationPrefix.Prefix.Ipv6.sin6_addr.u.Byte,16,rtbl->Table[r].DestinationPrefix.PrefixLength);
				break;
		}
		switch(rtbl->Table[r].NextHop.si_family) {
			case AF_INET:
				e.gateway.set(&(rtbl->Table[r].NextHop.Ipv4.sin_addr.S_un.S_addr),4,0);
				break;
			case AF_INET6:
				e.gateway.set(rtbl->Table[r].NextHop.Ipv6.sin6_addr.u.Byte,16,0);
				break;
		}
		e.deviceIndex = (int)rtbl->Table[r].InterfaceIndex;
		e.metric = (int)rtbl->Table[r].Metric;
		ConvertInterfaceLuidToNameA(&(rtbl->Table[r].InterfaceLuid),e.device,sizeof(e.device));
		if ((e.destination)&&((includeLinkLocal)||(!e.destination.isLinkLocal()))&&((includeLoopback)||((!e.destination.isLoopback())&&(!e.gateway.isLoopback()))))
			entries.push_back(e);
	}

	FreeMibTable(rtbl);
	std::sort(entries.begin(),entries.end());
	return entries;
}

RoutingTable::Entry RoutingTable::set(const InetAddress &destination,const InetAddress &gateway,const char *device,int metric,bool ifscope)
{
	NET_LUID luid;
	luid.Value = 0;
	if (ConvertInterfaceNameToLuidA(device,&luid) != NO_ERROR)
		return RoutingTable::Entry();

	bool needCreate = true;
	PMIB_IPFORWARD_TABLE2 rtbl = NULL;
	if (GetIpForwardTable2(AF_UNSPEC,&rtbl) != NO_ERROR)
		return RoutingTable::Entry();
	if (!rtbl)
		return RoutingTable::Entry();
	for(ULONG r=0;r<rtbl->NumEntries;++r) {
		if (rtbl->Table[r].InterfaceLuid.Value == luid.Value) {
			InetAddress rdest;
			switch(rtbl->Table[r].DestinationPrefix.Prefix.si_family) {
				case AF_INET:
					rdest.set(&(rtbl->Table[r].DestinationPrefix.Prefix.Ipv4.sin_addr.S_un.S_addr),4,rtbl->Table[r].DestinationPrefix.PrefixLength);
					break;
				case AF_INET6:
					rdest.set(rtbl->Table[r].DestinationPrefix.Prefix.Ipv6.sin6_addr.u.Byte,16,rtbl->Table[r].DestinationPrefix.PrefixLength);
					break;
			}
			if (rdest == destination) {
				if (metric >= 0) {
					_copyInetAddressToSockaddrInet(gateway,rtbl->Table[r].NextHop);
					rtbl->Table[r].Metric = metric;
					SetIpForwardEntry2(&(rtbl->Table[r]));
					needCreate = false;
				} else {
					DeleteIpForwardEntry2(&(rtbl->Table[r]));
					FreeMibTable(rtbl);
					return RoutingTable::Entry();
				}
			}
		}
	}
	FreeMibTable(rtbl);

	if ((metric >= 0)&&(needCreate)) {
		MIB_IPFORWARD_ROW2 nr;
		InitializeIpForwardEntry(&nr);
		nr.InterfaceLuid.Value = luid.Value;
		_copyInetAddressToSockaddrInet(destination,nr.DestinationPrefix.Prefix);
		nr.DestinationPrefix.PrefixLength = destination.netmaskBits();
		_copyInetAddressToSockaddrInet(gateway,nr.NextHop);
		nr.Metric = metric;
		nr.Protocol = MIB_IPPROTO_NETMGMT;
		DWORD result = CreateIpForwardEntry2(&nr);
		if (result != NO_ERROR)
			return RoutingTable::Entry();
	}

	std::vector<RoutingTable::Entry> rtab(get(true,true));
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

#endif // __WINDOWS__

// ---------------------------------------------------------------------------

} // namespace ZeroTier
