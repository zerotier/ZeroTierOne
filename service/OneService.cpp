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
#include <stdint.h>

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
#include "../node/Identity.hpp"

#include "../osdep/Phy.hpp"
#include "../osdep/Thread.hpp"
#include "../osdep/OSUtils.hpp"
#include "../osdep/Http.hpp"
#include "../osdep/BackgroundResolver.hpp"

#include "OneService.hpp"
#include "ControlPlane.hpp"

/**
 * Uncomment to enable UDP breakage switch
 *
 * If this is defined, the presence of a file called /tmp/ZT_BREAK_UDP
 * will cause direct UDP TX/RX to stop working. This can be used to
 * test TCP tunneling fallback and other robustness features. Deleting
 * this file will cause it to start working again.
 */
//#define ZT_BREAK_UDP

#ifdef ZT_ENABLE_NETWORK_CONTROLLER
#include "../controller/SqliteNetworkController.hpp"
#else
class SqliteNetworkController;
#endif // ZT_ENABLE_NETWORK_CONTROLLER

#ifdef __WINDOWS__
#include <ShlObj.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
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
#if defined(__BSD__) && (!defined(__APPLE__))
#include "../osdep/BSDEthernetTap.hpp"
namespace ZeroTier { typedef BSDEthernetTap EthernetTap; }
#endif

// Sanity limits for HTTP
#define ZT_MAX_HTTP_MESSAGE_SIZE (1024 * 1024 * 8)
#define ZT_MAX_HTTP_CONNECTIONS 64

// Interface metric for ZeroTier taps
#define ZT_IF_METRIC 32768

// How often to check for new multicast subscriptions on a tap device
#define ZT_TAP_CHECK_MULTICAST_INTERVAL 30000

// Path under ZT1 home for controller database if controller is enabled
#define ZT1_CONTROLLER_DB_PATH "controller.db"

// TCP fallback relay host -- geo-distributed using Amazon Route53 geo-aware DNS
#define ZT1_TCP_FALLBACK_RELAY "tcp-fallback.zerotier.com"
#define ZT1_TCP_FALLBACK_RELAY_PORT 443

// Frequency at which we re-resolve the TCP fallback relay
#define ZT1_TCP_FALLBACK_RERESOLVE_DELAY 86400000

// Attempt to engage TCP fallback after this many ms of no reply to packets sent to global-scope IPs
#define ZT1_TCP_FALLBACK_AFTER 60000

namespace ZeroTier {

namespace {

#ifdef ZT_AUTO_UPDATE
#define ZT_AUTO_UPDATE_MAX_HTTP_RESPONSE_SIZE (1024 * 1024 * 64)
#define ZT_AUTO_UPDATE_CHECK_PERIOD 21600000
class BackgroundSoftwareUpdateChecker
{
public:
	bool isValidSigningIdentity(const Identity &id)
	{
		return (
		  /* 0005 */   (id == Identity("ba57ea350e:0:9d4be6d7f86c5660d5ee1951a3d759aa6e12a84fc0c0b74639500f1dbc1a8c566622e7d1c531967ebceb1e9d1761342f88324a8ba520c93c35f92f35080fa23f"))
		  /* 0006 */ ||(id == Identity("5067b21b83:0:8af477730f5055c48135b84bed6720a35bca4c0e34be4060a4c636288b1ec22217eb22709d610c66ed464c643130c51411bbb0294eef12fbe8ecc1a1e2c63a7a"))
		  /* 0007 */ ||(id == Identity("4f5e97a8f1:0:57880d056d7baeb04bbc057d6f16e6cb41388570e87f01492fce882485f65a798648595610a3ad49885604e7fb1db2dd3c2c534b75e42c3c0b110ad07b4bb138"))
		  /* 0008 */ ||(id == Identity("580bbb8e15:0:ad5ef31155bebc6bc413991992387e083fed26d699997ef76e7c947781edd47d1997161fa56ba337b1a2b44b129fd7c7197ce5185382f06011bc88d1363b4ddd"))
		);
	}

	void doUpdateCheck()
	{
		std::string url(OneService::autoUpdateUrl());
		if ((url.length() <= 7)||(url.substr(0,7) != "http://"))
			return;

		std::string httpHost;
		std::string httpPath;
		{
			std::size_t slashIdx = url.substr(7).find_first_of('/');
			if (slashIdx == std::string::npos) {
				httpHost = url.substr(7);
				httpPath = "/";
			} else {
				httpHost = url.substr(7,slashIdx);
				httpPath = url.substr(slashIdx + 7);
			}
		}
		if (httpHost.length() == 0)
			return;

		std::vector<InetAddress> ips(OSUtils::resolve(httpHost.c_str()));
		for(std::vector<InetAddress>::iterator ip(ips.begin());ip!=ips.end();++ip) {
			if (!ip->port())
				ip->setPort(80);
			std::string nfoPath = httpPath + "LATEST.nfo";
			std::map<std::string,std::string> requestHeaders,responseHeaders;
			std::string body;
			requestHeaders["Host"] = httpHost;
			unsigned int scode = Http::GET(ZT_AUTO_UPDATE_MAX_HTTP_RESPONSE_SIZE,60000,reinterpret_cast<const struct sockaddr *>(&(*ip)),nfoPath.c_str(),requestHeaders,responseHeaders,body);
			//fprintf(stderr,"UPDATE %s %s %u %lu\n",ip->toString().c_str(),nfoPath.c_str(),scode,body.length());
			if ((scode == 200)&&(body.length() > 0)) {
				/* NFO fields:
				 *
				 * file=<filename>
				 * signedBy=<signing identity>
				 * ed25519=<ed25519 ECC signature of archive>
				 * vMajor=<major version>
				 * vMinor=<minor version>
				 * vRevision=<revision> */
				Dictionary nfo(body);

				unsigned int vMajor = Utils::strToUInt(nfo.get("vMajor","0").c_str());
				unsigned int vMinor = Utils::strToUInt(nfo.get("vMinor","0").c_str());
				unsigned int vRevision = Utils::strToUInt(nfo.get("vRevision","0").c_str());
				if (Utils::compareVersion(vMajor,vMinor,vRevision,ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION) <= 0) {
					//fprintf(stderr,"UPDATE %u.%u.%u is not newer than our version\n",vMajor,vMinor,vRevision);
					return;
				}

				Identity signedBy;
				if ((!signedBy.fromString(nfo.get("signedBy","")))||(!isValidSigningIdentity(signedBy))) {
					//fprintf(stderr,"UPDATE invalid signedBy or not authorized signing identity.\n");
					return;
				}

				std::string filePath(nfo.get("file",""));
				if ((!filePath.length())||(filePath.find("..") != std::string::npos))
					return;
				filePath = httpPath + filePath;

				std::string fileData;
				if (Http::GET(ZT_AUTO_UPDATE_MAX_HTTP_RESPONSE_SIZE,60000,reinterpret_cast<const struct sockaddr *>(&(*ip)),filePath.c_str(),requestHeaders,responseHeaders,fileData) != 200) {
					//fprintf(stderr,"UPDATE GET %s failed\n",filePath.c_str());
					return;
				}

				std::string ed25519(nfo.get("ed25519",""));
				if ((ed25519.length() == 0)||(!signedBy.verify(fileData.data(),(unsigned int)fileData.length(),ed25519.data(),(unsigned int)ed25519.length()))) {
					//fprintf(stderr,"UPDATE %s failed signature check!\n",filePath.c_str());
					return;
				}

				/* --------------------------------------------------------------- */
				/* We made it! Begin OS-specific installation code. */

#ifdef __APPLE__
				/* OSX version is in the form of a MacOSX .pkg file, so we will
				 * launch installer (normally in /usr/sbin) to install it. It will
				 * then turn around and shut down the service, update files, and
				 * relaunch. */
				{
					char bashp[128],pkgp[128];
					Utils::snprintf(bashp,sizeof(bashp),"/tmp/ZeroTierOne-update-%u.%u.%u.sh",vMajor,vMinor,vRevision);
					Utils::snprintf(pkgp,sizeof(pkgp),"/tmp/ZeroTierOne-update-%u.%u.%u.pkg",vMajor,vMinor,vRevision);
					FILE *pkg = fopen(pkgp,"w");
					if ((!pkg)||(fwrite(fileData.data(),fileData.length(),1,pkg) != 1)) {
						fclose(pkg);
						unlink(bashp);
						unlink(pkgp);
						fprintf(stderr,"UPDATE error writing %s\n",pkgp);
						return;
					}
					fclose(pkg);
					FILE *bash = fopen(bashp,"w");
					if (!bash) {
						fclose(pkg);
						unlink(bashp);
						unlink(pkgp);
						fprintf(stderr,"UPDATE error writing %s\n",bashp);
						return;
					}
					fprintf(bash,
						"#!/bin/bash\n"
						"export PATH=/bin:/usr/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/local/sbin\n"
						"sleep 2\n"
						"installer -pkg \"%s\" -target /\n"
						"sleep 1\n"
						"rm -f \"%s\" \"%s\"\n"
						"exit 0\n",
						pkgp,
						pkgp,
						bashp);
					fclose(bash);
					long pid = (long)vfork();
					if (pid == 0) {
						execl("/bin/bash","/bin/bash",bashp,(char *)0);
						exit(0);
					}
				}
#endif // __APPLE__

#ifdef __WINDOWS__
				/* Windows version comes in the form of .MSI package that
				 * takes care of everything. */
				{
					char tempp[512],batp[512],msip[512],cmdline[512];
					if (GetTempPathA(sizeof(tempp),tempp) <= 0)
						return;
					CreateDirectoryA(tempp,(LPSECURITY_ATTRIBUTES)0);
					Utils::snprintf(batp,sizeof(batp),"%s\\ZeroTierOne-update-%u.%u.%u.bat",tempp,vMajor,vMinor,vRevision);
					Utils::snprintf(msip,sizeof(msip),"%s\\ZeroTierOne-update-%u.%u.%u.msi",tempp,vMajor,vMinor,vRevision);
					FILE *msi = fopen(msip,"wb");
					if ((!msi)||(fwrite(fileData.data(),(size_t)fileData.length(),1,msi) != 1)) {
						fclose(msi);
						return;
					}
					fclose(msi);
					FILE *bat = fopen(batp,"wb");
					if (!bat)
						return;
					fprintf(bat,
						"TIMEOUT.EXE /T 1 /NOBREAK\r\n"
						"NET.EXE STOP \"ZeroTierOneService\"\r\n"
						"TIMEOUT.EXE /T 1 /NOBREAK\r\n"
						"MSIEXEC.EXE /i \"%s\" /qn\r\n"
						"TIMEOUT.EXE /T 1 /NOBREAK\r\n"
						"NET.EXE START \"ZeroTierOneService\"\r\n"
						"DEL \"%s\"\r\n"
						"DEL \"%s\"\r\n",
						msip,
						msip,
						batp);
					fclose(bat);
					STARTUPINFOA si;
					PROCESS_INFORMATION pi;
					memset(&si,0,sizeof(si));
					memset(&pi,0,sizeof(pi));
					Utils::snprintf(cmdline,sizeof(cmdline),"CMD.EXE /c \"%s\"",batp);
					CreateProcessA(NULL,cmdline,NULL,NULL,FALSE,CREATE_NO_WINDOW|CREATE_NEW_PROCESS_GROUP,NULL,NULL,&si,&pi);
				}
#endif // __WINDOWS__

				/* --------------------------------------------------------------- */

				return;
			} // else try to fetch from next IP address
		}
	}

	void threadMain()
		throw()
	{
		try {
			this->doUpdateCheck();
		} catch ( ... ) {}
	}
};
static BackgroundSoftwareUpdateChecker backgroundSoftwareUpdateChecker;
#endif // ZT_AUTO_UPDATE

class OneServiceImpl;

static int SnodeVirtualNetworkConfigFunction(ZT1_Node *node,void *uptr,uint64_t nwid,enum ZT1_VirtualNetworkConfigOperation op,const ZT1_VirtualNetworkConfig *nwconf);
static void SnodeEventCallback(ZT1_Node *node,void *uptr,enum ZT1_Event event,const void *metaData);
static long SnodeDataStoreGetFunction(ZT1_Node *node,void *uptr,const char *name,void *buf,unsigned long bufSize,unsigned long readIndex,unsigned long *totalSize);
static int SnodeDataStorePutFunction(ZT1_Node *node,void *uptr,const char *name,const void *data,unsigned long len,int secure);
static int SnodeWirePacketSendFunction(ZT1_Node *node,void *uptr,const struct sockaddr_storage *addr,const void *data,unsigned int len);
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
	Mutex writeBuf_m;
};

class OneServiceImpl : public OneService
{
public:
	OneServiceImpl(const char *hp,unsigned int port,const char *overrideRootTopology) :
		_homePath((hp) ? hp : "."),
		_tcpFallbackResolver(ZT1_TCP_FALLBACK_RELAY),
#ifdef ZT_ENABLE_NETWORK_CONTROLLER
		_controller((_homePath + ZT_PATH_SEPARATOR_S + ZT1_CONTROLLER_DB_PATH).c_str()),
#endif
		_phy(this,false),
		_overrideRootTopology((overrideRootTopology) ? overrideRootTopology : ""),
		_node((Node *)0),
		_controlPlane((ControlPlane *)0),
		_lastDirectReceiveFromGlobal(0),
		_lastSendToGlobal(0),
		_lastRestart(0),
		_nextBackgroundTaskDeadline(0),
		_tcpFallbackTunnel((TcpConnection *)0),
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

#ifdef ZT_ENABLE_NETWORK_CONTROLLER
			_node->setNetconfMaster((void *)&_controller);
#endif

			_controlPlane = new ControlPlane(this,_node,(_homePath + ZT_PATH_SEPARATOR_S + "ui").c_str());
			_controlPlane->addAuthToken(authToken.c_str());

#ifdef ZT_ENABLE_NETWORK_CONTROLLER
			_controlPlane->setController(&_controller);
#endif

			{	// Remember networks from previous session
				std::vector<std::string> networksDotD(OSUtils::listDirectory((_homePath + ZT_PATH_SEPARATOR_S + "networks.d").c_str()));
				for(std::vector<std::string>::iterator f(networksDotD.begin());f!=networksDotD.end();++f) {
					std::size_t dot = f->find_last_of('.');
					if ((dot == 16)&&(f->substr(16) == ".conf"))
						_node->join(Utils::hexStrToU64(f->substr(0,dot).c_str()));
				}
			}

			_nextBackgroundTaskDeadline = 0;
			uint64_t clockShouldBe = OSUtils::now();
			_lastRestart = clockShouldBe;
			uint64_t lastTapMulticastGroupCheck = 0;
			uint64_t lastTcpFallbackResolve = 0;
#ifdef ZT_AUTO_UPDATE
			uint64_t lastSoftwareUpdateCheck = 0;
#endif // ZT_AUTO_UPDATE
			for(;;) {
				_run_m.lock();
				if (!_run) {
					_run_m.unlock();
					_termReason_m.lock();
					_termReason = ONE_NORMAL_TERMINATION;
					_termReason_m.unlock();
					break;
				} else _run_m.unlock();

				uint64_t now = OSUtils::now();

				uint64_t dl = _nextBackgroundTaskDeadline;
				if (dl <= now) {
					_node->processBackgroundTasks(now,&_nextBackgroundTaskDeadline);
					dl = _nextBackgroundTaskDeadline;
				}

				// Attempt to detect sleep/wake events by detecting delay overruns
				if ((now > clockShouldBe)&&((now - clockShouldBe) > 2000))
					_lastRestart = now;

#ifdef ZT_AUTO_UPDATE
				if ((now - lastSoftwareUpdateCheck) >= ZT_AUTO_UPDATE_CHECK_PERIOD) {
					lastSoftwareUpdateCheck = OSUtils::now();
					Thread::start(&backgroundSoftwareUpdateChecker);
				}
#endif // ZT_AUTO_UPDATE

				if ((now - lastTcpFallbackResolve) >= ZT1_TCP_FALLBACK_RERESOLVE_DELAY) {
					lastTcpFallbackResolve = now;
					_tcpFallbackResolver.resolveNow();
				}

				if ((_tcpFallbackTunnel)&&((now - _lastDirectReceiveFromGlobal) < (ZT1_TCP_FALLBACK_AFTER / 2)))
					_phy.close(_tcpFallbackTunnel->sock);

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
				clockShouldBe = now + (uint64_t)delay;
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
			while (!_tcpConnections.empty())
				_phy.close((*_tcpConnections.begin())->sock);
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

	virtual bool tcpFallbackActive() const
	{
		return (_tcpFallbackTunnel != (TcpConnection *)0);
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
#ifdef ZT_BREAK_UDP
		if (OSUtils::fileExists("/tmp/ZT_BREAK_UDP"))
			return;
#endif
		if ((len >= 16)&&(reinterpret_cast<const InetAddress *>(from)->ipScope() == InetAddress::IP_SCOPE_GLOBAL))
			_lastDirectReceiveFromGlobal = OSUtils::now();
		ZT1_ResultCode rc = _node->processWirePacket(
			OSUtils::now(),
			(const struct sockaddr_storage *)from, // Phy<> uses sockaddr_storage, so it'll always be that big
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

		// Outgoing TCP connections are always TCP fallback tunnel connections.

		TcpConnection *tc = new TcpConnection();
		_tcpConnections.insert(tc);

		tc->type = TcpConnection::TCP_TUNNEL_OUTGOING;
		tc->shouldKeepAlive = true;
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

		_tcpFallbackTunnel = tc;
	}

	inline void phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from)
	{
		// Incoming TCP connections are HTTP JSON API requests.

		TcpConnection *tc = new TcpConnection();
		_tcpConnections.insert(tc);

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
		TcpConnection *tc = (TcpConnection *)*uptr;
		if (tc) {
			if (tc == _tcpFallbackTunnel)
				_tcpFallbackTunnel = (TcpConnection *)0;
			_tcpConnections.erase(tc);
			delete tc;
		}
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
				while (tc->body.length() >= 5) {
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

							if (from) {
								ZT1_ResultCode rc = _node->processWirePacket(
									OSUtils::now(),
									reinterpret_cast<struct sockaddr_storage *>(&from),
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
						}

						if (tc->body.length() > (mlen + 5))
							tc->body = tc->body.substr(mlen + 5);
						else tc->body = "";
					} else break;
				}
				break;

		}
	}

	inline void phyOnTcpWritable(PhySocket *sock,void **uptr)
	{
		TcpConnection *tc = reinterpret_cast<TcpConnection *>(*uptr);
		Mutex::Lock _l(tc->writeBuf_m);
		if (tc->writeBuf.length() > 0) {
			long sent = (long)_phy.tcpSend(sock,tc->writeBuf.data(),(unsigned long)tc->writeBuf.length(),true);
			if (sent > 0) {
				tc->lastActivity = OSUtils::now();
				if ((unsigned long)sent >= (unsigned long)tc->writeBuf.length()) {
					tc->writeBuf = "";
					_phy.tcpSetNotifyWritable(sock,false);
					if (!tc->shouldKeepAlive)
						_phy.close(sock); // will call close handler to delete from _tcpConnections
				} else {
					tc->writeBuf = tc->writeBuf.substr(sent);
				}
			}
		} else {
			_phy.tcpSetNotifyWritable(sock,false);
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

	inline int nodeWirePacketSendFunction(const struct sockaddr_storage *addr,const void *data,unsigned int len)
	{
		int result = -1;
		switch(addr->ss_family) {
			case AF_INET:
#ifdef ZT_BREAK_UDP
				if (!OSUtils::fileExists("/tmp/ZT_BREAK_UDP")) {
#endif
				if (_v4UdpSocket)
					result = ((_phy.udpSend(_v4UdpSocket,(const struct sockaddr *)addr,data,len) != 0) ? 0 : -1);
#ifdef ZT_BREAK_UDP
				}
#endif

#ifdef ZT1_TCP_FALLBACK_RELAY
				// TCP fallback tunnel support
				if ((len >= 16)&&(reinterpret_cast<const InetAddress *>(addr)->ipScope() == InetAddress::IP_SCOPE_GLOBAL)) {
					uint64_t now = OSUtils::now();

					// Engage TCP tunnel fallback if we haven't received anything valid from a global
					// IP address in ZT1_TCP_FALLBACK_AFTER milliseconds. If we do start getting
					// valid direct traffic we'll stop using it and close the socket after a while.
					if (((now - _lastDirectReceiveFromGlobal) > ZT1_TCP_FALLBACK_AFTER)&&((now - _lastRestart) > ZT1_TCP_FALLBACK_AFTER)) {
						if (_tcpFallbackTunnel) {
							Mutex::Lock _l(_tcpFallbackTunnel->writeBuf_m);
							if (!_tcpFallbackTunnel->writeBuf.length())
								_phy.tcpSetNotifyWritable(_tcpFallbackTunnel->sock,true);
							unsigned long mlen = len + 7;
							_tcpFallbackTunnel->writeBuf.push_back((char)0x17);
							_tcpFallbackTunnel->writeBuf.push_back((char)0x03);
							_tcpFallbackTunnel->writeBuf.push_back((char)0x03); // fake TLS 1.2 header
							_tcpFallbackTunnel->writeBuf.push_back((char)((mlen >> 8) & 0xff));
							_tcpFallbackTunnel->writeBuf.push_back((char)(mlen & 0xff));
							_tcpFallbackTunnel->writeBuf.push_back((char)4); // IPv4
							_tcpFallbackTunnel->writeBuf.append(reinterpret_cast<const char *>(reinterpret_cast<const void *>(&(reinterpret_cast<const struct sockaddr_in *>(addr)->sin_addr.s_addr))),4);
							_tcpFallbackTunnel->writeBuf.append(reinterpret_cast<const char *>(reinterpret_cast<const void *>(&(reinterpret_cast<const struct sockaddr_in *>(addr)->sin_port))),2);
							_tcpFallbackTunnel->writeBuf.append((const char *)data,len);
							result = 0;
						} else if (((now - _lastSendToGlobal) < ZT1_TCP_FALLBACK_AFTER)&&((now - _lastSendToGlobal) > (ZT_PING_CHECK_INVERVAL / 2))) {
							std::vector<InetAddress> tunnelIps(_tcpFallbackResolver.get());
							if (tunnelIps.empty()) {
								if (!_tcpFallbackResolver.running())
									_tcpFallbackResolver.resolveNow();
							} else {
								bool connected = false;
								InetAddress addr(tunnelIps[(unsigned long)now % tunnelIps.size()]);
								addr.setPort(ZT1_TCP_FALLBACK_RELAY_PORT);
								_phy.tcpConnect(reinterpret_cast<const struct sockaddr *>(&addr),connected);
							}
						}
					}

					_lastSendToGlobal = now;
				}
#endif // ZT1_TCP_FALLBACK_RELAY

				break;
			case AF_INET6:
#ifdef ZT_BREAK_UDP
				if (!OSUtils::fileExists("/tmp/ZT_BREAK_UDP")) {
#endif
				if (_v6UdpSocket)
					result = ((_phy.udpSend(_v6UdpSocket,(const struct sockaddr *)addr,data,len) != 0) ? 0 : -1);
#ifdef ZT_BREAK_UDP
				}
#endif
				break;
			default:
				return -1;
		}
		return result;
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
		{
			Mutex::Lock _l(tc->writeBuf_m);
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
		}

		_phy.tcpSetNotifyWritable(tc->sock,true);
	}

	inline void onHttpResponseFromClient(TcpConnection *tc)
	{
		if (!tc->shouldKeepAlive)
			_phy.close(tc->sock); // will call close handler, which deletes from _tcpConnections
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
	BackgroundResolver _tcpFallbackResolver;
#ifdef ZT_ENABLE_NETWORK_CONTROLLER
	SqliteNetworkController _controller;
#endif
	Phy<OneServiceImpl *> _phy;
	std::string _overrideRootTopology;
	Node *_node;
	PhySocket *_v4UdpSocket;
	PhySocket *_v6UdpSocket;
	PhySocket *_v4TcpListenSocket;
	PhySocket *_v6TcpListenSocket;
	ControlPlane *_controlPlane;
	uint64_t _lastDirectReceiveFromGlobal;
	uint64_t _lastSendToGlobal;
	uint64_t _lastRestart;
	volatile uint64_t _nextBackgroundTaskDeadline;

	std::map< uint64_t,EthernetTap * > _taps;
	std::map< uint64_t,std::vector<InetAddress> > _tapAssignedIps; // ZeroTier assigned IPs, not user or dhcp assigned
	Mutex _taps_m;

	std::set< TcpConnection * > _tcpConnections; // no mutex for this since it's done in the main loop thread only
	TcpConnection *_tcpFallbackTunnel;

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
static int SnodeWirePacketSendFunction(ZT1_Node *node,void *uptr,const struct sockaddr_storage *addr,const void *data,unsigned int len)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodeWirePacketSendFunction(addr,data,len); }
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

} // anonymous namespace

std::string OneService::platformDefaultHomePath()
{
#ifdef __UNIX_LIKE__

#ifdef __APPLE__
	// /Library/... on Apple
	return std::string("/Library/Application Support/ZeroTier/One");
#else

#ifdef __BSD__
	// BSD likes /var/db instead of /var/lib
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

std::string OneService::autoUpdateUrl()
{
#ifdef ZT_AUTO_UPDATE

/*
#if defined(__LINUX__) && ( defined(__i386__) || defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__i386) )
	if (sizeof(void *) == 8)
		return "http://download.zerotier.com/ZeroTierOneInstaller-linux-x64-LATEST.nfo";
	else return "http://download.zerotier.com/ZeroTierOneInstaller-linux-x86-LATEST.nfo";
#endif
*/

#if defined(__APPLE__) && ( defined(__i386__) || defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__i386) )
	return "http://download.zerotier.com/update/mac_intel/";
#endif

#ifdef __WINDOWS__
	return "http://download.zerotier.com/update/win_intel/";
#endif

#endif // ZT_AUTO_UPDATE
	return std::string();
}

OneService *OneService::newInstance(const char *hp,unsigned int port,const char *overrideRootTopology) { return new OneServiceImpl(hp,port,overrideRootTopology); }
OneService::~OneService() {}

} // namespace ZeroTier
