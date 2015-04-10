/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../version.h"
#include "../include/ZeroTierOne.h"

#include "../node/Constants.hpp"
#include "../node/Mutex.hpp"
#include "../node/Node.hpp"
#include "../node/Utils.hpp"
#include "../node/InetAddress.hpp"

#include "../osdep/Phy.hpp"
#include "../osdep/OSUtils.hpp"

#include "One.hpp"

namespace ZeroTier {

static void SphyOnDatagramFunction(PhySocket *sock,void **uptr,const struct sockaddr *from,void *data,unsigned long len);
static void SphyOnTcpConnectFunction(PhySocket *sock,void **uptr,bool success);
static void SphyOnTcpAcceptFunction(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from);
static void SphyOnTcpCloseFunction(PhySocket *sock,void **uptr);
static void SphyOnTcpDataFunction(PhySocket *sock,void **uptr,void *data,unsigned long len);
static void SphyOnTcpWritableFunction(PhySocket *sock,void **uptr);

static int SnodeVirtualNetworkConfigFunction(ZT1_Node *node,void *uptr,uint64_t nwid,enum ZT1_VirtualNetworkConfigOperation op,const ZT1_VirtualNetworkConfig *nwconf);
static void SnodeEventCallback(ZT1_Node *node,void *uptr,enum ZT1_Event event,const void *metaData);
static long SnodeDataStoreGetFunction(ZT1_Node *node,void *uptr,const char *name,void *buf,unsigned long bufSize,unsigned long readIndex,unsigned long *totalSize);
static int SnodeDataStorePutFunction(ZT1_Node *node,void *uptr,const char *name,const void *data,unsigned long len,int secure);
static int SnodeWirePacketSendFunction(ZT1_Node *node,void *uptr,const struct sockaddr_storage *addr,unsigned int desperation,const void *data,unsigned int len);
static void SnodeVirtualNetworkFrameFunction(ZT1_Node *node,void *uptr,uint64_t nwid,uint64_t sourceMac,uint64_t destMac,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len);

class OneImpl : public One
{
public:
	OneImpl(const char *hp,unsigned int port,NetworkConfigMaster *master,const char *overrideRootTopology) :
		_homePath((hp) ? hp : "."),
		_phy(SphyOnDatagramFunction,SphyOnTcpConnectFunction,SphyOnTcpAcceptFunction,SphyOnTcpCloseFunction,SphyOnTcpDataFunction,SphyOnTcpWritableFunction,true),
		_master(master),
		_overrideRootTopology((overrideRootTopology) ? overrideRootTopology : ""),
		_node((Node *)0),
		_nextBackgroundTaskDeadline(0),
		_termReason(ONE_STILL_RUNNING),
		_run(true)
	{
		struct sockaddr_in in4;
		struct sockaddr_in6 in6;

		if (*hp) {
			std::vector<std::string> hpsp(Utils::split(hp,ZT_PATH_SEPARATOR_S,"",""));
			std::string ptmp;
			if (*hp == '/')
				ptmp.push_back('/');
			for(std::vector<std::string>::iterator pi(hpsp.begin());pi!=hpsp.end();++pi) {
				if (ptmp.length() > 0)
					ptmp.push_back(ZT_PATH_SEPARATOR);
				ptmp.append(*pi);
				if ((*pi != ".")&&(*pi != "..")) {
					if (!OSUtils::mkdir(ptmp))
						throw std::runtime_error("home path does not exist, and could not create");
				}
			}
		}

		::memset((void *)&in4,0,sizeof(in4));
		in4.sin_family = AF_INET;
		in4.sin_port = Utils::hton(port);
		_v4UdpSocket = _phy.udpBind((const struct sockaddr *)&in4,this,131072);
		if (!_v4UdpSocket)
			throw std::runtime_error("cannot bind to port (UDP/IPv4)");
		_v4TcpListenSocket = _phy.tcpListen((const struct sockaddr *)&in4,this);
		if (!_v4TcpListenSocket) {
			_phy.close(_v4UdpSocket);
			throw std::runtime_error("cannot bind to port (TCP/IPv4)");
		}

		::memset((void *)&in6,0,sizeof(in6));
		in6.sin6_family = AF_INET6;
		in6.sin6_port = in4.sin_port;
		_v6UdpSocket = _phy.udpBind((const struct sockaddr *)&in6,this,131072);
		_v6TcpListenSocket = _phy.tcpListen((const struct sockaddr *)&in6,this);
	}

	virtual ~OneImpl()
	{
		_phy.close(_v4UdpSocket);
		_phy.close(_v6UdpSocket);
		_phy.close(_v4TcpListenSocket);
		_phy.close(_v6TcpListenSocket);
	}

	virtual ReasonForTermination run()
	{
		try {
			_node = new Node(
				OSUtils::now(),
				this,
				SnodeDataStoreGetFunction,
				SnodeDataStorePutFunction,
				SnodeWirePacketSendFunction,
				SnodeVirtualNetworkFrameFunction,
				SnodeVirtualNetworkConfigFunction,
				SnodeEventCallback,
				((_overrideRootTopology.length() > 0) ? _overrideRootTopology.c_str() : (const char *)0));

			if (_master)
				_node->setNetconfMaster((void *)_master);

			_nextBackgroundTaskDeadline = 0;
			for(;;) {
				_run_m.lock();
				if (!_run) {
					_run_m.unlock();
					_termReason_m.lock();
					_termReason = ONE_NORMAL_TERMINATION;
					_termReason_m.unlock();
					break;
				} else _run_m.unlock();

				uint64_t dl = _nextBackgroundTaskDeadline;
				uint64_t now = OSUtils::now();

				if (dl <= now) {
					_node->processBackgroundTasks(now,const_cast<uint64_t *>(&_nextBackgroundTaskDeadline));
					dl = _nextBackgroundTaskDeadline;
					now = OSUtils::now();
				}

				const unsigned long delay = (dl > now) ? (unsigned long)(dl - now) : 100;
				printf("polling: %lums timeout\n",delay);
				_phy.poll(delay);
			}
		} catch (std::exception &exc) {
			Mutex::Lock _l(_termReason_m);
			_termReason = ONE_UNRECOVERABLE_ERROR;
			_fatalErrorMessage = exc.what();
		} catch ( ... ) {
			Mutex::Lock _l(_termReason_m);
			_termReason = ONE_UNRECOVERABLE_ERROR;
			_fatalErrorMessage = "unexpected exception in main thread";
		}

		delete _node;
		_node = (Node *)0;

		return _termReason;
	}

	virtual ReasonForTermination reasonForTermination() const
	{
		Mutex::Lock _l(_termReason_m);
		return _termReason;
	}

	virtual std::string fatalErrorMessage() const
	{
		Mutex::Lock _l(_termReason_m);
		return _fatalErrorMessage;
	}

	virtual void terminate()
	{
		_run_m.lock();
		_run = false;
		_run_m.unlock();
		_phy.whack();
	}

	// Begin private implementation methods

	inline void phyOnDatagramFunction(PhySocket *sock,const struct sockaddr *from,void *data,unsigned long len)
	{
		try {
			ZT1_ResultCode rc = _node->processWirePacket(
				OSUtils::now(),
				(const struct sockaddr_storage *)from, // Phy<> uses sockaddr_storage, so it'll always be that big
				0,
				data,
				len,
				const_cast<uint64_t *>(&_nextBackgroundTaskDeadline));
			if (ZT1_ResultCode_isFatal(rc)) {
				char tmp[256];
				Utils::snprintf(tmp,sizeof(tmp),"fatal error code from processWirePacket(%d)",(int)rc);
				Mutex::Lock _l(_termReason_m);
				_termReason = ONE_UNRECOVERABLE_ERROR;
				_fatalErrorMessage = tmp;
				this->terminate();
			}
		} catch ( ... ) {}
	}

	inline void phyOnTcpConnectFunction(PhySocket *sock,bool success)
	{
	}

	inline void phyOnTcpAcceptFunction(PhySocket *sockN,const struct sockaddr *from)
	{
	}

	inline void phyOnTcpCloseFunction(PhySocket *sock)
	{
	}

	inline void phyOnTcpDataFunction(PhySocket *sock,void *data,unsigned long len)
	{
	}

	inline void phyOnTcpWritableFunction(PhySocket *sock)
	{
	}

	inline int nodeVirtualNetworkConfigFunction(uint64_t nwid,enum ZT1_VirtualNetworkConfigOperation op,const ZT1_VirtualNetworkConfig *nwconf)
	{
		return 0;
	}

	inline void nodeEventCallback(enum ZT1_Event event,const void *metaData)
	{
		switch(event) {
			case ZT1_EVENT_FATAL_ERROR_IDENTITY_COLLISION: {
				Mutex::Lock _l(_termReason_m);
				_termReason = ONE_IDENTITY_COLLISION;
				_fatalErrorMessage = "identity/address collision";
				this->terminate();
			}	break;

			case ZT1_EVENT_SAW_MORE_RECENT_VERSION: {
			}	break;

			case ZT1_EVENT_TRACE: {
				if (metaData) {
					::fprintf(stderr,"%s"ZT_EOL_S,(const char *)metaData);
					::fflush(stderr);
				}
			}	break;

			default:
				break;
		}
	}

	inline long nodeDataStoreGetFunction(const char *name,void *buf,unsigned long bufSize,unsigned long readIndex,unsigned long *totalSize)
	{
		std::string p(_homePath);
		p.push_back(ZT_PATH_SEPARATOR);
		char lastc = (char)0;
		for(const char *n=name;(*n);++n) {
			if ((*n == '.')&&(lastc == '.'))
				return -2; // security sanity check-- don't allow ../ stuff even though there's really no way Node will ever do this
			p.push_back((*n == '/') ? ZT_PATH_SEPARATOR : *n);
			lastc = *n;
		}

		FILE *f = fopen(p.c_str(),"rb");
		if (!f)
			return -1;
		if (fseek(f,0,SEEK_END) != 0) {
			fclose(f);
			return -2;
		}
		long ts = ftell(f);
		if (ts < 0) {
			fclose(f);
			return -2;
		}
		*totalSize = (unsigned long)ts;
		if (fseek(f,(long)readIndex,SEEK_SET) != 0) {
			fclose(f);
			return -2;
		}
		long n = (long)fread(buf,1,bufSize,f);
		fclose(f);
		return n;
	}

	inline int nodeDataStorePutFunction(const char *name,const void *data,unsigned long len,int secure)
	{
		std::string p(_homePath);
		p.push_back(ZT_PATH_SEPARATOR);
		char lastc = (char)0;
		for(const char *n=name;(*n);++n) {
			if ((*n == '.')&&(lastc == '.'))
				return -2; // security sanity check-- don't allow ../ stuff even though there's really no way Node will ever do this
			p.push_back((*n == '/') ? ZT_PATH_SEPARATOR : *n);
			lastc = *n;
		}

		if (!data) {
			OSUtils::rm(p.c_str());
			return 0;
		}

		FILE *f = fopen(p.c_str(),"wb");
		if (!f)
			return -1;
		if (fwrite(data,len,1,f) == 1) {
			fclose(f);
			if (secure)
				OSUtils::lockDownFile(p.c_str(),false);
			return 0;
		} else {
			fclose(f);
			OSUtils::rm(p.c_str());
			return -1;
		}
	}

	inline int nodeWirePacketSendFunction(const struct sockaddr_storage *addr,unsigned int desperation,const void *data,unsigned int len)
	{
		switch(addr->ss_family) {
			case AF_INET:
				if (_v4UdpSocket)
					return (_phy.udpSend(_v4UdpSocket,(const struct sockaddr *)addr,data,len) ? 0 : -1);
				break;
			case AF_INET6:
				if (_v6UdpSocket)
					return (_phy.udpSend(_v6UdpSocket,(const struct sockaddr *)addr,data,len) ? 0 : -1);
				break;
		}
		return -1;
	}

	inline void nodeVirtualNetworkFrameFunction(uint64_t nwid,uint64_t sourceMac,uint64_t destMac,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
	{
		fprintf(stderr,"VIRTUAL NETWORK FRAME from %.16llx : %.12llx -> %.12llx %.4x %u bytes\n",nwid,sourceMac,destMac,etherType,len);
		fflush(stderr);
	}

private:
	const std::string _homePath;
	SimpleFunctionPhy _phy;
	NetworkConfigMaster *_master;
	std::string _overrideRootTopology;
	Node *_node;
	PhySocket *_v4UdpSocket;
	PhySocket *_v6UdpSocket;
	PhySocket *_v4TcpListenSocket;
	PhySocket *_v6TcpListenSocket;
	volatile uint64_t _nextBackgroundTaskDeadline;

	ReasonForTermination _termReason;
	std::string _fatalErrorMessage;
	Mutex _termReason_m;

	bool _run;
	Mutex _run_m;
};

static void SphyOnDatagramFunction(PhySocket *sock,void **uptr,const struct sockaddr *from,void *data,unsigned long len)
{ reinterpret_cast<OneImpl *>(*uptr)->phyOnDatagramFunction(sock,from,data,len); }
static void SphyOnTcpConnectFunction(PhySocket *sock,void **uptr,bool success)
{ reinterpret_cast<OneImpl *>(*uptr)->phyOnTcpConnectFunction(sock,success); }
static void SphyOnTcpAcceptFunction(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from)
{ *uptrN = *uptrL; reinterpret_cast<OneImpl *>(*uptrL)->phyOnTcpAcceptFunction(sockN,from); }
static void SphyOnTcpCloseFunction(PhySocket *sock,void **uptr)
{ reinterpret_cast<OneImpl *>(*uptr)->phyOnTcpCloseFunction(sock); }
static void SphyOnTcpDataFunction(PhySocket *sock,void **uptr,void *data,unsigned long len)
{ reinterpret_cast<OneImpl *>(*uptr)->phyOnTcpDataFunction(sock,data,len); }
static void SphyOnTcpWritableFunction(PhySocket *sock,void **uptr)
{ reinterpret_cast<OneImpl *>(*uptr)->phyOnTcpWritableFunction(sock); }

static int SnodeVirtualNetworkConfigFunction(ZT1_Node *node,void *uptr,uint64_t nwid,enum ZT1_VirtualNetworkConfigOperation op,const ZT1_VirtualNetworkConfig *nwconf)
{ return reinterpret_cast<OneImpl *>(uptr)->nodeVirtualNetworkConfigFunction(nwid,op,nwconf); }
static void SnodeEventCallback(ZT1_Node *node,void *uptr,enum ZT1_Event event,const void *metaData)
{ reinterpret_cast<OneImpl *>(uptr)->nodeEventCallback(event,metaData); }
static long SnodeDataStoreGetFunction(ZT1_Node *node,void *uptr,const char *name,void *buf,unsigned long bufSize,unsigned long readIndex,unsigned long *totalSize)
{ return reinterpret_cast<OneImpl *>(uptr)->nodeDataStoreGetFunction(name,buf,bufSize,readIndex,totalSize); }
static int SnodeDataStorePutFunction(ZT1_Node *node,void *uptr,const char *name,const void *data,unsigned long len,int secure)
{ return reinterpret_cast<OneImpl *>(uptr)->nodeDataStorePutFunction(name,data,len,secure); }
static int SnodeWirePacketSendFunction(ZT1_Node *node,void *uptr,const struct sockaddr_storage *addr,unsigned int desperation,const void *data,unsigned int len)
{ return reinterpret_cast<OneImpl *>(uptr)->nodeWirePacketSendFunction(addr,desperation,data,len); }
static void SnodeVirtualNetworkFrameFunction(ZT1_Node *node,void *uptr,uint64_t nwid,uint64_t sourceMac,uint64_t destMac,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
{ reinterpret_cast<OneImpl *>(uptr)->nodeVirtualNetworkFrameFunction(nwid,sourceMac,destMac,etherType,vlanId,data,len); }

std::string One::platformDefaultHomePath()
{
#ifdef __UNIX_LIKE__

#ifdef __APPLE__
	// /Library/... on Apple
	return std::string("/Library/Application Support/ZeroTier/One");
#else

#ifdef __FreeBSD__
	// FreeBSD likes /var/db instead of /var/lib
	return std::string("/var/db/zerotier-one");
#else
	// Use /var/lib for Linux and other *nix
	return std::string("/var/lib/zerotier-one");
#endif

#endif

#else // not __UNIX_LIKE__

#ifdef __WINDOWS__
	// Look up app data folder on Windows, e.g. C:\ProgramData\...
	char buf[16384];
	if (SUCCEEDED(SHGetFolderPathA(NULL,CSIDL_COMMON_APPDATA,NULL,0,buf)))
		return (std::string(buf) + "\\ZeroTier\\One");
	else return std::string("C:\\ZeroTier\\One");
#else

	return std::string(); // UNKNOWN PLATFORM

#endif

#endif // __UNIX_LIKE__ or not...
}

One *One::newInstance(const char *hp,unsigned int port,NetworkConfigMaster *master,const char *overrideRootTopology) { return new OneImpl(hp,port,master,overrideRootTopology); }
One::~One() {}

} // namespace ZeroTier
