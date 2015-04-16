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

#include <string>
#include <map>
#include <set>
#include <vector>
#include <algorithm>

#include "../version.h"
#include "../include/ZeroTierOne.h"

#include "../ext/http-parser/http_parser.h"

#include "../node/Constants.hpp"
#include "../node/Mutex.hpp"
#include "../node/Node.hpp"
#include "../node/Utils.hpp"
#include "../node/InetAddress.hpp"
#include "../node/MAC.hpp"

#include "../osdep/Phy.hpp"
#include "../osdep/OSUtils.hpp"

#include "OneService.hpp"
#include "ControlPlane.hpp"

#ifdef __APPLE__
#include "../osdep/OSXEthernetTap.hpp"
namespace ZeroTier { typedef OSXEthernetTap EthernetTap; }
#endif

// Sanity limits for HTTP
#define ZT_MAX_HTTP_MESSAGE_SIZE (1024 * 1024 * 8)
#define ZT_MAX_HTTP_CONNECTIONS 64

// Interface metric for ZeroTier taps
#define ZT_IF_METRIC 32768

// How often to check for new multicast subscriptions on a tap device
#define ZT_TAP_CHECK_MULTICAST_INTERVAL 30000

namespace ZeroTier {

// Used to convert HTTP header names to ASCII lower case
static const unsigned char ZT_TOLOWER_TABLE[256] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, ' ', '!', '"', '#', '$', '%', '&', 0x27, '(', ')', '*', '+', ',', '-', '.', '/', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?', '@', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', '_', '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff };

class OneServiceImpl;

static int SnodeVirtualNetworkConfigFunction(ZT1_Node *node,void *uptr,uint64_t nwid,enum ZT1_VirtualNetworkConfigOperation op,const ZT1_VirtualNetworkConfig *nwconf);
static void SnodeEventCallback(ZT1_Node *node,void *uptr,enum ZT1_Event event,const void *metaData);
static long SnodeDataStoreGetFunction(ZT1_Node *node,void *uptr,const char *name,void *buf,unsigned long bufSize,unsigned long readIndex,unsigned long *totalSize);
static int SnodeDataStorePutFunction(ZT1_Node *node,void *uptr,const char *name,const void *data,unsigned long len,int secure);
static int SnodeWirePacketSendFunction(ZT1_Node *node,void *uptr,const struct sockaddr_storage *addr,unsigned int desperation,const void *data,unsigned int len);
static void SnodeVirtualNetworkFrameFunction(ZT1_Node *node,void *uptr,uint64_t nwid,uint64_t sourceMac,uint64_t destMac,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len);

static void StapFrameHandler(void *uptr,uint64_t nwid,const MAC &from,const MAC &to,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len);

static int ShttpOnMessageBegin(http_parser *parser);
static int ShttpOnUrl(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnStatus(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnHeaderField(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnValue(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnHeadersComplete(http_parser *parser);
static int ShttpOnBody(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnMessageComplete(http_parser *parser);

const struct http_parser_settings HTTP_PARSER_SETTINGS = {
	ShttpOnMessageBegin,
	ShttpOnUrl,
	ShttpOnStatus,
	ShttpOnHeaderField,
	ShttpOnValue,
	ShttpOnHeadersComplete,
	ShttpOnBody,
	ShttpOnMessageComplete
};

struct HttpConnection
{
	bool server;
	bool writing;
	bool shouldKeepAlive;
	OneServiceImpl *parent;
	PhySocket *sock;
	InetAddress from;
	http_parser parser;
	unsigned long messageSize;
	unsigned long writePtr;
	uint64_t lastActivity;

	std::string currentHeaderField;
	std::string currentHeaderValue;

	std::string url;
	std::string status;
	std::map< std::string,std::string > headers;
	std::string body; // also doubles as send queue for writes out to the socket
};

class OneServiceImpl : public OneService
{
public:
	OneServiceImpl(const char *hp,unsigned int port,NetworkController *master,const char *overrideRootTopology) :
		_homePath((hp) ? hp : "."),
		_port(port),
		_phy(this,true),
		_master(master),
		_overrideRootTopology((overrideRootTopology) ? overrideRootTopology : ""),
		_node((Node *)0),
		_controlPlane((ControlPlane *)0),
		_nextBackgroundTaskDeadline(0),
		_termReason(ONE_STILL_RUNNING),
		_run(true)
	{
		struct sockaddr_in in4;
		struct sockaddr_in6 in6;

		::memset((void *)&in4,0,sizeof(in4));
		in4.sin_family = AF_INET;
		in4.sin_port = Utils::hton((uint16_t)port);
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

		char portstr[64];
		Utils::snprintf(portstr,sizeof(portstr),"%u",port);
		OSUtils::writeFile((_homePath + ZT_PATH_SEPARATOR_S + "zerotier-one.port").c_str(),std::string(portstr));
	}

	virtual ~OneServiceImpl()
	{
		_phy.close(_v4UdpSocket);
		_phy.close(_v6UdpSocket);
		_phy.close(_v4TcpListenSocket);
		_phy.close(_v6TcpListenSocket);
	}

	virtual ReasonForTermination run()
	{
		try {
			std::string authToken;
			{
				std::string authTokenPath(_homePath + ZT_PATH_SEPARATOR_S + "authtoken.secret");
				if (!OSUtils::readFile(authTokenPath.c_str(),authToken)) {
					unsigned char foo[24];
					Utils::getSecureRandom(foo,sizeof(foo));
					authToken = "";
					for(unsigned int i=0;i<sizeof(foo);++i)
						authToken.push_back("abcdefghijklmnopqrstuvwxyz0123456789"[(unsigned long)foo[i] % 36]);
					if (!OSUtils::writeFile(authTokenPath.c_str(),authToken)) {
						Mutex::Lock _l(_termReason_m);
						_termReason = ONE_UNRECOVERABLE_ERROR;
						_fatalErrorMessage = "authtoken.secret could not be written";
						return _termReason;
					} else OSUtils::lockDownFile(authTokenPath.c_str(),false);
				}
			}
			authToken = Utils::trim(authToken);

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

			_controlPlane = new ControlPlane(this,_node);
			_controlPlane->addAuthToken(authToken.c_str());

			{	// Remember networks from previous session
				std::vector<std::string> networksDotD(OSUtils::listDirectory((_homePath + ZT_PATH_SEPARATOR_S + "networks.d").c_str()));
				for(std::vector<std::string>::iterator f(networksDotD.begin());f!=networksDotD.end();++f) {
					std::size_t dot = f->find_last_of('.');
					if ((dot == 16)&&(f->substr(16) == ".conf"))
						_node->join(Utils::hexStrToU64(f->substr(0,dot).c_str()));
				}
			}

			_nextBackgroundTaskDeadline = 0;
			uint64_t lastTapMulticastGroupCheck = 0;
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
					_node->processBackgroundTasks(now,&_nextBackgroundTaskDeadline);
					dl = _nextBackgroundTaskDeadline;
				}

				if ((now - lastTapMulticastGroupCheck) >= ZT_TAP_CHECK_MULTICAST_INTERVAL) {
					lastTapMulticastGroupCheck = now;
					Mutex::Lock _l(_taps_m);
					for(std::map< uint64_t,EthernetTap *>::const_iterator t(_taps.begin());t!=_taps.end();++t) {
						std::vector<MulticastGroup> added,removed;
						t->second->scanMulticastGroups(added,removed);
						for(std::vector<MulticastGroup>::iterator m(added.begin());m!=added.end();++m)
							_node->multicastSubscribe(t->first,m->mac().toInt(),m->adi());
						for(std::vector<MulticastGroup>::iterator m(removed.begin());m!=removed.end();++m)
							_node->multicastUnsubscribe(t->first,m->mac().toInt(),m->adi());
					}
				}

				const unsigned long delay = (dl > now) ? (unsigned long)(dl - now) : 100;
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

		try {
			while (!_httpConnections.empty())
				_phy.close(_httpConnections.begin()->first);
		} catch ( ... ) {}

		{
			Mutex::Lock _l(_taps_m);
			for(std::map< uint64_t,EthernetTap * >::iterator t(_taps.begin());t!=_taps.end();++t)
				delete t->second;
			_taps.clear();
		}

		delete _controlPlane;
		_controlPlane = (ControlPlane *)0;
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

	virtual std::string portDeviceName(uint64_t nwid) const
	{
		Mutex::Lock _l(_taps_m);
		std::map< uint64_t,EthernetTap * >::const_iterator t(_taps.find(nwid));
		if (t != _taps.end())
			return t->second->deviceName();
		return std::string();
	}

	virtual void terminate()
	{
		_run_m.lock();
		_run = false;
		_run_m.unlock();
		_phy.whack();
	}

	// Begin private implementation methods

	inline void phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *from,void *data,unsigned long len)
	{
		ZT1_ResultCode rc = _node->processWirePacket(
			OSUtils::now(),
			(const struct sockaddr_storage *)from, // Phy<> uses sockaddr_storage, so it'll always be that big
			0,
			data,
			len,
			&_nextBackgroundTaskDeadline);
		if (ZT1_ResultCode_isFatal(rc)) {
			char tmp[256];
			Utils::snprintf(tmp,sizeof(tmp),"fatal error code from processWirePacket(%d)",(int)rc);
			Mutex::Lock _l(_termReason_m);
			_termReason = ONE_UNRECOVERABLE_ERROR;
			_fatalErrorMessage = tmp;
			this->terminate();
		}
	}

	inline void phyOnTcpConnect(PhySocket *sock,void **uptr,bool success)
	{
		// TODO: outgoing HTTP connection success/failure
	}

	inline void phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from)
	{
		HttpConnection *htc = &(_httpConnections[sockN]);
		htc->server = true;
		htc->writing = false;
		htc->shouldKeepAlive = true;
		htc->parent = this;
		htc->sock = sockN;
		htc->from = from;
		http_parser_init(&(htc->parser),HTTP_REQUEST);
		htc->parser.data = (void *)htc;
		htc->messageSize = 0;
		htc->writePtr = 0;
		htc->lastActivity = OSUtils::now();
		htc->currentHeaderField = "";
		htc->currentHeaderValue = "";
		htc->url = "";
		htc->status = "";
		htc->headers.clear();
		htc->body = "";
		*uptrN = (void *)htc;
	}

	inline void phyOnTcpClose(PhySocket *sock,void **uptr)
	{
		_httpConnections.erase(sock);
	}

	inline void phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len)
	{
		HttpConnection *htc = reinterpret_cast<HttpConnection *>(*uptr);
		http_parser_execute(&(htc->parser),&HTTP_PARSER_SETTINGS,(const char *)data,len);
		if ((htc->parser.upgrade)||(htc->parser.http_errno != HPE_OK))
			_phy.close(sock);
	}

	inline void phyOnTcpWritable(PhySocket *sock,void **uptr)
	{
		HttpConnection *htc = reinterpret_cast<HttpConnection *>(*uptr);
		long sent = _phy.tcpSend(sock,htc->body.data() + htc->writePtr,htc->body.length() - htc->writePtr,true);
		if (sent < 0) {
			return; // close handler will have been called, so everything's dead
		} else {
			htc->lastActivity = OSUtils::now();
			htc->writePtr += sent;
			if (htc->writePtr >= htc->body.length()) {
				_phy.tcpSetNotifyWritable(sock,false);
				if (htc->shouldKeepAlive) {
					htc->writing = false;
					htc->writePtr = 0;
					htc->body.assign("",0);
				} else {
					_phy.close(sock); // will call close handler to delete from _httpConnections
				}
			}
		}
	}

	inline int nodeVirtualNetworkConfigFunction(uint64_t nwid,enum ZT1_VirtualNetworkConfigOperation op,const ZT1_VirtualNetworkConfig *nwc)
	{
		Mutex::Lock _l(_taps_m);
		std::map< uint64_t,EthernetTap * >::iterator t(_taps.find(nwid));
		switch(op) {
			case ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_UP:
				if (t == _taps.end()) {
					try {
						char friendlyName[1024];
						Utils::snprintf(friendlyName,sizeof(friendlyName),"ZeroTier One [%.16llx]",nwid);
						t = _taps.insert(std::pair< uint64_t,EthernetTap *>(nwid,new EthernetTap(
							_homePath.c_str(),
							MAC(nwc->mac),
							nwc->mtu,
							ZT_IF_METRIC,
							nwid,
							friendlyName,
							StapFrameHandler,
							(void *)this))).first;
					} catch ( ... ) {
						return -999; // tap init failed
					}
				}
				// fall through...
			case ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE:
				if (t != _taps.end()) {
					t->second->setEnabled(nwc->enabled != 0);

					std::vector<InetAddress> &assignedIps = _tapAssignedIps[nwid];
					std::vector<InetAddress> newAssignedIps;
					for(unsigned int i=0;i<nwc->assignedAddressCount;++i)
						newAssignedIps.push_back(InetAddress(nwc->assignedAddresses[i]));
					std::sort(newAssignedIps.begin(),newAssignedIps.end());
					std::unique(newAssignedIps.begin(),newAssignedIps.end());
					for(std::vector<InetAddress>::iterator ip(newAssignedIps.begin());ip!=newAssignedIps.end();++ip) {
						if (!std::binary_search(assignedIps.begin(),assignedIps.end(),*ip))
							t->second->addIp(*ip);
					}
					for(std::vector<InetAddress>::iterator ip(assignedIps.begin());ip!=assignedIps.end();++ip) {
						if (!std::binary_search(newAssignedIps.begin(),newAssignedIps.end(),*ip))
							t->second->removeIp(*ip);
					}
					assignedIps.swap(newAssignedIps);
				} else {
					return -999; // tap init failed
				}
				break;
			case ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN:
			case ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY:
				if (t != _taps.end()) {
					delete t->second;
					_taps.erase(t);
					_tapAssignedIps.erase(nwid);
				}
				break;
		}
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
		std::string p(_dataStorePrepPath(name));
		if (!p.length())
			return -2;

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
		std::string p(_dataStorePrepPath(name));
		if (!p.length())
			return -2;

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
		Mutex::Lock _l(_taps_m);
		std::map< uint64_t,EthernetTap * >::const_iterator t(_taps.find(nwid));
		if (t != _taps.end())
			t->second->put(MAC(sourceMac),MAC(destMac),etherType,data,len);
	}

	inline void tapFrameHandler(uint64_t nwid,const MAC &from,const MAC &to,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
	{
		_node->processVirtualNetworkFrame(OSUtils::now(),nwid,from.toInt(),to.toInt(),etherType,vlanId,data,len,&_nextBackgroundTaskDeadline);
	}

	inline void onHttpRequestToServer(HttpConnection *htc)
	{
		char tmpn[256];
		std::string data;
		std::string contentType("text/plain"); // default if not changed in handleRequest()
		unsigned int scode = 404;

		try {
			if (_controlPlane)
				scode = _controlPlane->handleRequest(htc->from,htc->parser.method,htc->url,htc->headers,htc->body,data,contentType);
			else scode = 500;
		} catch ( ... ) {
			scode = 500;
		}

		const char *scodestr;
		switch(scode) {
			case 200: scodestr = "OK"; break;
			case 400: scodestr = "Bad Request"; break;
			case 401: scodestr = "Unauthorized"; break;
			case 403: scodestr = "Forbidden"; break;
			case 404: scodestr = "Not Found"; break;
			case 500: scodestr = "Internal Server Error"; break;
			case 501: scodestr = "Not Implemented"; break;
			case 503: scodestr = "Service Unavailable"; break;
			default: scodestr = "Error"; break;
		}

		Utils::snprintf(tmpn,sizeof(tmpn),"HTTP/1.1 %.3u %s\r\nCache-Control: no-cache\r\nPragma: no-cache\r\n",scode,scodestr);
		htc->body.assign(tmpn);
		htc->body.append("Content-Type: ");
		htc->body.append(contentType);
		Utils::snprintf(tmpn,sizeof(tmpn),"\r\nContent-Length: %lu\r\n",(unsigned long)data.length());
		htc->body.append(tmpn);
		if (!htc->shouldKeepAlive)
			htc->body.append("Connection: close\r\n");
		htc->body.append("\r\n");
		if (htc->parser.method != HTTP_HEAD)
			htc->body.append(data);

		htc->writing = true;
		htc->writePtr = 0;
		_phy.tcpSetNotifyWritable(htc->sock,true);
	}

	inline void onHttpResponseFromClient(HttpConnection *htc)
	{
		if (!htc->shouldKeepAlive)
			_phy.close(htc->sock); // will call close handler, which deletes from _httpConnections
	}

private:
	std::string _dataStorePrepPath(const char *name) const
	{
		std::string p(_homePath);
		p.push_back(ZT_PATH_SEPARATOR);
		char lastc = (char)0;
		for(const char *n=name;(*n);++n) {
			if ((*n == '.')&&(lastc == '.'))
				return std::string(); // don't allow ../../ stuff as a precaution
			if (*n == '/') {
				OSUtils::mkdir(p.c_str());
				p.push_back(ZT_PATH_SEPARATOR);
			} else p.push_back(*n);
			lastc = *n;
		}
		return p;
	}

	const std::string _homePath;
	unsigned int _port;
	Phy<OneServiceImpl *> _phy;
	NetworkController *_master;
	std::string _overrideRootTopology;
	Node *_node;
	PhySocket *_v4UdpSocket;
	PhySocket *_v6UdpSocket;
	PhySocket *_v4TcpListenSocket;
	PhySocket *_v6TcpListenSocket;
	ControlPlane *_controlPlane;
	volatile uint64_t _nextBackgroundTaskDeadline;

	std::map< uint64_t,EthernetTap * > _taps;
	std::map< uint64_t,std::vector<InetAddress> > _tapAssignedIps; // ZeroTier assigned IPs, not user or dhcp assigned
	Mutex _taps_m;

	std::map< PhySocket *,HttpConnection > _httpConnections; // no mutex for this since it's done in the main loop thread only

	ReasonForTermination _termReason;
	std::string _fatalErrorMessage;
	Mutex _termReason_m;

	bool _run;
	Mutex _run_m;
};

static int SnodeVirtualNetworkConfigFunction(ZT1_Node *node,void *uptr,uint64_t nwid,enum ZT1_VirtualNetworkConfigOperation op,const ZT1_VirtualNetworkConfig *nwconf)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodeVirtualNetworkConfigFunction(nwid,op,nwconf); }
static void SnodeEventCallback(ZT1_Node *node,void *uptr,enum ZT1_Event event,const void *metaData)
{ reinterpret_cast<OneServiceImpl *>(uptr)->nodeEventCallback(event,metaData); }
static long SnodeDataStoreGetFunction(ZT1_Node *node,void *uptr,const char *name,void *buf,unsigned long bufSize,unsigned long readIndex,unsigned long *totalSize)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodeDataStoreGetFunction(name,buf,bufSize,readIndex,totalSize); }
static int SnodeDataStorePutFunction(ZT1_Node *node,void *uptr,const char *name,const void *data,unsigned long len,int secure)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodeDataStorePutFunction(name,data,len,secure); }
static int SnodeWirePacketSendFunction(ZT1_Node *node,void *uptr,const struct sockaddr_storage *addr,unsigned int desperation,const void *data,unsigned int len)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodeWirePacketSendFunction(addr,desperation,data,len); }
static void SnodeVirtualNetworkFrameFunction(ZT1_Node *node,void *uptr,uint64_t nwid,uint64_t sourceMac,uint64_t destMac,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
{ reinterpret_cast<OneServiceImpl *>(uptr)->nodeVirtualNetworkFrameFunction(nwid,sourceMac,destMac,etherType,vlanId,data,len); }

static void StapFrameHandler(void *uptr,uint64_t nwid,const MAC &from,const MAC &to,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
{ reinterpret_cast<OneServiceImpl *>(uptr)->tapFrameHandler(nwid,from,to,etherType,vlanId,data,len); }

static int ShttpOnMessageBegin(http_parser *parser)
{
	HttpConnection *htc = reinterpret_cast<HttpConnection *>(parser->data);
	htc->currentHeaderField.assign("",0);
	htc->currentHeaderValue.assign("",0);
	htc->messageSize = 0;
	htc->url.assign("",0);
	htc->status.assign("",0);
	htc->headers.clear();
	htc->body.assign("",0);
	return 0;
}
static int ShttpOnUrl(http_parser *parser,const char *ptr,size_t length)
{
	HttpConnection *htc = reinterpret_cast<HttpConnection *>(parser->data);
	htc->messageSize += length;
	if (htc->messageSize > ZT_MAX_HTTP_MESSAGE_SIZE)
		return -1;
	htc->url.append(ptr,length);
	return 0;
}
static int ShttpOnStatus(http_parser *parser,const char *ptr,size_t length)
{
	HttpConnection *htc = reinterpret_cast<HttpConnection *>(parser->data);
	htc->messageSize += length;
	if (htc->messageSize > ZT_MAX_HTTP_MESSAGE_SIZE)
		return -1;
	htc->status.append(ptr,length);
	return 0;
}
static int ShttpOnHeaderField(http_parser *parser,const char *ptr,size_t length)
{
	HttpConnection *htc = reinterpret_cast<HttpConnection *>(parser->data);
	htc->messageSize += length;
	if (htc->messageSize > ZT_MAX_HTTP_MESSAGE_SIZE)
		return -1;
	if ((htc->currentHeaderField.length())&&(htc->currentHeaderValue.length())) {
		htc->headers[htc->currentHeaderField] = htc->currentHeaderValue;
		htc->currentHeaderField.assign("",0);
		htc->currentHeaderValue.assign("",0);
	}
	for(size_t i=0;i<length;++i)
		htc->currentHeaderField.push_back((char)ZT_TOLOWER_TABLE[(unsigned int)ptr[i]]);
	return 0;
}
static int ShttpOnValue(http_parser *parser,const char *ptr,size_t length)
{
	HttpConnection *htc = reinterpret_cast<HttpConnection *>(parser->data);
	htc->messageSize += length;
	if (htc->messageSize > ZT_MAX_HTTP_MESSAGE_SIZE)
		return -1;
	htc->currentHeaderValue.append(ptr,length);
	return 0;
}
static int ShttpOnHeadersComplete(http_parser *parser)
{
	HttpConnection *htc = reinterpret_cast<HttpConnection *>(parser->data);
	if ((htc->currentHeaderField.length())&&(htc->currentHeaderValue.length()))
		htc->headers[htc->currentHeaderField] = htc->currentHeaderValue;
	return 0;
}
static int ShttpOnBody(http_parser *parser,const char *ptr,size_t length)
{
	HttpConnection *htc = reinterpret_cast<HttpConnection *>(parser->data);
	htc->messageSize += length;
	if (htc->messageSize > ZT_MAX_HTTP_MESSAGE_SIZE)
		return -1;
	htc->body.append(ptr,length);
	return 0;
}
static int ShttpOnMessageComplete(http_parser *parser)
{
	HttpConnection *htc = reinterpret_cast<HttpConnection *>(parser->data);
	htc->shouldKeepAlive = (http_should_keep_alive(parser) != 0);
	htc->lastActivity = OSUtils::now();
	if (htc->server) {
		htc->parent->onHttpRequestToServer(htc);
	} else {
		htc->parent->onHttpResponseFromClient(htc);
	}
	return 0;
}

std::string OneService::platformDefaultHomePath()
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

OneService *OneService::newInstance(const char *hp,unsigned int port,NetworkController *master,const char *overrideRootTopology) { return new OneServiceImpl(hp,port,master,overrideRootTopology); }
OneService::~OneService() {}

} // namespace ZeroTier
