/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_LINUX_NETLINK_HPP
#define ZT_LINUX_NETLINK_HPP

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

struct route_entry {
	InetAddress target;
	InetAddress via;
	int if_index;
	char iface[IFNAMSIZ];
};
typedef std::vector<route_entry> RouteList;

/**
 * Interface with Linux's RTNETLINK 
 */
class LinuxNetLink
{
private:
    LinuxNetLink();
    ~LinuxNetLink();

public:
    static LinuxNetLink& getInstance()
    {
        static LinuxNetLink instance;
        return instance;
    }

    LinuxNetLink(LinuxNetLink const&) = delete;
    void operator=(LinuxNetLink const&) = delete;

    void addRoute(const InetAddress &target, const InetAddress &via, const InetAddress &src, const char *ifaceName);
    void delRoute(const InetAddress &target, const InetAddress &via, const InetAddress &src, const char *ifaceName);
    RouteList getIPV4Routes() const;
    RouteList getIPV6Routes() const;

    void addAddress(const InetAddress &addr, const char *iface);
    void removeAddress(const InetAddress &addr, const char *iface);

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

    RouteList _routes_ipv4;
    Mutex _rv4_m;
    RouteList _routes_ipv6;
    Mutex _rv6_m;

    uint32_t _seq;

    struct iface_entry {
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

#endif // ZT_LINUX_NETLINK_HPPS