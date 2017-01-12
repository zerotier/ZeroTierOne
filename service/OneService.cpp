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
#include <list>

#include "../version.h"
#include "../include/ZeroTierOne.h"

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
#include "../osdep/Binder.hpp"
#include "../osdep/ManagedRoute.hpp"

#include "OneService.hpp"
#include "ControlPlane.hpp"
#include "ClusterGeoIpService.hpp"
#include "ClusterDefinition.hpp"
#include "SoftwareUpdater.hpp"

#ifdef ZT_USE_SYSTEM_HTTP_PARSER
#include <http_parser.h>
#else
#include "../ext/http-parser/http_parser.h"
#endif

#include "../ext/json/json.hpp"

using json = nlohmann::json;

/**
 * Uncomment to enable UDP breakage switch
 *
 * If this is defined, the presence of a file called /tmp/ZT_BREAK_UDP
 * will cause direct UDP TX/RX to stop working. This can be used to
 * test TCP tunneling fallback and other robustness features. Deleting
 * this file will cause it to start working again.
 */
//#define ZT_BREAK_UDP

#include "../controller/EmbeddedNetworkController.hpp"

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

// Interface metric for ZeroTier taps -- this ensures that if we are on WiFi and also
// bridged via ZeroTier to the same LAN traffic will (if the OS is sane) prefer WiFi.
#define ZT_IF_METRIC 5000

// How often to check for new multicast subscriptions on a tap device
#define ZT_TAP_CHECK_MULTICAST_INTERVAL 5000

// Path under ZT1 home for controller database if controller is enabled
#define ZT_CONTROLLER_DB_PATH "controller.d"

// TCP fallback relay host -- geo-distributed using Amazon Route53 geo-aware DNS
#define ZT_TCP_FALLBACK_RELAY "tcp-fallback.zerotier.com"
#define ZT_TCP_FALLBACK_RELAY_PORT 443

// Frequency at which we re-resolve the TCP fallback relay
#define ZT_TCP_FALLBACK_RERESOLVE_DELAY 86400000

// Attempt to engage TCP fallback after this many ms of no reply to packets sent to global-scope IPs
#define ZT_TCP_FALLBACK_AFTER 60000

// How often to check for local interface addresses
#define ZT_LOCAL_INTERFACE_CHECK_INTERVAL 60000

namespace ZeroTier {

namespace {

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
static int SnodePathCheckFunction(ZT_Node *node,void *uptr,uint64_t ztaddr,const struct sockaddr_storage *localAddr,const struct sockaddr_storage *remoteAddr);
static int SnodePathLookupFunction(ZT_Node *node,void *uptr,uint64_t ztaddr,int family,struct sockaddr_storage *result);

#ifdef ZT_ENABLE_CLUSTER
static void SclusterSendFunction(void *uptr,unsigned int toMemberId,const void *data,unsigned int len);
static int SclusterGeoIpFunction(void *uptr,const struct sockaddr_storage *addr,int *x,int *y,int *z);
#endif

static void StapFrameHandler(void *uptr,uint64_t nwid,const MAC &from,const MAC &to,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len);

static int ShttpOnMessageBegin(http_parser *parser);
static int ShttpOnUrl(http_parser *parser,const char *ptr,size_t length);
#if (HTTP_PARSER_VERSION_MAJOR >= 2) && (HTTP_PARSER_VERSION_MINOR >= 2)
static int ShttpOnStatus(http_parser *parser,const char *ptr,size_t length);
#else
static int ShttpOnStatus(http_parser *parser);
#endif
static int ShttpOnHeaderField(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnValue(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnHeadersComplete(http_parser *parser);
static int ShttpOnBody(http_parser *parser,const char *ptr,size_t length);
static int ShttpOnMessageComplete(http_parser *parser);

#if (HTTP_PARSER_VERSION_MAJOR >= 2) && (HTTP_PARSER_VERSION_MINOR >= 1)
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
#else
static const struct http_parser_settings HTTP_PARSER_SETTINGS = {
	ShttpOnMessageBegin,
	ShttpOnUrl,
	ShttpOnHeaderField,
	ShttpOnValue,
	ShttpOnHeadersComplete,
	ShttpOnBody,
	ShttpOnMessageComplete
};
#endif

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

// Used to pseudo-randomize local source port picking
static volatile unsigned int _udpPortPickerCounter = 0;

class OneServiceImpl : public OneService
{
public:
	// begin member variables --------------------------------------------------

	const std::string _homePath;
	BackgroundResolver _tcpFallbackResolver;
	EmbeddedNetworkController *_controller;
	Phy<OneServiceImpl *> _phy;
	Node *_node;
	SoftwareUpdater *_updater;
	bool _updateAutoApply;
	unsigned int _primaryPort;

	// Local configuration and memo-ized static path definitions
	json _localConfig;
	Hashtable< uint64_t,std::vector<InetAddress> > _v4Hints;
	Hashtable< uint64_t,std::vector<InetAddress> > _v6Hints;
	Hashtable< uint64_t,std::vector<InetAddress> > _v4Blacklists;
	Hashtable< uint64_t,std::vector<InetAddress> > _v6Blacklists;
	std::vector< InetAddress > _globalV4Blacklist;
	std::vector< InetAddress > _globalV6Blacklist;
	std::vector< InetAddress > _allowManagementFrom;
	std::vector< std::string > _interfacePrefixBlacklist;
	Mutex _localConfig_m;

	/*
	 * To attempt to handle NAT/gateway craziness we use three local UDP ports:
	 *
	 * [0] is the normal/default port, usually 9993
	 * [1] is a port dervied from our ZeroTier address
	 * [2] is a port computed from the normal/default for use with uPnP/NAT-PMP mappings
	 *
	 * [2] exists because on some gateways trying to do regular NAT-t interferes
	 * destructively with uPnP port mapping behavior in very weird buggy ways.
	 * It's only used if uPnP/NAT-PMP is enabled in this build.
	 */
	Binder _bindings[3];
	unsigned int _ports[3];
	uint16_t _portsBE[3]; // ports in big-endian network byte order as in sockaddr

	// Sockets for JSON API -- bound only to V4 and V6 localhost
	PhySocket *_v4TcpControlSocket;
	PhySocket *_v6TcpControlSocket;

	// JSON API handler
	ControlPlane *_controlPlane;

	// Time we last received a packet from a global address
	uint64_t _lastDirectReceiveFromGlobal;
#ifdef ZT_TCP_FALLBACK_RELAY
	uint64_t _lastSendToGlobalV4;
#endif

	// Last potential sleep/wake event
	uint64_t _lastRestart;

	// Deadline for the next background task service function
	volatile uint64_t _nextBackgroundTaskDeadline;

	// Configured networks
	struct NetworkState
	{
		NetworkState() :
			tap((EthernetTap *)0)
		{
			// Real defaults are in network 'up' code in network event handler
			settings.allowManaged = true;
			settings.allowGlobal = false;
			settings.allowDefault = false;
		}

		EthernetTap *tap;
		ZT_VirtualNetworkConfig config; // memcpy() of raw config from core
		std::vector<InetAddress> managedIps;
		std::list< SharedPtr<ManagedRoute> > managedRoutes;
		NetworkSettings settings;
	};
	std::map<uint64_t,NetworkState> _nets;
	Mutex _nets_m;

	// Active TCP/IP connections
	std::set< TcpConnection * > _tcpConnections; // no mutex for this since it's done in the main loop thread only
	TcpConnection *_tcpFallbackTunnel;

	// Termination status information
	ReasonForTermination _termReason;
	std::string _fatalErrorMessage;
	Mutex _termReason_m;

	// uPnP/NAT-PMP port mapper if enabled
#ifdef ZT_USE_MINIUPNPC
	PortMapper *_portMapper;
#endif

	// Cluster management instance if enabled
#ifdef ZT_ENABLE_CLUSTER
	PhySocket *_clusterMessageSocket;
	ClusterDefinition *_clusterDefinition;
	unsigned int _clusterMemberId;
#endif

	// Set to false to force service to stop
	volatile bool _run;
	Mutex _run_m;

	// end member variables ----------------------------------------------------

	OneServiceImpl(const char *hp,unsigned int port) :
		_homePath((hp) ? hp : ".")
		,_tcpFallbackResolver(ZT_TCP_FALLBACK_RELAY)
		,_controller((EmbeddedNetworkController *)0)
		,_phy(this,false,true)
		,_node((Node *)0)
		,_updater((SoftwareUpdater *)0)
		,_updateAutoApply(false)
		,_primaryPort(port)
		,_controlPlane((ControlPlane *)0)
		,_lastDirectReceiveFromGlobal(0)
#ifdef ZT_TCP_FALLBACK_RELAY
		,_lastSendToGlobalV4(0)
#endif
		,_lastRestart(0)
		,_nextBackgroundTaskDeadline(0)
		,_tcpFallbackTunnel((TcpConnection *)0)
		,_termReason(ONE_STILL_RUNNING)
#ifdef ZT_USE_MINIUPNPC
		,_portMapper((PortMapper *)0)
#endif
#ifdef ZT_ENABLE_CLUSTER
		,_clusterMessageSocket((PhySocket *)0)
		,_clusterDefinition((ClusterDefinition *)0)
		,_clusterMemberId(0)
#endif
		,_run(true)
	{
		_ports[0] = 0;
		_ports[1] = 0;
		_ports[2] = 0;
	}

	virtual ~OneServiceImpl()
	{
		for(int i=0;i<3;++i)
			_bindings[i].closeAll(_phy);

		_phy.close(_v4TcpControlSocket);
		_phy.close(_v6TcpControlSocket);

#ifdef ZT_ENABLE_CLUSTER
		_phy.close(_clusterMessageSocket);
#endif

#ifdef ZT_USE_MINIUPNPC
		delete _portMapper;
#endif
		delete _controller;
#ifdef ZT_ENABLE_CLUSTER
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
					} else {
						OSUtils::lockDownFile(authTokenPath.c_str(),false);
					}
				}
			}
			authToken = _trimString(authToken);

			// Clean up any legacy files if present
			OSUtils::rm((_homePath + ZT_PATH_SEPARATOR_S + "peers.save").c_str());

			{
				struct ZT_Node_Callbacks cb;
				cb.version = 0;
				cb.dataStoreGetFunction = SnodeDataStoreGetFunction;
				cb.dataStorePutFunction = SnodeDataStorePutFunction;
				cb.wirePacketSendFunction = SnodeWirePacketSendFunction;
				cb.virtualNetworkFrameFunction = SnodeVirtualNetworkFrameFunction;
				cb.virtualNetworkConfigFunction = SnodeVirtualNetworkConfigFunction;
				cb.eventCallback = SnodeEventCallback;
				cb.pathCheckFunction = SnodePathCheckFunction;
				cb.pathLookupFunction = SnodePathLookupFunction;
				_node = new Node(this,&cb,OSUtils::now());
			}

			// Read local configuration
			{
				uint64_t trustedPathIds[ZT_MAX_TRUSTED_PATHS];
				InetAddress trustedPathNetworks[ZT_MAX_TRUSTED_PATHS];
				unsigned int trustedPathCount = 0;

				// Old style "trustedpaths" flat file -- will eventually go away
				FILE *trustpaths = fopen((_homePath + ZT_PATH_SEPARATOR_S + "trustedpaths").c_str(),"r");
				if (trustpaths) {
					char buf[1024];
					while ((fgets(buf,sizeof(buf),trustpaths))&&(trustedPathCount < ZT_MAX_TRUSTED_PATHS)) {
						int fno = 0;
						char *saveptr = (char *)0;
						uint64_t trustedPathId = 0;
						InetAddress trustedPathNetwork;
						for(char *f=Utils::stok(buf,"=\r\n \t",&saveptr);(f);f=Utils::stok((char *)0,"=\r\n \t",&saveptr)) {
							if (fno == 0) {
								trustedPathId = Utils::hexStrToU64(f);
							} else if (fno == 1) {
								trustedPathNetwork = InetAddress(f);
							} else break;
							++fno;
						}
						if ( (trustedPathId != 0) && ((trustedPathNetwork.ss_family == AF_INET)||(trustedPathNetwork.ss_family == AF_INET6)) && (trustedPathNetwork.ipScope() != InetAddress::IP_SCOPE_GLOBAL) && (trustedPathNetwork.netmaskBits() > 0) ) {
							trustedPathIds[trustedPathCount] = trustedPathId;
							trustedPathNetworks[trustedPathCount] = trustedPathNetwork;
							++trustedPathCount;
						}
					}
					fclose(trustpaths);
				}

				// Read local config file
				Mutex::Lock _l2(_localConfig_m);
				std::string lcbuf;
				if (OSUtils::readFile((_homePath + ZT_PATH_SEPARATOR_S + "local.conf").c_str(),lcbuf)) {
					try {
						_localConfig = OSUtils::jsonParse(lcbuf);
						if (!_localConfig.is_object()) {
							fprintf(stderr,"WARNING: unable to parse local.conf (root element is not a JSON object)" ZT_EOL_S);
						}
					} catch ( ... ) {
						fprintf(stderr,"WARNING: unable to parse local.conf (invalid JSON)" ZT_EOL_S);
					}
				}

				// Get any trusted paths in local.conf (we'll parse the rest of physical[] elsewhere)
				json &physical = _localConfig["physical"];
				if (physical.is_object()) {
					for(json::iterator phy(physical.begin());phy!=physical.end();++phy) {
						InetAddress net(OSUtils::jsonString(phy.key(),""));
						if (net) {
							if (phy.value().is_object()) {
								uint64_t tpid;
								if ((tpid = OSUtils::jsonInt(phy.value()["trustedPathId"],0ULL)) != 0ULL) {
									if ( ((net.ss_family == AF_INET)||(net.ss_family == AF_INET6)) && (trustedPathCount < ZT_MAX_TRUSTED_PATHS) && (net.ipScope() != InetAddress::IP_SCOPE_GLOBAL) && (net.netmaskBits() > 0) ) {
										trustedPathIds[trustedPathCount] = tpid;
										trustedPathNetworks[trustedPathCount] = net;
										++trustedPathCount;
									}
								}
							}
						}
					}
				}

				// Set trusted paths if there are any
				if (trustedPathCount)
					_node->setTrustedPaths(reinterpret_cast<const struct sockaddr_storage *>(trustedPathNetworks),trustedPathIds,trustedPathCount);
			}
			applyLocalConfig();

			// Bind TCP control socket
			const int portTrials = (_primaryPort == 0) ? 256 : 1; // if port is 0, pick random
			for(int k=0;k<portTrials;++k) {
				if (_primaryPort == 0) {
					unsigned int randp = 0;
					Utils::getSecureRandom(&randp,sizeof(randp));
					_primaryPort = 20000 + (randp % 45500);
				}

				if (_trialBind(_primaryPort)) {
					struct sockaddr_in in4;
					memset(&in4,0,sizeof(in4));
					in4.sin_family = AF_INET;
					in4.sin_addr.s_addr = Utils::hton((uint32_t)((_allowManagementFrom.size() > 0) ? 0 : 0x7f000001)); // right now we just listen for TCP @127.0.0.1
					in4.sin_port = Utils::hton((uint16_t)_primaryPort);
					_v4TcpControlSocket = _phy.tcpListen((const struct sockaddr *)&in4,this);

					struct sockaddr_in6 in6;
					memset((void *)&in6,0,sizeof(in6));
					in6.sin6_family = AF_INET6;
					in6.sin6_port = in4.sin_port;
					if (_allowManagementFrom.size() == 0)
						in6.sin6_addr.s6_addr[15] = 1; // IPv6 localhost == ::1
					_v6TcpControlSocket = _phy.tcpListen((const struct sockaddr *)&in6,this);

					// We must bind one of IPv4 or IPv6 -- support either failing to support hosts that
					// have only IPv4 or only IPv6 stacks.
					if ((_v4TcpControlSocket)||(_v6TcpControlSocket)) {
						_ports[0] = _primaryPort;
						break;
					} else {
						if (_v4TcpControlSocket)
							_phy.close(_v4TcpControlSocket,false);
						if (_v6TcpControlSocket)
							_phy.close(_v6TcpControlSocket,false);
						_primaryPort = 0;
					}
				} else {
					_primaryPort = 0;
				}
			}
			if (_ports[0] == 0) {
				Mutex::Lock _l(_termReason_m);
				_termReason = ONE_UNRECOVERABLE_ERROR;
				_fatalErrorMessage = "cannot bind to local control interface port";
				return _termReason;
			}

			// Write file containing primary port to be read by CLIs, etc.
			char portstr[64];
			Utils::snprintf(portstr,sizeof(portstr),"%u",_ports[0]);
			OSUtils::writeFile((_homePath + ZT_PATH_SEPARATOR_S + "zerotier-one.port").c_str(),std::string(portstr));

			// Attempt to bind to a secondary port chosen from our ZeroTier address.
			// This exists because there are buggy NATs out there that fail if more
			// than one device behind the same NAT tries to use the same internal
			// private address port number.
			_ports[1] = 20000 + ((unsigned int)_node->address() % 45500);
			for(int i=0;;++i) {
				if (i > 1000) {
					_ports[1] = 0;
					break;
				} else if (++_ports[1] >= 65536) {
					_ports[1] = 20000;
				}
				if (_trialBind(_ports[1]))
					break;
			}

#ifdef ZT_USE_MINIUPNPC
			// If we're running uPnP/NAT-PMP, bind a *third* port for that. We can't
			// use the other two ports for that because some NATs do really funky
			// stuff with ports that are explicitly mapped that breaks things.
			if (_ports[1]) {
				_ports[2] = _ports[1];
				for(int i=0;;++i) {
					if (i > 1000) {
						_ports[2] = 0;
						break;
					} else if (++_ports[2] >= 65536) {
						_ports[2] = 20000;
					}
					if (_trialBind(_ports[2]))
						break;
				}
				if (_ports[2]) {
					char uniqueName[64];
					Utils::snprintf(uniqueName,sizeof(uniqueName),"ZeroTier/%.10llx@%u",_node->address(),_ports[2]);
					_portMapper = new PortMapper(_ports[2],uniqueName);
				}
			}
#endif

			// Populate ports in big-endian format for quick compare
			for(int i=0;i<3;++i)
				_portsBE[i] = Utils::hton((uint16_t)_ports[i]);

			_controller = new EmbeddedNetworkController(_node,(_homePath + ZT_PATH_SEPARATOR_S + ZT_CONTROLLER_DB_PATH).c_str(),(FILE *)0);
			_node->setNetconfMaster((void *)_controller);

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
								_fatalErrorMessage = "cluster: can't determine my cluster member ID: able to bind more than one cluster message socket IP/port!";
								return _termReason;
							}
							_clusterMessageSocket = cs;
							_clusterMemberId = m->id;
						}
					}

					if (!_clusterMessageSocket) {
						Mutex::Lock _l(_termReason_m);
						_termReason = ONE_UNRECOVERABLE_ERROR;
						_fatalErrorMessage = "cluster: can't determine my cluster member ID: unable to bind to any cluster message socket IP/port.";
						return _termReason;
					}

					const ClusterDefinition::MemberDefinition &me = (*_clusterDefinition)[_clusterMemberId];
					InetAddress endpoints[255];
					unsigned int numEndpoints = 0;
					for(std::vector<InetAddress>::const_iterator i(me.zeroTierEndpoints.begin());i!=me.zeroTierEndpoints.end();++i)
						endpoints[numEndpoints++] = *i;

					if (_node->clusterInit(_clusterMemberId,reinterpret_cast<const struct sockaddr_storage *>(endpoints),numEndpoints,me.x,me.y,me.z,&SclusterSendFunction,this,_clusterDefinition->geo().available() ? &SclusterGeoIpFunction : 0,this) == ZT_RESULT_OK) {
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
			_controlPlane->setController(_controller);

			{	// Remember networks from previous session
				std::vector<std::string> networksDotD(OSUtils::listDirectory((_homePath + ZT_PATH_SEPARATOR_S + "networks.d").c_str()));
				for(std::vector<std::string>::iterator f(networksDotD.begin());f!=networksDotD.end();++f) {
					std::size_t dot = f->find_last_of('.');
					if ((dot == 16)&&(f->substr(16) == ".conf"))
						_node->join(Utils::hexStrToU64(f->substr(0,dot).c_str()),(void *)0);
				}
			}

			_nextBackgroundTaskDeadline = 0;
			uint64_t clockShouldBe = OSUtils::now();
			_lastRestart = clockShouldBe;
			uint64_t lastTapMulticastGroupCheck = 0;
			uint64_t lastTcpFallbackResolve = 0;
			uint64_t lastBindRefresh = 0;
			uint64_t lastUpdateCheck = clockShouldBe;
			uint64_t lastLocalInterfaceAddressCheck = (clockShouldBe - ZT_LOCAL_INTERFACE_CHECK_INTERVAL) + 15000; // do this in 15s to give portmapper time to configure and other things time to settle
			for(;;) {
				_run_m.lock();
				if (!_run) {
					_run_m.unlock();
					_termReason_m.lock();
					_termReason = ONE_NORMAL_TERMINATION;
					_termReason_m.unlock();
					break;
				} else {
					_run_m.unlock();
				}

				const uint64_t now = OSUtils::now();

				// Attempt to detect sleep/wake events by detecting delay overruns
				bool restarted = false;
				if ((now > clockShouldBe)&&((now - clockShouldBe) > 10000)) {
					_lastRestart = now;
					restarted = true;
				}

				// Check for updates (if enabled)
				if ((_updater)&&((now - lastUpdateCheck) > 10000)) {
					lastUpdateCheck = now;
					if (_updater->check(now) && _updateAutoApply)
						_updater->apply();
				}

				// Refresh bindings in case device's interfaces have changed, and also sync routes to update any shadow routes (e.g. shadow default)
				if (((now - lastBindRefresh) >= ZT_BINDER_REFRESH_PERIOD)||(restarted)) {
					lastBindRefresh = now;
					for(int i=0;i<3;++i) {
						if (_ports[i]) {
							_bindings[i].refresh(_phy,_ports[i],*this);
						}
					}
					{
						Mutex::Lock _l(_nets_m);
						for(std::map<uint64_t,NetworkState>::iterator n(_nets.begin());n!=_nets.end();++n) {
							if (n->second.tap)
								syncManagedStuff(n->second,false,true);
						}
					}
				}

				uint64_t dl = _nextBackgroundTaskDeadline;
				if (dl <= now) {
					_node->processBackgroundTasks(now,&_nextBackgroundTaskDeadline);
					dl = _nextBackgroundTaskDeadline;
				}

				if ((now - lastTcpFallbackResolve) >= ZT_TCP_FALLBACK_RERESOLVE_DELAY) {
					lastTcpFallbackResolve = now;
					_tcpFallbackResolver.resolveNow();
				}

				if ((_tcpFallbackTunnel)&&((now - _lastDirectReceiveFromGlobal) < (ZT_TCP_FALLBACK_AFTER / 2)))
					_phy.close(_tcpFallbackTunnel->sock);

				if ((now - lastTapMulticastGroupCheck) >= ZT_TAP_CHECK_MULTICAST_INTERVAL) {
					lastTapMulticastGroupCheck = now;
					Mutex::Lock _l(_nets_m);
					for(std::map<uint64_t,NetworkState>::const_iterator n(_nets.begin());n!=_nets.end();++n) {
						if (n->second.tap) {
							std::vector<MulticastGroup> added,removed;
							n->second.tap->scanMulticastGroups(added,removed);
							for(std::vector<MulticastGroup>::iterator m(added.begin());m!=added.end();++m)
								_node->multicastSubscribe(n->first,m->mac().toInt(),m->adi());
							for(std::vector<MulticastGroup>::iterator m(removed.begin());m!=removed.end();++m)
								_node->multicastUnsubscribe(n->first,m->mac().toInt(),m->adi());
						}
					}
				}

				if ((now - lastLocalInterfaceAddressCheck) >= ZT_LOCAL_INTERFACE_CHECK_INTERVAL) {
					lastLocalInterfaceAddressCheck = now;

					_node->clearLocalInterfaceAddresses();

#ifdef ZT_USE_MINIUPNPC
					if (_portMapper) {
						std::vector<InetAddress> mappedAddresses(_portMapper->get());
						for(std::vector<InetAddress>::const_iterator ext(mappedAddresses.begin());ext!=mappedAddresses.end();++ext)
							_node->addLocalInterfaceAddress(reinterpret_cast<const struct sockaddr_storage *>(&(*ext)));
					}
#endif

					std::vector<InetAddress> boundAddrs(_bindings[0].allBoundLocalInterfaceAddresses());
					for(std::vector<InetAddress>::const_iterator i(boundAddrs.begin());i!=boundAddrs.end();++i)
						_node->addLocalInterfaceAddress(reinterpret_cast<const struct sockaddr_storage *>(&(*i)));
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
			Mutex::Lock _l(_nets_m);
			for(std::map<uint64_t,NetworkState>::iterator n(_nets.begin());n!=_nets.end();++n)
				delete n->second.tap;
			_nets.clear();
		}

		delete _controlPlane;
		_controlPlane = (ControlPlane *)0;
		delete _updater;
		_updater = (SoftwareUpdater *)0;
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
		Mutex::Lock _l(_nets_m);
		std::map<uint64_t,NetworkState>::const_iterator n(_nets.find(nwid));
		if ((n != _nets.end())&&(n->second.tap))
			return n->second.tap->deviceName();
		else return std::string();
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

	virtual bool getNetworkSettings(const uint64_t nwid,NetworkSettings &settings) const
	{
		Mutex::Lock _l(_nets_m);
		std::map<uint64_t,NetworkState>::const_iterator n(_nets.find(nwid));
		if (n == _nets.end())
			return false;
		memcpy(&settings,&(n->second.settings),sizeof(NetworkSettings));
		return true;
	}

	virtual bool setNetworkSettings(const uint64_t nwid,const NetworkSettings &settings)
	{
		Mutex::Lock _l(_nets_m);

		std::map<uint64_t,NetworkState>::iterator n(_nets.find(nwid));
		if (n == _nets.end())
			return false;
		memcpy(&(n->second.settings),&settings,sizeof(NetworkSettings));

		char nlcpath[256];
		Utils::snprintf(nlcpath,sizeof(nlcpath),"%s" ZT_PATH_SEPARATOR_S "networks.d" ZT_PATH_SEPARATOR_S "%.16llx.local.conf",_homePath.c_str(),nwid);
		FILE *out = fopen(nlcpath,"w");
		if (out) {
			fprintf(out,"allowManaged=%d\n",(int)n->second.settings.allowManaged);
			fprintf(out,"allowGlobal=%d\n",(int)n->second.settings.allowGlobal);
			fprintf(out,"allowDefault=%d\n",(int)n->second.settings.allowDefault);
			fclose(out);
		}

		if (n->second.tap)
			syncManagedStuff(n->second,true,true);

		return true;
	}

	// Internal implementation methods -----------------------------------------

	// Must be called after _localConfig is read or modified
	void applyLocalConfig()
	{
		Mutex::Lock _l(_localConfig_m);

		_v4Hints.clear();
		_v6Hints.clear();
		_v4Blacklists.clear();
		_v6Blacklists.clear();
		json &virt = _localConfig["virtual"];
		if (virt.is_object()) {
			for(json::iterator v(virt.begin());v!=virt.end();++v) {
				const std::string nstr = v.key();
				if ((nstr.length() == ZT_ADDRESS_LENGTH_HEX)&&(v.value().is_object())) {
					const Address ztaddr(nstr.c_str());
					if (ztaddr) {
						const std::string rstr(OSUtils::jsonString(v.value()["role"],""));
						_node->setRole(ztaddr.toInt(),((rstr == "upstream")||(rstr == "UPSTREAM")) ? ZT_PEER_ROLE_UPSTREAM : ZT_PEER_ROLE_LEAF);

						const uint64_t ztaddr2 = ztaddr.toInt();
						std::vector<InetAddress> &v4h = _v4Hints[ztaddr2];
						std::vector<InetAddress> &v6h = _v6Hints[ztaddr2];
						std::vector<InetAddress> &v4b = _v4Blacklists[ztaddr2];
						std::vector<InetAddress> &v6b = _v6Blacklists[ztaddr2];

						json &tryAddrs = v.value()["try"];
						if (tryAddrs.is_array()) {
							for(unsigned long i=0;i<tryAddrs.size();++i) {
								const InetAddress ip(OSUtils::jsonString(tryAddrs[i],""));
								if (ip.ss_family == AF_INET)
									v4h.push_back(ip);
								else if (ip.ss_family == AF_INET6)
									v6h.push_back(ip);
							}
						}
						json &blAddrs = v.value()["blacklist"];
						if (blAddrs.is_array()) {
							for(unsigned long i=0;i<blAddrs.size();++i) {
								const InetAddress ip(OSUtils::jsonString(tryAddrs[i],""));
								if (ip.ss_family == AF_INET)
									v4b.push_back(ip);
								else if (ip.ss_family == AF_INET6)
									v6b.push_back(ip);
							}
						}

						if (v4h.empty()) _v4Hints.erase(ztaddr2);
						if (v6h.empty()) _v6Hints.erase(ztaddr2);
						if (v4b.empty()) _v4Blacklists.erase(ztaddr2);
						if (v6b.empty()) _v6Blacklists.erase(ztaddr2);
					}
				}
			}
		}

		_globalV4Blacklist.clear();
		_globalV6Blacklist.clear();
		json &physical = _localConfig["physical"];
		if (physical.is_object()) {
			for(json::iterator phy(physical.begin());phy!=physical.end();++phy) {
				const InetAddress net(OSUtils::jsonString(phy.key(),""));
				if ((net)&&(net.netmaskBits() > 0)) {
					if (phy.value().is_object()) {
						if (OSUtils::jsonBool(phy.value()["blacklist"],false)) {
							if (net.ss_family == AF_INET)
								_globalV4Blacklist.push_back(net);
							else if (net.ss_family == AF_INET6)
								_globalV6Blacklist.push_back(net);
						}
					}
				}
			}
		}

		_allowManagementFrom.clear();
		_interfacePrefixBlacklist.clear();
		json &settings = _localConfig["settings"];
		if (settings.is_object()) {
			const std::string rp(OSUtils::jsonString(settings["relayPolicy"],""));
			if ((rp == "always")||(rp == "ALWAYS"))
				_node->setRelayPolicy(ZT_RELAY_POLICY_ALWAYS);
			else if ((rp == "never")||(rp == "NEVER"))
				_node->setRelayPolicy(ZT_RELAY_POLICY_NEVER);
			else _node->setRelayPolicy(ZT_RELAY_POLICY_TRUSTED);

			const std::string up(OSUtils::jsonString(settings["softwareUpdate"],ZT_SOFTWARE_UPDATE_DEFAULT));
			const bool udist = OSUtils::jsonBool(settings["softwareUpdateDist"],false);
			if (((up == "apply")||(up == "download"))||(udist)) {
				if (!_updater)
					_updater = new SoftwareUpdater(*_node,_homePath);
				_updateAutoApply = (up == "apply");
				_updater->setUpdateDistribution(udist);
				_updater->setChannel(OSUtils::jsonString(settings["softwareUpdateChannel"],ZT_SOFTWARE_UPDATE_DEFAULT_CHANNEL));
			} else {
				delete _updater;
				_updater = (SoftwareUpdater *)0;
				_updateAutoApply = false;
			}

			json &ignoreIfs = settings["interfacePrefixBlacklist"];
			if (ignoreIfs.is_array()) {
				for(unsigned long i=0;i<ignoreIfs.size();++i) {
					const std::string tmp(OSUtils::jsonString(ignoreIfs[i],""));
					if (tmp.length() > 0)
						_interfacePrefixBlacklist.push_back(tmp);
				}
			}

			json &amf = settings["allowManagementFrom"];
			if (amf.is_array()) {
				for(unsigned long i=0;i<amf.size();++i) {
					const InetAddress nw(OSUtils::jsonString(amf[i],""));
					if (nw)
						_allowManagementFrom.push_back(nw);
				}
			}
		}
	}

	// Checks if a managed IP or route target is allowed
	bool checkIfManagedIsAllowed(const NetworkState &n,const InetAddress &target)
	{
		if (!n.settings.allowManaged)
			return false;
		if (target.isDefaultRoute())
			return n.settings.allowDefault;
		switch(target.ipScope()) {
			case InetAddress::IP_SCOPE_NONE:
			case InetAddress::IP_SCOPE_MULTICAST:
			case InetAddress::IP_SCOPE_LOOPBACK:
			case InetAddress::IP_SCOPE_LINK_LOCAL:
				return false;
			case InetAddress::IP_SCOPE_GLOBAL:
				return n.settings.allowGlobal;
			default:
				return true;
		}
	}

	// Match only an IP from a vector of IPs -- used in syncManagedStuff()
	bool matchIpOnly(const std::vector<InetAddress> &ips,const InetAddress &ip) const
	{
		for(std::vector<InetAddress>::const_iterator i(ips.begin());i!=ips.end();++i) {
			if (i->ipsEqual(ip))
				return true;
		}
		return false;
	}

	// Apply or update managed IPs for a configured network (be sure n.tap exists)
	void syncManagedStuff(NetworkState &n,bool syncIps,bool syncRoutes)
	{
		// assumes _nets_m is locked
		if (syncIps) {
			std::vector<InetAddress> newManagedIps;
			newManagedIps.reserve(n.config.assignedAddressCount);
			for(unsigned int i=0;i<n.config.assignedAddressCount;++i) {
				const InetAddress *ii = reinterpret_cast<const InetAddress *>(&(n.config.assignedAddresses[i]));
				if (checkIfManagedIsAllowed(n,*ii))
					newManagedIps.push_back(*ii);
			}
			std::sort(newManagedIps.begin(),newManagedIps.end());
			newManagedIps.erase(std::unique(newManagedIps.begin(),newManagedIps.end()),newManagedIps.end());

			for(std::vector<InetAddress>::iterator ip(n.managedIps.begin());ip!=n.managedIps.end();++ip) {
				if (std::find(newManagedIps.begin(),newManagedIps.end(),*ip) == newManagedIps.end()) {
					if (!n.tap->removeIp(*ip))
						fprintf(stderr,"ERROR: unable to remove ip address %s" ZT_EOL_S, ip->toString().c_str());
				}
			}
			for(std::vector<InetAddress>::iterator ip(newManagedIps.begin());ip!=newManagedIps.end();++ip) {
				if (std::find(n.managedIps.begin(),n.managedIps.end(),*ip) == n.managedIps.end()) {
					if (!n.tap->addIp(*ip))
						fprintf(stderr,"ERROR: unable to add ip address %s" ZT_EOL_S, ip->toString().c_str());
				}
			}

			n.managedIps.swap(newManagedIps);
		}

		if (syncRoutes) {
			char tapdev[64];
#ifdef __WINDOWS__
			Utils::snprintf(tapdev,sizeof(tapdev),"%.16llx",(unsigned long long)n.tap->luid().Value);
#else
			Utils::scopy(tapdev,sizeof(tapdev),n.tap->deviceName().c_str());
#endif

			std::vector<InetAddress> myIps(n.tap->ips());

			// Nuke applied routes that are no longer in n.config.routes[] and/or are not allowed
			for(std::list< SharedPtr<ManagedRoute> >::iterator mr(n.managedRoutes.begin());mr!=n.managedRoutes.end();) {
				bool haveRoute = false;
				if ( (checkIfManagedIsAllowed(n,(*mr)->target())) && (((*mr)->via().ss_family != (*mr)->target().ss_family)||(!matchIpOnly(myIps,(*mr)->via()))) ) {
					for(unsigned int i=0;i<n.config.routeCount;++i) {
						const InetAddress *const target = reinterpret_cast<const InetAddress *>(&(n.config.routes[i].target));
						const InetAddress *const via = reinterpret_cast<const InetAddress *>(&(n.config.routes[i].via));
						if ( ((*mr)->target() == *target) && ( ((via->ss_family == target->ss_family)&&((*mr)->via().ipsEqual(*via))) || (tapdev == (*mr)->device()) ) ) {
							haveRoute = true;
							break;
						}
					}
				}
				if (haveRoute) {
					++mr;
				} else {
					n.managedRoutes.erase(mr++);
				}
			}

			// Apply routes in n.config.routes[] that we haven't applied yet, and sync those we have in case shadow routes need to change
			for(unsigned int i=0;i<n.config.routeCount;++i) {
				const InetAddress *const target = reinterpret_cast<const InetAddress *>(&(n.config.routes[i].target));
				const InetAddress *const via = reinterpret_cast<const InetAddress *>(&(n.config.routes[i].via));

				if ( (!checkIfManagedIsAllowed(n,*target)) || ((via->ss_family == target->ss_family)&&(matchIpOnly(myIps,*via))) )
					continue;

				bool haveRoute = false;

				// Ignore routes implied by local managed IPs since adding the IP adds the route
				for(std::vector<InetAddress>::iterator ip(n.managedIps.begin());ip!=n.managedIps.end();++ip) {
					if ((target->netmaskBits() == ip->netmaskBits())&&(target->containsAddress(*ip))) {
						haveRoute = true;
						break;
					}
				}
				if (haveRoute)
					continue;

				// If we've already applied this route, just sync it and continue
				for(std::list< SharedPtr<ManagedRoute> >::iterator mr(n.managedRoutes.begin());mr!=n.managedRoutes.end();++mr) {
					if ( ((*mr)->target() == *target) && ( ((via->ss_family == target->ss_family)&&((*mr)->via().ipsEqual(*via))) || (tapdev == (*mr)->device()) ) ) {
						haveRoute = true;
						(*mr)->sync();
						break;
					}
				}
				if (haveRoute)
					continue;

				// Add and apply new routes
				n.managedRoutes.push_back(SharedPtr<ManagedRoute>(new ManagedRoute(*target,*via,tapdev)));
				if (!n.managedRoutes.back()->sync())
					n.managedRoutes.pop_back();
			}
		}
	}

	// Handlers for Node and Phy<> callbacks -----------------------------------

	inline void phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *localAddr,const struct sockaddr *from,void *data,unsigned long len)
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
			reinterpret_cast<const struct sockaddr_storage *>(localAddr),
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
		if (!from) {
			_phy.close(sockN,false);
			return;
		} else {
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
								InetAddress fakeTcpLocalInterfaceAddress((uint32_t)0xffffffff,0xffff);
								const ZT_ResultCode rc = _node->processWirePacket(
									OSUtils::now(),
									reinterpret_cast<struct sockaddr_storage *>(&fakeTcpLocalInterfaceAddress),
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
		Mutex::Lock _l(_nets_m);
		NetworkState &n = _nets[nwid];

		switch(op) {

			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP:
				if (!n.tap) {
					try {
						char friendlyName[128];
						Utils::snprintf(friendlyName,sizeof(friendlyName),"ZeroTier One [%.16llx]",nwid);
						n.tap = new EthernetTap(
							_homePath.c_str(),
							MAC(nwc->mac),
							nwc->mtu,
							(unsigned int)ZT_IF_METRIC,
							nwid,
							friendlyName,
							StapFrameHandler,
							(void *)this);
						*nuptr = (void *)&n;

						char nlcpath[256];
						Utils::snprintf(nlcpath,sizeof(nlcpath),"%s" ZT_PATH_SEPARATOR_S "networks.d" ZT_PATH_SEPARATOR_S "%.16llx.local.conf",_homePath.c_str(),nwid);
						std::string nlcbuf;
						if (OSUtils::readFile(nlcpath,nlcbuf)) {
							Dictionary<4096> nc;
							nc.load(nlcbuf.c_str());
							n.settings.allowManaged = nc.getB("allowManaged",true);
							n.settings.allowGlobal = nc.getB("allowGlobal",false);
							n.settings.allowDefault = nc.getB("allowDefault",false);
						}
					} catch (std::exception &exc) {
#ifdef __WINDOWS__
						FILE *tapFailLog = fopen((_homePath + ZT_PATH_SEPARATOR_S"port_error_log.txt").c_str(),"a");
						if (tapFailLog) {
							fprintf(tapFailLog,"%.16llx: %s" ZT_EOL_S,(unsigned long long)nwid,exc.what());
							fclose(tapFailLog);
						}
#else
						fprintf(stderr,"ERROR: unable to configure virtual network port: %s" ZT_EOL_S,exc.what());
#endif
						_nets.erase(nwid);
						return -999;
					} catch ( ... ) {
						return -999; // tap init failed
					}
				}
				// After setting up tap, fall through to CONFIG_UPDATE since we also want to do this...

			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE:
				memcpy(&(n.config),nwc,sizeof(ZT_VirtualNetworkConfig));
				if (n.tap) { // sanity check
#ifdef __WINDOWS__
                    // wait for up to 5 seconds for the WindowsEthernetTap to actually be initialized
                    // 
                    // without WindowsEthernetTap::isInitialized() returning true, the won't actually
                    // be online yet and setting managed routes on it will fail.
                    const int MAX_SLEEP_COUNT = 500;
                    for (int i = 0; !n.tap->isInitialized() && i < MAX_SLEEP_COUNT; i++) {
                        Sleep(10);
                    }
#endif
					syncManagedStuff(n,true,true);
				} else {
					_nets.erase(nwid);
					return -999; // tap init failed
				}
				break;

			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN:
			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY:
				if (n.tap) { // sanity check
#ifdef __WINDOWS__
					std::string winInstanceId(n.tap->instanceId());
#endif
					*nuptr = (void *)0;
					delete n.tap;
					_nets.erase(nwid);
#ifdef __WINDOWS__
					if ((op == ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY)&&(winInstanceId.length() > 0))
						WindowsEthernetTap::deletePersistentTapDevice(winInstanceId.c_str());
#endif
					if (op == ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY) {
						char nlcpath[256];
						Utils::snprintf(nlcpath,sizeof(nlcpath),"%s" ZT_PATH_SEPARATOR_S "networks.d" ZT_PATH_SEPARATOR_S "%.16llx.local.conf",_homePath.c_str(),nwid);
						OSUtils::rm(nlcpath);
					}
				} else {
					_nets.erase(nwid);
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
					::fprintf(stderr,"%s" ZT_EOL_S,(const char *)metaData);
					::fflush(stderr);
				}
			}	break;

			case ZT_EVENT_USER_MESSAGE: {
				const ZT_UserMessage *um = reinterpret_cast<const ZT_UserMessage *>(metaData);
				if ((um->typeId == ZT_SOFTWARE_UPDATE_USER_MESSAGE_TYPE)&&(_updater)) {
					_updater->handleSoftwareUpdateUserMessage(um->origin,um->data,um->length);
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
		unsigned int fromBindingNo = 0;

		if (addr->ss_family == AF_INET) {
			if (reinterpret_cast<const struct sockaddr_in *>(localAddr)->sin_port == 0) {
				// If sender is sending from wildcard (null address), choose the secondary backup
				// port 1/4 of the time. (but only for IPv4)
				fromBindingNo = (++_udpPortPickerCounter & 0x4) >> 2;
				if (!_ports[fromBindingNo])
					fromBindingNo = 0;
			} else {
				const uint16_t lp = reinterpret_cast<const struct sockaddr_in *>(localAddr)->sin_port;
				if (lp == _portsBE[1])
					fromBindingNo = 1;
				else if (lp == _portsBE[2])
					fromBindingNo = 2;
			}

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
				const uint16_t lp = reinterpret_cast<const struct sockaddr_in6 *>(localAddr)->sin6_port;
				if (lp == _portsBE[1])
					fromBindingNo = 1;
				else if (lp == _portsBE[2])
					fromBindingNo = 2;
			}
		} else {
			return -1;
		}

#ifdef ZT_BREAK_UDP
		if (OSUtils::fileExists("/tmp/ZT_BREAK_UDP"))
			return 0; // silently break UDP
#endif

		return (_bindings[fromBindingNo].udpSend(_phy,*(reinterpret_cast<const InetAddress *>(localAddr)),*(reinterpret_cast<const InetAddress *>(addr)),data,len,ttl)) ? 0 : -1;
	}

	inline void nodeVirtualNetworkFrameFunction(uint64_t nwid,void **nuptr,uint64_t sourceMac,uint64_t destMac,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
	{
		NetworkState *n = reinterpret_cast<NetworkState *>(*nuptr);
		if ((!n)||(!n->tap))
			return;
		n->tap->put(MAC(sourceMac),MAC(destMac),etherType,data,len);
	}

	inline int nodePathCheckFunction(uint64_t ztaddr,const struct sockaddr_storage *localAddr,const struct sockaddr_storage *remoteAddr)
	{
		// Make sure we're not trying to do ZeroTier-over-ZeroTier
		{
			Mutex::Lock _l(_nets_m);
			for(std::map<uint64_t,NetworkState>::const_iterator n(_nets.begin());n!=_nets.end();++n) {
				if (n->second.tap) {
					std::vector<InetAddress> ips(n->second.tap->ips());
					for(std::vector<InetAddress>::const_iterator i(ips.begin());i!=ips.end();++i) {
						if (i->containsAddress(*(reinterpret_cast<const InetAddress *>(remoteAddr)))) {
							return 0;
						}
					}
				}
			}
		}

		/* Note: I do not think we need to scan for overlap with managed routes
		 * because of the "route forking" and interface binding that we do. This
		 * ensures (we hope) that ZeroTier traffic will still take the physical
		 * path even if its managed routes override this for other traffic. Will
		 * revisit if we see recursion problems. */

		// Check blacklists
		const Hashtable< uint64_t,std::vector<InetAddress> > *blh = (const Hashtable< uint64_t,std::vector<InetAddress> > *)0;
		const std::vector<InetAddress> *gbl = (const std::vector<InetAddress> *)0;
		if (remoteAddr->ss_family == AF_INET) {
			blh = &_v4Blacklists;
			gbl = &_globalV4Blacklist;
		} else if (remoteAddr->ss_family == AF_INET6) {
			blh = &_v6Blacklists;
			gbl = &_globalV6Blacklist;
		}
		if (blh) {
			Mutex::Lock _l(_localConfig_m);
			const std::vector<InetAddress> *l = blh->get(ztaddr);
			if (l) {
				for(std::vector<InetAddress>::const_iterator a(l->begin());a!=l->end();++a) {
					if (a->containsAddress(*reinterpret_cast<const InetAddress *>(remoteAddr)))
						return 0;
				}
			}
			for(std::vector<InetAddress>::const_iterator a(gbl->begin());a!=gbl->end();++a) {
				if (a->containsAddress(*reinterpret_cast<const InetAddress *>(remoteAddr)))
					return 0;
			}
		}

		return 1;
	}

	inline int nodePathLookupFunction(uint64_t ztaddr,int family,struct sockaddr_storage *result)
	{
		const Hashtable< uint64_t,std::vector<InetAddress> > *lh = (const Hashtable< uint64_t,std::vector<InetAddress> > *)0;
		if (family < 0)
			lh = (_node->prng() & 1) ? &_v4Hints : &_v6Hints;
		else if (family == AF_INET)
			lh = &_v4Hints;
		else if (family == AF_INET6)
			lh = &_v6Hints;
		else return 0;
		const std::vector<InetAddress> *l = lh->get(ztaddr);
		if ((l)&&(l->size() > 0)) {
			memcpy(result,&((*l)[(unsigned long)_node->prng() % l->size()]),sizeof(struct sockaddr_storage));
			return 1;
		} else return 0;
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

		bool allow;
		{
			Mutex::Lock _l(_localConfig_m);
			if (_allowManagementFrom.size() == 0) {
				allow = (tc->from.ipScope() == InetAddress::IP_SCOPE_LOOPBACK);
			} else {
				allow = false;
				for(std::vector<InetAddress>::const_iterator i(_allowManagementFrom.begin());i!=_allowManagementFrom.end();++i) {
					if (i->containsAddress(tc->from)) {
						allow = true;
						break;
					}
				}
			}
		}

		if (allow) {
			try {
				if (_controlPlane)
					scode = _controlPlane->handleRequest(tc->from,tc->parser.method,tc->url,tc->headers,tc->body,data,contentType);
				else scode = 500;
			} catch (std::exception &exc) {
				fprintf(stderr,"WARNING: unexpected exception processing control HTTP request: %s" ZT_EOL_S,exc.what());
				scode = 500;
			} catch ( ... ) {
				fprintf(stderr,"WARNING: unexpected exception processing control HTTP request: unknown exceptino" ZT_EOL_S);
				scode = 500;
			}
		} else {
			scode = 401;
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

	bool shouldBindInterface(const char *ifname,const InetAddress &ifaddr)
	{
#if defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)
		if ((ifname[0] == 'l')&&(ifname[1] == 'o')) return false; // loopback
		if ((ifname[0] == 'z')&&(ifname[1] == 't')) return false; // sanity check: zt#
		if ((ifname[0] == 't')&&(ifname[1] == 'u')&&(ifname[2] == 'n')) return false; // tun# is probably an OpenVPN tunnel or similar
		if ((ifname[0] == 't')&&(ifname[1] == 'a')&&(ifname[2] == 'p')) return false; // tap# is probably an OpenVPN tunnel or similar
#endif

#ifdef __APPLE__
		if ((ifname[0] == 'l')&&(ifname[1] == 'o')) return false; // loopback
		if ((ifname[0] == 'z')&&(ifname[1] == 't')) return false; // sanity check: zt#
		if ((ifname[0] == 't')&&(ifname[1] == 'u')&&(ifname[2] == 'n')) return false; // tun# is probably an OpenVPN tunnel or similar
		if ((ifname[0] == 't')&&(ifname[1] == 'a')&&(ifname[2] == 'p')) return false; // tap# is probably an OpenVPN tunnel or similar
		if ((ifname[0] == 'u')&&(ifname[1] == 't')&&(ifname[2] == 'u')&&(ifname[3] == 'n')) return false; // ... as is utun#
#endif

		{
			Mutex::Lock _l(_localConfig_m);
			for(std::vector<std::string>::const_iterator p(_interfacePrefixBlacklist.begin());p!=_interfacePrefixBlacklist.end();++p) {
				if (!strncmp(p->c_str(),ifname,p->length()))
					return false;
			}
		}

		{
			Mutex::Lock _l(_nets_m);
			for(std::map<uint64_t,NetworkState>::const_iterator n(_nets.begin());n!=_nets.end();++n) {
				if (n->second.tap) {
					std::vector<InetAddress> ips(n->second.tap->ips());
					for(std::vector<InetAddress>::const_iterator i(ips.begin());i!=ips.end();++i) {
						if (i->ipsEqual(ifaddr))
							return false;
					}
				}
			}
		}

		return true;
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

	bool _trialBind(unsigned int port)
	{
		struct sockaddr_in in4;
		struct sockaddr_in6 in6;
		PhySocket *tb;

		memset(&in4,0,sizeof(in4));
		in4.sin_family = AF_INET;
		in4.sin_port = Utils::hton((uint16_t)port);
		tb = _phy.udpBind(reinterpret_cast<const struct sockaddr *>(&in4),(void *)0,0);
		if (tb) {
			_phy.close(tb,false);
			tb = _phy.tcpListen(reinterpret_cast<const struct sockaddr *>(&in4),(void *)0);
			if (tb) {
				_phy.close(tb,false);
				return true;
			}
		}

		memset(&in6,0,sizeof(in6));
		in6.sin6_family = AF_INET6;
		in6.sin6_port = Utils::hton((uint16_t)port);
		tb = _phy.udpBind(reinterpret_cast<const struct sockaddr *>(&in6),(void *)0,0);
		if (tb) {
			_phy.close(tb,false);
			tb = _phy.tcpListen(reinterpret_cast<const struct sockaddr *>(&in6),(void *)0);
			if (tb) {
				_phy.close(tb,false);
				return true;
			}
		}

		return false;
	}
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
static int SnodePathCheckFunction(ZT_Node *node,void *uptr,uint64_t ztaddr,const struct sockaddr_storage *localAddr,const struct sockaddr_storage *remoteAddr)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodePathCheckFunction(ztaddr,localAddr,remoteAddr); }
static int SnodePathLookupFunction(ZT_Node *node,void *uptr,uint64_t ztaddr,int family,struct sockaddr_storage *result)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodePathLookupFunction(ztaddr,family,result); }

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
	return (int)(impl->_clusterDefinition->geo().locate(*(reinterpret_cast<const InetAddress *>(addr)),*x,*y,*z));
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
#if (HTTP_PARSER_VERSION_MAJOR >= 2) && (HTTP_PARSER_VERSION_MINOR >= 2)
static int ShttpOnStatus(http_parser *parser,const char *ptr,size_t length)
#else
static int ShttpOnStatus(http_parser *parser)
#endif
{
	/*
	TcpConnection *tc = reinterpret_cast<TcpConnection *>(parser->data);
	tc->messageSize += (unsigned long)length;
	if (tc->messageSize > ZT_MAX_HTTP_MESSAGE_SIZE)
		return -1;
	tc->status.append(ptr,length);
	*/
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
	return OSUtils::platformDefaultHomePath();
}

OneService *OneService::newInstance(const char *hp,unsigned int port) { return new OneServiceImpl(hp,port); }
OneService::~OneService() {}

} // namespace ZeroTier
