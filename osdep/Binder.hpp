/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

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
#include <set>
#include <atomic>

#include "../node/InetAddress.hpp"
#include "../node/Mutex.hpp"
#include "../node/Utils.hpp"

#include "Phy.hpp"
#include "OSUtils.hpp"

// Period between refreshes of bindings
#define ZT_BINDER_REFRESH_PERIOD 30000

// Max number of bindings
#define ZT_BINDER_MAX_BINDINGS 256

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
class Binder
{
private:
	struct _Binding
	{
		_Binding() : udpSock((PhySocket *)0),tcpListenSock((PhySocket *)0) {}
		PhySocket *udpSock;
		PhySocket *tcpListenSock;
		InetAddress address;
	};

public:
	Binder() : _bindingCount(0) {}

	/**
	 * Close all bound ports, should be called on shutdown
	 *
	 * @param phy Physical interface
	 */
	template<typename PHY_HANDLER_TYPE>
	void closeAll(Phy<PHY_HANDLER_TYPE> &phy)
	{
		Mutex::Lock _l(_lock);
		for(unsigned int b=0,c=_bindingCount;b<c;++b) {
			phy.close(_bindings[b].udpSock,false);
			phy.close(_bindings[b].tcpListenSock,false);
		}
		_bindingCount = 0;
	}

	/**
	 * Scan local devices and addresses and rebind TCP and UDP
	 *
	 * This should be called after wake from sleep, on detected network device
	 * changes, on startup, or periodically (e.g. every 30-60s).
	 *
	 * @param phy Physical interface
	 * @param ports Ports to bind on all interfaces
	 * @param portCount Number of ports
	 * @param explicitBind If present, override interface IP detection and bind to these (if possible)
	 * @param ifChecker Interface checker function to see if an interface should be used
	 * @tparam PHY_HANDLER_TYPE Type for Phy<> template
	 * @tparam INTERFACE_CHECKER Type for class containing shouldBindInterface() method
	 */
	template<typename PHY_HANDLER_TYPE,typename INTERFACE_CHECKER>
	void refresh(Phy<PHY_HANDLER_TYPE> &phy,unsigned int *ports,unsigned int portCount,const std::vector<InetAddress> explicitBind,INTERFACE_CHECKER &ifChecker)
	{
		std::map<InetAddress,std::string> localIfAddrs;
		PhySocket *udps,*tcps;
		Mutex::Lock _l(_lock);
		bool interfacesEnumerated = true;

		if (explicitBind.empty()) {
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
									for(int x=0;x<(int)portCount;++x) {
										ip.setPort(ports[x]);
										localIfAddrs.insert(std::pair<InetAddress,std::string>(ip,std::string()));
									}
									break;
							}
						}
						ua = ua->Next;
					}
					a = a->Next;
				}
			}
			else {
				interfacesEnumerated = false;
			}

#else // not __WINDOWS__

			/* On Linux we use an alternative method if available since getifaddrs()
			 * gets very slow when there are lots of network namespaces. This won't
			 * work unless /proc/PID/net/if_inet6 exists and it may not on some
			 * embedded systems, so revert to getifaddrs() there. */

#ifdef __LINUX__
			char fn[256],tmp[256];
			std::set<std::string> ifnames;
			const unsigned long pid = (unsigned long)getpid();

			// Get all device names
			OSUtils::ztsnprintf(fn,sizeof(fn),"/proc/%lu/net/dev",pid);
			FILE *procf = fopen(fn,"r");
			if (procf) {
				while (fgets(tmp,sizeof(tmp),procf)) {
					tmp[255] = 0;
					char *saveptr = (char *)0;
					for(char *f=Utils::stok(tmp," \t\r\n:|",&saveptr);(f);f=Utils::stok((char *)0," \t\r\n:|",&saveptr)) {
						if ((strcmp(f,"Inter-") != 0)&&(strcmp(f,"face") != 0)&&(f[0] != 0))
							ifnames.insert(f);
						break; // we only want the first field
					}
				}
				fclose(procf);
			}
			else {
				interfacesEnumerated = false;
			}

			// Get IPv6 addresses (and any device names we don't already know)
			OSUtils::ztsnprintf(fn,sizeof(fn),"/proc/%lu/net/if_inet6",pid);
			procf = fopen(fn,"r");
			if (procf) {
				while (fgets(tmp,sizeof(tmp),procf)) {
					tmp[255] = 0;
					char *saveptr = (char *)0;
					unsigned char ipbits[16];
					memset(ipbits,0,sizeof(ipbits));
					char *devname = (char *)0;
					int n = 0;
					for(char *f=Utils::stok(tmp," \t\r\n",&saveptr);(f);f=Utils::stok((char *)0," \t\r\n",&saveptr)) {
						switch(n++) {
							case 0: // IP in hex
								Utils::unhex(f,32,ipbits,16);
								break;
							case 5: // device name
								devname = f;
								break;
						}
					}
					if (devname) {
						ifnames.insert(devname);
						InetAddress ip(ipbits,16,0);
						if (ifChecker.shouldBindInterface(devname,ip)) {
							switch(ip.ipScope()) {
								default: break;
								case InetAddress::IP_SCOPE_PSEUDOPRIVATE:
								case InetAddress::IP_SCOPE_GLOBAL:
								case InetAddress::IP_SCOPE_SHARED:
								case InetAddress::IP_SCOPE_PRIVATE:
									for(int x=0;x<(int)portCount;++x) {
										ip.setPort(ports[x]);
										localIfAddrs.insert(std::pair<InetAddress,std::string>(ip,std::string(devname)));
									}
									break;
							}
						}
					}
				}
				fclose(procf);
			}

			// Get IPv4 addresses for each device
			if (ifnames.size() > 0) {
				const int controlfd = (int)socket(AF_INET,SOCK_DGRAM,0);
				struct ifconf configuration;
				configuration.ifc_len = 0;
				configuration.ifc_buf = nullptr;

				if (controlfd < 0) goto ip4_address_error;
				if (ioctl(controlfd, SIOCGIFCONF, &configuration) < 0) goto ip4_address_error;
				configuration.ifc_buf = (char*)malloc(configuration.ifc_len);
				if (ioctl(controlfd, SIOCGIFCONF, &configuration) < 0) goto ip4_address_error;

				for (int i=0; i < (int)(configuration.ifc_len / sizeof(ifreq)); i ++) {
					struct ifreq& request = configuration.ifc_req[i];
					struct sockaddr* addr = &request.ifr_ifru.ifru_addr;
					if (addr->sa_family != AF_INET) continue;
					std::string ifname = request.ifr_ifrn.ifrn_name;
					// name can either be just interface name or interface name followed by ':' and arbitrary label
					if (ifname.find(':') != std::string::npos)
						ifname = ifname.substr(0, ifname.find(':'));

					InetAddress ip(&(((struct sockaddr_in *)addr)->sin_addr),4,0);
					if (ifChecker.shouldBindInterface(ifname.c_str(), ip)) {
						switch(ip.ipScope()) {
						default: break;
						case InetAddress::IP_SCOPE_PSEUDOPRIVATE:
						case InetAddress::IP_SCOPE_GLOBAL:
						case InetAddress::IP_SCOPE_SHARED:
						case InetAddress::IP_SCOPE_PRIVATE:
							for(int x=0;x<(int)portCount;++x) {
								ip.setPort(ports[x]);
								localIfAddrs.insert(std::pair<InetAddress,std::string>(ip,ifname));
							}
							break;
						}
					}
				}

			ip4_address_error:
				free(configuration.ifc_buf);
				if (controlfd > 0) close(controlfd);
			}

			const bool gotViaProc = (localIfAddrs.size() > 0);
#else
			const bool gotViaProc = false;
#endif
#if !defined(ZT_SDK) || !defined(__ANDROID__) // getifaddrs() freeifaddrs() not available on Android
			if (!gotViaProc) {
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
										for(int x=0;x<(int)portCount;++x) {
											ip.setPort(ports[x]);
											localIfAddrs.insert(std::pair<InetAddress,std::string>(ip,std::string(ifa->ifa_name)));
										}
										break;
								}
							}
						}
						ifa = ifa->ifa_next;
					}
					freeifaddrs(ifatbl);
				}
				else {
					interfacesEnumerated = false;
				}
			}
#endif

#endif
		} else {
			for(std::vector<InetAddress>::const_iterator i(explicitBind.begin());i!=explicitBind.end();++i)
				localIfAddrs.insert(std::pair<InetAddress,std::string>(*i,std::string()));
		}

		// Default to binding to wildcard if we can't enumerate addresses
		if (!interfacesEnumerated && localIfAddrs.empty()) {
			for(int x=0;x<(int)portCount;++x) {
				localIfAddrs.insert(std::pair<InetAddress,std::string>(InetAddress((uint32_t)0,ports[x]),std::string()));
				localIfAddrs.insert(std::pair<InetAddress,std::string>(InetAddress((const void *)"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",16,ports[x]),std::string()));
			}
		}

		const unsigned int oldBindingCount = _bindingCount;
		_bindingCount = 0;

		// Save bindings that are still valid, close those that are not
		for(unsigned int b=0;b<oldBindingCount;++b) {
			if (localIfAddrs.find(_bindings[b].address) != localIfAddrs.end()) {
				if (_bindingCount != b)
					_bindings[(unsigned int)_bindingCount] = _bindings[b];
				++_bindingCount;
			} else {
				PhySocket *const udps = _bindings[b].udpSock;
				PhySocket *const tcps = _bindings[b].tcpListenSock;
				_bindings[b].udpSock = (PhySocket *)0;
				_bindings[b].tcpListenSock = (PhySocket *)0;
				phy.close(udps,false);
				phy.close(tcps,false);
			}
		}

		// Generate set of unique interface names (used for formation of logical slave set in multipath code)
		for(std::map<InetAddress,std::string>::const_iterator ii(localIfAddrs.begin());ii!=localIfAddrs.end();++ii) {
			slaveIfNames.insert(ii->second);
		}
		for (std::set<std::string>::iterator si(slaveIfNames.begin());si!=slaveIfNames.end();si++) {
			bool bFoundMatch = false;
			for(std::map<InetAddress,std::string>::const_iterator ii(localIfAddrs.begin());ii!=localIfAddrs.end();++ii) {
				if (ii->second == *si) {
					bFoundMatch = true;
					break;
				}
			}
			if (!bFoundMatch) {
				slaveIfNames.erase(si);
			}
		}

		// Create new bindings for those not already bound
		for(std::map<InetAddress,std::string>::const_iterator ii(localIfAddrs.begin());ii!=localIfAddrs.end();++ii) {
			unsigned int bi = 0;
			while (bi != _bindingCount) {
				if (_bindings[bi].address == ii->first)
					break;
				++bi;
			}
			if (bi == _bindingCount) {
				udps = phy.udpBind(reinterpret_cast<const struct sockaddr *>(&(ii->first)),(void *)0,ZT_UDP_DESIRED_BUF_SIZE);
				tcps = phy.tcpListen(reinterpret_cast<const struct sockaddr *>(&(ii->first)),(void *)0);
				if ((udps)&&(tcps)) {
#ifdef __LINUX__
					// Bind Linux sockets to their device so routes that we manage do not override physical routes (wish all platforms had this!)
					if (ii->second.length() > 0) {
						char tmp[256];
						Utils::scopy(tmp,sizeof(tmp),ii->second.c_str());
						int fd = (int)Phy<PHY_HANDLER_TYPE>::getDescriptor(udps);
						if (fd >= 0)
							setsockopt(fd,SOL_SOCKET,SO_BINDTODEVICE,tmp,strlen(tmp));
						fd = (int)Phy<PHY_HANDLER_TYPE>::getDescriptor(tcps);
						if (fd >= 0)
							setsockopt(fd,SOL_SOCKET,SO_BINDTODEVICE,tmp,strlen(tmp));
					}
#endif // __LINUX__
					if (_bindingCount < ZT_BINDER_MAX_BINDINGS) {
						_bindings[_bindingCount].udpSock = udps;
						_bindings[_bindingCount].tcpListenSock = tcps;
						_bindings[_bindingCount].address = ii->first;
						phy.setIfName(udps,(char*)ii->second.c_str(),(int)ii->second.length());
						++_bindingCount;
					}
				} else {
					phy.close(udps,false);
					phy.close(tcps,false);
				}
			}
		}
	}

	/**
	 * @return All currently bound local interface addresses
	 */
	inline std::vector<InetAddress> allBoundLocalInterfaceAddresses() const
	{
		std::vector<InetAddress> aa;
		Mutex::Lock _l(_lock);
		for(unsigned int b=0,c=_bindingCount;b<c;++b)
			aa.push_back(_bindings[b].address);
		return aa;
	}

	/**
	 * Send from all bound UDP sockets
	 */
	template<typename PHY_HANDLER_TYPE>
	inline bool udpSendAll(Phy<PHY_HANDLER_TYPE> &phy,const struct sockaddr_storage *addr,const void *data,unsigned int len,unsigned int ttl)
	{
		bool r = false;
		Mutex::Lock _l(_lock);
		for(unsigned int b=0,c=_bindingCount;b<c;++b) {
			if (ttl) phy.setIp4UdpTtl(_bindings[b].udpSock,ttl);
			if (phy.udpSend(_bindings[b].udpSock,(const struct sockaddr *)addr,data,len)) r = true;
			if (ttl) phy.setIp4UdpTtl(_bindings[b].udpSock,255);
		}
		return r;
	}

	/**
	 * @param addr Address to check
	 * @return True if this is a bound local interface address
	 */
	inline bool isBoundLocalInterfaceAddress(const InetAddress &addr) const
	{
		Mutex::Lock _l(_lock);
		for(unsigned int b=0;b<_bindingCount;++b) {
			if (_bindings[b].address == addr)
				return true;
		}
		return false;
	}

	/**
	 * Quickly check that a UDP socket is valid
	 *
	 * @param udpSock UDP socket to check
	 * @return True if socket is currently bound/allocated
	 */
	inline bool isUdpSocketValid(PhySocket *const udpSock)
	{
		for(unsigned int b=0,c=_bindingCount;b<c;++b) {
			if (_bindings[b].udpSock == udpSock)
				return (b < _bindingCount); // double check atomic which may have changed
		}
		return false;
	}

	inline std::set<std::string> getSlaveInterfaceNames()
	{
		Mutex::Lock _l(_lock);
		return slaveIfNames;
	}

private:

	std::set<std::string> slaveIfNames;
	_Binding _bindings[ZT_BINDER_MAX_BINDINGS];
	std::atomic<unsigned int> _bindingCount;
	Mutex _lock;
};

} // namespace ZeroTier

#endif
