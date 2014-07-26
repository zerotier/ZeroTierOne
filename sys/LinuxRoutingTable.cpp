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
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#include <algorithm>
#include <utility>

#include "../Constants.hpp"
#include "../Utils.hpp"
#include "LinuxRoutingTable.hpp"

#define ZT_LINUX_IP_COMMAND "/sbin/ip"

namespace ZeroTier {

LinuxRoutingTable::LinuxRoutingTable()
{
}

LinuxRoutingTable::~LinuxRoutingTable()
{
}

std::vector<RoutingTable::Entry> LinuxRoutingTable::get(bool includeLinkLocal,bool includeLoopback) const
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

RoutingTable::Entry LinuxRoutingTable::set(const InetAddress &destination,const InetAddress &gateway,const char *device,int metric)
{
	if ((!gateway)&&((!device)||(!device[0])))
		return RoutingTable::Entry();

	std::vector<RoutingTable::Entry> rtab(get(true,true));

	for(std::vector<RoutingTable::Entry>::iterator e(rtab.begin());e!=rtab.end();++e) {
		if (e->destination == destination) {
			if (((!device)||(!device[0]))||(!strcmp(device,e->device))) {
			}
		}
	}

	if (metric < 0)
		return RoutingTable::Entry();



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
//#if 0
using namespace ZeroTier;
int main(int argc,char **argv)
{
	LinuxRoutingTable rt;

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
//#endif
