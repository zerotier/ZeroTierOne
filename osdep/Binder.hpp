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

#ifndef ZT_BINDER_HPP
#define ZT_BINDER_HPP

#include "../node/Constants.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#include <ShlObj.h>
#include <netioapi.h>
#include <iphlpapi.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ifaddrs.h>
#endif

#include <string>
#include <vector>
#include <algorithm>
#include <utility>

#include "../node/InetAddress.hpp"
#include "../node/Mutex.hpp"

#include "Phy.hpp"

namespace ZeroTier {

/**
 * Enumerates local devices and binds ports to all potential ZeroTier path endpoints
 *
 * This replaces binding to wildcard (0.0.0.0 and ::0) with explicit binding
 * as part of the path to default gateway support. Under the hood it uses
 * different queries on different OSes to enumerate devices, and also exposes
 * device enumeration and endpoint IP data for use elsewhere.
 *
 * On OSes that do not support local port enumeration or where this is not
 * meaningful, this degrades to binding to wildcard.
 */
template<typename PHY_HANDLER_TYPE>
class Binder
{
private:
	struct _Binding
	{
		PhySocket *udpSock;
		PhySocket *tcpListenSock;
		InetAddress address;
	};

public:
	/**
	 * @param phy Physical interface to use -- be sure not to delete phy before binder
	 * @param port Port to bind to on all interfaces (TCP and UDP)
	 */
	Binder(typename Phy<PHY_HANDLER_TYPE> &phy,unsigned int port) :
		_phy(phy),
		_port(port)
	{
	}

	/**
	 * Closes all bound ports -- but NOT accepted connections on those ports
	 */
	~Binder()
	{
	}

	/**
	 * Scan local devices and addresses and rebind TCP and UDP
	 *
	 * This should be called after wake from sleep, on detected network device
	 * changes, on startup, or periodically (e.g. every 30-60s).
	 *
	 * @param ignoreInterfacesByName Ignore these interfaces by name
	 * @param ignoreInterfacesByNamePrefix Ignore these interfaces by name-prefix (starts-with, e.g. zt ignores zt*)
	 * @param ignoreInterfacesByAddress Ignore these interfaces by address
	 */
	void refresh(const std::vector<std::string> &ignoreInterfacesByName,const std::vector<std::string> &ignoreInterfacesByNamePrefix,const std::vector<InetAddress> &ignoreInterfacesByAddress)
	{
		std::vector<InetAddress> localIfAddrs;
		std::vector<_Binding> newBindings;
		const char *na,*nb;
		std::vector<std::string>::const_iterator si;
		std::vector<InetAddress>::const_iterator ii;
		std::vector<_Binding>::const_iterator bi;
		PhySocket *udps,*tcps;
		InetAddress ip;
		Mutex::Lock _l(_lock);

#ifdef __WINDOWS__

#else // not __WINDOWS__

		struct ifaddrs *ifatbl = (struct ifaddrs *)0;
		struct ifaddrs *ifa;

		if ((getifaddrs(&ifatbl) == 0)&&(ifatbl)) {
			ifa = ifatbl;
			while (ifa) {
				if ((ifa->ifa_name)&&(ifa->ifa_addr)) {
					for(si=ignoreInterfacesByName.begin();si!=ignoreInterfacesByName.end();++si) {
						if (*si == ifa->ifa_name)
							goto binder_hpp_ignore_interface;
					}

					for(si=ignoreInterfacesByNamePrefix.begin();si!=ignoreInterfacesByNamePrefix.end();++si) {
						na = si->c_str();
						nb = ifa->ifa_name;
						while (*nb) {
							if (*(na++) != *(nb++))
								goto binder_hpp_interface_prefixes_dont_match;
						}
						goto binder_hpp_ignore_interface;
					}

binder_hpp_interface_prefixes_dont_match:
					ip = *(ifa->ifa_addr);
					switch(ip.ipScope()) {
						default: break;
						case InetAddress::IP_SCOPE_PSEUDOPRIVATE:
						case InetAddress::IP_SCOPE_GLOBAL:
						case InetAddress::IP_SCOPE_LINK_LOCAL:
						case InetAddress::IP_SCOPE_SHARED:
						case InetAddress::IP_SCOPE_PRIVATE:
							for(ii=ignoreInterfacesByAddress.begin();ii!=ignoreInterfacesByAddress.end();++ii) {
								if (ip.ipsEqual(*ii))
									goto binder_hpp_ignore_interface;
							}

							ip.setPort(_port);
							localIfAddrs.push(ip);

							break;
					}
				}

binder_hpp_ignore_interface:
				ifa = ifa->ifa_next;
			}
		}

		freeifaddrs(ifatbl);

#endif

		if (localIfAddrs.size() == 0) {
			localIfAddrs.push_back(InetAddress((uint32_t)0,_port));
			localIfAddrs.push_back(InetAddress((const void *)"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",16,_port));
		} else {
			std::sort(localIfAddrs.begin(),localIfAddrs.end());
		}

		for(bi=_bindings.begin();bi!=_bindings.end();++bi) {
			if (std::find(localIfAddrs.begin(),localIfAddrs.end(),bi->address) == localIfAddrs.end()) {
				_phy.close(bi->udpSock,false);
				_phy.close(bi->tcpListenSock,false);
			}
		}

		for(ii=localIfAddrs.begin();ii!=localIfAddrs.end();++ii) {
			for(bi=_bindings.begin();bi!=_bindings.end();++bi) {
				if (bi->address == *ii) {
					newBindings.push_back(*bi);
					break;
				}
			}

			if (bi == _bindings.end()) {
				udps = _phy.udpBind(reinterpret_cast<const struct sockaddr *>(&ii),(void *)0,131072);
				if (udps) {
					tcps = _phy.tcpListen(reinterpret_cast<const struct sockaddr *>(&ii),(void *)0);
					if (tcps) {
						newBindings.push_back(_Binding());
						newBindings.back().udpSock = udps;
						newBindings.back().tcpListenSock = tcps;
						newBindings.back().address = *ii;
					} else {
						_phy.close(udps,false);
					}
				}
			}
		}

		_bindings.swap(newBindings);
	}

private:
	std::vector<_Binding> _bindings;
	typename Phy<PHY_HANDLER_TYPE> &_phy;
	unsigned int _port;
	Mutex _lock;
};

} // namespace ZeroTier

#endif
