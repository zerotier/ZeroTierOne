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

#include "../node/NonCopyable.hpp"
#include "../node/InetAddress.hpp"
#include "../node/Mutex.hpp"

#include "Phy.hpp"

/**
 * Period between binder rescans/refreshes
 *
 * OneService also does this on detected restarts.
 */
#define ZT_BINDER_REFRESH_PERIOD 30000

namespace ZeroTier {

/**
 * Enumerates local devices and binds to all potential ZeroTier path endpoints
 *
 * This replaces binding to wildcard (0.0.0.0 and ::0) with explicit binding
 * as part of the path to default gateway support. Under the hood it uses
 * different queries on different OSes to enumerate devices, and also exposes
 * device enumeration and endpoint IP data for use elsewhere.
 *
 * On OSes that do not support local port enumeration or where this is not
 * meaningful, this degrades to binding to wildcard.
 */
class Binder : NonCopyable
{
private:
	struct _Binding
	{
		_Binding() :
			udpSock((PhySocket *)0),
			tcpListenSock((PhySocket *)0),
			address() {}

		PhySocket *udpSock;
		PhySocket *tcpListenSock;
		InetAddress address;
	};

public:
	Binder() {}

	/**
	 * Close all bound ports
	 *
	 * This should be called on shutdown. It closes listen sockets and UDP ports
	 * but not TCP connections from any TCP listen sockets.
	 *
	 * @param phy Physical interface
	 */
	template<typename PHY_HANDLER_TYPE>
	void closeAll(Phy<PHY_HANDLER_TYPE> &phy)
	{
		Mutex::Lock _l(_lock);
		for(typename std::vector<_Binding>::const_iterator i(_bindings.begin());i!=_bindings.end();++i) {
			phy.close(i->udpSock,false);
			phy.close(i->tcpListenSock,false);
		}
	}

	/**
	 * Scan local devices and addresses and rebind TCP and UDP
	 *
	 * This should be called after wake from sleep, on detected network device
	 * changes, on startup, or periodically (e.g. every 30-60s).
	 *
	 * @param phy Physical interface
	 * @param port Port to bind to on all interfaces (TCP and UDP)
	 * @param ignoreInterfacesByName Ignore these interfaces by name
	 * @param ignoreInterfacesByNamePrefix Ignore these interfaces by name-prefix (starts-with, e.g. zt ignores zt*)
	 * @param ignoreInterfacesByAddress Ignore these interfaces by address
	 * @tparam PHY_HANDLER_TYPE Type for Phy<> template
	 * @tparam INTERFACE_CHECKER Type for class containing shouldBindInterface() method
	 */
	template<typename PHY_HANDLER_TYPE,typename INTERFACE_CHECKER>
	void refresh(Phy<PHY_HANDLER_TYPE> &phy,unsigned int port,INTERFACE_CHECKER &ifChecker)
	{
		std::vector<InetAddress> localIfAddrs;
		std::vector<_Binding> newBindings;
		std::vector<std::string>::const_iterator si;
		std::vector<InetAddress>::const_iterator ii;
		typename std::vector<_Binding>::const_iterator bi;
		PhySocket *udps;
		//PhySocket *tcps;
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
					ip = *(ifa->ifa_addr);
					if (ifChecker.shouldBindInterface(ifa->ifa_name,ip)) {
						switch(ip.ipScope()) {
							default: break;
							case InetAddress::IP_SCOPE_PSEUDOPRIVATE:
							case InetAddress::IP_SCOPE_GLOBAL:
							//case InetAddress::IP_SCOPE_LINK_LOCAL:
							case InetAddress::IP_SCOPE_SHARED:
							case InetAddress::IP_SCOPE_PRIVATE:
								ip.setPort(port);
								localIfAddrs.push_back(ip);
								break;
						}
					}
				}
				ifa = ifa->ifa_next;
			}
		}

		freeifaddrs(ifatbl);

#endif

		// Default to binding to wildcard if we can't enumerate addresses
		if (localIfAddrs.size() == 0) {
			localIfAddrs.push_back(InetAddress((uint32_t)0,port));
			localIfAddrs.push_back(InetAddress((const void *)"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",16,port));
		}

		// Close any old bindings to anything that doesn't exist anymore
		for(bi=_bindings.begin();bi!=_bindings.end();++bi) {
			if (std::find(localIfAddrs.begin(),localIfAddrs.end(),bi->address) == localIfAddrs.end()) {
				phy.close(bi->udpSock,false);
				phy.close(bi->tcpListenSock,false);
			}
		}

		for(ii=localIfAddrs.begin();ii!=localIfAddrs.end();++ii) {
			// Copy over bindings that still are valid
			for(bi=_bindings.begin();bi!=_bindings.end();++bi) {
				if (bi->address == *ii) {
					newBindings.push_back(*bi);
					break;
				}
			}

			// Add new bindings
			if (bi == _bindings.end()) {
				udps = phy.udpBind(reinterpret_cast<const struct sockaddr *>(&(*ii)),(void *)0,ZT_UDP_DESIRED_BUF_SIZE);
				if (udps) {
					//tcps = phy.tcpListen(reinterpret_cast<const struct sockaddr *>(&ii),(void *)0);
					//if (tcps) {
						newBindings.push_back(_Binding());
						newBindings.back().udpSock = udps;
						//newBindings.back().tcpListenSock = tcps;
						newBindings.back().address = *ii;
					//} else {
					//	phy.close(udps,false);
					//}
				}
			}
		}

		/*
		for(bi=newBindings.begin();bi!=newBindings.end();++bi) {
			printf("Binder: bound to %s\n",bi->address.toString().c_str());
		}
		*/

		// Swapping pointers and then letting the old one fall out of scope is faster than copying again
		_bindings.swap(newBindings);
	}

	/**
	 * Send a UDP packet from the specified local interface, or all
	 *
	 * Unfortunately even by examining the routing table there is no ultimately
	 * robust way to tell where we might reach another host that works in all
	 * environments. As a result, we send packets with null (wildcard) local
	 * addresses from *every* bound interface.
	 *
	 * These are typically initial HELLOs, path probes, etc., since normal
	 * conversations will have a local endpoint address. So the cost is low and
	 * if the peer is not reachable via that route then the packet will go
	 * nowhere and nothing will happen.
	 *
	 * It will of course only send via interface bindings of the same socket
	 * family. No point in sending V4 via V6 or vice versa.
	 *
	 * In any case on most hosts there's only one or two interfaces that we
	 * will use, so none of this is particularly costly.
	 *
	 * @param local Local interface address or null address for 'all'
	 * @param remote Remote address
	 * @param data Data to send
	 * @param len Length of data
	 * @param v4ttl If non-zero, send this packet with the specified IP TTL (IPv4 only)
	 */
	template<typename PHY_HANDLER_TYPE>
	inline bool udpSend(Phy<PHY_HANDLER_TYPE> &phy,const InetAddress &local,const InetAddress &remote,const void *data,unsigned int len,unsigned int v4ttl = 0) const
	{
		Mutex::Lock _l(_lock);
		if (local) {
			for(typename std::vector<_Binding>::const_iterator i(_bindings.begin());i!=_bindings.end();++i) {
				if (i->address == local) {
					if ((v4ttl)&&(local.ss_family == AF_INET))
						phy.setIp4UdpTtl(i->udpSock,v4ttl);
					const bool result = phy.udpSend(i->udpSock,reinterpret_cast<const struct sockaddr *>(&remote),data,len);
					if ((v4ttl)&&(local.ss_family == AF_INET))
						phy.setIp4UdpTtl(i->udpSock,255);
					return result;
				}
			}
			return false;
		} else {
			bool result = false;
			for(typename std::vector<_Binding>::const_iterator i(_bindings.begin());i!=_bindings.end();++i) {
				if (i->address.ss_family == remote.ss_family) {
					if ((v4ttl)&&(remote.ss_family == AF_INET))
						phy.setIp4UdpTtl(i->udpSock,v4ttl);
					result |= phy.udpSend(i->udpSock,reinterpret_cast<const struct sockaddr *>(&remote),data,len);
					if ((v4ttl)&&(remote.ss_family == AF_INET))
						phy.setIp4UdpTtl(i->udpSock,255);
				}
			}
			return result;
		}
	}

private:
	std::vector<_Binding> _bindings;
	Mutex _lock;
};

} // namespace ZeroTier

#endif
