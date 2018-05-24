/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2018  ZeroTier, Inc.  https://www.zerotier.com/
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
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_LINUX_NETLINK_HPP
#define ZT_LINUX_NETLINK_HPP

#include <vector>

#include <bits/sockaddr.h>
#include <asm/types.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>


#include "../node/InetAddress.hpp"
#include "Thread.hpp"


namespace ZeroTier {

struct route_entry {
    InetAddress target;
    InetAddress via;
    const char *iface;
};

typedef std::vector<route_entry> RouteList;

struct nl_req {
    struct nlmsghdr nl;
    struct rtmsg rt;
    char buf[8192];
};

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

    void addRoute(const InetAddress &target, const InetAddress &via, const char *ifaceName);
    void delRoute(const InetAddress &target, const InetAddress &via, const char *ifaceName);
    RouteList getIPV4Routes() const;
    RouteList getIPV6Routes() const;

    void addInterface(const char *iface);

    void addAddress(const InetAddress &addr, const char *iface);

    void threadMain() throw();
private:
    void _processMessage();
    void _routeAdded();
    void _routeDeleted();
    void _linkAdded();
    void _linkDeleted();
    void _ipAddressAdded();
    void _ipAddressDeleted();


    void _requestIPv4Routes();
    void _requestIPv6Routes();


    Thread _t;
    bool _running;
    RouteList _routes_ipv4;
    RouteList _routes_ipv6;

    // socket communication vars;
    int _fd;
    struct sockaddr_nl _la;
    struct sockaddr_nl _pa;
    struct msghdr _msg;
    struct iovec _iov;
    int _rtn;
    char _buf[8192];

    // RTNETLINK message pointers & lengths
    // used for processing messages
    struct nlmsghdr *_nlp;
    int _nll;

    struct rtmsg *_rtp;
    int _rtl;
    struct rtattr *_rtap;
    
    struct ifinfomsg *_ifip;
    int _ifil;

    struct ifaddrmsg *_ifap;
    int _ifal;
};    

}

#endif // ZT_LINUX_NETLINK_HPPS