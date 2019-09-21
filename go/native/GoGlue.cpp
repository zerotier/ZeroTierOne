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

#include "GoGlue.h"

#include "../../node/Constants.hpp"
#include "../../node/InetAddress.hpp"
#include "../../node/Node.hpp"
#include "../../node/Utils.hpp"
#include "../../node/MAC.hpp"
#include "../../node/Address.hpp"
#include "../../osdep/OSUtils.hpp"
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
#include <memory>

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

	std::string path;
	std::atomic_bool run;

	std::map< ZT_SOCKET,ZT_GoNodeThread > threads;
	std::mutex threads_l;

	std::map< uint64_t,std::shared_ptr<EthernetTap> > taps;
	std::mutex taps_l;

	std::thread backgroundTaskThread;
};

/****************************************************************************/

/* These functions are implemented in Go in pkg/ztnode/node-callbacks.go */
extern "C" int goPathCheckFunc(ZT_GoNode *,uint64_t,int,const void *,int);
extern "C" int goPathLookupFunc(ZT_GoNode *,uint64_t,int,int *,uint8_t [16],int *);
extern "C" void goStateObjectPutFunc(ZT_GoNode *,int,const uint64_t [2],const void *,int);
extern "C" int goStateObjectGetFunc(ZT_GoNode *,int,const uint64_t [2],void *,unsigned int);
extern "C" void goDNSResolverFunc(ZT_GoNode *,const uint8_t *,int,const char *,uintptr_t);
extern "C" int goVirtualNetworkConfigFunc(ZT_GoNode *,ZT_GoTap *,uint64_t,int,const ZT_VirtualNetworkConfig *);
extern "C" void goZtEvent(ZT_GoNode *,int,const void *);

static int ZT_GoNode_VirtualNetworkConfigFunction(
	ZT_Node *node,
	void *uptr,
	void *tptr,
	uint64_t nwid,
	void **nptr,
	enum ZT_VirtualNetworkConfigOperation op,
	const ZT_VirtualNetworkConfig *cfg)
{
	return goVirtualNetworkConfigFunc(reinterpret_cast<ZT_GoNode *>(uptr),reinterpret_cast<ZT_GoTap *>(*nptr),nwid,op,cfg);
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
	goZtEvent(reinterpret_cast<ZT_GoNode *>(uptr),et,data);
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
	goStateObjectPutFunc(reinterpret_cast<ZT_GoNode *>(uptr),objType,id,data,len);
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
	return goStateObjectGetFunc(
		reinterpret_cast<ZT_GoNode *>(uptr),
		(int)objType,
		id,
		buf,
		buflen);
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

static int ZT_GoNode_WirePacketSendFunction(
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
	return 0;
}

static int ZT_GoNode_PathCheckFunction(
	ZT_Node *node,
	void *uptr,
	void *tptr,
	uint64_t ztAddress,
	int64_t localSocket,
	const struct sockaddr_storage *sa)
{
	switch(sa->ss_family) {
		case AF_INET:
			return goPathCheckFunc(
				reinterpret_cast<ZT_GoNode *>(uptr),
				ztAddress,
				AF_INET,
				&(reinterpret_cast<const struct sockaddr_in *>(sa)->sin_addr.s_addr),
				Utils::ntoh((uint16_t)reinterpret_cast<const struct sockaddr_in *>(sa)->sin_port));
		case AF_INET6:
			return goPathCheckFunc(
				reinterpret_cast<ZT_GoNode *>(uptr),
				ztAddress,
				AF_INET6,
				reinterpret_cast<const struct sockaddr_in6 *>(sa)->sin6_addr.s6_addr,
				Utils::ntoh((uint16_t)reinterpret_cast<const struct sockaddr_in6 *>(sa)->sin6_port));
	}
	return 0;
}

static int ZT_GoNode_PathLookupFunction(
	ZT_Node *node,
	void *uptr,
	void *tptr,
	uint64_t ztAddress,
	int desiredAddressFamily,
	struct sockaddr_storage *sa)
{
	int family = 0;
	uint8_t ip[16];
	int port = 0;
	const int result = goPathLookupFunc(
		reinterpret_cast<ZT_GoNode *>(uptr),
		ztAddress,
		desiredAddressFamily,
		&family,
		ip,
		&port
	);
	if (result != 0) {
		switch(family) {
			case AF_INET:
				reinterpret_cast<struct sockaddr_in *>(sa)->sin_family = AF_INET;
				memcpy(&(reinterpret_cast<struct sockaddr_in *>(sa)->sin_addr.s_addr),ip,4);
				reinterpret_cast<struct sockaddr_in *>(sa)->sin_port = Utils::hton((uint16_t)port);
				return 1;
			case AF_INET6:
				reinterpret_cast<struct sockaddr_in6 *>(sa)->sin6_family = AF_INET6;
				memcpy(reinterpret_cast<struct sockaddr_in6 *>(sa)->sin6_addr.s6_addr,ip,16);
				reinterpret_cast<struct sockaddr_in6 *>(sa)->sin6_port = Utils::hton((uint16_t)port);
				return 1;
		}
	}
	return 0;
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
	uint8_t t[256];
	for(unsigned int i=0;(i<numTypes)&&(i<256);++i) t[i] = (uint8_t)types[i];
	goDNSResolverFunc(reinterpret_cast<ZT_GoNode *>(uptr),t,(int)numTypes,name,requestId);
}

/****************************************************************************/

extern "C" ZT_GoNode *ZT_GoNode_new(const char *workingPath)
{
	try {
		struct ZT_Node_Callbacks cb;
		cb.statePutFunction = &ZT_GoNode_StatePutFunction;
		cb.stateGetFunction = &ZT_GoNode_StateGetFunction;
		cb.wirePacketSendFunction = &ZT_GoNode_WirePacketSendFunction;
		cb.virtualNetworkFrameFunction = &ZT_GoNode_VirtualNetworkFrameFunction;
		cb.virtualNetworkConfigFunction = &ZT_GoNode_VirtualNetworkConfigFunction;
		cb.eventCallback = &ZT_GoNode_EventCallback;
		cb.dnsResolver = &ZT_GoNode_DNSResolver;
		cb.pathCheckFunction = &ZT_GoNode_PathCheckFunction;
		cb.pathLookupFunction = &ZT_GoNode_PathLookupFunction;

		ZT_GoNode_Impl *gn = new ZT_GoNode_Impl;
		const int64_t now = OSUtils::now();
		gn->node = new Node(reinterpret_cast<void *>(gn),nullptr,&cb,now);
		gn->nextBackgroundTaskDeadline = now;
		gn->path = workingPath;
		gn->run = true;

		gn->backgroundTaskThread = std::thread([gn] {
			while (gn->run) {
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
				const int64_t now = OSUtils::now();
				if (now >= gn->nextBackgroundTaskDeadline)
					gn->node->processBackgroundTasks(nullptr,now,&(gn->nextBackgroundTaskDeadline));
			}
		});

		return gn;
	} catch ( ... ) {
		fprintf(stderr,"FATAL: unable to create new instance of Node (out of memory?)" ZT_EOL_S);
		exit(1);
	}
}

extern "C" void ZT_GoNode_delete(ZT_GoNode *gn)
{
	gn->run = false;

	gn->threads_l.lock();
	for(auto t=gn->threads.begin();t!=gn->threads.end();++t) {
		t->second.run = false;
		shutdown(t->first,SHUT_RDWR);
		close(t->first);
		t->second.thr.join();
	}
	gn->threads_l.unlock();

	gn->taps_l.lock();
	for(auto t=gn->taps.begin();t!=gn->taps.end();++t)
		gn->node->leave(t->first,nullptr,nullptr);
	gn->taps.clear();
	gn->taps_l.unlock();

	gn->backgroundTaskThread.join();

	delete gn->node;
	delete gn;
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
	return 0;
}

static void tapFrameHandler(void *uptr,void *tptr,uint64_t nwid,const MAC &from,const MAC &to,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
{
	ZT_GoNode *const gn = reinterpret_cast<ZT_GoNode *>(uptr);
	gn->node->processVirtualNetworkFrame(tptr,OSUtils::now(),nwid,from.toInt(),to.toInt(),etherType,vlanId,data,len,&(gn->nextBackgroundTaskDeadline));
}

extern "C" ZT_GoTap *ZT_GoNode_join(ZT_GoNode *gn,uint64_t nwid)
{
	try {
		std::lock_guard<std::mutex> l(gn->taps_l);
		auto existingTap = gn->taps.find(nwid);
		if (existingTap != gn->taps.end())
			return (ZT_GoTap *)existingTap->second.get();
		char tmp[256];
		OSUtils::ztsnprintf(tmp,sizeof(tmp),"ZeroTier Network %.16llx",(unsigned long long)nwid);
		std::shared_ptr<EthernetTap> tap(EthernetTap::newInstance(nullptr,gn->path.c_str(),MAC(Address(gn->node->address()),nwid),ZT_DEFAULT_MTU,0,nwid,tmp,&tapFrameHandler,gn));
		if (!tap)
			return nullptr;
		gn->taps[nwid] = tap;
		gn->node->join(nwid,tap.get(),nullptr);
		return (ZT_GoTap *)tap.get();
	} catch ( ... ) {
		return nullptr;
	}
}

extern "C" void ZT_GoNode_leave(ZT_GoNode *gn,uint64_t nwid)
{
	std::lock_guard<std::mutex> l(gn->taps_l);
	auto existingTap = gn->taps.find(nwid);
	if (existingTap != gn->taps.end()) {
		gn->node->leave(nwid,nullptr,nullptr);
		gn->taps.erase(existingTap);
	}
}

/****************************************************************************/

extern "C" void ZT_GoTap_setEnabled(ZT_GoTap *tap,int enabled)
{
	reinterpret_cast<EthernetTap *>(tap)->setEnabled(enabled != 0);
}

extern "C" int ZT_GoTap_addIp(ZT_GoTap *tap,int af,const void *ip,int netmaskBits)
{
	switch(af) {
		case AF_INET:
			return (reinterpret_cast<EthernetTap *>(tap)->addIp(InetAddress(ip,4,(unsigned int)netmaskBits)) ? 1 : 0);
		case AF_INET6:
			return (reinterpret_cast<EthernetTap *>(tap)->addIp(InetAddress(ip,16,(unsigned int)netmaskBits)) ? 1 : 0);
	}
	return 0;
}

extern "C" int ZT_GoTap_removeIp(ZT_GoTap *tap,int af,const void *ip,int netmaskBits)
{
	switch(af) {
		case AF_INET:
			return (reinterpret_cast<EthernetTap *>(tap)->removeIp(InetAddress(ip,4,(unsigned int)netmaskBits)) ? 1 : 0);
		case AF_INET6:
			return (reinterpret_cast<EthernetTap *>(tap)->removeIp(InetAddress(ip,16,(unsigned int)netmaskBits)) ? 1 : 0);
	}
	return 0;
}

extern "C" int ZT_GoTap_ips(ZT_GoTap *tap,void *buf,unsigned int bufSize)
{
	auto ips = reinterpret_cast<EthernetTap *>(tap)->ips();
	unsigned int p = 0;
	uint8_t *const b = reinterpret_cast<uint8_t *>(buf);
	for(auto ip=ips.begin();ip!=ips.end();++ip) {
		if ((p + 6) > bufSize)
			break;
		const uint8_t *const ipd = reinterpret_cast<const uint8_t *>(ip->rawIpData());
		if (ip->isV4()) {
			b[p++] = AF_INET;
			b[p++] = ipd[0];
			b[p++] = ipd[1];
			b[p++] = ipd[2];
			b[p++] = ipd[3];
			b[p++] = (uint8_t)ip->netmaskBits();
		} else if (ip->isV6()) {
			if ((p + 18) <= bufSize) {
				b[p++] = AF_INET6;
				for(int j=0;j<16;++j)
					b[p++] = ipd[j];
				b[p++] = (uint8_t)ip->netmaskBits();
			}
		}
	}
	return (int)p;
}

extern "C" void ZT_GoTap_deviceName(ZT_GoTap *tap,char nbuf[256])
{
	Utils::scopy(nbuf,256,reinterpret_cast<EthernetTap *>(tap)->deviceName().c_str());
}

extern "C" void ZT_GoTap_setFriendlyName(ZT_GoTap *tap,const char *friendlyName)
{
	reinterpret_cast<EthernetTap *>(tap)->setFriendlyName(friendlyName);
}

extern "C" void ZT_GoTap_setMtu(ZT_GoTap *tap,unsigned int mtu)
{
	reinterpret_cast<EthernetTap *>(tap)->setMtu(mtu);
}
