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
#include <linux/if_tun.h>


namespace ZeroTier {

struct nl_route_req {
    struct nlmsghdr nl;
    struct rtmsg rt;
    char buf[8192];
};

struct nl_if_req {
    struct nlmsghdr nl;
    struct ifinfomsg ifa;
    char buf[8192];
};

struct nl_adr_req {
	struct nlmsghdr nl;
	struct ifaddrmsg ifa;
	char buf[8192];
};

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
	Thread::sleep(10);
	fprintf(stderr, "Requesting Interface List\n");
	_requestInterfaceList();
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
	char mac[18] = {0};
	char mac_bin[6] = {0};
	unsigned int mtu = 0;
	char ifname[IFNAMSIZ] = {0};

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
			memcpy(mac_bin, ptr, 6);
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

	struct iface_entry &entry = _interfaces[ifip->ifi_index];
	entry.index = ifip->ifi_index;
	memcpy(entry.ifacename, ifname, sizeof(ifname));
	memcpy(entry.mac, mac, sizeof(mac));
	memcpy(entry.mac_bin, mac_bin, 6);
	entry.mtu = mtu;

	fprintf(stderr, "Link Added: %s mac: %s, mtu: %d\n", ifname, mac, mtu);
}

void LinuxNetLink::_linkDeleted(struct nlmsghdr *nlp)
{
	char mac[18] = {0};
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
	if(_interfaces.contains(ifip->ifi_index)) {
		_interfaces.erase(ifip->ifi_index);
	}
}

void LinuxNetLink::_requestIPv4Routes()
{
	struct nl_route_req req;
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
	struct nl_route_req req;
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

	sendmsg(_fd, &msg, 0);
}

void LinuxNetLink::_requestInterfaceList()
{
	struct nl_if_req req;
	bzero(&req, sizeof(req));
	req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req.nl.nlmsg_type = RTM_GETLINK;
	req.nl.nlmsg_pid = 0;
	req.nl.nlmsg_seq = ++_seq;
	req.ifa.ifi_family = AF_UNSPEC;

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

void LinuxNetLink::addRoute(const InetAddress &target, const InetAddress &via, const char *ifaceName)
{
	int rtl = sizeof(struct rtmsg);
	struct nl_route_req req;
	bzero(&req, sizeof(req));

	struct rtattr *rtap = (struct rtattr *)req.buf;
	rtap->rta_type = RTA_DST;
	if (target.isV4()) {
		rtap->rta_len = sizeof(struct rtattr)+sizeof(struct in_addr);
		memcpy((void*)((char*)rtap+sizeof(struct rtattr)), &((struct sockaddr_in*)&target)->sin_addr, sizeof(struct in_addr));
	} else {
		rtap->rta_len = sizeof(struct rtattr)+sizeof(struct in6_addr);
		memcpy((void*)((char*)rtap+sizeof(struct rtattr)), &((struct sockaddr_in6*)&target)->sin6_addr, sizeof(struct in6_addr));
	}
	rtl += rtap->rta_len;

	int interface_index = -1;
	if (ifaceName != NULL) {
		Hashtable<int, iface_entry>::Iterator iter(_interfaces);
		int *k = NULL;
		iface_entry *v = NULL;
		while(iter.next(k, v)) {
			if(strcmp(ifaceName, v->ifacename) == 0) {
				interface_index = v->index;
				break;
			}
		}
		if (interface_index != -1) {
			rtap = (struct rtattr *) (((char*)rtap) + rtap->rta_len);
			rtap->rta_type = RTA_OIF;
			rtap->rta_len = sizeof(struct rtattr)+sizeof(int);
			memcpy(((char*)rtap)+sizeof(rtattr), &interface_index, sizeof(int));
			rtl += rtap->rta_len;
		}
	}

	if(via) {
		rtap = (struct rtattr *)(((char*)rtap)+rtap->rta_len);
		rtap->rta_type = RTA_GATEWAY;
		if(via.isV4()) {
			rtap->rta_len = sizeof(struct rtattr)+sizeof(struct in_addr);
			memcpy((char*)rtap+sizeof(struct rtattr), &((struct sockaddr_in*)&via)->sin_addr, sizeof(struct in_addr));
		} else {
			rtap->rta_len = sizeof(struct rtattr)+sizeof(struct in6_addr);
			memcpy((char*)rtap+sizeof(struct rtattr), &((struct sockaddr_in6*)&via)->sin6_addr, sizeof(struct in6_addr));
		}
		rtl += rtap->rta_len;
	}

	req.nl.nlmsg_len = NLMSG_LENGTH(rtl);
	req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
	req.nl.nlmsg_type = RTM_NEWROUTE;
	req.nl.nlmsg_pid = 0;
	req.nl.nlmsg_seq = ++_seq;
	req.rt.rtm_family = target.ss_family;
	req.rt.rtm_table = RT_TABLE_MAIN;
	req.rt.rtm_protocol = RTPROT_STATIC;
	req.rt.rtm_scope = RT_SCOPE_UNIVERSE;
	req.rt.rtm_type = RTN_UNICAST;
	req.rt.rtm_dst_len = target.netmaskBits();

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

void LinuxNetLink::delRoute(const InetAddress &target, const InetAddress &via, const char *ifaceName)
{
	int rtl = sizeof(struct rtmsg);
	struct nl_route_req req;
	bzero(&req, sizeof(req));

	struct rtattr *rtap = (struct rtattr *)req.buf;
	rtap->rta_type = RTA_DST;
	if (target.isV4()) {
		rtap->rta_len = sizeof(struct rtattr)+sizeof(struct in_addr);
		memcpy((void*)((char*)rtap+sizeof(struct rtattr)), &((struct sockaddr_in*)&target)->sin_addr, sizeof(struct in_addr));
	} else {
		rtap->rta_len = sizeof(struct rtattr)+sizeof(struct in6_addr);
		memcpy((void*)((char*)rtap+sizeof(struct rtattr)), &((struct sockaddr_in6*)&target)->sin6_addr, sizeof(struct in6_addr));
	}
	rtl += rtap->rta_len;

	int interface_index = -1;
	if (ifaceName != NULL) {
		Hashtable<int, iface_entry>::Iterator iter(_interfaces);
		int *k = NULL;
		iface_entry *v = NULL;
		while(iter.next(k, v)) {
			if(strcmp(ifaceName, v->ifacename) == 0) {
				interface_index = v->index;
				break;
			}
		}
		if (interface_index != -1) {
			rtap = (struct rtattr *) (((char*)rtap) + rtap->rta_len);
			rtap->rta_type = RTA_OIF;
			rtap->rta_len = sizeof(struct rtattr)+sizeof(int);
			memcpy(((char*)rtap)+sizeof(rtattr), &interface_index, sizeof(int));
			rtl += rtap->rta_len;
		}
	}

	if(via) {
		rtap = (struct rtattr *)(((char*)rtap)+rtap->rta_len);
		rtap->rta_type = RTA_GATEWAY;
		if(via.isV4()) {
			rtap->rta_len = sizeof(struct rtattr)+sizeof(struct in_addr);
			memcpy((char*)rtap+sizeof(struct rtattr), &((struct sockaddr_in*)&via)->sin_addr, sizeof(struct in_addr));
		} else {
			rtap->rta_len = sizeof(struct rtattr)+sizeof(struct in6_addr);
			memcpy((char*)rtap+sizeof(struct rtattr), &((struct sockaddr_in6*)&via)->sin6_addr, sizeof(struct in6_addr));
		}
		rtl += rtap->rta_len;
	}

	req.nl.nlmsg_len = NLMSG_LENGTH(rtl);
	req.nl.nlmsg_flags = NLM_F_REQUEST;
	req.nl.nlmsg_type = RTM_DELROUTE;
	req.nl.nlmsg_pid = 0;
	req.nl.nlmsg_seq = ++_seq;
	req.rt.rtm_family = target.ss_family;
	req.rt.rtm_table = RT_TABLE_MAIN;
	req.rt.rtm_protocol = RTPROT_STATIC;
	req.rt.rtm_scope = RT_SCOPE_UNIVERSE;
	req.rt.rtm_type = RTN_UNICAST;
	req.rt.rtm_dst_len = target.netmaskBits();

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

// void LinuxNetLink::addInterface(const char *iface, unsigned int mtu, const MAC &mac)
// {
// 	int rtl = sizeof(struct ifinfomsg);
// 	struct nl_if_req req;
// 	bzero(&req, sizeof(nl_if_req));

// 	struct rtattr *rtap = (struct rtattr *)req.buf;
// 	rtap->rta_type = IFLA_IFNAME;
// 	rtap->rta_len =  sizeof(struct rtattr)+strlen(iface)+1;
// 	rtl += rtap->rta_len;

// 	rtap = (struct rtattr*)(((char*)rtap)+rtap->rta_len);
// 	rtap->rta_type = IFLA_MTU;
// 	rtap->rta_len = sizeof(struct rtattr)+sizeof(unsigned int);
// 	rtl += rtap->rta_len;

// 	rtap = (struct rtattr*)(((char*)rtap)+rtap->rta_len);
// 	rtap->rta_type = IFLA_ADDRESS;
// 	rtap->rta_len = sizeof(struct rtattr)+6;
// 	mac.copyTo(((char*)rtap)+sizeof(struct rtattr), 6);
// 	rtl += rtap->rta_len;

// 	IFLA_LINKINFO;
// 	req.nl.nlmsg_len = NLMSG_LENGTH(rtl);
// 	req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
// 	req.nl.nlmsg_type = RTM_NEWLINK;
// 	req.nl.nlmsg_pid = 0;
// 	req.nl.nlmsg_seq = ++_seq;

// 	req.ifa.ifi_family = AF_UNSPEC;
// 	req.ifa.ifi_type = 0; // TODO figure this one out
// 	req.ifa.ifi_index = 0;
// 	req.ifa.ifi_flags = IFF_UP;

// 	struct sockaddr_nl pa;
// 	bzero(&pa, sizeof(pa));
// 	pa.nl_family = AF_NETLINK;

// 	struct msghdr msg;
// 	bzero(&msg, sizeof(msg));
// 	msg.msg_name = (void*)&pa;
// 	msg.msg_namelen = sizeof(pa);

// 	struct iovec iov;
// 	iov.iov_base = (void*)&req.nl;
// 	iov.iov_len = req.nl.nlmsg_len;
// 	msg.msg_iov = &iov;
// 	msg.msg_iovlen = 1;
// 	sendmsg(_fd, &msg, 0);
// }

// void LinuxNetLink::removeInterface(const char *iface)
// {

// }

void LinuxNetLink::addAddress(const InetAddress &addr, const char *iface)
{

}

void LinuxNetLink::removeAddress(const InetAddress &addr, const char *iface)
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