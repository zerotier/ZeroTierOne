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

#ifndef ZT_LINUX_NETLINK_HPP
#define ZT_LINUX_NETLINK_HPP

#include "../node/Constants.hpp"

#ifdef __LINUX__

#include <vector>

#include <sys/socket.h>
#include <asm/types.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <linux/if.h>

#include "../node/InetAddress.hpp"
#include "../node/MAC.hpp"
#include "Thread.hpp"
#include "../node/Hashtable.hpp"
#include "../node/Mutex.hpp"


namespace ZeroTier {

/**
 * Interface with Linux's RTNETLINK
 */
class LinuxNetLink
{
private:
	LinuxNetLink();
	~LinuxNetLink();

public:
	struct Route {
		InetAddress target;
		InetAddress via;
		InetAddress src;
		int ifidx;

		inline bool operator==(const Route &r) const
		{ return ((target == r.target)&&(via == r.via)&&(src == r.src)&&(ifidx == r.ifidx)); }
		inline bool operator!=(const Route &r) const
		{ return (!(*this == r)); }
		inline bool operator<(const Route &r) const
		{
			if (target < r.target) {
				return true;
			} else if (target == r.target) {
				if (via < r.via) {
					return true;
				} else if (via == r.via) {
					if (src < r.src) {
						return true;
					} else if (src == r.src) {
						return (ifidx < r.ifidx);
					}
				}
			}
			return false;
		}
		inline bool operator>(const Route &r) const
		{ return (r < *this); }
		inline bool operator<=(const Route &r) const
		{ return !(r < *this); }
		inline bool operator>=(const Route &r) const
		{ return !(*this < r); }
	};

	static LinuxNetLink& getInstance()
	{
		static LinuxNetLink instance;
		return instance;
	}

	LinuxNetLink(LinuxNetLink const&) = delete;
	void operator=(LinuxNetLink const&) = delete;

	void addRoute(const InetAddress &target, const InetAddress &via, const InetAddress &src, const char *ifaceName);
	void delRoute(const InetAddress &target, const InetAddress &via, const InetAddress &src, const char *ifaceName);

	void addAddress(const InetAddress &addr, const char *iface);
	void removeAddress(const InetAddress &addr, const char *iface);

	bool routeIsSet(const InetAddress &target, const InetAddress &via, const InetAddress &src, const char *ifname);

	void threadMain() throw();

private:
	int _doRecv(int fd);

	void _processMessage(struct nlmsghdr *nlp, int nll);
	void _routeAdded(struct nlmsghdr *nlp);
	void _routeDeleted(struct nlmsghdr *nlp);
	void _linkAdded(struct nlmsghdr *nlp);
	void _linkDeleted(struct nlmsghdr *nlp);
	void _ipAddressAdded(struct nlmsghdr *nlp);
	void _ipAddressDeleted(struct nlmsghdr *nlp);

	void _requestInterfaceList();
	void _requestIPv4Routes();
	void _requestIPv6Routes();

	int _indexForInterface(const char *iface);

	void _setSocketTimeout(int fd, int seconds = 1);

	Thread _t;
	bool _running;

	uint32_t _seq;

	std::map< InetAddress,std::set<Route> > _routes;
	Mutex _routes_m;

	struct iface_entry {
		iface_entry()
		{ memset(this,0,sizeof(iface_entry)); }
		int index;
		char ifacename[IFNAMSIZ];
		char mac[18];
		char mac_bin[6];
		unsigned int mtu;
	};
	Hashtable<int, iface_entry> _interfaces;
	Mutex _if_m;

	// socket communication vars;
	int _fd;
	struct sockaddr_nl _la;
};

}

#endif

#endif // ZT_LINUX_NETLINK_HPPS