/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
#include "../osdep/PortMapper.hpp"

#include "OneService.hpp"
#include "ControlPlane.hpp"
#include "ClusterGeoIpService.hpp"
#include "ClusterDefinition.hpp"

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
#include <WinSock2.h>
#include <Windows.h>
#include <ShlObj.h>
#include <netioapi.h>
#include <iphlpapi.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ifaddrs.h>
#endif

// Include the right tap device driver for this platform -- add new platforms here
#ifdef ZT_SERVICE_NETCON

// In network containers builds, use the virtual netcon endpoint instead of a tun/tap port driver
#include "../netcon/NetconEthernetTap.hpp"
namespace ZeroTier { typedef NetconEthernetTap EthernetTap; }

#else // not ZT_SERVICE_NETCON so pick a tap driver

#ifdef __APPLE__
#include "../osdep/OSXEthernetTap.hpp"
namespace ZeroTier { typedef OSXEthernetTap EthernetTap; }
#endif // __APPLE__
#ifdef __LINUX__
#include "../osdep/LinuxEthernetTap.hpp"
namespace ZeroTier { typedef LinuxEthernetTap EthernetTap; }
#endif // __LINUX__
#ifdef __WINDOWS__
#include "../osdep/WindowsEthernetTap.hpp"
namespace ZeroTier { typedef WindowsEthernetTap EthernetTap; }
#endif // __WINDOWS__
#ifdef __FreeBSD__
#include "../osdep/BSDEthernetTap.hpp"
namespace ZeroTier { typedef BSDEthernetTap EthernetTap; }
#endif // __FreeBSD__

#endif // ZT_SERVICE_NETCON

// Sanity limits for HTTP
#define ZT_MAX_HTTP_MESSAGE_SIZE (1024 * 1024 * 64)
#define ZT_MAX_HTTP_CONNECTIONS 64

// Interface metric for ZeroTier taps
#define ZT_IF_METRIC 32768

// How often to check for new multicast subscriptions on a tap device
#define ZT_TAP_CHECK_MULTICAST_INTERVAL 5000

// Path under ZT1 home for controller database if controller is enabled
#define ZT_CONTROLLER_DB_PATH "controller.db"

// TCP fallback relay host -- geo-distributed using Amazon Route53 geo-aware DNS
#define ZT_TCP_FALLBACK_RELAY "tcp-fallback.zerotier.com"
#define ZT_TCP_FALLBACK_RELAY_PORT 443

// Frequency at which we re-resolve the TCP fallback relay
#define ZT_TCP_FALLBACK_RERESOLVE_DELAY 86400000

// Attempt to engage TCP fallback after this many ms of no reply to packets sent to global-scope IPs
#define ZT_TCP_FALLBACK_AFTER 60000

// How often to check for local interface addresses
#define ZT_LOCAL_INTERFACE_CHECK_INTERVAL 300000

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
			/* 0001 - 0004 : obsolete, used in old versions */
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

				std::string ed25519(Utils::unhex(nfo.get("ed25519","")));
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
						"sleep 1\n"
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
						setsid(); // detach from parent so that shell isn't killed when parent is killed
						signal(SIGHUP,SIG_IGN);
						signal(SIGTERM,SIG_IGN);
						signal(SIGQUIT,SIG_IGN);
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

static bool isBlacklistedLocalInterfaceForZeroTierTraffic(const char *ifn)
{
#if defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)
	if ((ifn[0] == 'l')&&(ifn[1] == 'o')) return true; // loopback
	if ((ifn[0] == 'z')&&(ifn[1] == 't')) return true; // sanity check: zt#
	if ((ifn[0] == 't')&&(ifn[1] == 'u')&&(ifn[2] == 'n')) return true; // tun# is probably an OpenVPN tunnel or similar
	if ((ifn[0] == 't')&&(ifn[1] == 'a')&&(ifn[2] == 'p')) return true; // tap# is probably an OpenVPN tunnel or similar
#endif

#ifdef __APPLE__
	if ((ifn[0] == 'l')&&(ifn[1] == 'o')) return true; // loopback
	if ((ifn[0] == 'z')&&(ifn[1] == 't')) return true; // sanity check: zt#
	if ((ifn[0] == 't')&&(ifn[1] == 'u')&&(ifn[2] == 'n')) return true; // tun# is probably an OpenVPN tunnel or similar
	if ((ifn[0] == 't')&&(ifn[1] == 'a')&&(ifn[2] == 'p')) return true; // tap# is probably an OpenVPN tunnel or similar
	if ((ifn[0] == 'u')&&(ifn[1] == 't')&&(ifn[2] == 'u')&&(ifn[3] == 'n')) return true; // ... as is utun#
#endif

	return false;
}

static std::string _trimString(const std::string &s)
{
	unsigned long end = (unsigned long)s.length();
	while (end) {
		char c = s[end - 1];
		if ((c == ' ')||(c == '\r')||(c == '\n')||(!c)||(c == '\t'))
			--end;
		else break;
	}
	unsigned long start = 0;
	while (start < end) {
		char c = s[start];
		if ((c == ' ')||(c == '\r')||(c == '\n')||(!c)||(c == '\t'))
			++start;
		else break;
	}
	return s.substr(start,end - start);
}

class OneServiceImpl;

static int SnodeVirtualNetworkConfigFunction(ZT_Node *node,void *uptr,uint64_t nwid,void **nuptr,enum ZT_VirtualNetworkConfigOperation op,const ZT_VirtualNetworkConfig *nwconf);
static void SnodeEventCallback(ZT_Node *node,void *uptr,enum ZT_Event event,const void *metaData);
static long SnodeDataStoreGetFunction(ZT_Node *node,void *uptr,const char *name,void *buf,unsigned long bufSize,unsigned long readIndex,unsigned long *totalSize);
static int SnodeDataStorePutFunction(ZT_Node *node,void *uptr,const char *name,const void *data,unsigned long len,int secure);
static int SnodeWirePacketSendFunction(ZT_Node *node,void *uptr,const struct sockaddr_storage *localAddr,const struct sockaddr_storage *addr,const void *data,unsigned int len,unsigned int ttl);
static void SnodeVirtualNetworkFrameFunction(ZT_Node *node,void *uptr,uint64_t nwid,void **nuptr,uint64_t sourceMac,uint64_t destMac,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len);
static int SnodePathCheckFunction(ZT_Node *node,void *uptr,const struct sockaddr_storage *localAddr,const struct sockaddr_storage *remoteAddr);

#ifdef ZT_ENABLE_CLUSTER
static void SclusterSendFunction(void *uptr,unsigned int toMemberId,const void *data,unsigned int len);
static int SclusterGeoIpFunction(void *uptr,const struct sockaddr_storage *addr,int *x,int *y,int *z);
#endif

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

// Use a bigger buffer on AMD64 since these are likely to be bigger and
// servers. Otherwise use a smaller buffer. This makes no difference
// except under very high load.
#if (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__))
#define ZT_UDP_DESIRED_BUF_SIZE 1048576
#else
#define ZT_UDP_DESIRED_BUF_SIZE 131072
#endif

// Used to pseudo-randomize local source port picking
static volatile unsigned int _udpPortPickerCounter = 0;

class OneServiceImpl : public OneService
{
public:
	OneServiceImpl(const char *hp,unsigned int port) :
		_homePath((hp) ? hp : ".")
		,_tcpFallbackResolver(ZT_TCP_FALLBACK_RELAY)
#ifdef ZT_ENABLE_NETWORK_CONTROLLER
		,_controller((SqliteNetworkController *)0)
#endif
		,_phy(this,false,true)
		,_node((Node *)0)
		,_controlPlane((ControlPlane *)0)
		,_lastDirectReceiveFromGlobal(0)
#ifdef ZT_TCP_FALLBACK_RELAY
		,_lastSendToGlobalV4(0)
#endif
		,_lastRestart(0)
		,_nextBackgroundTaskDeadline(0)
		,_tcpFallbackTunnel((TcpConnection *)0)
		,_termReason(ONE_STILL_RUNNING)
		,_port(0)
#ifdef ZT_USE_MINIUPNPC
		,_portMapper((PortMapper *)0)
#endif
#ifdef ZT_ENABLE_CLUSTER
		,_clusterMessageSocket((PhySocket *)0)
		,_clusterGeoIpService((ClusterGeoIpService *)0)
		,_clusterDefinition((ClusterDefinition *)0)
		,_clusterMemberId(0)
#endif
		,_run(true)
	{
		memset((void *)_udp,0,sizeof(_udp));

		const int portTrials = (port == 0) ? 256 : 1; // if port is 0, pick random
		for(int k=0;k<portTrials;++k) {
			if (port == 0) {
				unsigned int randp = 0;
				Utils::getSecureRandom(&randp,sizeof(randp));
				port = 40000 + (randp % 25500);
			}

			_udp[0].v4a = InetAddress((uint32_t)0,port);
			_udp[0].v4s = _phy.udpBind((const struct sockaddr *)&(_udp[0].v4a),(void *)&(_udp[0].v4a),ZT_UDP_DESIRED_BUF_SIZE);

			if (_udp[0].v4s) {
				struct sockaddr_in in4;
				memset(&in4,0,sizeof(in4));
				in4.sin_family = AF_INET;
				in4.sin_addr.s_addr = Utils::hton((uint32_t)0x7f000001); // right now we just listen for TCP @127.0.0.1
				in4.sin_port = Utils::hton((uint16_t)port);
				_v4TcpListenSocket = _phy.tcpListen((const struct sockaddr *)&in4,this);

				if (_v4TcpListenSocket) {
					_udp[0].v6a = InetAddress("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",16,port);
					_udp[0].v6s = _phy.udpBind((const struct sockaddr *)&(_udp[0].v6a),(void *)&(_udp[0].v6a),ZT_UDP_DESIRED_BUF_SIZE);

					struct sockaddr_in6 in6;
					memset((void *)&in6,0,sizeof(in6));
					in6.sin6_family = AF_INET6;
					in6.sin6_port = in4.sin_port;
					in6.sin6_addr.s6_addr[15] = 1; // IPv6 localhost == ::1
					_v6TcpListenSocket = _phy.tcpListen((const struct sockaddr *)&in6,this);

					_port = port;
					break; // success!
				} else {
					_phy.close(_udp[0].v4s,false);
				}
			}

			port = 0;
		}

		if (_port == 0)
			throw std::runtime_error("cannot bind to port");

		char portstr[64];
		Utils::snprintf(portstr,sizeof(portstr),"%u",_port);
		OSUtils::writeFile((_homePath + ZT_PATH_SEPARATOR_S + "zerotier-one.port").c_str(),std::string(portstr));
	}

	virtual ~OneServiceImpl()
	{
		for(int i=0;i<3;++i) {
			if (_udp[i].v4s)
				_phy.close(_udp[i].v4s);
			if (_udp[i].v6s)
				_phy.close(_udp[i].v6s);
		}
		_phy.close(_v4TcpListenSocket);
		_phy.close(_v6TcpListenSocket);
#ifdef ZT_ENABLE_CLUSTER
		_phy.close(_clusterMessageSocket);
#endif
#ifdef ZT_USE_MINIUPNPC
		delete _portMapper;
#endif
#ifdef ZT_ENABLE_NETWORK_CONTROLLER
		delete _controller;
#endif
#ifdef ZT_ENABLE_CLUSTER
		delete _clusterGeoIpService;
		delete _clusterDefinition;
#endif
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
			authToken = _trimString(authToken);

			_node = new Node(
				OSUtils::now(),
				this,
				SnodeDataStoreGetFunction,
				SnodeDataStorePutFunction,
				SnodeWirePacketSendFunction,
				SnodeVirtualNetworkFrameFunction,
				SnodeVirtualNetworkConfigFunction,
				SnodePathCheckFunction,
				SnodeEventCallback);

			// Bind secondary randomized port. If this fails we continue anyway.
			for(int k=0;k<512;++k) {
				const unsigned int randomizedPort = 40000 + (((unsigned int)_node->address() + k) % 25500);
				_udp[1].v4a = InetAddress(0,randomizedPort);
				_udp[1].v4s = _phy.udpBind((const struct sockaddr *)&(_udp[1].v4a),(void *)&(_udp[1].v4a),ZT_UDP_DESIRED_BUF_SIZE);
				if (_udp[1].v4s) {
					_udp[1].v6a = InetAddress("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",16,randomizedPort);
					_udp[1].v6s = _phy.udpBind((const struct sockaddr *)&(_udp[1].v6a),(void *)&(_udp[1].v6a),ZT_UDP_DESIRED_BUF_SIZE);
					if (_udp[1].v6s) {
						break;
					} else {
						_phy.close(_udp[1].v4s);
						_udp[1].v4s = (PhySocket *)0;
					}
				}
			}

#ifdef ZT_USE_MINIUPNPC
			// Bind tertiary uPnP/NAT-PMP redirect port. If this succeeds start port mapper.
			for(int k=0;k<512;++k) {
				const unsigned int mapperPort = 40000 + (((_port + 1) * (k + 1)) % 25500);
				char uniqueName[64];
				_udp[2].v4a = InetAddress(0,mapperPort);
				_udp[2].v4s = _phy.udpBind((const struct sockaddr *)&(_udp[2].v4a),(void *)&(_udp[2].v4a),ZT_UDP_DESIRED_BUF_SIZE);
				if (_udp[2].v4s) {
					Utils::snprintf(uniqueName,sizeof(uniqueName),"ZeroTier/%.10llx",_node->address());
					_portMapper = new PortMapper(mapperPort,uniqueName);

					_udp[2].v6a = InetAddress("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",16,mapperPort);
					_udp[2].v6s = _phy.udpBind((const struct sockaddr *)&(_udp[2].v6a),(void *)&(_udp[2].v6a),ZT_UDP_DESIRED_BUF_SIZE); // okay if this fails, but it shouldn't

					break;
				}
			}
#endif

#ifdef ZT_ENABLE_NETWORK_CONTROLLER
			_controller = new SqliteNetworkController(_node,(_homePath + ZT_PATH_SEPARATOR_S + ZT_CONTROLLER_DB_PATH).c_str(),(_homePath + ZT_PATH_SEPARATOR_S + "circuitTestResults.d").c_str());
			_node->setNetconfMaster((void *)_controller);
#endif

#ifdef ZT_ENABLE_CLUSTER
			if (OSUtils::fileExists((_homePath + ZT_PATH_SEPARATOR_S + "cluster").c_str())) {
				_clusterDefinition = new ClusterDefinition(_node->address(),(_homePath + ZT_PATH_SEPARATOR_S + "cluster").c_str());
				if (_clusterDefinition->size() > 0) {
					std::vector<ClusterDefinition::MemberDefinition> members(_clusterDefinition->members());
					for(std::vector<ClusterDefinition::MemberDefinition>::iterator m(members.begin());m!=members.end();++m) {
						PhySocket *cs = _phy.udpBind(reinterpret_cast<const struct sockaddr *>(&(m->clusterEndpoint)));
						if (cs) {
							if (_clusterMessageSocket) {
								_phy.close(_clusterMessageSocket,false);
								_phy.close(cs,false);

								Mutex::Lock _l(_termReason_m);
								_termReason = ONE_UNRECOVERABLE_ERROR;
								_fatalErrorMessage = "Cluster: can't determine my cluster member ID: able to bind more than one cluster message socket IP/port!";
								return _termReason;
							}
							_clusterMessageSocket = cs;
							_clusterMemberId = m->id;
						}
					}

					if (!_clusterMessageSocket) {
						Mutex::Lock _l(_termReason_m);
						_termReason = ONE_UNRECOVERABLE_ERROR;
						_fatalErrorMessage = "Cluster: can't determine my cluster member ID: unable to bind to any cluster message socket IP/port.";
						return _termReason;
					}

					if (OSUtils::fileExists((_homePath + ZT_PATH_SEPARATOR_S + "cluster-geo.exe").c_str()))
						_clusterGeoIpService = new ClusterGeoIpService((_homePath + ZT_PATH_SEPARATOR_S + "cluster-geo.exe").c_str());

					const ClusterDefinition::MemberDefinition &me = (*_clusterDefinition)[_clusterMemberId];
					InetAddress endpoints[255];
					unsigned int numEndpoints = 0;
					for(std::vector<InetAddress>::const_iterator i(me.zeroTierEndpoints.begin());i!=me.zeroTierEndpoints.end();++i)
						endpoints[numEndpoints++] = *i;

					if (_node->clusterInit(
						_clusterMemberId,
						reinterpret_cast<const struct sockaddr_storage *>(endpoints),
						numEndpoints,
						me.x,
						me.y,
						me.z,
						&SclusterSendFunction,
						this,
						(_clusterGeoIpService) ? &SclusterGeoIpFunction : 0,
						this) == ZT_RESULT_OK) {

						std::vector<ClusterDefinition::MemberDefinition> members(_clusterDefinition->members());
						for(std::vector<ClusterDefinition::MemberDefinition>::iterator m(members.begin());m!=members.end();++m) {
							if (m->id != _clusterMemberId)
								_node->clusterAddMember(m->id);
						}

					}
				} else {
					delete _clusterDefinition;
					_clusterDefinition = (ClusterDefinition *)0;
				}
			}
#endif

			_controlPlane = new ControlPlane(this,_node,(_homePath + ZT_PATH_SEPARATOR_S + "ui").c_str());
			_controlPlane->addAuthToken(authToken.c_str());

#ifdef ZT_ENABLE_NETWORK_CONTROLLER
			_controlPlane->setController(_controller);
#endif

			{	// Remember networks from previous session
				std::vector<std::string> networksDotD(OSUtils::listDirectory((_homePath + ZT_PATH_SEPARATOR_S + "networks.d").c_str()));
				for(std::vector<std::string>::iterator f(networksDotD.begin());f!=networksDotD.end();++f) {
					std::size_t dot = f->find_last_of('.');
					if ((dot == 16)&&(f->substr(16) == ".conf"))
						_node->join(Utils::hexStrToU64(f->substr(0,dot).c_str()),(void *)0);
				}
			}

			// Start two background threads to handle expensive ops out of line
			Thread::start(_node);
			Thread::start(_node);

			_nextBackgroundTaskDeadline = 0;
			uint64_t clockShouldBe = OSUtils::now();
			_lastRestart = clockShouldBe;
			uint64_t lastTapMulticastGroupCheck = 0;
			uint64_t lastTcpFallbackResolve = 0;
			uint64_t lastLocalInterfaceAddressCheck = (OSUtils::now() - ZT_LOCAL_INTERFACE_CHECK_INTERVAL) + 15000; // do this in 15s to give portmapper time to configure and other things time to settle
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
					lastSoftwareUpdateCheck = now;
					Thread::start(&backgroundSoftwareUpdateChecker);
				}
#endif // ZT_AUTO_UPDATE

				if ((now - lastTcpFallbackResolve) >= ZT_TCP_FALLBACK_RERESOLVE_DELAY) {
					lastTcpFallbackResolve = now;
					_tcpFallbackResolver.resolveNow();
				}

				if ((_tcpFallbackTunnel)&&((now - _lastDirectReceiveFromGlobal) < (ZT_TCP_FALLBACK_AFTER / 2)))
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

				if ((now - lastLocalInterfaceAddressCheck) >= ZT_LOCAL_INTERFACE_CHECK_INTERVAL) {
					lastLocalInterfaceAddressCheck = now;

				_node->clearLocalInterfaceAddresses();
#ifdef ZT_USE_MINIUPNPC
				std::vector<InetAddress> mappedAddresses(_portMapper->get());
				for(std::vector<InetAddress>::const_iterator ext(mappedAddresses.begin());ext!=mappedAddresses.end();++ext)
					_node->addLocalInterfaceAddress(reinterpret_cast<const struct sockaddr_storage *>(&(*ext)));
#endif

#ifdef __UNIX_LIKE__
					std::vector<std::string> ztDevices;
					{
						Mutex::Lock _l(_taps_m);
						for(std::map< uint64_t,EthernetTap *>::const_iterator t(_taps.begin());t!=_taps.end();++t)
							ztDevices.push_back(t->second->deviceName());
					}
					struct ifaddrs *ifatbl = (struct ifaddrs *)0;
					if ((getifaddrs(&ifatbl) == 0)&&(ifatbl)) {
						struct ifaddrs *ifa = ifatbl;
						while (ifa) {
							if ((ifa->ifa_name)&&(ifa->ifa_addr)&&(!isBlacklistedLocalInterfaceForZeroTierTraffic(ifa->ifa_name))) {
								bool isZT = false;
								for(std::vector<std::string>::const_iterator d(ztDevices.begin());d!=ztDevices.end();++d) {
									if (*d == ifa->ifa_name) {
										isZT = true;
										break;
									}
								}
								if (!isZT) {
									InetAddress ip(ifa->ifa_addr);
									ip.setPort(_port);
									_node->addLocalInterfaceAddress(reinterpret_cast<const struct sockaddr_storage *>(&ip));
								}
							}
							ifa = ifa->ifa_next;
						}
						freeifaddrs(ifatbl);
					}
#endif // __UNIX_LIKE__

#ifdef __WINDOWS__
					std::vector<NET_LUID> ztDevices;
					{
						Mutex::Lock _l(_taps_m);
						for(std::map< uint64_t,EthernetTap *>::const_iterator t(_taps.begin());t!=_taps.end();++t)
							ztDevices.push_back(t->second->luid());
					}
					char aabuf[16384];
					ULONG aalen = sizeof(aabuf);
					if (GetAdaptersAddresses(AF_UNSPEC,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,(void *)0,reinterpret_cast<PIP_ADAPTER_ADDRESSES>(aabuf),&aalen) == NO_ERROR) {
						PIP_ADAPTER_ADDRESSES a = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(aabuf);
						while (a) {
							bool isZT = false;
							for(std::vector<NET_LUID>::const_iterator d(ztDevices.begin());d!=ztDevices.end();++d) {
								if (a->Luid.Value == d->Value) {
									isZT = true;
									break;
								}
							}
							if (!isZT) {
								PIP_ADAPTER_UNICAST_ADDRESS ua = a->FirstUnicastAddress;
								while (ua) {
									InetAddress ip(ua->Address.lpSockaddr);
									ip.setPort(_port);
									_node->addLocalInterfaceAddress(reinterpret_cast<const struct sockaddr_storage *>(&ip));
									ua = ua->Next;
								}
							}
							a = a->Next;
						}
					}
#endif // __WINDOWS__
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
#ifdef ZT_ENABLE_CLUSTER
		if (sock == _clusterMessageSocket) {
			_lastDirectReceiveFromGlobal = OSUtils::now();
			_node->clusterHandleIncomingMessage(data,len);
			return;
		}
#endif

#ifdef ZT_BREAK_UDP
		if (OSUtils::fileExists("/tmp/ZT_BREAK_UDP"))
			return;
#endif

		if ((len >= 16)&&(reinterpret_cast<const InetAddress *>(from)->ipScope() == InetAddress::IP_SCOPE_GLOBAL))
			_lastDirectReceiveFromGlobal = OSUtils::now();

		const ZT_ResultCode rc = _node->processWirePacket(
			OSUtils::now(),
			reinterpret_cast<const struct sockaddr_storage *>(*uptr), // *uptr points to InetAddress/sockaddr of local listen port
			(const struct sockaddr_storage *)from, // Phy<> uses sockaddr_storage, so it'll always be that big
			data,
			len,
			&_nextBackgroundTaskDeadline);
		if (ZT_ResultCode_isFatal(rc)) {
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
		_phy.setNotifyWritable(sock,true);

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
								const ZT_ResultCode rc = _node->processWirePacket(
									OSUtils::now(),
									reinterpret_cast<struct sockaddr_storage *>(&(_udp[0].v4a)), // TCP tunneled packets are "from" the default local port's address
									reinterpret_cast<struct sockaddr_storage *>(&from),
									data,
									plen,
									&_nextBackgroundTaskDeadline);
								if (ZT_ResultCode_isFatal(rc)) {
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
			long sent = (long)_phy.streamSend(sock,tc->writeBuf.data(),(unsigned long)tc->writeBuf.length(),true);
			if (sent > 0) {
				tc->lastActivity = OSUtils::now();
				if ((unsigned long)sent >= (unsigned long)tc->writeBuf.length()) {
					tc->writeBuf = "";
					_phy.setNotifyWritable(sock,false);
					if (!tc->shouldKeepAlive)
						_phy.close(sock); // will call close handler to delete from _tcpConnections
				} else {
					tc->writeBuf = tc->writeBuf.substr(sent);
				}
			}
		} else {
			_phy.setNotifyWritable(sock,false);
		}
	}

	inline void phyOnFileDescriptorActivity(PhySocket *sock,void **uptr,bool readable,bool writable) {}
	inline void phyOnUnixAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN) {}
	inline void phyOnUnixClose(PhySocket *sock,void **uptr) {}
	inline void phyOnUnixData(PhySocket *sock,void **uptr,void *data,unsigned long len) {}
	inline void phyOnUnixWritable(PhySocket *sock,void **uptr,bool lwip_invoked) {}

	inline int nodeVirtualNetworkConfigFunction(uint64_t nwid,void **nuptr,enum ZT_VirtualNetworkConfigOperation op,const ZT_VirtualNetworkConfig *nwc)
	{
		Mutex::Lock _l(_taps_m);
		std::map< uint64_t,EthernetTap * >::iterator t(_taps.find(nwid));
		switch(op) {
			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP:
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
						*nuptr = (void *)t->second;
					} catch (std::exception &exc) {
#ifdef __WINDOWS__
						FILE *tapFailLog = fopen((_homePath + ZT_PATH_SEPARATOR_S"port_error_log.txt").c_str(),"a");
						if (tapFailLog) {
							fprintf(tapFailLog,"%.16llx: %s"ZT_EOL_S,(unsigned long long)nwid,exc.what());
							fclose(tapFailLog);
						}
#else
						fprintf(stderr,"ERROR: unable to configure virtual network port: %s"ZT_EOL_S,exc.what());
#endif
						return -999;
					} catch ( ... ) {
						return -999; // tap init failed
					}
				}
				// fall through...
			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE:
				if (t != _taps.end()) {
					t->second->setEnabled(nwc->enabled != 0);

					std::vector<InetAddress> &assignedIps = _tapAssignedIps[nwid];
					std::vector<InetAddress> newAssignedIps;
					for(unsigned int i=0;i<nwc->assignedAddressCount;++i)
						newAssignedIps.push_back(InetAddress(nwc->assignedAddresses[i]));
					std::sort(newAssignedIps.begin(),newAssignedIps.end());
					newAssignedIps.erase(std::unique(newAssignedIps.begin(),newAssignedIps.end()),newAssignedIps.end());
					for(std::vector<InetAddress>::iterator ip(newAssignedIps.begin());ip!=newAssignedIps.end();++ip) {
						if (!std::binary_search(assignedIps.begin(),assignedIps.end(),*ip))
							if (!t->second->addIp(*ip))
								fprintf(stderr,"ERROR: unable to add ip address %s"ZT_EOL_S, ip->toString().c_str());
					}
					for(std::vector<InetAddress>::iterator ip(assignedIps.begin());ip!=assignedIps.end();++ip) {
						if (!std::binary_search(newAssignedIps.begin(),newAssignedIps.end(),*ip))
							if (!t->second->removeIp(*ip))
								fprintf(stderr,"ERROR: unable to remove ip address %s"ZT_EOL_S, ip->toString().c_str());
					}
					assignedIps.swap(newAssignedIps);
				} else {
					return -999; // tap init failed
				}
				break;
			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN:
			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY:
				if (t != _taps.end()) {
#ifdef __WINDOWS__
					std::string winInstanceId(t->second->instanceId());
#endif
					*nuptr = (void *)0;
					delete t->second;
					_taps.erase(t);
					_tapAssignedIps.erase(nwid);
#ifdef __WINDOWS__
					if ((op == ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY)&&(winInstanceId.length() > 0))
						WindowsEthernetTap::deletePersistentTapDevice(winInstanceId.c_str());
#endif
				}
				break;
		}
		return 0;
	}

	inline void nodeEventCallback(enum ZT_Event event,const void *metaData)
	{
		switch(event) {
			case ZT_EVENT_FATAL_ERROR_IDENTITY_COLLISION: {
				Mutex::Lock _l(_termReason_m);
				_termReason = ONE_IDENTITY_COLLISION;
				_fatalErrorMessage = "identity/address collision";
				this->terminate();
			}	break;

			case ZT_EVENT_TRACE: {
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

	inline int nodeWirePacketSendFunction(const struct sockaddr_storage *localAddr,const struct sockaddr_storage *addr,const void *data,unsigned int len,unsigned int ttl)
	{
		PhySocket *froms = (PhySocket *)0;

		if (addr->ss_family == AF_INET) {
			if (reinterpret_cast<const struct sockaddr_in *>(localAddr)->sin_port == 0) {
				// If sender specifies any local address, use secondary port 1/4 times
				froms = _udp[(++_udpPortPickerCounter & 0x4) >> 2].v4s;
			} else {
				// If sender specifies a local address, find it by just checking port since right now we always bind wildcard
				for(int k=1;k<=2;++k) {
					// Match fast on port only, since right now we always bind wildcard
					if (reinterpret_cast<const struct sockaddr_in *>(&(_udp[k].v4a))->sin_port == reinterpret_cast<const struct sockaddr_in *>(localAddr)->sin_port) {
						froms = _udp[k].v4s;
						break;
					}
				}
			}
			if (!froms)
				froms = _udp[0].v4s;

#ifdef ZT_TCP_FALLBACK_RELAY
			// TCP fallback tunnel support, currently IPv4 only
			if ((len >= 16)&&(reinterpret_cast<const InetAddress *>(addr)->ipScope() == InetAddress::IP_SCOPE_GLOBAL)) {
				// Engage TCP tunnel fallback if we haven't received anything valid from a global
				// IP address in ZT_TCP_FALLBACK_AFTER milliseconds. If we do start getting
				// valid direct traffic we'll stop using it and close the socket after a while.
				const uint64_t now = OSUtils::now();
				if (((now - _lastDirectReceiveFromGlobal) > ZT_TCP_FALLBACK_AFTER)&&((now - _lastRestart) > ZT_TCP_FALLBACK_AFTER)) {
					if (_tcpFallbackTunnel) {
						Mutex::Lock _l(_tcpFallbackTunnel->writeBuf_m);
						if (!_tcpFallbackTunnel->writeBuf.length())
							_phy.setNotifyWritable(_tcpFallbackTunnel->sock,true);
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
					} else if (((now - _lastSendToGlobalV4) < ZT_TCP_FALLBACK_AFTER)&&((now - _lastSendToGlobalV4) > (ZT_PING_CHECK_INVERVAL / 2))) {
						std::vector<InetAddress> tunnelIps(_tcpFallbackResolver.get());
						if (tunnelIps.empty()) {
							if (!_tcpFallbackResolver.running())
								_tcpFallbackResolver.resolveNow();
						} else {
							bool connected = false;
							InetAddress addr(tunnelIps[(unsigned long)now % tunnelIps.size()]);
							addr.setPort(ZT_TCP_FALLBACK_RELAY_PORT);
							_phy.tcpConnect(reinterpret_cast<const struct sockaddr *>(&addr),connected);
						}
					}
				}
				_lastSendToGlobalV4 = now;
			}
#endif // ZT_TCP_FALLBACK_RELAY
		} else if (addr->ss_family == AF_INET6) {
			if (reinterpret_cast<const struct sockaddr_in6 *>(localAddr)->sin6_port != 0) {
				// If sender specifies a local address, find it by just checking port since right now we always bind wildcard
				for(int k=1;k<=2;++k) {
					// Match fast on port only, since right now we always bind wildcard
					if (reinterpret_cast<const struct sockaddr_in6 *>(&(_udp[k].v6a))->sin6_port == reinterpret_cast<const struct sockaddr_in6 *>(localAddr)->sin6_port) {
						froms = _udp[k].v6s;
						break;
					}
				}
			}
			if (!froms)
				froms = _udp[0].v6s;
		} else {
			return -1;
		}

#ifdef ZT_BREAK_UDP
		if (OSUtils::fileExists("/tmp/ZT_BREAK_UDP"))
			return 0; // silently break UDP
#endif

		if ((ttl)&&(addr->ss_family == AF_INET))
			_phy.setIp4UdpTtl(froms,ttl);
		const int result = (_phy.udpSend(froms,(const struct sockaddr *)addr,data,len) != 0) ? 0 : -1;
		if ((ttl)&&(addr->ss_family == AF_INET))
			_phy.setIp4UdpTtl(froms,255);
		return result;
	}

	inline void nodeVirtualNetworkFrameFunction(uint64_t nwid,void **nuptr,uint64_t sourceMac,uint64_t destMac,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
	{
		EthernetTap *tap = reinterpret_cast<EthernetTap *>(*nuptr);
		if (!tap)
			return;
		tap->put(MAC(sourceMac),MAC(destMac),etherType,data,len);
	}

	inline int nodePathCheckFunction(const struct sockaddr_storage *localAddr,const struct sockaddr_storage *remoteAddr)
	{
		Mutex::Lock _l(_taps_m);
		for(std::map< uint64_t,EthernetTap * >::const_iterator t(_taps.begin());t!=_taps.end();++t) {
			if (t->second) {
				std::vector<InetAddress> ips(t->second->ips());
				for(std::vector<InetAddress>::const_iterator i(ips.begin());i!=ips.end();++i) {
					if (i->containsAddress(*(reinterpret_cast<const InetAddress *>(remoteAddr)))) {
						return 0;
					}
				}
			}
		}
		return 1;
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

		_phy.setNotifyWritable(tc->sock,true);
	}

	inline void onHttpResponseFromClient(TcpConnection *tc)
	{
		if (!tc->shouldKeepAlive)
			_phy.close(tc->sock); // will call close handler, which deletes from _tcpConnections
	}

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
	SqliteNetworkController *_controller;
#endif
	Phy<OneServiceImpl *> _phy;
	Node *_node;

	/*
	 * To properly handle NAT/gateway craziness we use three local UDP ports:
	 *
	 * [0] is the normal/default port, usually 9993
	 * [1] is a port dervied from our ZeroTier address
	 * [2] is a port computed from the normal/default for use with uPnP/NAT-PMP mappings
	 *
	 * [2] exists because on some gateways trying to do regular NAT-t interferes
	 * destructively with uPnP port mapping behavior in very weird buggy ways.
	 * It's only used if uPnP/NAT-PMP is enabled in this build.
	 */
	struct {
		InetAddress v4a,v6a;
		PhySocket *v4s,*v6s;
	} _udp[3];

	PhySocket *_v4TcpListenSocket;
	PhySocket *_v6TcpListenSocket;

	ControlPlane *_controlPlane;

	uint64_t _lastDirectReceiveFromGlobal;
#ifdef ZT_TCP_FALLBACK_RELAY
	uint64_t _lastSendToGlobalV4;
#endif
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

	unsigned int _port;

#ifdef ZT_USE_MINIUPNPC
	PortMapper *_portMapper;
#endif

#ifdef ZT_ENABLE_CLUSTER
	PhySocket *_clusterMessageSocket;
	ClusterGeoIpService *_clusterGeoIpService;
	ClusterDefinition *_clusterDefinition;
	unsigned int _clusterMemberId;
#endif

	bool _run;
	Mutex _run_m;
};

static int SnodeVirtualNetworkConfigFunction(ZT_Node *node,void *uptr,uint64_t nwid,void **nuptr,enum ZT_VirtualNetworkConfigOperation op,const ZT_VirtualNetworkConfig *nwconf)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodeVirtualNetworkConfigFunction(nwid,nuptr,op,nwconf); }
static void SnodeEventCallback(ZT_Node *node,void *uptr,enum ZT_Event event,const void *metaData)
{ reinterpret_cast<OneServiceImpl *>(uptr)->nodeEventCallback(event,metaData); }
static long SnodeDataStoreGetFunction(ZT_Node *node,void *uptr,const char *name,void *buf,unsigned long bufSize,unsigned long readIndex,unsigned long *totalSize)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodeDataStoreGetFunction(name,buf,bufSize,readIndex,totalSize); }
static int SnodeDataStorePutFunction(ZT_Node *node,void *uptr,const char *name,const void *data,unsigned long len,int secure)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodeDataStorePutFunction(name,data,len,secure); }
static int SnodeWirePacketSendFunction(ZT_Node *node,void *uptr,const struct sockaddr_storage *localAddr,const struct sockaddr_storage *addr,const void *data,unsigned int len,unsigned int ttl)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodeWirePacketSendFunction(localAddr,addr,data,len,ttl); }
static void SnodeVirtualNetworkFrameFunction(ZT_Node *node,void *uptr,uint64_t nwid,void **nuptr,uint64_t sourceMac,uint64_t destMac,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
{ reinterpret_cast<OneServiceImpl *>(uptr)->nodeVirtualNetworkFrameFunction(nwid,nuptr,sourceMac,destMac,etherType,vlanId,data,len); }
static int SnodePathCheckFunction(ZT_Node *node,void *uptr,const struct sockaddr_storage *localAddr,const struct sockaddr_storage *remoteAddr)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodePathCheckFunction(localAddr,remoteAddr); }

#ifdef ZT_ENABLE_CLUSTER
static void SclusterSendFunction(void *uptr,unsigned int toMemberId,const void *data,unsigned int len)
{
	OneServiceImpl *const impl = reinterpret_cast<OneServiceImpl *>(uptr);
	const ClusterDefinition::MemberDefinition &md = (*(impl->_clusterDefinition))[toMemberId];
	if (md.clusterEndpoint)
		impl->_phy.udpSend(impl->_clusterMessageSocket,reinterpret_cast<const struct sockaddr *>(&(md.clusterEndpoint)),data,len);
}
static int SclusterGeoIpFunction(void *uptr,const struct sockaddr_storage *addr,int *x,int *y,int *z)
{
	OneServiceImpl *const impl = reinterpret_cast<OneServiceImpl *>(uptr);
	return (int)(impl->_clusterGeoIpService->locate(*(reinterpret_cast<const InetAddress *>(addr)),*x,*y,*z));
}
#endif

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

OneService *OneService::newInstance(const char *hp,unsigned int port) { return new OneServiceImpl(hp,port); }
OneService::~OneService() {}

} // namespace ZeroTier
