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
#ifdef __LINUX__
#include <sys/ioctl.h>
#include <net/if.h>
#endif
#endif

#include <string>
#include <vector>
#include <algorithm>
#include <utility>
#include <map>

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
		std::map<InetAddress,std::string> localIfAddrs;
		PhySocket *udps;
		//PhySocket *tcps;
		Mutex::Lock _l(_lock);

#ifdef __WINDOWS__

		char aabuf[32768];
		ULONG aalen = sizeof(aabuf);
		if (GetAdaptersAddresses(AF_UNSPEC,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,(void *)0,reinterpret_cast<PIP_ADAPTER_ADDRESSES>(aabuf),&aalen) == NO_ERROR) {
			PIP_ADAPTER_ADDRESSES a = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(aabuf);
			while (a) {
				PIP_ADAPTER_UNICAST_ADDRESS ua = a->FirstUnicastAddress;
				while (ua) {
					InetAddress ip(ua->Address.lpSockaddr);
					if (ifChecker.shouldBindInterface("",ip)) {
						switch(ip.ipScope()) {
							default: break;
							case InetAddress::IP_SCOPE_PSEUDOPRIVATE:
							case InetAddress::IP_SCOPE_GLOBAL:
							case InetAddress::IP_SCOPE_SHARED:
							case InetAddress::IP_SCOPE_PRIVATE:
								ip.setPort(port);
								localIfAddrs.insert(std::pair<InetAddress,std::string>(ip,std::string()));
								break;
						}
					}
					ua = ua->Next;
				}
				a = a->Next;
			}
		}

#else // not __WINDOWS__

		struct ifaddrs *ifatbl = (struct ifaddrs *)0;
		struct ifaddrs *ifa;
		if ((getifaddrs(&ifatbl) == 0)&&(ifatbl)) {
			ifa = ifatbl;
			while (ifa) {
				if ((ifa->ifa_name)&&(ifa->ifa_addr)) {
					InetAddress ip = *(ifa->ifa_addr);
					if (ifChecker.shouldBindInterface(ifa->ifa_name,ip)) {
						switch(ip.ipScope()) {
							default: break;
							case InetAddress::IP_SCOPE_PSEUDOPRIVATE:
							case InetAddress::IP_SCOPE_GLOBAL:
							case InetAddress::IP_SCOPE_SHARED:
							case InetAddress::IP_SCOPE_PRIVATE:
								ip.setPort(port);
								localIfAddrs.insert(std::pair<InetAddress,std::string>(ip,std::string(ifa->ifa_name)));
								break;
						}
					}
				}
				ifa = ifa->ifa_next;
			}
			freeifaddrs(ifatbl);
		}

#endif

		// Default to binding to wildcard if we can't enumerate addresses
		if (localIfAddrs.empty()) {
			localIfAddrs.insert(std::pair<InetAddress,std::string>(InetAddress((uint32_t)0,port),std::string()));
			localIfAddrs.insert(std::pair<InetAddress,std::string>(InetAddress((const void *)"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",16,port),std::string()));
		}

		// Close any old bindings to anything that doesn't exist anymore
		for(typename std::vector<_Binding>::const_iterator bi(_bindings.begin());bi!=_bindings.end();++bi) {
			if (localIfAddrs.find(bi->address) == localIfAddrs.end()) {
				phy.close(bi->udpSock,false);
				phy.close(bi->tcpListenSock,false);
			}
		}

		std::vector<_Binding> newBindings;
		for(std::map<InetAddress,std::string>::const_iterator ii(localIfAddrs.begin());ii!=localIfAddrs.end();++ii) {
			typename std::vector<_Binding>::const_iterator bi(_bindings.begin());
			while (bi != _bindings.end()) {
				if (bi->address == ii->first) {
					newBindings.push_back(*bi);
					break;
				}
				++bi;
			}

			if (bi == _bindings.end()) {
				udps = phy.udpBind(reinterpret_cast<const struct sockaddr *>(&(ii->first)),(void *)0,ZT_UDP_DESIRED_BUF_SIZE);
				if (udps) {
					//tcps = phy.tcpListen(reinterpret_cast<const struct sockaddr *>(&ii),(void *)0);
					//if (tcps) {
#ifdef __LINUX__
						// Bind Linux sockets to their device so routes tha we manage do not override physical routes (wish all platforms had this!)
						if (ii->second.length() > 0) {
							int fd = (int)Phy<PHY_HANDLER_TYPE>::getDescriptor(udps);
							char tmp[256];
							Utils::scopy(tmp,sizeof(tmp),ii->second.c_str());
							if (fd >= 0) {
								if (setsockopt(fd,SOL_SOCKET,SO_BINDTODEVICE,tmp,strlen(tmp)) != 0) {
									fprintf(stderr,"WARNING: unable to set SO_BINDTODEVICE to bind %s to %s\n",ii->first.toIpString().c_str(),ii->second.c_str());
								}
							}
						}
#endif // __LINUX__
						newBindings.push_back(_Binding());
						newBindings.back().udpSock = udps;
						//newBindings.back().tcpListenSock = tcps;
						newBindings.back().address = ii->first;
					//} else {
					//	phy.close(udps,false);
					//}
				}
			}
		}

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

	/**
	 * @return All currently bound local interface addresses
	 */
	inline std::vector<InetAddress> allBoundLocalInterfaceAddresses()
	{
		Mutex::Lock _l(_lock);
		std::vector<InetAddress> aa;
		for(std::vector<_Binding>::const_iterator i(_bindings.begin());i!=_bindings.end();++i)
			aa.push_back(i->address);
		return aa;
	}

private:
	std::vector<_Binding> _bindings;
	Mutex _lock;
};

} // namespace ZeroTier

#endif
