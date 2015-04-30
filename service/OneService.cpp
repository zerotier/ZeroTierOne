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

#ifdef __WINDOWS__
#include <ShlObj.h>
#endif

// Include the right tap device driver for this platform -- add new platforms here
#ifdef __APPLE__
#include "../osdep/OSXEthernetTap.hpp"
namespace ZeroTier { typedef OSXEthernetTap EthernetTap; }
#endif
#ifdef __LINUX__
#include "../osdep/LinuxEthernetTap.hpp"
namespace ZeroTier { typedef LinuxEthernetTap EthernetTap; }
#endif
#ifdef __WINDOWS__
#include "../osdep/WindowsEthernetTap.hpp"
namespace ZeroTier { typedef WindowsEthernetTap EthernetTap; }
#endif

// Sanity limits for HTTP
#define ZT_MAX_HTTP_MESSAGE_SIZE (1024 * 1024 * 8)
#define ZT_MAX_HTTP_CONNECTIONS 64

// Interface metric for ZeroTier taps
#define ZT_IF_METRIC 32768

// How often to check for new multicast subscriptions on a tap device
#define ZT_TAP_CHECK_MULTICAST_INTERVAL 30000

namespace ZeroTier {

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
static const struct http_parser_settings HTTP_PARSER_SETTINGS = {
	ShttpOnMessageBegin,
	ShttpOnUrl,
	ShttpOnStatus,
	ShttpOnHeaderField,
	ShttpOnValue,
	ShttpOnHeadersComplete,
	ShttpOnBody,
	ShttpOnMessageComplete
};

struct TcpConnection
{
	enum {
		TCP_HTTP_INCOMING,
		TCP_HTTP_OUTGOING, // not currently used
		TCP_TUNNEL_OUTGOING // fale-SSL outgoing tunnel -- HTTP-related fields are not used
	} type;

	bool shouldKeepAlive;
	OneServiceImpl *parent;
	PhySocket *sock;
	InetAddress from;
	http_parser parser;
	unsigned long messageSize;
	uint64_t lastActivity;

	std::string currentHeaderField;
	std::string currentHeaderValue;

	std::string url;
	std::string status;
	std::map< std::string,std::string > headers;
	std::string body;

	std::string writeBuf;
};

class OneServiceImpl : public OneService
{
public:
	OneServiceImpl(const char *hp,unsigned int port,NetworkController *master,const char *overrideRootTopology) :
		_homePath((hp) ? hp : "."),
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
		in4.sin_addr.s_addr = Utils::hton((uint32_t)0x7f000001); // right now we just listen for TCP @localhost
		_v4TcpListenSocket = _phy.tcpListen((const struct sockaddr *)&in4,this);
		if (!_v4TcpListenSocket) {
			_phy.close(_v4UdpSocket);
			throw std::runtime_error("cannot bind to port (TCP/IPv4)");
		}

		::memset((void *)&in6,0,sizeof(in6));
		in6.sin6_family = AF_INET6;
		in6.sin6_port = in4.sin_port;
		_v6UdpSocket = _phy.udpBind((const struct sockaddr *)&in6,this,131072);
		in6.sin6_addr.s6_addr[15] = 1; // listen for TCP only at localhost
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
			if (_master)
				_controlPlane->mount("controller",reinterpret_cast<ControlPlaneSubsystem *>(_master));

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
			while (!_tcpConections.empty())
				_phy.close(_tcpConections.begin()->first);
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
			0, // desperation == 0, direct UDP
			data,
			len,
			&_nextBackgroundTaskDeadline);
		if (ZT1_ResultCode_isFatal(rc)) {
			char tmp[256];
			Utils::snprintf(tmp,sizeof(tmp),"fatal error code from processWirePacket: %d",(int)rc);
			Mutex::Lock _l(_termReason_m);
			_termReason = ONE_UNRECOVERABLE_ERROR;
			_fatalErrorMessage = tmp;
			this->terminate();
		}
	}

	inline void phyOnTcpConnect(PhySocket *sock,void **uptr,bool success)
	{
		if (!success)
			return;

		// Outgoing connections are right now only tunnel connections
		TcpConnection *tc = &(_tcpConections[sock]);
		tc->type = TcpConnection::TCP_TUNNEL_OUTGOING;
		tc->shouldKeepAlive = true; // unused
		tc->parent = this;
		tc->sock = sock;
		// from and parser are not used
		tc->messageSize = 0; // unused
		tc->lastActivity = OSUtils::now();
		// HTTP stuff is not used
		tc->writeBuf = "";
		*uptr = (void *)tc;

		// Send "hello" message
		tc->writeBuf.push_back((char)0x17);
		tc->writeBuf.push_back((char)0x03);
		tc->writeBuf.push_back((char)0x03); // fake TLS 1.2 header
		tc->writeBuf.push_back((char)0x00);
		tc->writeBuf.push_back((char)0x04); // mlen == 4
		tc->writeBuf.push_back((char)ZEROTIER_ONE_VERSION_MAJOR);
		tc->writeBuf.push_back((char)ZEROTIER_ONE_VERSION_MINOR);
		tc->writeBuf.push_back((char)((ZEROTIER_ONE_VERSION_REVISION >> 8) & 0xff));
		tc->writeBuf.push_back((char)(ZEROTIER_ONE_VERSION_REVISION & 0xff));
		_phy.tcpSetNotifyWritable(sock,true);
	}

	inline void phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from)
	{
		// Incoming connections are TCP HTTP requests
		TcpConnection *tc = &(_tcpConections[sockN]);
		tc->type = TcpConnection::TCP_HTTP_INCOMING;
		tc->shouldKeepAlive = true;
		tc->parent = this;
		tc->sock = sockN;
		tc->from = from;
		http_parser_init(&(tc->parser),HTTP_REQUEST);
		tc->parser.data = (void *)tc;
		tc->messageSize = 0;
		tc->lastActivity = OSUtils::now();
		tc->currentHeaderField = "";
		tc->currentHeaderValue = "";
		tc->url = "";
		tc->status = "";
		tc->headers.clear();
		tc->body = "";
		tc->writeBuf = "";
		*uptrN = (void *)tc;
	}

	inline void phyOnTcpClose(PhySocket *sock,void **uptr)
	{
		_tcpConections.erase(sock);
	}

	inline void phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len)
	{
		TcpConnection *tc = reinterpret_cast<TcpConnection *>(*uptr);
		switch(tc->type) {
			case TcpConnection::TCP_HTTP_INCOMING:
			case TcpConnection::TCP_HTTP_OUTGOING:
				http_parser_execute(&(tc->parser),&HTTP_PARSER_SETTINGS,(const char *)data,len);
				if ((tc->parser.upgrade)||(tc->parser.http_errno != HPE_OK)) {
					_phy.close(sock);
					return;
				}
				break;
			case TcpConnection::TCP_TUNNEL_OUTGOING:
				tc->body.append((const char *)data,len);
				if (tc->body.length() > 65535) {
					// sanity limit -- a message will never be this big since mlen is 16-bit
					_phy.close(sock);
					return;
				} else if (tc->body.length() >= 5) {
					const char *data = tc->body.data();
					const unsigned long mlen = ( ((((unsigned long)data[3]) & 0xff) << 8) | (((unsigned long)data[4]) & 0xff) );
					if (tc->body.length() >= (mlen + 5)) {
						InetAddress from;

						unsigned long plen = mlen; // payload length, modified if there's an IP header
						data += 5; // skip forward past pseudo-TLS junk and mlen
						if (plen == 4) {
							// Hello message, which isn't sent by proxy and would be ignored by client
						} else if (plen) {
							// Messages should contain IPv4 or IPv6 source IP address data
							switch(data[0]) {
								case 4: // IPv4
									if (plen >= 7) {
										from.set((const void *)(data + 1),4,((((unsigned int)data[5]) & 0xff) << 8) | (((unsigned int)data[6]) & 0xff));
										data += 7; // type + 4 byte IP + 2 byte port
										plen -= 7;
									} else {
										_phy.close(sock);
										return;
									}
									break;
								case 6: // IPv6
									if (plen >= 19) {
										from.set((const void *)(data + 1),16,((((unsigned int)data[17]) & 0xff) << 8) | (((unsigned int)data[18]) & 0xff));
										data += 19; // type + 16 byte IP + 2 byte port
										plen -= 19;
									} else {
										_phy.close(sock);
										return;
									}
									break;
								case 0: // none/omitted
									++data;
									--plen;
									break;
								default: // invalid address type
									_phy.close(sock);
									return;
							}

							ZT1_ResultCode rc = _node->processWirePacket(
								OSUtils::now(),
								(const struct sockaddr_storage *)&from, // Phy<> uses sockaddr_storage, so it'll always be that big
								1, // desperation == 1, TCP tunnel proxy
								data,
								plen,
								&_nextBackgroundTaskDeadline);
							if (ZT1_ResultCode_isFatal(rc)) {
								char tmp[256];
								Utils::snprintf(tmp,sizeof(tmp),"fatal error code from processWirePacket: %d",(int)rc);
								Mutex::Lock _l(_termReason_m);
								_termReason = ONE_UNRECOVERABLE_ERROR;
								_fatalErrorMessage = tmp;
								this->terminate();
								_phy.close(sock);
								return;
							}
						}
						if (tc->body.length() > (mlen + 5))
							tc->body = tc->body.substr(mlen + 5);
						else tc->body = "";
					}
				}
				break;
		}
	}

	inline void phyOnTcpWritable(PhySocket *sock,void **uptr)
	{
		TcpConnection *tc = reinterpret_cast<TcpConnection *>(*uptr);
		if (tc->writeBuf.length()) {
			long sent = _phy.tcpSend(sock,tc->writeBuf.data(),tc->writeBuf.length(),true);
			if (sent > 0) {
				tc->lastActivity = OSUtils::now();
				if (sent == tc->writeBuf.length()) {
					tc->writeBuf = "";
					_phy.tcpSetNotifyWritable(sock,false);
					if (!tc->shouldKeepAlive)
						_phy.close(sock); // will call close handler to delete from _tcpConections
				} else tc->writeBuf = tc->writeBuf.substr(sent);
			}
		} else _phy.tcpSetNotifyWritable(sock,false); // sanity check... shouldn't happen
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
							(unsigned int)ZT_IF_METRIC,
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
#ifdef __WINDOWS__
					std::string winInstanceId(t->second->instanceId());
#endif
					delete t->second;
					_taps.erase(t);
					_tapAssignedIps.erase(nwid);
#ifdef __WINDOWS__
					if ((op == ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY)&&(winInstanceId.length() > 0))
						WindowsEthernetTap::deletePersistentTapDevice(_homePath.c_str(),winInstanceId.c_str());
#endif
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

	inline void onHttpRequestToServer(TcpConnection *tc)
	{
		char tmpn[256];
		std::string data;
		std::string contentType("text/plain"); // default if not changed in handleRequest()
		unsigned int scode = 404;

		try {
			if (_controlPlane)
				scode = _controlPlane->handleRequest(tc->from,tc->parser.method,tc->url,tc->headers,tc->body,data,contentType);
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
		tc->writeBuf.assign(tmpn);
		tc->writeBuf.append("Content-Type: ");
		tc->writeBuf.append(contentType);
		Utils::snprintf(tmpn,sizeof(tmpn),"\r\nContent-Length: %lu\r\n",(unsigned long)data.length());
		tc->writeBuf.append(tmpn);
		if (!tc->shouldKeepAlive)
			tc->writeBuf.append("Connection: close\r\n");
		tc->writeBuf.append("\r\n");
		if (tc->parser.method != HTTP_HEAD)
			tc->writeBuf.append(data);

		_phy.tcpSetNotifyWritable(tc->sock,true);
	}

	inline void onHttpResponseFromClient(TcpConnection *tc)
	{
		if (!tc->shouldKeepAlive)
			_phy.close(tc->sock); // will call close handler, which deletes from _tcpConections
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

	std::map< PhySocket *,TcpConnection > _tcpConections; // no mutex for this since it's done in the main loop thread only

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
	TcpConnection *tc = reinterpret_cast<TcpConnection *>(parser->data);
	tc->currentHeaderField = "";
	tc->currentHeaderValue = "";
	tc->messageSize = 0;
	tc->url = "";
	tc->status = "";
	tc->headers.clear();
	tc->body = "";
	return 0;
}
static int ShttpOnUrl(http_parser *parser,const char *ptr,size_t length)
{
	TcpConnection *tc = reinterpret_cast<TcpConnection *>(parser->data);
	tc->messageSize += (unsigned long)length;
	if (tc->messageSize > ZT_MAX_HTTP_MESSAGE_SIZE)
		return -1;
	tc->url.append(ptr,length);
	return 0;
}
static int ShttpOnStatus(http_parser *parser,const char *ptr,size_t length)
{
	TcpConnection *tc = reinterpret_cast<TcpConnection *>(parser->data);
	tc->messageSize += (unsigned long)length;
	if (tc->messageSize > ZT_MAX_HTTP_MESSAGE_SIZE)
		return -1;
	tc->status.append(ptr,length);
	return 0;
}
static int ShttpOnHeaderField(http_parser *parser,const char *ptr,size_t length)
{
	TcpConnection *tc = reinterpret_cast<TcpConnection *>(parser->data);
	tc->messageSize += (unsigned long)length;
	if (tc->messageSize > ZT_MAX_HTTP_MESSAGE_SIZE)
		return -1;
	if ((tc->currentHeaderField.length())&&(tc->currentHeaderValue.length())) {
		tc->headers[tc->currentHeaderField] = tc->currentHeaderValue;
		tc->currentHeaderField = "";
		tc->currentHeaderValue = "";
	}
	for(size_t i=0;i<length;++i)
		tc->currentHeaderField.push_back(OSUtils::toLower(ptr[i]));
	return 0;
}
static int ShttpOnValue(http_parser *parser,const char *ptr,size_t length)
{
	TcpConnection *tc = reinterpret_cast<TcpConnection *>(parser->data);
	tc->messageSize += (unsigned long)length;
	if (tc->messageSize > ZT_MAX_HTTP_MESSAGE_SIZE)
		return -1;
	tc->currentHeaderValue.append(ptr,length);
	return 0;
}
static int ShttpOnHeadersComplete(http_parser *parser)
{
	TcpConnection *tc = reinterpret_cast<TcpConnection *>(parser->data);
	if ((tc->currentHeaderField.length())&&(tc->currentHeaderValue.length()))
		tc->headers[tc->currentHeaderField] = tc->currentHeaderValue;
	return 0;
}
static int ShttpOnBody(http_parser *parser,const char *ptr,size_t length)
{
	TcpConnection *tc = reinterpret_cast<TcpConnection *>(parser->data);
	tc->messageSize += (unsigned long)length;
	if (tc->messageSize > ZT_MAX_HTTP_MESSAGE_SIZE)
		return -1;
	tc->body.append(ptr,length);
	return 0;
}
static int ShttpOnMessageComplete(http_parser *parser)
{
	TcpConnection *tc = reinterpret_cast<TcpConnection *>(parser->data);
	tc->shouldKeepAlive = (http_should_keep_alive(parser) != 0);
	tc->lastActivity = OSUtils::now();
	if (tc->type == TcpConnection::TCP_HTTP_INCOMING) {
		tc->parent->onHttpRequestToServer(tc);
	} else {
		tc->parent->onHttpResponseFromClient(tc);
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
