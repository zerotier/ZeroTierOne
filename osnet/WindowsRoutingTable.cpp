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
#include <WinSock2.h>
#include <Windows.h>
#include <netioapi.h>
#include <IPHlpApi.h>

#include <vector>

#include "../node/Constants.hpp"
#include "WindowsRoutingTable.hpp"

namespace ZeroTier {

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

WindowsRoutingTable::WindowsRoutingTable()
{
}

WindowsRoutingTable::~WindowsRoutingTable()
{
}

std::vector<RoutingTable::Entry> WindowsRoutingTable::get(bool includeLinkLocal,bool includeLoopback) const
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

RoutingTable::Entry WindowsRoutingTable::set(const InetAddress &destination,const InetAddress &gateway,const char *device,int metric)
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
		CreateIpForwardEntry2(&nr);
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

} // namespace ZeroTier
