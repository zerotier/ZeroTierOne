/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by vergnn 2.0 of the Apache License.
 */
/****/

#include "GoNode.h"

#include "../../node/Constants.hpp"
#include "../../node/InetAddress.hpp"
#include "../../node/Node.hpp"
#include "../../node/Utils.hpp"
#include "../../osdep/OSUtils.hpp"
#include "../../osdep/BlockingQueue.hpp"
#include "../../osdep/EthernetTap.hpp"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifndef __WINDOWS__
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#ifdef __BSD__
#include <net/if.h>
#endif
#ifdef __LINUX__
#ifndef IPV6_DONTFRAG
#define IPV6_DONTFRAG 62
#endif
#endif
#endif // !__WINDOWS__

#include <thread>
#include <mutex>
#include <map>
#include <vector>
#include <array>
#include <set>

#ifdef __WINDOWS__
#define SETSOCKOPT_FLAG_TYPE BOOL
#define SETSOCKOPT_FLAG_TRUE TRUE
#define SETSOCKOPT_FLAG_FALSE FALSE
#else
#define SETSOCKOPT_FLAG_TYPE int
#define SETSOCKOPT_FLAG_TRUE 1
#define SETSOCKOPT_FLAG_FALSE 0
#endif

#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

using namespace ZeroTier;

struct ZT_GoNodeThread
{
	std::string ip;
	int port;
	int af;
	std::atomic_bool run;
	std::thread thr;
};

struct ZT_GoNode_Impl
{
	Node *node;
	volatile int64_t nextBackgroundTaskDeadline;

	int (*goPathCheckFunc)(ZT_GoNode *,ZT_Node *,uint64_t ztAddress,const void *);
	int (*goPathLookupFunc)(ZT_GoNode *,ZT_Node *,int desiredAddressFamily,void *);
	int (*goStateObjectGetFunc)(ZT_GoNode *,ZT_Node *,int objType,const uint64_t id[2],void *buf,unsigned int bufSize);

	std::map< ZT_SOCKET,ZT_GoNodeThread > threads;
	std::mutex threads_l;

	BlockingQueue<ZT_GoNodeEvent> eq;
};

//////////////////////////////////////////////////////////////////////////////

static int ZT_GoNode_VirtualNetworkConfigFunction(
	ZT_Node *node,
	void *uptr,
	void *tptr,
	uint64_t nwid,
	void **nptr,
	enum ZT_VirtualNetworkConfigOperation op,
	const ZT_VirtualNetworkConfig *cfg)
{
	ZT_GoNodeEvent ev;
	ev.type = ZT_GONODE_EVENT_NETWORK_CONFIG_UPDATE;
	ev.data.nconf.op = op;
	if (cfg)
		ev.data.nconf.conf = *cfg;
	reinterpret_cast<ZT_GoNode *>(uptr)->eq.post(ev);
}

static void ZT_GoNode_VirtualNetworkFrameFunction(
	ZT_Node *node,
	void *uptr,
	void *tptr,
	uint64_t nwid,
	void **nptr,
	uint64_t srcMac,
	uint64_t destMac,
	unsigned int etherType,
	unsigned int vlanId,
	const void *data,
	unsigned int len)
{
	if (*nptr)
		reinterpret_cast<EthernetTap *>(*nptr)->put(MAC(srcMac),MAC(destMac),etherType,data,len);
}

static void ZT_GoNode_EventCallback(
	ZT_Node *node,
	void *uptr,
	void *tptr,
	enum ZT_Event et,
	const void *data)
{
	ZT_GoNodeEvent ev;
	ev.type = ZT_GONODE_EVENT_ZTEVENT;
	ev.data.zt.type = et;
	reinterpret_cast<ZT_GoNode *>(uptr)->eq.post(ev);
}

static void ZT_GoNode_StatePutFunction(
	ZT_Node *node,
	void *uptr,
	void *tptr,
	enum ZT_StateObjectType objType,
	const uint64_t id[2],
	const void *data,
	int len)
{
	if (len < ZT_MAX_STATE_OBJECT_SIZE) { // sanity check
		ZT_GoNodeEvent ev;
		ev.type = (len >= 0) ? ZT_GONODE_EVENT_STATE_PUT : ZT_GONODE_EVENT_STATE_DELETE;
		if (len > 0) {
			memcpy(ev.data.sobj.data,data,len);
			ev.data.sobj.len = (unsigned int)len;
		}
		ev.data.sobj.objType = objType;
		ev.data.sobj.id[0] = id[0];
		ev.data.sobj.id[1] = id[1];
		reinterpret_cast<ZT_GoNode *>(uptr)->eq.post(ev);
	}
}

static int ZT_GoNode_StateGetFunction(
	ZT_Node *node,
	void *uptr,
	void *tptr,
	enum ZT_StateObjectType objType,
	const uint64_t id[2],
	void *buf,
	unsigned int buflen)
{
	return reinterpret_cast<ZT_GoNode *>(uptr)->goStateObjectGetFunc(reinterpret_cast<ZT_GoNode *>(uptr),reinterpret_cast<ZT_GoNode *>(uptr)->node,(int)objType,id,buf,buflen);
}

static ZT_ALWAYS_INLINE void doUdpSend(ZT_SOCKET sock,const struct sockaddr_storage *addr,const void *data,const unsigned int len,const unsigned int ipTTL)
{
	switch(addr->ss_family) {
		case AF_INET:
			if ((ipTTL > 0)&&(ipTTL < 255)) {
#ifdef __WINDOWS__
				DWORD tmp = (DWORD)ipTTL;
#else
				int tmp = (int)ipTTL;
#endif
				setsockopt(sock,IPPROTO_IP,IP_TTL,&tmp,sizeof(tmp));
				sendto(sock,data,len,MSG_DONTWAIT,(const sockaddr *)addr,sizeof(struct sockaddr_in));
				tmp = 255;
				setsockopt(sock,IPPROTO_IP,IP_TTL,&tmp,sizeof(tmp));
			} else {
				sendto(sock,data,len,MSG_DONTWAIT,(const sockaddr *)addr,sizeof(struct sockaddr_in));
			}
			break;
		case AF_INET6:
			// The ipTTL option isn't currently used with IPv6. It's only used
			// with IPv4 "firewall opener" / "NAT buster" preamble packets as part
			// of IPv4 NAT traversal.
			sendto(sock,data,len,MSG_DONTWAIT,(const sockaddr *)addr,sizeof(struct sockaddr_in6));
			break;
	}
}

static void ZT_GoNode_WirePacketSendFunction(
	ZT_Node *node,
	void *uptr,
	void *tptr,
	int64_t localSocket,
	const struct sockaddr_storage *addr,
	const void *data,
	unsigned int len,
	unsigned int ipTTL)
{
	if ((localSocket != -1)&&(localSocket != ZT_INVALID_SOCKET)) {
		doUdpSend((ZT_SOCKET)localSocket,addr,data,len,ipTTL);
	} else {
		ZT_GoNode *const gn = reinterpret_cast<ZT_GoNode *>(uptr);
		std::set<std::string> ipsSentFrom;
		std::lock_guard<std::mutex> l(gn->threads_l);
		for(auto t=gn->threads.begin();t!=gn->threads.end();++t) {
			if (t->second.af == addr->ss_family) {
				if (ipsSentFrom.insert(t->second.ip).second) {
					doUdpSend(t->first,addr,data,len,ipTTL);
				}
			}
		}
	}
}

static int ZT_GoNode_PathCheckFunction(
	ZT_Node *node,
	void *uptr,
	void *tptr,
	uint64_t ztAddress,
	int64_t localSocket,
	const struct sockaddr_storage *sa)
{
	return reinterpret_cast<ZT_GoNode *>(uptr)->goPathCheckFunc(reinterpret_cast<ZT_GoNode *>(uptr),reinterpret_cast<ZT_GoNode *>(uptr)->node,ztAddress,sa);
}

static int ZT_GoNode_PathLookupFunction(
	ZT_Node *node,
	void *uptr,
	void *tptr,
	uint64_t ztAddress,
	int desiredAddressFamily,
	struct sockaddr_storage *sa)
{
	return reinterpret_cast<ZT_GoNode *>(uptr)->goPathLookupFunc(reinterpret_cast<ZT_GoNode *>(uptr),reinterpret_cast<ZT_GoNode *>(uptr)->node,desiredAddressFamily,sa);
}

static void ZT_GoNode_DNSResolver(
	ZT_Node *node,
	void *uptr,
	void *tptr,
	const enum ZT_DNSRecordType *types,
	unsigned int numTypes,
	const char *name,
	uintptr_t requestId)
{
	ZT_GoNodeEvent ev;
	ev.type = ZT_GONODE_EVENT_DNS_GET_TXT;
	Utils::scopy(ev.data.dns.dnsName,sizeof(ev.data.dns.dnsName),name);
	reinterpret_cast<ZT_GoNode *>(uptr)->eq.post(ev);
}

//////////////////////////////////////////////////////////////////////////////

extern "C" ZT_GoNode *ZT_GoNode_new(
	int (*goPathCheckFunc)(ZT_GoNode *,ZT_Node *,uint64_t ztAddress,const void *),
	int (*goPathLookupFunc)(ZT_GoNode *,ZT_Node *,int desiredAddressFamily,void *),
	int (*goStateObjectGetFunc)(ZT_GoNode *,ZT_Node *,int objType,const uint64_t id[2],void *buf,unsigned int bufSize)
)
{
	try {
		struct ZT_Node_Callbacks cb;
		cb.virtualNetworkConfigFunction = &ZT_GoNode_VirtualNetworkConfigFunction;
		cb.virtualNetworkFrameFunction = &ZT_GoNode_VirtualNetworkFrameFunction;
		cb.eventCallback = &ZT_GoNode_EventCallback;
		cb.statePutFunction = &ZT_GoNode_StatePutFunction;
		cb.stateGetFunction = &ZT_GoNode_StateGetFunction;
		cb.pathCheckFunction = &ZT_GoNode_PathCheckFunction;
		cb.pathLookupFunction = &ZT_GoNode_PathLookupFunction;
		cb.dnsResolver = &ZT_GoNode_DNSResolver;

		ZT_GoNode_Impl *gn = new ZT_GoNode_Impl;
		const int64_t now = OSUtils::now();
		gn->node = new Node(reinterpret_cast<void *>(gn),nullptr,&cb,now);
		gn->nextBackgroundTaskDeadline = now;
		gn->goPathCheckFunc = goPathCheckFunc;
		gn->goPathLookupFunc = goPathLookupFunc;
		gn->goStateObjectGetFunc = goStateObjectGetFunc;
		return gn;
	} catch ( ... ) {
		fprintf(stderr,"FATAL: unable to create new instance of Node (out of memory?)" ZT_EOL_S);
		exit(1);
	}
}

extern "C" void ZT_GoNode_delete(ZT_GoNode *gn)
{
	ZT_GoNodeEvent sd;
	sd.type = ZT_GONODE_EVENT_SHUTDOWN;
	gn->eq.post(sd);

	std::vector<std::thread> th;
	gn->threads_l.lock();
	for(auto t=gn->threads.begin();t!=gn->threads.end();++t) {
		t->second.run = false;
		shutdown(t->first,SHUT_RDWR);
		close(t->first);
		th.emplace_back(t->second.thr);
	}
	gn->threads_l.unlock();
	for(auto t=th.begin();t!=th.end();++t)
		t->join();

	delete gn->node;
	delete gn;
}

extern "C" ZT_Node *ZT_GoNode_getNode(ZT_GoNode *gn)
{
	return gn->node;
}

// Sets flags and socket options common to both IPv4 and IPv6 UDP sockets
static void setCommonUdpSocketSettings(ZT_SOCKET udpSock,const char *dev)
{
	int bufSize = 1048576;
	while (bufSize > 131072) {
		if (setsockopt(udpSock,SOL_SOCKET,SO_RCVBUF,(const char *)&bufSize,sizeof(bufSize)) == 0)
			break;
		bufSize -= 131072;
	}
	bufSize = 1048576;
	while (bufSize > 131072) {
		if (setsockopt(udpSock,SOL_SOCKET,SO_SNDBUF,(const char *)&bufSize,sizeof(bufSize)) == 0)
			break;
		bufSize -= 131072;
	}

	SETSOCKOPT_FLAG_TYPE fl;

#ifdef SO_REUSEPORT
	fl = SETSOCKOPT_FLAG_TRUE;
	setsockopt(udpSock,SOL_SOCKET,SO_REUSEPORT,(void *)&fl,sizeof(fl));
#endif
#ifndef __LINUX__ // linux wants just SO_REUSEPORT
	fl = SETSOCKOPT_FLAG_TRUE;
	setsockopt(udpSock,SOL_SOCKET,SO_REUSEADDR,(void *)&fl,sizeof(fl));
#endif

	fl = SETSOCKOPT_FLAG_TRUE;
	setsockopt(udpSock,SOL_SOCKET,SO_BROADCAST,(void *)&fl,sizeof(fl));

#ifdef IP_DONTFRAG
	fl = SETSOCKOPT_FLAG_FALSE;
	setsockopt(udpSock,IPPROTO_IP,IP_DONTFRAG,(void *)&fl,sizeof(fl));
#endif
#ifdef IP_MTU_DISCOVER
	fl = SETSOCKOPT_FLAG_FALSE;
	setsockopt(udpSock,IPPROTO_IP,IP_MTU_DISCOVER,(void *)&fl,sizeof(fl));
#endif

#ifdef SO_BINDTODEVICE
	if ((dev)&&(strlen(dev)))
		setsockopt(udpSock,SOL_SOCKET,SO_BINDTODEVICE,dev,strlen(dev));
#endif
#if defined(__BSD__) && defined(IP_BOUND_IF)
	if ((dev)&&(strlen(dev))) {
		int idx = if_nametoindex(dev);
		if (idx != 0)
			setsockopt(udpSock,IPPROTO_IP,IP_BOUND_IF,(void *)&idx,sizeof(idx));
	}
#endif
}

extern "C" int ZT_GoNode_phyStartListen(ZT_GoNode *gn,const char *dev,const char *ip,const int port)
{
	if (strchr(ip,':')) {
		struct sockaddr_in6 in6;
		memset(&in6,0,sizeof(in6));
		in6.sin6_family = AF_INET6;
		if (inet_pton(AF_INET6,ip,&(in6.sin6_addr)) <= 0)
			return errno;
		in6.sin6_port = htons((uint16_t)port);

		ZT_SOCKET udpSock = socket(AF_INET6,SOCK_DGRAM,0);
		if (udpSock == ZT_INVALID_SOCKET)
			return errno;
		setCommonUdpSocketSettings(udpSock,dev);
		SETSOCKOPT_FLAG_TYPE fl = SETSOCKOPT_FLAG_TRUE;
		setsockopt(udpSock,IPPROTO_IPV6,IPV6_V6ONLY,(const char *)&fl,sizeof(fl));
#ifdef IPV6_DONTFRAG
		fl = SETSOCKOPT_FLAG_FALSE;
		setsockopt(udpSock,IPPROTO_IPV6,IPV6_DONTFRAG,&fl,sizeof(fl));
#endif

		if (bind(udpSock,reinterpret_cast<const struct sockaddr *>(&in6),sizeof(in6)) != 0)
			return errno;

		{
			std::lock_guard<std::mutex> l(gn->threads_l);
			ZT_GoNodeThread &gnt = gn->threads[udpSock];
			gnt.ip = ip;
			gnt.port = port;
			gnt.af = AF_INET6;
			gnt.run = true;
			gnt.thr = std::thread([udpSock,gn,&gnt] {
				struct sockaddr_in6 in6;
				socklen_t salen;
				char buf[16384];
				while (gnt.run) {
					salen = sizeof(in6);
					int s = (int)recvfrom(udpSock,buf,sizeof(buf),0,reinterpret_cast<struct sockaddr *>(&in6),&salen);
					if (s > 0) {
						gn->node->processWirePacket(&gnt,OSUtils::now(),(int64_t)udpSock,reinterpret_cast<const struct sockaddr_storage *>(&in6),buf,(unsigned int)s,&(gn->nextBackgroundTaskDeadline));
					}
				}
			});
		}
	} else {
		struct sockaddr_in in;
		memset(&in,0,sizeof(in));
		in.sin_family = AF_INET;
		if (inet_pton(AF_INET,ip,&(in.sin_addr)) <= 0)
			return errno;
		in.sin_port = htons((uint16_t)port);

		ZT_SOCKET udpSock = socket(AF_INET,SOCK_DGRAM,0);
		if (udpSock == ZT_INVALID_SOCKET)
			return errno;
		setCommonUdpSocketSettings(udpSock,dev);
#ifdef SO_NO_CHECK
		SETSOCKOPT_FLAG_TYPE fl = SETSOCKOPT_FLAG_TRUE;
		setsockopt(udpSock,SOL_SOCKET,SO_NO_CHECK,&fl,sizeof(fl));
#endif

		if (bind(udpSock,reinterpret_cast<const struct sockaddr *>(&in),sizeof(in)) != 0)
			return errno;

		{
			std::lock_guard<std::mutex> l(gn->threads_l);
			ZT_GoNodeThread &gnt = gn->threads[udpSock];
			gnt.ip = ip;
			gnt.port = port;
			gnt.af = AF_INET6;
			gnt.run = true;
			gnt.thr = std::thread([udpSock,gn,&gnt] {
				struct sockaddr_in in4;
				socklen_t salen;
				char buf[16384];
				while (gnt.run) {
					salen = sizeof(in4);
					int s = (int)recvfrom(udpSock,buf,sizeof(buf),0,reinterpret_cast<struct sockaddr *>(&in4),&salen);
					if (s > 0) {
						gn->node->processWirePacket(&gnt,OSUtils::now(),(int64_t)udpSock,reinterpret_cast<const struct sockaddr_storage *>(&in4),buf,(unsigned int)s,&(gn->nextBackgroundTaskDeadline));
					}
				}
			});
		}
	}

	return 0;
}

extern "C" int ZT_GoNode_phyStopListen(ZT_GoNode *gn,const char *dev,const char *ip,const int port)
{
	{
		std::lock_guard<std::mutex> l(gn->threads_l);
		for(auto t=gn->threads.begin();t!=gn->threads.end();) {
			if ((t->second.ip == ip)&&(t->second.port == port)) {
				t->second.run = false;
				shutdown(t->first,SHUT_RDWR);
				close(t->first);
				t->second.thr.join();
				gn->threads.erase(t++);
			} else ++t;
		}
	}
}

extern "C" int ZT_GoNode_waitForEvent(ZT_GoNode *gn,ZT_GoNodeEvent *ev)
{
	gn->eq.get(*ev);
}
