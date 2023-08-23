/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "../node/Constants.hpp"
#include "../osdep/OSUtils.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __WINDOWS__
#include <winsock2.h>
#include <windows.h>
#include <netioapi.h>
#include <IPHlpApi.h>
#endif

#ifdef __UNIX_LIKE__
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifndef ZT_SDK
#include <net/route.h>
#endif
#include <net/if.h>
#ifdef __BSD__
#include <net/if_dl.h>
#include <sys/sysctl.h>
#endif
#include <ifaddrs.h>
#endif

#include <vector>
#include <algorithm>
#include <utility>

#include "ManagedRoute.hpp"
#ifdef __LINUX__
#include "LinuxNetLink.hpp"
#endif

#define ZT_BSD_ROUTE_CMD "/sbin/route"

namespace ZeroTier {

namespace {

// Fork a target into two more specific targets e.g. 0.0.0.0/0 -> 0.0.0.0/1, 128.0.0.0/1
// If the target is already maximally-specific, 'right' will be unchanged and 'left' will be 't'
static void _forkTarget(const InetAddress &t,InetAddress &left,InetAddress &right)
{
	const unsigned int bits = t.netmaskBits() + 1;
	left = t;
	if (t.ss_family == AF_INET) {
		if (bits <= 32) {
			left.setPort(bits);
			right = t;
			reinterpret_cast<struct sockaddr_in *>(&right)->sin_addr.s_addr ^= Utils::hton((uint32_t)(1 << (32 - bits)));
			right.setPort(bits);
		} else {
			right.zero();
		}
	} else if (t.ss_family == AF_INET6) {
		if (bits <= 128) {
			left.setPort(bits);
			right = t;
			uint8_t *b = reinterpret_cast<uint8_t *>(reinterpret_cast<struct sockaddr_in6 *>(&right)->sin6_addr.s6_addr);
			b[bits / 8] ^= 1 << (8 - (bits % 8));
			right.setPort(bits);
		} else {
			right.zero();
		}
	}
}

struct _RTE
{
	InetAddress target;
	InetAddress via;
	char device[128];
	int metric;
	bool ifscope;
	bool isDefault;
};

#ifdef __BSD__ // ------------------------------------------------------------
#define ZT_ROUTING_SUPPORT_FOUND 1

#ifndef ZT_SDK
static std::vector<_RTE> _getRTEs(const InetAddress &target,bool contains)
{
	std::vector<_RTE> rtes;
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
			return rtes;

		char *buf = (char *)::malloc(needed);
		if (buf) {
			if (!sysctl(mib,6,buf,&needed,NULL,0)) {
		    struct rt_msghdr *rtm;
				for(char *next=buf,*end=buf+needed;next<end;) {
					rtm = (struct rt_msghdr *)next;
					char *saptr = (char *)(rtm + 1);
					char *saend = next + rtm->rtm_msglen;

					InetAddress sa_t,sa_v;
					int deviceIndex = -9999;
					bool isDefault = false;

					if (((rtm->rtm_flags & RTF_LLINFO) == 0)&&((rtm->rtm_flags & RTF_HOST) == 0)&&((rtm->rtm_flags & RTF_UP) != 0)&&((rtm->rtm_flags & RTF_MULTICAST) == 0)) {
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
										if ((sin6->sin6_addr.s6_addr[0] == 0xfe)&&((sin6->sin6_addr.s6_addr[1] & 0xc0) == 0x80)) {
											// BSD uses this fucking strange in-band signaling method to encode device scope IDs for IPv6 addresses... probably a holdover from very early versions of the spec.
											unsigned int interfaceIndex = ((((unsigned int)sin6->sin6_addr.s6_addr[2]) << 8) & 0xff) | (((unsigned int)sin6->sin6_addr.s6_addr[3]) & 0xff);
											sin6->sin6_addr.s6_addr[2] = 0;
											sin6->sin6_addr.s6_addr[3] = 0;
											if (!sin6->sin6_scope_id)
												sin6->sin6_scope_id = interfaceIndex;
										}

#ifdef __APPLE__
										isDefault = IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr) && !(rtm->rtm_flags & RTF_IFSCOPE);
#endif
									} else {
										struct sockaddr_in *sin4 = (struct sockaddr_in *)sa;
										isDefault = sin4->sin_addr.s_addr == 0;
									}
									sa_t = *sa;
									break;
								case 1:
									//printf("RTA_GATEWAY\n");
									switch(sa->sa_family) {
										case AF_LINK:
											// deviceIndex = (int)((const struct sockaddr_dl *)sa)->sdl_index;
										case AF_INET:
										case AF_INET6:
											sa_v = *sa;
											break;
									}
									break;
								case 2: {
									//printf("RTA_NETMASK\n");
									if (sa_t.ss_family == AF_INET6) {
										salen = sizeof(struct sockaddr_in6);
										unsigned int bits = 0;
										for(int i=0;i<16;++i) {
											unsigned char c = (unsigned char)((const struct sockaddr_in6 *)sa)->sin6_addr.s6_addr[i];
											if (c == 0xff)
												bits += 8;
											else break;
										}
										sa_t.setPort(bits);
									} else if (sa_t.ss_family == AF_INET) {
										salen = sizeof(struct sockaddr_in);
										sa_t.setPort((unsigned int)Utils::countBits((uint32_t)((const struct sockaddr_in *)sa)->sin_addr.s_addr));
									}
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


						deviceIndex = rtm->rtm_index;


						if (((contains)&&(sa_t.containsAddress(target)))||(sa_t == target)) {
							rtes.push_back(_RTE());
							rtes.back().target = sa_t;
							rtes.back().via = sa_v;
							rtes.back().isDefault = isDefault;
							if (deviceIndex >= 0) {
								if_indextoname(deviceIndex,rtes.back().device);
							} else {
								rtes.back().device[0] = (char)0;
							}
							rtes.back().metric = ((int)rtm->rtm_rmx.rmx_hopcount < 0) ? 0 : (int)rtm->rtm_rmx.rmx_hopcount;
						}
					}

					next = saend;
				}
			}

			::free(buf);
		}
	}

	return rtes;
}
#endif

static void _routeCmd(const char *op,const InetAddress &target,const InetAddress &via,const char *ifscope,const char *localInterface)
{
	// char f1[1024],f2[1024]; printf("cmd %s %s %s %s %s\n",op,target.toString(f1),via.toString(f2),ifscope,localInterface);
	long p = (long)fork();
	if (p > 0) {
		int exitcode = -1;
		::waitpid(p,&exitcode,0);
	} else if (p == 0) {
		::close(STDOUT_FILENO);
		::close(STDERR_FILENO);
		char ttmp[64];
		char iptmp[64];
		if (via) {
			if ((ifscope)&&(ifscope[0])) {
#ifdef ZT_TRACE
				fprintf(stderr, "DEBUG: route %s -ifscope %s %s %s" ZT_EOL_S, ifscope,((target.ss_family == AF_INET6) ? "-inet6" : "-inet"),target.toString(ttmp),via.toIpString(iptmp));
#endif
				::execl(ZT_BSD_ROUTE_CMD,ZT_BSD_ROUTE_CMD,op,"-ifscope",ifscope,((target.ss_family == AF_INET6) ? "-inet6" : "-inet"),target.toString(ttmp),via.toIpString(iptmp),(const char *)0);
			} else {
#ifdef ZT_TRACE
				fprintf(stderr, "DEBUG: route %s %s %s %s" ZT_EOL_S, op,((target.ss_family == AF_INET6) ? "-inet6" : "-inet"),target.toString(ttmp),via.toIpString(iptmp));
#endif
				::execl(ZT_BSD_ROUTE_CMD,ZT_BSD_ROUTE_CMD,op,((target.ss_family == AF_INET6) ? "-inet6" : "-inet"),target.toString(ttmp),via.toIpString(iptmp),(const char *)0);
			}
		} else if ((localInterface)&&(localInterface[0])) {
			if ((ifscope)&&(ifscope[0])) {
#ifdef ZT_TRACE
				fprintf(stderr, "DEBUG: route %s -ifscope %s %s %s -interface %s" ZT_EOL_S, op, ifscope,((target.ss_family == AF_INET6) ? "-inet6" : "-inet"),target.toString(ttmp),localInterface);
#endif
				::execl(ZT_BSD_ROUTE_CMD,ZT_BSD_ROUTE_CMD,op,"-ifscope",ifscope,((target.ss_family == AF_INET6) ? "-inet6" : "-inet"),target.toString(ttmp),"-interface",localInterface,(const char *)0);
			} else {
#ifdef ZT_TRACE
				fprintf(stderr, "DEBUG: route %s %s %s -interface %s" ZT_EOL_S, op,((target.ss_family == AF_INET6) ? "-inet6" : "-inet"),target.toString(ttmp),localInterface);
#endif
				::execl(ZT_BSD_ROUTE_CMD,ZT_BSD_ROUTE_CMD,op,((target.ss_family == AF_INET6) ? "-inet6" : "-inet"),target.toString(ttmp),"-interface",localInterface,(const char *)0);
			}
		}
		::_exit(-1);
	}
}

#endif // __BSD__ ------------------------------------------------------------

#ifdef __LINUX__ // ----------------------------------------------------------
#define ZT_ROUTING_SUPPORT_FOUND 1

// This has been replaced by LinuxNetLink

#endif // __LINUX__ ----------------------------------------------------------

#ifdef __WINDOWS__ // --------------------------------------------------------
#define ZT_ROUTING_SUPPORT_FOUND 1

static bool _winRoute(bool del,const NET_LUID &interfaceLuid,const NET_IFINDEX &interfaceIndex,const InetAddress &target,const InetAddress &via)
{
	MIB_IPFORWARD_ROW2 rtrow;
	InitializeIpForwardEntry(&rtrow);
	rtrow.InterfaceLuid.Value = interfaceLuid.Value;
	rtrow.InterfaceIndex = interfaceIndex;
	if (target.ss_family == AF_INET) {
		rtrow.DestinationPrefix.Prefix.si_family = AF_INET;
		rtrow.DestinationPrefix.Prefix.Ipv4.sin_family = AF_INET;
		rtrow.DestinationPrefix.Prefix.Ipv4.sin_addr.S_un.S_addr = reinterpret_cast<const struct sockaddr_in *>(&target)->sin_addr.S_un.S_addr;
		if (via.ss_family == AF_INET) {
			rtrow.NextHop.si_family = AF_INET;
			rtrow.NextHop.Ipv4.sin_family = AF_INET;
			rtrow.NextHop.Ipv4.sin_addr.S_un.S_addr = reinterpret_cast<const struct sockaddr_in *>(&via)->sin_addr.S_un.S_addr;
		}
	} else if (target.ss_family == AF_INET6) {
		rtrow.DestinationPrefix.Prefix.si_family = AF_INET6;
		rtrow.DestinationPrefix.Prefix.Ipv6.sin6_family = AF_INET6;
		memcpy(rtrow.DestinationPrefix.Prefix.Ipv6.sin6_addr.u.Byte,reinterpret_cast<const struct sockaddr_in6 *>(&target)->sin6_addr.u.Byte,16);
		if (via.ss_family == AF_INET6) {
			rtrow.NextHop.si_family = AF_INET6;
			rtrow.NextHop.Ipv6.sin6_family = AF_INET6;
			memcpy(rtrow.NextHop.Ipv6.sin6_addr.u.Byte,reinterpret_cast<const struct sockaddr_in6 *>(&via)->sin6_addr.u.Byte,16);
		}
	} else {
		return false;
	}
	rtrow.DestinationPrefix.PrefixLength = target.netmaskBits();
	rtrow.SitePrefixLength = rtrow.DestinationPrefix.PrefixLength;
	rtrow.ValidLifetime = 0xffffffff;
	rtrow.PreferredLifetime = 0xffffffff;
	rtrow.Metric = -1;
	rtrow.Protocol = MIB_IPPROTO_NETMGMT;
	rtrow.Loopback = FALSE;
	rtrow.AutoconfigureAddress = FALSE;
	rtrow.Publish = FALSE;
	rtrow.Immortal = FALSE;
	rtrow.Age = 0;
	rtrow.Origin = NlroManual;
	if (del) {
		return (DeleteIpForwardEntry2(&rtrow) == NO_ERROR);
	} else {
		NTSTATUS r = CreateIpForwardEntry2(&rtrow);
		if (r == NO_ERROR) {
			return true;
		} else if (r == ERROR_OBJECT_ALREADY_EXISTS) {
			return (SetIpForwardEntry2(&rtrow) == NO_ERROR);
		} else {
			return false;
		}
	}
}

static bool _winHasRoute(const NET_LUID &interfaceLuid, const NET_IFINDEX &interfaceIndex, const InetAddress &target, const InetAddress &via)
{
	MIB_IPFORWARD_ROW2 rtrow;
	InitializeIpForwardEntry(&rtrow);
	rtrow.InterfaceLuid.Value = interfaceLuid.Value;
	rtrow.InterfaceIndex = interfaceIndex;
	if (target.ss_family == AF_INET) {
		rtrow.DestinationPrefix.Prefix.si_family = AF_INET;
		rtrow.DestinationPrefix.Prefix.Ipv4.sin_family = AF_INET;
		rtrow.DestinationPrefix.Prefix.Ipv4.sin_addr.S_un.S_addr = reinterpret_cast<const struct sockaddr_in *>(&target)->sin_addr.S_un.S_addr;
		if (via.ss_family == AF_INET) {
			rtrow.NextHop.si_family = AF_INET;
			rtrow.NextHop.Ipv4.sin_family = AF_INET;
			rtrow.NextHop.Ipv4.sin_addr.S_un.S_addr = reinterpret_cast<const struct sockaddr_in *>(&via)->sin_addr.S_un.S_addr;
		}
	} else if (target.ss_family == AF_INET6) {
		rtrow.DestinationPrefix.Prefix.si_family = AF_INET6;
		rtrow.DestinationPrefix.Prefix.Ipv6.sin6_family = AF_INET6;
		memcpy(rtrow.DestinationPrefix.Prefix.Ipv6.sin6_addr.u.Byte, reinterpret_cast<const struct sockaddr_in6 *>(&target)->sin6_addr.u.Byte, 16);
		if (via.ss_family == AF_INET6) {
			rtrow.NextHop.si_family = AF_INET6;
			rtrow.NextHop.Ipv6.sin6_family = AF_INET6;
			memcpy(rtrow.NextHop.Ipv6.sin6_addr.u.Byte, reinterpret_cast<const struct sockaddr_in6 *>(&via)->sin6_addr.u.Byte, 16);
		}
	} else {
		return false;
	}
	rtrow.DestinationPrefix.PrefixLength = target.netmaskBits();
	rtrow.SitePrefixLength = rtrow.DestinationPrefix.PrefixLength;
	return (GetIpForwardEntry2(&rtrow) == NO_ERROR);
}

#endif // __WINDOWS__ --------------------------------------------------------

#ifndef ZT_ROUTING_SUPPORT_FOUND
#error "ManagedRoute.cpp has no support for managing routes on this platform! You'll need to check and see if one of the existing ones will work and make sure proper defines are set, or write one. Please do a GitHub pull request if you do this for a new OS."
#endif

} // anonymous namespace

ManagedRoute::ManagedRoute(const InetAddress &target,const InetAddress &via,const InetAddress &src,const char *device)
{
	_target = target;
	_via = via;
	_src = src;

	if (_via.ss_family == AF_INET) {
		_via.setPort(32);
	} else if (_via.ss_family == AF_INET6) {
		_via.setPort(128);
	}

	if (_src.ss_family == AF_INET) {
		_src.setPort(32);
	} else if (_src.ss_family == AF_INET6) {
		_src.setPort(128);
	}

	Utils::scopy(_device,sizeof(_device),device);
	_systemDevice[0] = (char)0;
}

ManagedRoute::~ManagedRoute()
{
	this->remove();
}

/* Linux NOTE: for default route override, some Linux distributions will
 * require a change to the rp_filter parameter. A value of '1' will prevent
 * default route override from working properly.
 *
 * sudo sysctl -w net.ipv4.conf.all.rp_filter=2
 *
 * Add to /etc/sysctl.conf or /etc/sysctl.d/... to make permanent.
 *
 * This is true of CentOS/RHEL 6+ and possibly others. This is because
 * Linux default route override implies asymmetric routes, which then
 * trigger Linux's "martian packet" filter. */

#ifndef ZT_SDK
bool ManagedRoute::sync()
{
#ifdef __WINDOWS__
	NET_LUID interfaceLuid;
	interfaceLuid.Value = (ULONG64)Utils::hexStrToU64(_device); // on Windows we use the hex LUID as the "interface name" for ManagedRoute
	NET_IFINDEX interfaceIndex = -1;
	if (ConvertInterfaceLuidToIndex(&interfaceLuid,&interfaceIndex) != NO_ERROR)
		return false;
#endif

	InetAddress leftt,rightt;
	if (_target.netmaskBits() == 0) // bifurcate only the default route
		_forkTarget(_target,leftt,rightt);
	else leftt = _target;

#ifdef __BSD__ // ------------------------------------------------------------

	if (_device[0]) {
		bool haveDevice = false;
		struct ifaddrs *ifa = (struct ifaddrs *)0;
		if (!getifaddrs(&ifa)) {
			struct ifaddrs *p = ifa;
			while (p) {
				if ((p->ifa_name)&&(!strcmp(_device, p->ifa_name))) {
					haveDevice = true;
					break;
				}
				p = p->ifa_next;
			}
			freeifaddrs(ifa);
		}
		if (!haveDevice)
			return false;
	}

	std::vector<_RTE> rtes(_getRTEs(_target,false));

	bool hasRoute = false;
	for(std::vector<_RTE>::iterator r(rtes.begin());r!=rtes.end();++r) {
		hasRoute = _target == r->target && _via.ipOnly() == r->via.ipOnly() && (strcmp(r->device,_device) == 0);
		if (hasRoute) { break; }
	}

	// char buf[255];
	// fprintf(stderr, "hasRoute %d %s\n", !!hasRoute, _target.toString(buf));


	if (!hasRoute) {
		if (_target && _target.netmaskBits() == 0) {
			InetAddress newSystemVia;
			char newSystemDevice[128];
			newSystemDevice[0] = (char)0;

			// If macos has a network hiccup, it deletes what _systemVia we had set.
			// Then we don't know how to set the default route again.
			// So use the one we had set previously. Don't overwrite it.
			if (!_systemVia) {
				// Find system default route that this route should override
				// We need to put it back when default route is turned off
				for(std::vector<_RTE>::iterator r(rtes.begin());r!=rtes.end();++r) {
					if (r->via) {
						if ( !_systemVia && r->isDefault == 1 && (strcmp(r->device,_device) != 0) ) {

							newSystemVia = r->via;
							Utils::scopy(newSystemDevice,sizeof(newSystemDevice),r->device);
						}
					}
				}
				if (newSystemVia) { _systemVia = newSystemVia; }
			}


			// char buf1[255], buf2[255];
			// fprintf(stderr, "_systemVia %s new %s\n", _systemVia.toString(buf1), newSystemVia.toString(buf2));
			if (!_systemVia) { return false; }

			if (!_systemDevice[0]) {
				// Get device corresponding to route if we don't have that already
				if ((newSystemVia)&&(!newSystemDevice[0])) {
					rtes = _getRTEs(newSystemVia,true);
					for(std::vector<_RTE>::iterator r(rtes.begin());r!=rtes.end();++r) {
						if ( (r->device[0]) && (strcmp(r->device,_device) != 0) && r->target.netmaskBits() != 0) {
							Utils::scopy(newSystemDevice,sizeof(newSystemDevice),r->device);
							break;
						}
					}
				}

				if (newSystemDevice[0]) {
					Utils::scopy(_systemDevice,sizeof(_systemDevice),newSystemDevice);
				}
			}
			// fprintf(stderr, "_systemDevice %s new %s\n", _systemDevice, newSystemDevice);
			if (!_systemDevice[0]) { return false; }


			// Do Default Route route commands
			_routeCmd("delete",_target,_systemVia,(const char *)0,(const char *)0);
			_routeCmd("add",_target,_via,(const char *)0,(const char *)0);
			_routeCmd("add",_target,_systemVia,_systemDevice,(const char *)0);

			_applied[_target] = true;

		} else {
			// Do Non-Default route commands
			_applied[_target] = true;
			_routeCmd("add",leftt,_via,(const char *)0,(_via) ? (const char *)0 : _device);
		}
	}


#endif // __BSD__ ------------------------------------------------------------

#ifdef __LINUX__ // ----------------------------------------------------------

	if ((leftt)&&(!LinuxNetLink::getInstance().routeIsSet(leftt,_via,_src,_device))) {
		_applied[leftt] = false; // boolean unused
		LinuxNetLink::getInstance().addRoute(leftt, _via, _src, _device);
	}
	if ((rightt)&&(!LinuxNetLink::getInstance().routeIsSet(rightt,_via,_src,_device))) {
		_applied[rightt] = false; // boolean unused
		LinuxNetLink::getInstance().addRoute(rightt, _via, _src, _device);
	}

#endif // __LINUX__ ----------------------------------------------------------

#ifdef __WINDOWS__ // --------------------------------------------------------

	if ( (!_applied.count(leftt)) || (!_winHasRoute(interfaceLuid,interfaceIndex,leftt,_via)) ) {
		_applied[leftt] = false; // boolean unused
		_winRoute(false,interfaceLuid,interfaceIndex,leftt,_via);
	}
	if ( (rightt) && ( (!_applied.count(rightt)) || (!_winHasRoute(interfaceLuid,interfaceIndex,rightt,_via)) ) ) {
		_applied[rightt] = false; // boolean unused
		_winRoute(false,interfaceLuid,interfaceIndex,rightt,_via);
	}

#endif // __WINDOWS__ --------------------------------------------------------

	return true;
}
#endif

void ManagedRoute::remove()
{
#ifdef __WINDOWS__
	NET_LUID interfaceLuid;
	interfaceLuid.Value = (ULONG64)Utils::hexStrToU64(_device); // on Windows we use the hex LUID as the "interface name" for ManagedRoute
	NET_IFINDEX interfaceIndex = -1;
	if (ConvertInterfaceLuidToIndex(&interfaceLuid,&interfaceIndex) != NO_ERROR)
		return;
#endif

#ifdef __BSD__
#endif // __BSD__ ------------------------------------------------------------

	for(std::map<InetAddress,bool>::iterator r(_applied.begin());r!=_applied.end();++r) {
#ifdef __BSD__ // ------------------------------------------------------------
		if (_target && _target.netmaskBits() == 0) {
			if (_systemVia) {
				_routeCmd("delete",_target,_via,(const char *)0,(const char *)0);
				_routeCmd("delete",_target,_systemVia,_systemDevice,(const char *)0);

				_routeCmd("add",_target,_systemVia,(const char *)0,(const char *)0);

			}
		} else {
		_routeCmd("delete",_target,_via, (const char *)0, _via ? (const char *)0 : _device);
	}
	break;
#endif // __BSD__ ------------------------------------------------------------

#ifdef __LINUX__ // ----------------------------------------------------------
		//_routeCmd("del",r->first,_via,(_via) ? (const char *)0 : _device);
		LinuxNetLink::getInstance().delRoute(r->first,_via,_src,(_via) ? (const char *)0 : _device);
#endif // __LINUX__ ----------------------------------------------------------

#ifdef __WINDOWS__ // --------------------------------------------------------
		_winRoute(true,interfaceLuid,interfaceIndex,r->first,_via);
#endif // __WINDOWS__ --------------------------------------------------------
	}

	_target.zero();
	_via.zero();
	_systemVia.zero();
	_device[0] = (char)0;
	_systemDevice[0] = (char)0;
	_applied.clear();
}

} // namespace ZeroTier
