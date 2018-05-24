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

#include "LinuxNetLink.hpp"

#include <unistd.h>

namespace ZeroTier {

LinuxNetLink::LinuxNetLink()
    : _t()
    , _running(false)
    , _routes_ipv4()
    , _routes_ipv6()
    , _seq(0)
    , _fd(socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE))
    , _la({0})
{

    // set socket timeout to 1 sec so we're not permablocking recv() calls
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if(setsockopt(_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) != 0) {
        fprintf(stderr, "setsockopt failed: %s\n", strerror(errno));
    }

    _la.nl_family = AF_NETLINK;
    _la.nl_pid = getpid();
    _la.nl_groups = RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV6_IFADDR|RTMGRP_IPV4_ROUTE|RTMGRP_IPV6_ROUTE|RTMGRP_NOTIFY;
    if (bind(_fd, (struct sockaddr*)&_la, sizeof(_la))) {
        fprintf(stderr, "Error connecting to RTNETLINK: %s\n", strerror(errno));
        ::exit(1);
    }

    _running = true;
    _t = Thread::start(this);

    fprintf(stderr, "Requesting IPV4 Routes\n");
    _requestIPv4Routes();
    Thread::sleep(10);
    fprintf(stderr, "Requesting IPV6 Routes\n");
    _requestIPv6Routes();
}

LinuxNetLink::~LinuxNetLink()
{
    _running = false;
    Thread::join(_t);

    ::close(_fd);
}

void LinuxNetLink::threadMain() throw()
{
    char buf[8192];
    char *p = NULL;
    struct nlmsghdr *nlp;
    int nll = 0;
    int rtn = 0;
    p = buf;

    while(_running) {
        rtn = recv(_fd, p, sizeof(buf) - nll, 0);

        if (rtn > 0) {
            nlp = (struct nlmsghdr *)p;

            if(nlp->nlmsg_type == NLMSG_ERROR && (nlp->nlmsg_flags & NLM_F_ACK) != NLM_F_ACK) {
                fprintf(stderr, "NLMSG_ERROR\n");
                struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(nlp);
                if (err->error != 0) {
                    fprintf(stderr, "rtnetlink error: %s\n", strerror(-(err->error)));
                }
                p = buf;
                nll = 0;
                continue;
            }

            if (nlp->nlmsg_type == NLMSG_NOOP) {
                fprintf(stderr, "noop\n");
                continue;
            }

            if( (nlp->nlmsg_flags & NLM_F_MULTI) == NLM_F_MULTI || (nlp->nlmsg_type == NLMSG_DONE))
            {
                if (nlp->nlmsg_type == NLMSG_DONE) {
                    _processMessage(nlp, nll);
                    p = buf;
                    nll = 0;
                    continue;
                }
                p += rtn;
                nll += rtn;
            }

            if (nlp->nlmsg_type == NLMSG_OVERRUN) {
                fprintf(stderr, "NLMSG_OVERRUN: Data lost\n");
                p = buf;
                nll = 0;
                continue;
            }
            
            nll += rtn;

            _processMessage(nlp, nll);
            p = buf;
            nll = 0;
        }
        else {
            Thread::sleep(100);
            continue;
        }
    }
}

void LinuxNetLink::_processMessage(struct nlmsghdr *nlp, int nll)
{
    for(; NLMSG_OK(nlp, nll); nlp=NLMSG_NEXT(nlp, nll))
    {
        switch(nlp->nlmsg_type) 
        {
        case RTM_NEWLINK:
            _linkAdded(nlp);
            break;
        case RTM_DELLINK:
            _linkDeleted(nlp);
            break;
        case RTM_NEWADDR:
            _ipAddressAdded(nlp);
            break;
        case RTM_DELADDR:
            _ipAddressDeleted(nlp);
            break;
        case RTM_NEWROUTE:
            _routeAdded(nlp);
            break;
        case RTM_DELROUTE:
            _routeDeleted(nlp);
            break;
        default:
            fprintf(stderr, "ignore msgtype %d...\n", nlp->nlmsg_type);
        }
    }
}

void LinuxNetLink::_ipAddressAdded(struct nlmsghdr *nlp)
{
    struct ifaddrmsg *ifap = (struct ifaddrmsg *)NLMSG_DATA(nlp);
    struct rtattr *rtap = (struct rtattr *)IFA_RTA(ifap);
    int ifal = IFA_PAYLOAD(nlp);

    char addr[40] = {0};
    char local[40] = {0};
    char label[40] = {0};
    char bcast[40] = {0};
    
    for(;RTA_OK(rtap, ifal); rtap=RTA_NEXT(rtap,ifal))
    {
        switch(rtap->rta_type) {
        case IFA_ADDRESS:
            inet_ntop(ifap->ifa_family, RTA_DATA(rtap), addr, 40);
            break;
        case IFA_LOCAL:
            inet_ntop(ifap->ifa_family, RTA_DATA(rtap), local, 40);
            break;
        case IFA_LABEL:
            memcpy(label, RTA_DATA(rtap), 40);
            break;
        case IFA_BROADCAST:
            inet_ntop(ifap->ifa_family, RTA_DATA(rtap), bcast, 40);
            break;
        }
    }

    fprintf(stderr, "Added IP Address %s local: %s label: %s broadcast: %s\n", addr, local, label, bcast);
}

void LinuxNetLink::_ipAddressDeleted(struct nlmsghdr *nlp)
{
    struct ifaddrmsg *ifap = (struct ifaddrmsg *)NLMSG_DATA(nlp);
    struct rtattr *rtap = (struct rtattr *)IFA_RTA(ifap);
    int ifal = IFA_PAYLOAD(nlp);

    char addr[40] = {0};
    char local[40] = {0};
    char label[40] = {0};
    char bcast[40] = {0};
    
    for(;RTA_OK(rtap, ifal); rtap=RTA_NEXT(rtap,ifal))
    {
        switch(rtap->rta_type) {
        case IFA_ADDRESS:
            inet_ntop(ifap->ifa_family, RTA_DATA(rtap), addr, 40);
            break;
        case IFA_LOCAL:
            inet_ntop(ifap->ifa_family, RTA_DATA(rtap), local, 40);
            break;
        case IFA_LABEL:
            memcpy(label, RTA_DATA(rtap), 40);
            break;
        case IFA_BROADCAST:
            inet_ntop(ifap->ifa_family, RTA_DATA(rtap), bcast, 40);
            break;
        }
    }

    fprintf(stderr, "Removed IP Address %s local: %s label: %s broadcast: %s\n", addr, local, label, bcast);
}

void LinuxNetLink::_routeAdded(struct nlmsghdr *nlp)
{
    char dsts[40] = {0};
    char gws[40] = {0};
    char ifs[16] = {0};
    char ms[24] = {0};

    struct rtmsg *rtp = (struct rtmsg *) NLMSG_DATA(nlp);
    struct rtattr *rtap = (struct rtattr *)RTM_RTA(rtp);
    int rtl = RTM_PAYLOAD(nlp);

    for(;RTA_OK(rtap, rtl); rtap=RTA_NEXT(rtap, rtl))
    {
        switch(rtap->rta_type)
        {
        case RTA_DST:
            inet_ntop(rtp->rtm_family, RTA_DATA(rtap), dsts, rtp->rtm_family == AF_INET ? 24 : 40);
            break;
        case RTA_GATEWAY:
            inet_ntop(rtp->rtm_family, RTA_DATA(rtap), gws, rtp->rtm_family == AF_INET ? 24 : 40);
            break;
        case RTA_OIF:
            sprintf(ifs, "%d", *((int*)RTA_DATA(rtap)));
            break;
        }
    }
    sprintf(ms, "%d", rtp->rtm_dst_len);

    fprintf(stderr, "Route Added: dst %s/%s gw %s if %s\n", dsts, ms, gws, ifs);
}

void LinuxNetLink::_routeDeleted(struct nlmsghdr *nlp)
{
    char dsts[40] = {0};
    char gws[40] = {0};
    char ifs[16] = {0};
    char ms[24] = {0};

    struct rtmsg *rtp = (struct rtmsg *) NLMSG_DATA(nlp);
    struct rtattr *rtap = (struct rtattr *)RTM_RTA(rtp);
    int rtl = RTM_PAYLOAD(nlp);

    for(;RTA_OK(rtap, rtl); rtap=RTA_NEXT(rtap, rtl))
    {
        switch(rtap->rta_type)
        {
        case RTA_DST:
            inet_ntop(rtp->rtm_family, RTA_DATA(rtap), dsts, rtp->rtm_family == AF_INET ? 24 : 40);
            break;
        case RTA_GATEWAY:
            inet_ntop(rtp->rtm_family, RTA_DATA(rtap), gws, rtp->rtm_family == AF_INET ? 24 : 40);
            break;
        case RTA_OIF:
            sprintf(ifs, "%d", *((int*)RTA_DATA(rtap)));
            break;
        }
    }
    sprintf(ms, "%d", rtp->rtm_dst_len);

    fprintf(stderr, "Route Deleted: dst %s/%s gw %s if %s\n", dsts, ms, gws, ifs);
}

void LinuxNetLink::_linkAdded(struct nlmsghdr *nlp)
{
    char mac[20] = {0};
    unsigned int mtu = 0;
    char ifname[40] = {0};

    struct ifinfomsg *ifip = (struct ifinfomsg *)NLMSG_DATA(nlp);
    struct rtattr *rtap = (struct rtattr *)IFLA_RTA(ifip);
    int ifil = RTM_PAYLOAD(nlp);

    const char *ptr;
    unsigned char *ptr2;
    for(;RTA_OK(rtap, ifil);rtap=RTA_NEXT(rtap, ifil))
    {
        switch(rtap->rta_type) {
        case IFLA_ADDRESS:
            ptr2 = (unsigned char*)RTA_DATA(rtap);
            snprintf(mac, 20, "%02x:%02x:%02x:%02x:%02x:%02x",
                ptr2[0], ptr2[1], ptr2[2], ptr2[3], ptr2[4], ptr2[5]);
            break;
        case IFLA_IFNAME:
            ptr = (const char*)RTA_DATA(rtap);
            memcpy(ifname, ptr, strlen(ptr));
            break;
        case IFLA_MTU:
            memcpy(&mtu, RTA_DATA(rtap), sizeof(unsigned int));
            break;
        }
    }

    fprintf(stderr, "Link Added: %s mac: %s, mtu: %d\n", ifname, mac, mtu);
}

void LinuxNetLink::_linkDeleted(struct nlmsghdr *nlp)
{
    char mac[20] = {0};
    unsigned int mtu = 0;
    char ifname[40] = {0};

    struct ifinfomsg *ifip = (struct ifinfomsg *)NLMSG_DATA(nlp);
    struct rtattr *rtap = (struct rtattr *)IFLA_RTA(ifip);
    int ifil = RTM_PAYLOAD(nlp);

    const char *ptr;
    unsigned char *ptr2;
    for(;RTA_OK(rtap, ifil);rtap=RTA_NEXT(rtap, ifil))
    {
        switch(rtap->rta_type) {
        case IFLA_ADDRESS:
            ptr2 = (unsigned char*)RTA_DATA(rtap);
            snprintf(mac, 20, "%02x:%02x:%02x:%02x:%02x:%02x",
                ptr2[0], ptr2[1], ptr2[2], ptr2[3], ptr2[4], ptr2[5]);
            break;
        case IFLA_IFNAME:
            ptr = (const char*)RTA_DATA(rtap);
            memcpy(ifname, ptr, strlen(ptr));
            break;
        case IFLA_MTU:
            memcpy(&mtu, RTA_DATA(rtap), sizeof(unsigned int));
            break;
        }
    }

    fprintf(stderr, "Link Deleted: %s mac: %s, mtu: %d\n", ifname, mac, mtu);
}

void LinuxNetLink::_requestIPv4Routes()
{
    struct nl_req req;
    bzero(&req, sizeof(req));
    req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.nl.nlmsg_type = RTM_GETROUTE;
    req.nl.nlmsg_pid = 0;
    req.nl.nlmsg_seq = ++_seq;
    req.rt.rtm_family = AF_INET;
    req.rt.rtm_table = RT_TABLE_MAIN;

    struct sockaddr_nl pa;
    bzero(&pa, sizeof(pa));
    pa.nl_family = AF_NETLINK;

    struct msghdr msg;
    bzero(&msg, sizeof(msg));
    msg.msg_name = (void*)&pa;
    msg.msg_namelen = sizeof(pa);

    struct iovec iov;
    bzero(&iov, sizeof(iov));
    iov.iov_base = (void*)&req.nl;
    iov.iov_len = req.nl.nlmsg_len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    sendmsg(_fd, &msg, 0);
}

void LinuxNetLink::_requestIPv6Routes()
{
    struct nl_req req;
    bzero(&req, sizeof(req));
    req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.nl.nlmsg_type = RTM_GETROUTE;
    req.nl.nlmsg_pid = 0;
    req.nl.nlmsg_seq = ++_seq;
    req.rt.rtm_family = AF_INET6;
    req.rt.rtm_table = RT_TABLE_MAIN;

    struct sockaddr_nl pa;
    bzero(&pa, sizeof(pa));
    pa.nl_family = AF_NETLINK;

    struct msghdr msg;
    bzero(&msg, sizeof(msg));
    msg.msg_name = (void*)&pa;
    msg.msg_namelen = sizeof(pa);

    struct iovec iov;
    bzero(&iov, sizeof(iov));
    iov.iov_base = (void*)&req.nl;
    iov.iov_len = req.nl.nlmsg_len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    while((sendmsg(_fd, &msg, 0)) == -1) {
        fprintf(stderr, "ipv6 waiting...");
        Thread::sleep(100);
    }
}

void LinuxNetLink::addRoute(const InetAddress &target, const InetAddress &via, const char *ifaceName)
{

}

void LinuxNetLink::delRoute(const InetAddress &target, const InetAddress &via, const char *ifaceName)
{

}

void LinuxNetLink::addInterface(const char *iface, unsigned int mtu)
{

}

void LinuxNetLink::addAddress(const InetAddress &addr, const char *iface)
{

}

RouteList LinuxNetLink::getIPV4Routes() const 
{
    return _routes_ipv4;
}

RouteList LinuxNetLink::getIPV6Routes() const
{
    return _routes_ipv6;
}

} // namespace ZeroTier