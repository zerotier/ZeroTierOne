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
    , _fd(socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE))
    , _la({0})
    , _pa({0})
    , _msg({0})
    , _iov({0})
    , _rtn(0)
    , _nlp(NULL)
    , _nll(0)
    , _rtp(NULL)
    , _rtl(0)
    , _rtap(NULL)
    , _ifip(NULL)
    , _ifil(0)
    , _ifap(NULL)
    , _ifal(0)
{
    memset(_buf, 0, sizeof(_buf));

    // set socket timeout to 1 sec so we're not permablocking recv() calls
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if(setsockopt(_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) != 0) {
        fprintf(stderr, "setsockopt failed: %s\n", strerror(errno));
    }

    _la.nl_family = AF_NETLINK;
    _la.nl_pid = getpid();
    _la.nl_groups = RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV6_IFADDR|RTMGRP_IPV4_ROUTE|RTMGRP_IPV6_ROUTE;
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
    char *p;
    p = _buf;
    _nll = 0;

    while(_running) {
        _rtn = recv(_fd, p, sizeof(_buf) - _nll, 0);

        if (_rtn > 0) {
            _nlp = (struct nlmsghdr *) p;

            if(_nlp->nlmsg_type == NLMSG_ERROR && (_nlp->nlmsg_flags & NLM_F_ACK) != NLM_F_ACK) {
                fprintf(stderr, "NLMSG_ERROR\n");
                struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(_nlp);
                if (err->error != 0) {
                    fprintf(stderr, "rtnetlink error: %s\n", strerror(-(err->error)));
                }
                p = _buf;
                _nll = 0;
                continue;
            }

            if (_nlp->nlmsg_type == NLMSG_NOOP) {
                fprintf(stderr, "noop\n");
                continue;
            }

            if( (_nlp->nlmsg_flags & NLM_F_MULTI) == NLM_F_MULTI || (_nlp->nlmsg_type == NLMSG_DONE))
            {
                if (_nlp->nlmsg_type == NLMSG_DONE) {
                    _processMessage();
                    p = _buf;
                    _nll = 0;
                    continue;
                }
                p += _rtn;
                _nll += _rtn;
            }

            if (_nlp->nlmsg_type == NLMSG_OVERRUN) {
                fprintf(stderr, "NLMSG_OVERRUN: Data lost\n");
                p = _buf;
                _nll = 0;
                continue;
            }
            
            _nll += _rtn;

            _processMessage();
            p = _buf;
            _nll = 0;
        }
        else {
            Thread::sleep(100);
            continue;
        }
    }
}

void LinuxNetLink::_processMessage()
{
    for(_nlp = (struct nlmsghdr *)_buf; NLMSG_OK(_nlp, _nll); _nlp=NLMSG_NEXT(_nlp, _nll))
    {
        switch(_nlp->nlmsg_type) 
        {
        case RTM_NEWLINK:
            _linkAdded();
            break;
        case RTM_DELLINK:
            _linkDeleted();
            break;
        case RTM_GETLINK:
            fprintf(stderr, "Get Link\n");
            break;
        case RTM_SETLINK:
            fprintf(stderr, "Set Link\n");
            break;
        case RTM_NEWADDR:
            _ipAddressAdded();
            break;
        case RTM_DELADDR:
            _ipAddressDeleted();
            break;
        case RTM_GETADDR:
            fprintf(stderr, "Get IP Address\n");
            break;
        case RTM_NEWROUTE:
            _routeAdded();
            break;
        case RTM_DELROUTE:
            _routeDeleted();
            break;
        case RTM_GETROUTE:
            break;
        default:
            fprintf(stderr, "ignore msgtype %d...\n", _nlp->nlmsg_type);
        }
    }
    _nlp = NULL;
    _nll = 0;
    _rtp = NULL;
    _rtl = 0;
    _ifip = NULL;
    _ifil = 0;
    _ifap = NULL;
    _ifal = 0;
}

void LinuxNetLink::_ipAddressAdded()
{
    _ifap = (struct ifaddrmsg *)NLMSG_DATA(_nlp);
    _rtap = (struct rtattr *)IFA_RTA(_ifap);
    _ifal = IFA_PAYLOAD(_nlp);

    char addr[40] = {0};
    char local[40] = {0};
    char label[40] = {0};
    char bcast[40] = {0};
    
    for(;RTA_OK(_rtap, _ifal); _rtap=RTA_NEXT(_rtap,_ifal))
    {
        switch(_rtap->rta_type) {
        case IFA_ADDRESS:
            inet_ntop(_ifap->ifa_family, RTA_DATA(_rtap), addr, 40);
            break;
        case IFA_LOCAL:
            inet_ntop(_ifap->ifa_family, RTA_DATA(_rtap), local, 40);
            break;
        case IFA_LABEL:
            memcpy(label, RTA_DATA(_rtap), 40);
            break;
        case IFA_BROADCAST:
            inet_ntop(_ifap->ifa_family, RTA_DATA(_rtap), bcast, 40);
            break;
        }
    }

    fprintf(stderr, "Added IP Address %s local: %s label: %s broadcast: %s\n", addr, local, label, bcast);
}

void LinuxNetLink::_ipAddressDeleted()
{
    _ifap = (struct ifaddrmsg *)NLMSG_DATA(_nlp);
    _rtap = (struct rtattr *)IFA_RTA(_ifap);
    _ifal = IFA_PAYLOAD(_nlp);

    char addr[40] = {0};
    char local[40] = {0};
    char label[40] = {0};
    char bcast[40] = {0};
    
    for(;RTA_OK(_rtap, _ifal); _rtap=RTA_NEXT(_rtap,_ifal))
    {
        switch(_rtap->rta_type) {
        case IFA_ADDRESS:
            inet_ntop(_ifap->ifa_family, RTA_DATA(_rtap), addr, 40);
            break;
        case IFA_LOCAL:
            inet_ntop(_ifap->ifa_family, RTA_DATA(_rtap), local, 40);
            break;
        case IFA_LABEL:
            memcpy(label, RTA_DATA(_rtap), 40);
            break;
        case IFA_BROADCAST:
            inet_ntop(_ifap->ifa_family, RTA_DATA(_rtap), bcast, 40);
            break;
        }
    }

    fprintf(stderr, "Removed IP Address %s local: %s label: %s broadcast: %s\n", addr, local, label, bcast);
}

void LinuxNetLink::_routeAdded()
{
    char dsts[40] = {0};
    char gws[40] = {0};
    char ifs[16] = {0};
    char ms[24] = {0};

    _rtp = (struct rtmsg *) NLMSG_DATA(_nlp);

    _rtap = (struct rtattr *)RTM_RTA(_rtp);
    _rtl = RTM_PAYLOAD(_nlp);
    for(;RTA_OK(_rtap, _rtl); _rtap=RTA_NEXT(_rtap, _rtl))
    {
        switch(_rtap->rta_type)
        {
        case RTA_DST:
            inet_ntop(_rtp->rtm_family, RTA_DATA(_rtap), dsts, _rtp->rtm_family == AF_INET ? 24 : 40);
            break;
        case RTA_GATEWAY:
            inet_ntop(_rtp->rtm_family, RTA_DATA(_rtap), gws, _rtp->rtm_family == AF_INET ? 24 : 40);
            break;
        case RTA_OIF:
            sprintf(ifs, "%d", *((int*)RTA_DATA(_rtap)));
            break;
        }
    }
    sprintf(ms, "%d", _rtp->rtm_dst_len);

    fprintf(stderr, "Route Added: dst %s/%s gw %s if %s\n", dsts, ms, gws, ifs);
}

void LinuxNetLink::_routeDeleted()
{
    char dsts[40] = {0};
    char gws[40] = {0};
    char ifs[16] = {0};
    char ms[24] = {0};

    _rtp = (struct rtmsg *) NLMSG_DATA(_nlp);

    _rtap = (struct rtattr *)RTM_RTA(_rtp);
    _rtl = RTM_PAYLOAD(_nlp);
    for(;RTA_OK(_rtap, _rtl); _rtap=RTA_NEXT(_rtap, _rtl))
    {
        switch(_rtap->rta_type)
        {
        case RTA_DST:
            inet_ntop(_rtp->rtm_family, RTA_DATA(_rtap), dsts, _rtp->rtm_family == AF_INET ? 24 : 40);
            break;
        case RTA_GATEWAY:
            inet_ntop(_rtp->rtm_family, RTA_DATA(_rtap), gws, _rtp->rtm_family == AF_INET ? 24 : 40);
            break;
        case RTA_OIF:
            sprintf(ifs, "%d", *((int*)RTA_DATA(_rtap)));
            break;
        }
    }
    sprintf(ms, "%d", _rtp->rtm_dst_len);

    fprintf(stderr, "Route Deleted: dst %s/%s gw %s if %s\n", dsts, ms, gws, ifs);
}

void LinuxNetLink::_linkAdded()
{
    char mac[20] = {0};
    unsigned int mtu = 0;
    char ifname[40] = {0};

    _ifip = (struct ifinfomsg *)NLMSG_DATA(_nlp);
    _rtap = (struct rtattr *)IFLA_RTA(_ifip);
    _ifil = RTM_PAYLOAD(_nlp);

    const char *ptr;
    unsigned char *ptr2;
    for(;RTA_OK(_rtap, _ifil);_rtap=RTA_NEXT(_rtap, _ifil))
    {
        switch(_rtap->rta_type) {
        case IFLA_ADDRESS:
            ptr2 = (unsigned char*)RTA_DATA(_rtap);
            snprintf(mac, 20, "%02x:%02x:%02x:%02x:%02x:%02x",
                ptr2[0], ptr2[1], ptr2[2], ptr2[3], ptr2[4], ptr2[5]);
            break;
        case IFLA_IFNAME:
            ptr = (const char*)RTA_DATA(_rtap);
            memcpy(ifname, ptr, strlen(ptr));
            break;
        case IFLA_MTU:
            memcpy(&mtu, RTA_DATA(_rtap), sizeof(unsigned int));
            break;
        }
    }

    fprintf(stderr, "Link Added: %s mac: %s, mtu: %d\n", ifname, mac, mtu);
}

void LinuxNetLink::_linkDeleted()
{
    char mac[20] = {0};
    unsigned int mtu = 0;
    char ifname[40] = {0};

    _ifip = (struct ifinfomsg *)NLMSG_DATA(_nlp);
    _rtap = (struct rtattr *)IFLA_RTA(_ifip);
    _ifil = RTM_PAYLOAD(_nlp);

    const char *ptr;
    unsigned char *ptr2;
    for(;RTA_OK(_rtap, _ifil);_rtap=RTA_NEXT(_rtap, _ifil))
    {
        switch(_rtap->rta_type) {
        case IFLA_ADDRESS:
            ptr2 = (unsigned char*)RTA_DATA(_rtap);
            snprintf(mac, 20, "%02x:%02x:%02x:%02x:%02x:%02x",
                ptr2[0], ptr2[1], ptr2[2], ptr2[3], ptr2[4], ptr2[5]);
            break;
        case IFLA_IFNAME:
            ptr = (const char*)RTA_DATA(_rtap);
            memcpy(ifname, ptr, strlen(ptr));
            break;
        case IFLA_MTU:
            memcpy(&mtu, RTA_DATA(_rtap), sizeof(unsigned int));
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
    req.rt.rtm_family = AF_INET;
    req.rt.rtm_table = RT_TABLE_MAIN;


    bzero(&_pa, sizeof(_pa));
    _pa.nl_family = AF_NETLINK;

    bzero(&_msg, sizeof(_msg));
    _msg.msg_name = (void*)&_pa;
    _msg.msg_namelen = sizeof(_pa);

    _iov.iov_base = (void*)&req.nl;
    _iov.iov_len = req.nl.nlmsg_len;
    _msg.msg_iov = &_iov;
    _msg.msg_iovlen = 1;

    _rtn = sendmsg(_fd, &_msg, 0);
}

void LinuxNetLink::_requestIPv6Routes()
{
    struct nl_req req;
    bzero(&req, sizeof(req));
    req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.nl.nlmsg_type = RTM_GETROUTE;
    req.rt.rtm_family = AF_INET6;
    req.rt.rtm_table = RT_TABLE_MAIN;


    bzero(&_pa, sizeof(_pa));
    _pa.nl_family = AF_NETLINK;

    bzero(&_msg, sizeof(_msg));
    _msg.msg_name = (void*)&_pa;
    _msg.msg_namelen = sizeof(_pa);

    _iov.iov_base = (void*)&req.nl;
    _iov.iov_len = req.nl.nlmsg_len;
    _msg.msg_iov = &_iov;
    _msg.msg_iovlen = 1;

    while((_rtn = sendmsg(_fd, &_msg, 0)) == -1) {
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

void LinuxNetLink::addInterface(const char *iface)
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