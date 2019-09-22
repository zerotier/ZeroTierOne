/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "../include/ZeroTierCore.h"

#include "../node/Constants.hpp"
#include "../node/Mutex.hpp"
#include "../node/Node.hpp"
#include "../node/Utils.hpp"
#include "../node/InetAddress.hpp"
#include "../node/MAC.hpp"
#include "../node/Identity.hpp"
#include "../node/Salsa20.hpp"
#include "../node/Poly1305.hpp"
#include "../node/SHA512.hpp"

#include "../osdep/Phy.hpp"
#include "../osdep/Thread.hpp"
#include "../osdep/OSUtils.hpp"
#include "../osdep/Http.hpp"
#include "../osdep/PortMapper.hpp"
#include "../osdep/Binder.hpp"
#include "../osdep/ManagedRoute.hpp"
#include "../osdep/BlockingQueue.hpp"

#include "OneService.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#include <ShlObj.h>
#include <netioapi.h>
#include <iphlpapi.h>
//#include <unistd.h>
#define stat _stat
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ifaddrs.h>
#endif

#ifdef ZT_USE_SYSTEM_HTTP_PARSER
#include <http_parser.h>
#else
#include "../ext/http-parser/http_parser.h"
#endif

#include "../ext/json/json.hpp"

using json = nlohmann::json;

#include "../controller/EmbeddedNetworkController.hpp"
#include "../controller/RabbitMQ.hpp"
#include "../osdep/EthernetTap.hpp"
#ifdef __WINDOWS__
#include "../osdep/WindowsEthernetTap.hpp"
#endif

// Sanity limits for HTTP
#define ZT_MAX_HTTP_MESSAGE_SIZE (1024 * 1024 * 64)
#define ZT_MAX_HTTP_CONNECTIONS 65536

// Interface metric for ZeroTier taps -- this ensures that if we are on WiFi and also
// bridged via ZeroTier to the same LAN traffic will (if the OS is sane) prefer WiFi.
#define ZT_IF_METRIC 5000

// How often to check for new multicast subscriptions on a tap device
#define ZT_TAP_CHECK_MULTICAST_INTERVAL 5000

// How often to check for local interface addresses
#define ZT_LOCAL_INTERFACE_CHECK_INTERVAL 60000

// How often local.conf is checked for changes
#define ZT_LOCAL_CONF_FILE_CHECK_INTERVAL 10000

namespace ZeroTier {

namespace {

static void _networkToJson(nlohmann::json &nj,const ZT_VirtualNetworkConfig *nc,const std::string &portDeviceName,const OneService::NetworkSettings &localSettings)
{
	char tmp[256];

	const char *nstatus = "",*ntype = "";
	switch(nc->status) {
		case ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION: nstatus = "REQUESTING_CONFIGURATION"; break;
		case ZT_NETWORK_STATUS_OK:                       nstatus = "OK"; break;
		case ZT_NETWORK_STATUS_ACCESS_DENIED:            nstatus = "ACCESS_DENIED"; break;
		case ZT_NETWORK_STATUS_NOT_FOUND:                nstatus = "NOT_FOUND"; break;
		case ZT_NETWORK_STATUS_PORT_ERROR:               nstatus = "PORT_ERROR"; break;
		case ZT_NETWORK_STATUS_CLIENT_TOO_OLD:           nstatus = "CLIENT_TOO_OLD"; break;
	}
	switch(nc->type) {
		case ZT_NETWORK_TYPE_PRIVATE:                    ntype = "PRIVATE"; break;
		case ZT_NETWORK_TYPE_PUBLIC:                     ntype = "PUBLIC"; break;
	}

	OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.16llx",nc->nwid);
	nj["id"] = tmp;
	nj["nwid"] = tmp;
	OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",(unsigned int)((nc->mac >> 40) & 0xff),(unsigned int)((nc->mac >> 32) & 0xff),(unsigned int)((nc->mac >> 24) & 0xff),(unsigned int)((nc->mac >> 16) & 0xff),(unsigned int)((nc->mac >> 8) & 0xff),(unsigned int)(nc->mac & 0xff));
	nj["mac"] = tmp;
	nj["name"] = nc->name;
	nj["status"] = nstatus;
	nj["type"] = ntype;
	nj["mtu"] = nc->mtu;
	nj["dhcp"] = (bool)(nc->dhcp != 0);
	nj["bridge"] = (bool)(nc->bridge != 0);
	nj["broadcastEnabled"] = (bool)(nc->broadcastEnabled != 0);
	nj["portError"] = nc->portError;
	nj["netconfRevision"] = nc->netconfRevision;
	nj["portDeviceName"] = portDeviceName;
	nj["allowManaged"] = localSettings.allowManaged;
	nj["allowGlobal"] = localSettings.allowGlobal;
	nj["allowDefault"] = localSettings.allowDefault;

	nlohmann::json aa = nlohmann::json::array();
	for(unsigned int i=0;i<nc->assignedAddressCount;++i) {
		aa.push_back(reinterpret_cast<const InetAddress *>(&(nc->assignedAddresses[i]))->toString(tmp));
	}
	nj["assignedAddresses"] = aa;

	nlohmann::json ra = nlohmann::json::array();
	for(unsigned int i=0;i<nc->routeCount;++i) {
		nlohmann::json rj;
		rj["target"] = reinterpret_cast<const InetAddress *>(&(nc->routes[i].target))->toString(tmp);
		if (nc->routes[i].via.ss_family == nc->routes[i].target.ss_family)
			rj["via"] = reinterpret_cast<const InetAddress *>(&(nc->routes[i].via))->toIpString(tmp);
		else rj["via"] = nlohmann::json();
		rj["flags"] = (int)nc->routes[i].flags;
		rj["metric"] = (int)nc->routes[i].metric;
		ra.push_back(rj);
	}
	nj["routes"] = ra;

	nlohmann::json mca = nlohmann::json::array();
	for(unsigned int i=0;i<nc->multicastSubscriptionCount;++i) {
		nlohmann::json m;
		m["mac"] = MAC(nc->multicastSubscriptions[i].mac).toString(tmp);
		m["adi"] = nc->multicastSubscriptions[i].adi;
		mca.push_back(m);
	}
	nj["multicastSubscriptions"] = mca;
}

static void _peerToJson(nlohmann::json &pj,const ZT_Peer *peer)
{
	char tmp[256];

	const char *prole = "";
	switch(peer->role) {
		case ZT_PEER_ROLE_LEAF: prole = "LEAF"; break;
		case ZT_PEER_ROLE_MOON: prole = "MOON"; break;
		case ZT_PEER_ROLE_PLANET: prole = "PLANET"; break;
	}

	OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.10llx",peer->address);
	pj["address"] = tmp;
	pj["versionMajor"] = peer->versionMajor;
	pj["versionMinor"] = peer->versionMinor;
	pj["versionRev"] = peer->versionRev;
	OSUtils::ztsnprintf(tmp,sizeof(tmp),"%d.%d.%d",peer->versionMajor,peer->versionMinor,peer->versionRev);
	pj["version"] = tmp;
	pj["latency"] = peer->latency;
	pj["role"] = prole;

	nlohmann::json pa = nlohmann::json::array();
	for(unsigned int i=0;i<peer->pathCount;++i) {
		int64_t lastSend = peer->paths[i].lastSend;
		int64_t lastReceive = peer->paths[i].lastReceive;
		nlohmann::json j;
		j["address"] = reinterpret_cast<const InetAddress *>(&(peer->paths[i].address))->toString(tmp);
		j["lastSend"] = (lastSend < 0) ? 0 : lastSend;
		j["lastReceive"] = (lastReceive < 0) ? 0 : lastReceive;
		j["trustedPathId"] = peer->paths[i].trustedPathId;
		j["active"] = (bool)(peer->paths[i].expired == 0);
		j["expired"] = (bool)(peer->paths[i].expired != 0);
		j["preferred"] = (bool)(peer->paths[i].preferred != 0);
		pa.push_back(j);
	}
	pj["paths"] = pa;
}

static void _peerAggregateLinkToJson(nlohmann::json &pj,const ZT_Peer *peer)
{
	char tmp[256];
	OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.10llx",peer->address);
	pj["aggregateLinkLatency"] = peer->latency;

	nlohmann::json pa = nlohmann::json::array();
	for(unsigned int i=0;i<peer->pathCount;++i) {
		int64_t lastSend = peer->paths[i].lastSend;
		int64_t lastReceive = peer->paths[i].lastReceive;
		nlohmann::json j;
		j["address"] = reinterpret_cast<const InetAddress *>(&(peer->paths[i].address))->toString(tmp);
		j["lastSend"] = (lastSend < 0) ? 0 : lastSend;
		j["lastReceive"] = (lastReceive < 0) ? 0 : lastReceive;
		//j["trustedPathId"] = peer->paths[i].trustedPathId;
		//j["active"] = (bool)(peer->paths[i].expired == 0);
		//j["expired"] = (bool)(peer->paths[i].expired != 0);
		//j["preferred"] = (bool)(peer->paths[i].preferred != 0);
		j["latency"] = peer->paths[i].latency;
		j["pdv"] = peer->paths[i].packetDelayVariance;
		//j["throughputDisturbCoeff"] = peer->paths[i].throughputDisturbCoeff;
		//j["packetErrorRatio"] = peer->paths[i].packetErrorRatio;
		//j["packetLossRatio"] = peer->paths[i].packetLossRatio;
		j["stability"] = peer->paths[i].stability;
		j["throughput"] = peer->paths[i].throughput;
		//j["maxThroughput"] = peer->paths[i].maxThroughput;
		j["allocation"] = peer->paths[i].allocation;
		j["ifname"] = peer->paths[i].ifname;
		pa.push_back(j);
	}
	pj["paths"] = pa;
}

class OneServiceImpl;

static int SnodeVirtualNetworkConfigFunction(ZT_Node *node,void *uptr,void *tptr,uint64_t nwid,void **nuptr,enum ZT_VirtualNetworkConfigOperation op,const ZT_VirtualNetworkConfig *nwconf);
static void SnodeEventCallback(ZT_Node *node,void *uptr,void *tptr,enum ZT_Event event,const void *metaData);
static void SnodeStatePutFunction(ZT_Node *node,void *uptr,void *tptr,enum ZT_StateObjectType type,const uint64_t id[2],const void *data,int len);
static int SnodeStateGetFunction(ZT_Node *node,void *uptr,void *tptr,enum ZT_StateObjectType type,const uint64_t id[2],void *data,unsigned int maxlen);
static int SnodeWirePacketSendFunction(ZT_Node *node,void *uptr,void *tptr,int64_t localSocket,const struct sockaddr_storage *addr,const void *data,unsigned int len,unsigned int ttl);
static void SnodeVirtualNetworkFrameFunction(ZT_Node *node,void *uptr,void *tptr,uint64_t nwid,void **nuptr,uint64_t sourceMac,uint64_t destMac,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len);
static int SnodePathCheckFunction(ZT_Node *node,void *uptr,void *tptr,uint64_t ztaddr,int64_t localSocket,const struct sockaddr_storage *remoteAddr);
static int SnodePathLookupFunction(ZT_Node *node,void *uptr,void *tptr,uint64_t ztaddr,int family,struct sockaddr_storage *result);
static void StapFrameHandler(void *uptr,void *tptr,uint64_t nwid,const MAC &from,const MAC &to,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len);

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

/**
 * A TCP connection and related state and buffers
 */
struct TcpConnection
{
	enum {
		TCP_UNCATEGORIZED_INCOMING, // uncategorized incoming connection
		TCP_HTTP_INCOMING,
	} type;

	OneServiceImpl *parent;
	PhySocket *sock;
	InetAddress remoteAddr;
	uint64_t lastReceive;

	// Used for inbound HTTP connections
	http_parser parser;
	unsigned long messageSize;
	std::string currentHeaderField;
	std::string currentHeaderValue;
	std::string url;
	std::string status;
	std::map< std::string,std::string > headers;

	std::string readq;
	std::string writeq;
	Mutex writeq_m;
};

class OneServiceImpl : public OneService
{
public:
	// begin member variables --------------------------------------------------

	const std::string _homePath;
	std::string _authToken;
	std::string _controllerDbPath;
	const std::string _networksPath;
	const std::string _moonsPath;

	EmbeddedNetworkController *_controller;
	Phy<OneServiceImpl *> _phy;
	Node *_node;
	PhySocket *_localControlSocket4;
	PhySocket *_localControlSocket6;
	bool _updateAutoApply;
	bool _allowSecondaryPort;
	unsigned int _multipathMode;
	unsigned int _primaryPort;
	unsigned int _secondaryPort;
	unsigned int _tertiaryPort;

	// Local configuration and memo-ized information from it
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

	std::vector<InetAddress> _explicitBind;

	/*
	 * To attempt to handle NAT/gateway craziness we use three local UDP ports:
	 *
	 * [0] is the normal/default port, usually 9993
	 * [1] is a port derived from our ZeroTier address
	 * [2] is a port computed from the normal/default for use with uPnP/NAT-PMP mappings
	 *
	 * [2] exists because on some gateways trying to do regular NAT-t interferes
	 * destructively with uPnP port mapping behavior in very weird buggy ways.
	 * It's only used if uPnP/NAT-PMP is enabled in this build.
	 */
	unsigned int _ports[3];
	Binder _binder;

	// Time we last received a packet from a global address
	uint64_t _lastDirectReceiveFromGlobal;

	// Last potential sleep/wake event
	uint64_t _lastRestart;

	// Deadline for the next background task service function
	volatile int64_t _nextBackgroundTaskDeadline;

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

		std::shared_ptr<EthernetTap> tap;
		ZT_VirtualNetworkConfig config; // memcpy() of raw config from core
		std::vector<InetAddress> managedIps;
		std::list< SharedPtr<ManagedRoute> > managedRoutes;
		NetworkSettings settings;
	};
	std::map<uint64_t,NetworkState> _nets;
	Mutex _nets_m;

	// Active TCP/IP connections
	std::vector< TcpConnection * > _tcpConnections;
	Mutex _tcpConnections_m;

	// Termination status information
	ReasonForTermination _termReason;
	std::string _fatalErrorMessage;
	Mutex _termReason_m;

	// uPnP/NAT-PMP port mapper if enabled
	bool _portMappingEnabled; // local.conf settings
	PortMapper *_portMapper;

	// Set to false to force service to stop
	volatile bool _run;
	Mutex _run_m;

	MQConfig *_mqc;

	// end member variables ----------------------------------------------------

	OneServiceImpl(const char *hp,unsigned int port) :
		_homePath((hp) ? hp : ".")
		,_controllerDbPath(_homePath + ZT_PATH_SEPARATOR_S "controller.d")
		,_networksPath(_homePath + ZT_PATH_SEPARATOR_S "networks.d")
		,_moonsPath(_homePath + ZT_PATH_SEPARATOR_S "moons.d")
		,_controller((EmbeddedNetworkController *)0)
		,_phy(this,false,true)
		,_node((Node *)0)
		,_localControlSocket4((PhySocket *)0)
		,_localControlSocket6((PhySocket *)0)
		,_updateAutoApply(false)
		,_primaryPort(port)
		,_lastDirectReceiveFromGlobal(0)
		,_lastRestart(0)
		,_nextBackgroundTaskDeadline(0)
		,_termReason(ONE_STILL_RUNNING)
		,_portMappingEnabled(true)
		,_portMapper((PortMapper *)0)
		,_run(true)
		,_mqc(NULL)
	{
		_ports[0] = 0;
		_ports[1] = 0;
		_ports[2] = 0;
	}

	virtual ~OneServiceImpl()
	{
		_binder.closeAll(_phy);
		_phy.close(_localControlSocket4);
		_phy.close(_localControlSocket6);

		delete _portMapper;
		delete _controller;
		delete _mqc;
	}

	virtual ReasonForTermination run()
	{
		try {
			{
				const std::string authTokenPath(_homePath + ZT_PATH_SEPARATOR_S "authtoken.secret");
				if (!OSUtils::readFile(authTokenPath.c_str(),_authToken)) {
					unsigned char foo[24];
					Utils::getSecureRandom(foo,sizeof(foo));
					_authToken = "";
					for(unsigned int i=0;i<sizeof(foo);++i)
						_authToken.push_back("abcdefghijklmnopqrstuvwxyz0123456789"[(unsigned long)foo[i] % 36]);
					if (!OSUtils::writeFile(authTokenPath.c_str(),_authToken)) {
						Mutex::Lock _l(_termReason_m);
						_termReason = ONE_UNRECOVERABLE_ERROR;
						_fatalErrorMessage = "authtoken.secret could not be written";
						return _termReason;
					} else {
						OSUtils::lockDownFile(authTokenPath.c_str(),false);
					}
				}
				_authToken = OSUtils::trimString(_authToken);
			}

			{
				struct ZT_Node_Callbacks cb;
				cb.stateGetFunction = SnodeStateGetFunction;
				cb.statePutFunction = SnodeStatePutFunction;
				cb.wirePacketSendFunction = SnodeWirePacketSendFunction;
				cb.virtualNetworkFrameFunction = SnodeVirtualNetworkFrameFunction;
				cb.virtualNetworkConfigFunction = SnodeVirtualNetworkConfigFunction;
				cb.eventCallback = SnodeEventCallback;
				cb.pathCheckFunction = SnodePathCheckFunction;
				cb.pathLookupFunction = SnodePathLookupFunction;
				_node = new Node(this,(void *)0,&cb,OSUtils::now());
			}

			// local.conf
			readLocalSettings();
			applyLocalConfig();

			// Make sure we can use the primary port, and hunt for one if configured to do so
			const int portTrials = (_primaryPort == 0) ? 256 : 1; // if port is 0, pick random
			for(int k=0;k<portTrials;++k) {
				if (_primaryPort == 0) {
					unsigned int randp = 0;
					Utils::getSecureRandom(&randp,sizeof(randp));
					_primaryPort = 20000 + (randp % 45500);
				}
				if (_trialBind(_primaryPort)) {
					_ports[0] = _primaryPort;
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

			// Bind TCP control socket to 127.0.0.1 and ::1 as well for loopback TCP control socket queries
			{
				struct sockaddr_in lo4;
				memset(&lo4,0,sizeof(lo4));
				lo4.sin_family = AF_INET;
				lo4.sin_addr.s_addr = Utils::hton((uint32_t)0x7f000001);
				lo4.sin_port = Utils::hton((uint16_t)_ports[0]);
				_localControlSocket4 = _phy.tcpListen((const struct sockaddr *)&lo4);
				struct sockaddr_in6 lo6;
				memset(&lo6,0,sizeof(lo6));
				lo6.sin6_family = AF_INET6;
				lo6.sin6_addr.s6_addr[15] = 1;
				lo6.sin6_port = lo4.sin_port;
				_localControlSocket6 = _phy.tcpListen((const struct sockaddr *)&lo6);
			}

			// Save primary port to a file so CLIs and GUIs can learn it easily
			char portstr[64];
			OSUtils::ztsnprintf(portstr,sizeof(portstr),"%u",_ports[0]);
			OSUtils::writeFile((_homePath + ZT_PATH_SEPARATOR_S "zerotier-one.port").c_str(),std::string(portstr));

			// Attempt to bind to a secondary port chosen from our ZeroTier address.
			// This exists because there are buggy NATs out there that fail if more
			// than one device behind the same NAT tries to use the same internal
			// private address port number. Buggy NATs are a running theme.
			if (_allowSecondaryPort) {
				_ports[1] = (_secondaryPort == 0) ? 20000 + ((unsigned int)_node->address() % 45500) : _secondaryPort;
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
			}

			if (_portMappingEnabled) {
				// If we're running uPnP/NAT-PMP, bind a *third* port for that. We can't
				// use the other two ports for that because some NATs do really funky
				// stuff with ports that are explicitly mapped that breaks things.
				if (_ports[1]) {
					_ports[2] = (_tertiaryPort == 0) ? _ports[1] : _tertiaryPort;
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
						OSUtils::ztsnprintf(uniqueName,sizeof(uniqueName),"ZeroTier/%.10llx@%u",_node->address(),_ports[2]);
						_portMapper = new PortMapper(_ports[2],uniqueName);
					}
				}
			}

			// Delete legacy iddb.d if present (cleanup)
			OSUtils::rmDashRf((_homePath + ZT_PATH_SEPARATOR_S "iddb.d").c_str());

			// Network controller is now enabled by default for desktop and server
			_controller = new EmbeddedNetworkController(_node,_homePath.c_str(),_controllerDbPath.c_str(),_ports[0], _mqc);
			_node->setController((void *)_controller);

			// Join existing networks in networks.d
			{
				std::vector<std::string> networksDotD(OSUtils::listDirectory((_homePath + ZT_PATH_SEPARATOR_S "networks.d").c_str()));
				for(std::vector<std::string>::iterator f(networksDotD.begin());f!=networksDotD.end();++f) {
					std::size_t dot = f->find_last_of('.');
					if ((dot == 16)&&(f->substr(16) == ".conf"))
						_node->join(Utils::hexStrToU64(f->substr(0,dot).c_str()),(void *)0,(void *)0);
				}
			}

			// Main I/O loop
			_nextBackgroundTaskDeadline = 0;
			int64_t clockShouldBe = OSUtils::now();
			_lastRestart = clockShouldBe;
			int64_t lastTapMulticastGroupCheck = 0;
			int64_t lastBindRefresh = 0;
			int64_t lastMultipathModeUpdate = 0;
			int64_t lastCleanedPeersDb = 0;
			int64_t lastLocalInterfaceAddressCheck = (clockShouldBe - ZT_LOCAL_INTERFACE_CHECK_INTERVAL) + 15000; // do this in 15s to give portmapper time to configure and other things time to settle
			int64_t lastLocalConfFileCheck = OSUtils::now();
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

				const int64_t now = OSUtils::now();

				// Attempt to detect sleep/wake events by detecting delay overruns
				bool restarted = false;
				if ((now > clockShouldBe)&&((now - clockShouldBe) > 10000)) {
					_lastRestart = now;
					restarted = true;
				}

				// Reload local.conf if anything changed recently
				if ((now - lastLocalConfFileCheck) >= ZT_LOCAL_CONF_FILE_CHECK_INTERVAL) {
					lastLocalConfFileCheck = now;
					struct stat result;
					if(stat((_homePath + ZT_PATH_SEPARATOR_S "local.conf").c_str(), &result)==0) {
						int64_t mod_time = result.st_mtime * 1000;
						if ((now - mod_time) <= ZT_LOCAL_CONF_FILE_CHECK_INTERVAL) {
							readLocalSettings();
							applyLocalConfig();
						}
					}
				}

				// Refresh bindings in case device's interfaces have changed, and also sync routes to update any shadow routes (e.g. shadow default)
				if (((now - lastBindRefresh) >= (_multipathMode ? ZT_BINDER_REFRESH_PERIOD / 8 : ZT_BINDER_REFRESH_PERIOD))||(restarted)) {
					lastBindRefresh = now;
					unsigned int p[3];
					unsigned int pc = 0;
					for(int i=0;i<3;++i) {
						if (_ports[i])
							p[pc++] = _ports[i];
					}
					_binder.refresh(_phy,p,pc,_explicitBind,*this);
					{
						Mutex::Lock _l(_nets_m);
						for(std::map<uint64_t,NetworkState>::iterator n(_nets.begin());n!=_nets.end();++n) {
							if (n->second.tap)
								syncManagedStuff(n->second,false,true);
						}
					}
				}
				// Update multipath mode (if needed)
				if (((now - lastMultipathModeUpdate) >= ZT_BINDER_REFRESH_PERIOD / 8)||(restarted)) {
					lastMultipathModeUpdate = now;
					_node->setMultipathMode(_multipathMode);
				}

				// Run background task processor in core if it's time to do so
				int64_t dl = _nextBackgroundTaskDeadline;
				if (dl <= now) {
					_node->processBackgroundTasks((void *)0,now,&_nextBackgroundTaskDeadline);
					dl = _nextBackgroundTaskDeadline;
				}

				// Sync multicast group memberships
				if ((now - lastTapMulticastGroupCheck) >= ZT_TAP_CHECK_MULTICAST_INTERVAL) {
					lastTapMulticastGroupCheck = now;
					std::vector< std::pair< uint64_t,std::pair< std::vector<MulticastGroup>,std::vector<MulticastGroup> > > > mgChanges;
					{
						Mutex::Lock _l(_nets_m);
						mgChanges.reserve(_nets.size() + 1);
						for(std::map<uint64_t,NetworkState>::const_iterator n(_nets.begin());n!=_nets.end();++n) {
							if (n->second.tap) {
								mgChanges.push_back(std::pair< uint64_t,std::pair< std::vector<MulticastGroup>,std::vector<MulticastGroup> > >(n->first,std::pair< std::vector<MulticastGroup>,std::vector<MulticastGroup> >()));
								n->second.tap->scanMulticastGroups(mgChanges.back().second.first,mgChanges.back().second.second);
							}
						}
					}
					for(std::vector< std::pair< uint64_t,std::pair< std::vector<MulticastGroup>,std::vector<MulticastGroup> > > >::iterator c(mgChanges.begin());c!=mgChanges.end();++c) {
						for(std::vector<MulticastGroup>::iterator m(c->second.first.begin());m!=c->second.first.end();++m)
							_node->multicastSubscribe((void *)0,c->first,m->mac().toInt(),m->adi());
						for(std::vector<MulticastGroup>::iterator m(c->second.second.begin());m!=c->second.second.end();++m)
							_node->multicastUnsubscribe(c->first,m->mac().toInt(),m->adi());
					}
				}

				// Sync information about physical network interfaces
				if ((now - lastLocalInterfaceAddressCheck) >= (_multipathMode ? ZT_LOCAL_INTERFACE_CHECK_INTERVAL / 8 : ZT_LOCAL_INTERFACE_CHECK_INTERVAL)) {
					lastLocalInterfaceAddressCheck = now;

					_node->clearLocalInterfaceAddresses();

					if (_portMapper) {
						std::vector<InetAddress> mappedAddresses(_portMapper->get());
						for(std::vector<InetAddress>::const_iterator ext(mappedAddresses.begin());ext!=mappedAddresses.end();++ext)
							_node->addLocalInterfaceAddress(reinterpret_cast<const struct sockaddr_storage *>(&(*ext)));
					}

					std::vector<InetAddress> boundAddrs(_binder.allBoundLocalInterfaceAddresses());
					for(std::vector<InetAddress>::const_iterator i(boundAddrs.begin());i!=boundAddrs.end();++i)
						_node->addLocalInterfaceAddress(reinterpret_cast<const struct sockaddr_storage *>(&(*i)));
				}

				// Clean peers.d periodically
				if ((now - lastCleanedPeersDb) >= 3600000) {
					lastCleanedPeersDb = now;
					OSUtils::cleanDirectory((_homePath + ZT_PATH_SEPARATOR_S "peers.d").c_str(),now - 2592000000LL); // delete older than 30 days
				}

				const unsigned long delay = (dl > now) ? (unsigned long)(dl - now) : 100;
				clockShouldBe = now + (uint64_t)delay;
				_phy.poll(delay);
			}
		} catch (std::exception &e) {
			Mutex::Lock _l(_termReason_m);
			_termReason = ONE_UNRECOVERABLE_ERROR;
			_fatalErrorMessage = std::string("unexpected exception in main thread: ")+e.what();
		} catch ( ... ) {
			Mutex::Lock _l(_termReason_m);
			_termReason = ONE_UNRECOVERABLE_ERROR;
			_fatalErrorMessage = "unexpected exception in main thread: unknown exception";
		}

		try {
			Mutex::Lock _l(_tcpConnections_m);
			while (!_tcpConnections.empty())
				_phy.close((*_tcpConnections.begin())->sock);
		} catch ( ... ) {}

		{
			Mutex::Lock _l(_nets_m);
			_nets.clear();
		}

		delete _node;
		_node = (Node *)0;

		return _termReason;
	}

	void readLocalSettings()
	{
		// Read local configuration
		std::map<InetAddress,ZT_PhysicalPathConfiguration> ppc;

		// LEGACY: support old "trustedpaths" flat file
		FILE *trustpaths = fopen((_homePath + ZT_PATH_SEPARATOR_S "trustedpaths").c_str(),"r");
		if (trustpaths) {
			fprintf(stderr,"WARNING: 'trustedpaths' flat file format is deprecated in favor of path definitions in local.conf" ZT_EOL_S);
			char buf[1024];
			while (fgets(buf,sizeof(buf),trustpaths)) {
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
				if ( (trustedPathId != 0) && ((trustedPathNetwork.ss_family == AF_INET)||(trustedPathNetwork.ss_family == AF_INET6)) && (trustedPathNetwork.netmaskBits() > 0) ) {
					ppc[trustedPathNetwork].trustedPathId = trustedPathId;
					ppc[trustedPathNetwork].mtu = 0; // use default
				}
			}
			fclose(trustpaths);
		}

		// Read local config file
		Mutex::Lock _l2(_localConfig_m);
		std::string lcbuf;
		if (OSUtils::readFile((_homePath + ZT_PATH_SEPARATOR_S "local.conf").c_str(),lcbuf)) {
			if (lcbuf.length() > 0) {
				try {
					_localConfig = OSUtils::jsonParse(lcbuf);
					if (!_localConfig.is_object()) {
						fprintf(stderr,"ERROR: unable to parse local.conf (root element is not a JSON object)" ZT_EOL_S);
						exit(1);
					}
				} catch ( ... ) {
					fprintf(stderr,"ERROR: unable to parse local.conf (invalid JSON)" ZT_EOL_S);
					exit(1);
				}
			}
		}

		// Get any trusted paths in local.conf (we'll parse the rest of physical[] elsewhere)
		json &physical = _localConfig["physical"];
		if (physical.is_object()) {
			for(json::iterator phy(physical.begin());phy!=physical.end();++phy) {
				InetAddress net(OSUtils::jsonString(phy.key(),"").c_str());
				if (net) {
					if (phy.value().is_object()) {
						uint64_t tpid;
						if ((tpid = OSUtils::jsonInt(phy.value()["trustedPathId"],0ULL)) != 0ULL) {
							if ((net.ss_family == AF_INET)||(net.ss_family == AF_INET6))
								ppc[net].trustedPathId = tpid;
						}
						ppc[net].mtu = (int)OSUtils::jsonInt(phy.value()["mtu"],0ULL); // 0 means use default
					}
				}
			}
		}

		json &settings = _localConfig["settings"];
		if (settings.is_object()) {
			// Allow controller DB path to be put somewhere else
			const std::string cdbp(OSUtils::jsonString(settings["controllerDbPath"],""));
			if (cdbp.length() > 0)
				_controllerDbPath = cdbp;

			json &rmq = settings["rabbitmq"];
			if (rmq.is_object() && _mqc == NULL) {
				fprintf(stderr, "Reading RabbitMQ Config\n");
				_mqc = new MQConfig;
				_mqc->port = rmq["port"];
				_mqc->host = OSUtils::jsonString(rmq["host"], "").c_str();
				_mqc->username = OSUtils::jsonString(rmq["username"], "").c_str();
				_mqc->password = OSUtils::jsonString(rmq["password"], "").c_str();
			}

			// Bind to wildcard instead of to specific interfaces (disables full tunnel capability)
			json &bind = settings["bind"];
			if (bind.is_array()) {
				for(unsigned long i=0;i<bind.size();++i) {
					const std::string ips(OSUtils::jsonString(bind[i],""));
					if (ips.length() > 0) {
						InetAddress ip(ips.c_str());
						if ((ip.ss_family == AF_INET)||(ip.ss_family == AF_INET6))
							_explicitBind.push_back(ip);
					}
				}
			}
		}

		// Set trusted paths if there are any
		if (ppc.size() > 0) {
			for(std::map<InetAddress,ZT_PhysicalPathConfiguration>::iterator i(ppc.begin());i!=ppc.end();++i)
				_node->setPhysicalPathConfiguration(reinterpret_cast<const struct sockaddr_storage *>(&(i->first)),&(i->second));
		}
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

#ifdef ZT_SDK
	virtual std::string givenHomePath()
	{
		return _homePath;
	}

	void getRoutes(uint64_t nwid, void *routeArray, unsigned int *numRoutes)
	{
		Mutex::Lock _l(_nets_m);
		NetworkState &n = _nets[nwid];
		*numRoutes = *numRoutes < n.config.routeCount ? *numRoutes : n.config.routeCount;
		for(unsigned int i=0; i<*numRoutes; i++) {
			ZT_VirtualNetworkRoute *vnr = (ZT_VirtualNetworkRoute*)routeArray;
			memcpy(&vnr[i], &(n.config.routes[i]), sizeof(ZT_VirtualNetworkRoute));
		}
	}

	virtual Node *getNode()
	{
		return _node;
	}
#endif // ZT_SDK

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
		settings = n->second.settings;
		return true;
	}

	virtual bool setNetworkSettings(const uint64_t nwid,const NetworkSettings &settings)
	{
		Mutex::Lock _l(_nets_m);

		std::map<uint64_t,NetworkState>::iterator n(_nets.find(nwid));
		if (n == _nets.end())
			return false;
		n->second.settings = settings;

		char nlcpath[4096];
		OSUtils::ztsnprintf(nlcpath,sizeof(nlcpath),"%s" ZT_PATH_SEPARATOR_S "%.16llx.local.conf",_networksPath.c_str(),nwid);
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

	// =========================================================================
	// Internal implementation methods for control plane, route setup, etc.
	// =========================================================================

	inline unsigned int handleControlPlaneHttpRequest(
		const InetAddress &fromAddress,
		unsigned int httpMethod,
		const std::string &path,
		const std::map<std::string,std::string> &headers,
		const std::string &body,
		std::string &responseBody,
		std::string &responseContentType)
	{
		char tmp[256];
		unsigned int scode = 404;
		json res;
		std::vector<std::string> ps(OSUtils::split(path.c_str(),"/","",""));
		std::map<std::string,std::string> urlArgs;

		/* Note: this is kind of restricted in what it'll take. It does not support
		 * URL encoding, and /'s in URL args will screw it up. But the only URL args
		 * it really uses in ?jsonp=funcionName, and otherwise it just takes simple
		 * paths to simply-named resources. */
		if (ps.size() > 0) {
			std::size_t qpos = ps[ps.size() - 1].find('?');
			if (qpos != std::string::npos) {
				std::string args(ps[ps.size() - 1].substr(qpos + 1));
				ps[ps.size() - 1] = ps[ps.size() - 1].substr(0,qpos);
				std::vector<std::string> asplit(OSUtils::split(args.c_str(),"&","",""));
				for(std::vector<std::string>::iterator a(asplit.begin());a!=asplit.end();++a) {
					std::size_t eqpos = a->find('=');
					if (eqpos == std::string::npos)
						urlArgs[*a] = "";
					else urlArgs[a->substr(0,eqpos)] = a->substr(eqpos + 1);
				}
			}
		} else {
			return 404;
		}

		bool isAuth = false;
		{
			std::map<std::string,std::string>::const_iterator ah(headers.find("x-zt1-auth"));
			if ((ah != headers.end())&&(_authToken == ah->second)) {
				isAuth = true;
			} else {
				ah = urlArgs.find("auth");
				if ((ah != urlArgs.end())&&(_authToken == ah->second))
					isAuth = true;
			}
		}

#ifdef __SYNOLOGY__
		// Authenticate via Synology's built-in cgi script
		if (!isAuth) {
			int synotoken_pos = path.find("SynoToken");
			int argpos = path.find("?");
			if(synotoken_pos != std::string::npos && argpos != std::string::npos) {
				std::string cookie = path.substr(argpos+1, synotoken_pos-(argpos+1));
				std::string synotoken = path.substr(synotoken_pos);
				std::string cookie_val = cookie.substr(cookie.find("=")+1);
				std::string synotoken_val = synotoken.substr(synotoken.find("=")+1);
				// Set necessary env for auth script
				std::map<std::string,std::string>::const_iterator ah2(headers.find("x-forwarded-for"));
				setenv("HTTP_COOKIE", cookie_val.c_str(), true);
				setenv("HTTP_X_SYNO_TOKEN", synotoken_val.c_str(), true);
				setenv("REMOTE_ADDR", ah2->second.c_str(),true);
				char user[256], buf[1024];
				FILE *fp = NULL;
				bzero(user, 256);
				fp = popen("/usr/syno/synoman/webman/modules/authenticate.cgi", "r");
				if(!fp)
					isAuth = false;
				else {
					bzero(buf, sizeof(buf));
					fread(buf, 1024, 1, fp);
					if(strlen(buf) > 0) {
						snprintf(user, 256, "%s", buf);
						isAuth = true;
					}
				}
				pclose(fp);
			}
		}
#endif

		if (httpMethod == HTTP_GET) {
			if (isAuth) {
				if (ps[0] == "status") {
					ZT_NodeStatus status;
					_node->status(&status);

					OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.10llx",status.address);
					res["address"] = tmp;
					res["publicIdentity"] = status.publicIdentity;
					res["online"] = (bool)(status.online != 0);
					res["versionMajor"] = ZEROTIER_ONE_VERSION_MAJOR;
					res["versionMinor"] = ZEROTIER_ONE_VERSION_MINOR;
					res["versionRev"] = ZEROTIER_ONE_VERSION_REVISION;
					res["versionBuild"] = ZEROTIER_ONE_VERSION_BUILD;
					OSUtils::ztsnprintf(tmp,sizeof(tmp),"%d.%d.%d",ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION);
					res["version"] = tmp;
					res["clock"] = OSUtils::now();

					{
						Mutex::Lock _l(_localConfig_m);
						res["config"] = _localConfig;
					}
					json &settings = res["config"]["settings"];
					settings["primaryPort"] = OSUtils::jsonInt(settings["primaryPort"],(uint64_t)_primaryPort) & 0xffff;

					if (_multipathMode) {
						json &multipathConfig = res["multipath"];
						ZT_PeerList *pl = _node->peers();
						char peerAddrStr[256];
						if (pl) {
							for(unsigned long i=0;i<pl->peerCount;++i) {
								if (pl->peers[i].hadAggregateLink) {
									nlohmann::json pj;
									_peerAggregateLinkToJson(pj,&(pl->peers[i]));
									OSUtils::ztsnprintf(peerAddrStr,sizeof(peerAddrStr),"%.10llx",pl->peers[i].address);
									multipathConfig[peerAddrStr] = (pj);
								}
							}
						}
					}

					settings["portMappingEnabled"] = OSUtils::jsonBool(settings["portMappingEnabled"],true);

					scode = 200;
				} else if (ps[0] == "network") {
					ZT_VirtualNetworkList *nws = _node->networks();
					if (nws) {
						if (ps.size() == 1) {
							// Return [array] of all networks

							res = nlohmann::json::array();
							for(unsigned long i=0;i<nws->networkCount;++i) {
								OneService::NetworkSettings localSettings;
								getNetworkSettings(nws->networks[i].nwid,localSettings);
								nlohmann::json nj;
								_networkToJson(nj,&(nws->networks[i]),portDeviceName(nws->networks[i].nwid),localSettings);
								res.push_back(nj);
							}

							scode = 200;
						} else if (ps.size() == 2) {
							// Return a single network by ID or 404 if not found

							const uint64_t wantnw = Utils::hexStrToU64(ps[1].c_str());
							for(unsigned long i=0;i<nws->networkCount;++i) {
								if (nws->networks[i].nwid == wantnw) {
									OneService::NetworkSettings localSettings;
									getNetworkSettings(nws->networks[i].nwid,localSettings);
									_networkToJson(res,&(nws->networks[i]),portDeviceName(nws->networks[i].nwid),localSettings);
									scode = 200;
									break;
								}
							}

						} else scode = 404;
						_node->freeQueryResult((void *)nws);
					} else scode = 500;
				} else if (ps[0] == "peer") {
					ZT_PeerList *pl = _node->peers();
					if (pl) {
						if (ps.size() == 1) {
							// Return [array] of all peers

							res = nlohmann::json::array();
							for(unsigned long i=0;i<pl->peerCount;++i) {
								nlohmann::json pj;
								_peerToJson(pj,&(pl->peers[i]));
								res.push_back(pj);
							}

							scode = 200;
						} else if (ps.size() == 2) {
							// Return a single peer by ID or 404 if not found

							uint64_t wantp = Utils::hexStrToU64(ps[1].c_str());
							for(unsigned long i=0;i<pl->peerCount;++i) {
								if (pl->peers[i].address == wantp) {
									_peerToJson(res,&(pl->peers[i]));
									scode = 200;
									break;
								}
							}

						} else scode = 404;
						_node->freeQueryResult((void *)pl);
					} else scode = 500;
				} else {
					if (_controller) {
						scode = _controller->handleControlPlaneHttpGET(std::vector<std::string>(ps.begin()+1,ps.end()),urlArgs,headers,body,responseBody,responseContentType);
					} else scode = 404;
				}

			} else scode = 401; // isAuth == false
		} else if ((httpMethod == HTTP_POST)||(httpMethod == HTTP_PUT)) {
			if (isAuth) {

				if (ps[0] == "network") {
					if (ps.size() == 2) {

						uint64_t wantnw = Utils::hexStrToU64(ps[1].c_str());
						_node->join(wantnw,(void *)0,(void *)0); // does nothing if we are a member
						ZT_VirtualNetworkList *nws = _node->networks();
						if (nws) {
							for(unsigned long i=0;i<nws->networkCount;++i) {
								if (nws->networks[i].nwid == wantnw) {
									OneService::NetworkSettings localSettings;
									getNetworkSettings(nws->networks[i].nwid,localSettings);

									try {
										json j(OSUtils::jsonParse(body));
										if (j.is_object()) {
											json &allowManaged = j["allowManaged"];
											if (allowManaged.is_boolean()) localSettings.allowManaged = (bool)allowManaged;
											json &allowGlobal = j["allowGlobal"];
											if (allowGlobal.is_boolean()) localSettings.allowGlobal = (bool)allowGlobal;
											json &allowDefault = j["allowDefault"];
											if (allowDefault.is_boolean()) localSettings.allowDefault = (bool)allowDefault;
										}
									} catch (std::exception &exc) {
									} catch ( ... ) {
									}

									setNetworkSettings(nws->networks[i].nwid,localSettings);
									_networkToJson(res,&(nws->networks[i]),portDeviceName(nws->networks[i].nwid),localSettings);

									scode = 200;
									break;
								}
							}
							_node->freeQueryResult((void *)nws);
						} else scode = 500;

					} else scode = 404;
				} else {
					if (_controller)
						scode = _controller->handleControlPlaneHttpPOST(std::vector<std::string>(ps.begin()+1,ps.end()),urlArgs,headers,body,responseBody,responseContentType);
					else scode = 404;
				}

			} else scode = 401; // isAuth == false
		} else if (httpMethod == HTTP_DELETE) {
			if (isAuth) {

				if (ps[0] == "network") {
					ZT_VirtualNetworkList *nws = _node->networks();
					if (nws) {
						if (ps.size() == 2) {
							uint64_t wantnw = Utils::hexStrToU64(ps[1].c_str());
							for(unsigned long i=0;i<nws->networkCount;++i) {
								if (nws->networks[i].nwid == wantnw) {
									_node->leave(wantnw,(void **)0,(void *)0);
									res["result"] = true;
									scode = 200;
									break;
								}
							}
						} // else 404
						_node->freeQueryResult((void *)nws);
					} else scode = 500;
				} else {
					if (_controller)
						scode = _controller->handleControlPlaneHttpDELETE(std::vector<std::string>(ps.begin()+1,ps.end()),urlArgs,headers,body,responseBody,responseContentType);
					else scode = 404;
				}

			} else scode = 401; // isAuth = false
		} else {
			scode = 400;
		}

		if (responseBody.length() == 0) {
			if ((res.is_object())||(res.is_array()))
				responseBody = OSUtils::jsonDump(res);
			else responseBody = "{}";
			responseContentType = "application/json";
		}

		// Wrap result in jsonp function call if the user included a jsonp= url argument.
		// Also double-check isAuth since forbidding this without auth feels safer.
		std::map<std::string,std::string>::const_iterator jsonp(urlArgs.find("jsonp"));
		if ((isAuth)&&(jsonp != urlArgs.end())&&(responseContentType == "application/json")) {
			if (responseBody.length() > 0)
				responseBody = jsonp->second + "(" + responseBody + ");";
			else responseBody = jsonp->second + "(null);";
			responseContentType = "application/javascript";
		}

		return scode;
	}

	// Must be called after _localConfig is read or modified
	void applyLocalConfig()
	{
		Mutex::Lock _l(_localConfig_m);
		json lc(_localConfig);

		_v4Hints.clear();
		_v6Hints.clear();
		_v4Blacklists.clear();
		_v6Blacklists.clear();
		json &virt = lc["virtual"];
		if (virt.is_object()) {
			for(json::iterator v(virt.begin());v!=virt.end();++v) {
				const std::string nstr = v.key();
				if ((nstr.length() == ZT_ADDRESS_LENGTH_HEX)&&(v.value().is_object())) {
					const Address ztaddr(Utils::hexStrToU64(nstr.c_str()));
					if (ztaddr) {
						const uint64_t ztaddr2 = ztaddr.toInt();
						std::vector<InetAddress> &v4h = _v4Hints[ztaddr2];
						std::vector<InetAddress> &v6h = _v6Hints[ztaddr2];
						std::vector<InetAddress> &v4b = _v4Blacklists[ztaddr2];
						std::vector<InetAddress> &v6b = _v6Blacklists[ztaddr2];

						json &tryAddrs = v.value()["try"];
						if (tryAddrs.is_array()) {
							for(unsigned long i=0;i<tryAddrs.size();++i) {
								const InetAddress ip(OSUtils::jsonString(tryAddrs[i],"").c_str());
								if (ip.ss_family == AF_INET)
									v4h.push_back(ip);
								else if (ip.ss_family == AF_INET6)
									v6h.push_back(ip);
							}
						}
						json &blAddrs = v.value()["blacklist"];
						if (blAddrs.is_array()) {
							for(unsigned long i=0;i<blAddrs.size();++i) {
								const InetAddress ip(OSUtils::jsonString(blAddrs[i],"").c_str());
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
		json &physical = lc["physical"];
		if (physical.is_object()) {
			for(json::iterator phy(physical.begin());phy!=physical.end();++phy) {
				const InetAddress net(OSUtils::jsonString(phy.key(),"").c_str());
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

		json &settings = lc["settings"];

		_primaryPort = (unsigned int)OSUtils::jsonInt(settings["primaryPort"],(uint64_t)_primaryPort) & 0xffff;
		_allowSecondaryPort = OSUtils::jsonBool(settings["allowSecondaryPort"],true);
		_secondaryPort = (unsigned int)OSUtils::jsonInt(settings["secondaryPort"],0);
		_tertiaryPort = (unsigned int)OSUtils::jsonInt(settings["tertiaryPort"],0);
		if (_secondaryPort != 0 || _tertiaryPort != 0) {
			fprintf(stderr,"WARNING: using manually-specified ports. This can cause NAT issues." ZT_EOL_S);
		}
		_multipathMode = (unsigned int)OSUtils::jsonInt(settings["multipathMode"],0);
		_portMappingEnabled = OSUtils::jsonBool(settings["portMappingEnabled"],true);

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
				const InetAddress nw(OSUtils::jsonString(amf[i],"").c_str());
				if (nw)
					_allowManagementFrom.push_back(nw);
			}
		}
	}

	// Checks if a managed IP or route target is allowed
	bool checkIfManagedIsAllowed(const NetworkState &n,const InetAddress &target)
	{
		if (!n.settings.allowManaged)
			return false;

		if (n.settings.allowManagedWhitelist.size() > 0) {
			bool allowed = false;
			for (InetAddress addr : n.settings.allowManagedWhitelist) {
				if (addr.containsAddress(target) && addr.netmaskBits() <= target.netmaskBits()) {
					allowed = true;
					break;
				}
			}
			if (!allowed) return false;
		}

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
		char ipbuf[64];

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
						fprintf(stderr,"ERROR: unable to remove ip address %s" ZT_EOL_S, ip->toString(ipbuf));
				}
			}
#ifdef __SYNOLOGY__
			if (!n.tap->addIps(newManagedIps)) {
				fprintf(stderr,"ERROR: unable to add ip addresses to ifcfg" ZT_EOL_S);
			}
#else
			for(std::vector<InetAddress>::iterator ip(newManagedIps.begin());ip!=newManagedIps.end();++ip) {
				if (std::find(n.managedIps.begin(),n.managedIps.end(),*ip) == n.managedIps.end()) {
					if (!n.tap->addIp(*ip))
						fprintf(stderr,"ERROR: unable to add ip address %s" ZT_EOL_S, ip->toString(ipbuf));
				}
			}
#endif
			n.managedIps.swap(newManagedIps);
		}

		if (syncRoutes) {
			char tapdev[64];
#if defined(__WINDOWS__) && !defined(ZT_SDK)
			OSUtils::ztsnprintf(tapdev,sizeof(tapdev),"%.16llx",(unsigned long long)((WindowsEthernetTap *)(n.tap.get()))->luid().Value);
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
						if ( ((*mr)->target() == *target) && ( ((via->ss_family == target->ss_family)&&((*mr)->via().ipsEqual(*via))) || (strcmp(tapdev,(*mr)->device())==0) ) ) {
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
#ifndef __APPLE__
				for(std::vector<InetAddress>::iterator ip(n.managedIps.begin());ip!=n.managedIps.end();++ip) {
					if ((target->netmaskBits() == ip->netmaskBits())&&(target->containsAddress(*ip))) {
						haveRoute = true;
						break;
					}
				}
#endif
				if (haveRoute)
					continue;
#ifndef ZT_SDK
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
#endif
			}
		}
	}

	// =========================================================================
	// Handlers for Node and Phy<> callbacks
	// =========================================================================

	inline void phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *localAddr,const struct sockaddr *from,void *data,unsigned long len)
	{
		const uint64_t now = OSUtils::now();
		if ((len >= 16)&&(reinterpret_cast<const InetAddress *>(from)->ipScope() == InetAddress::IP_SCOPE_GLOBAL))
			_lastDirectReceiveFromGlobal = now;
		const ZT_ResultCode rc = _node->processWirePacket(nullptr,now,reinterpret_cast<int64_t>(sock),reinterpret_cast<const struct sockaddr_storage *>(from),data,len,&_nextBackgroundTaskDeadline);
		if (ZT_ResultCode_isFatal(rc)) {
			char tmp[256];
			OSUtils::ztsnprintf(tmp,sizeof(tmp),"fatal error code from processWirePacket: %d",(int)rc);
			Mutex::Lock _l(_termReason_m);
			_termReason = ONE_UNRECOVERABLE_ERROR;
			_fatalErrorMessage = tmp;
			this->terminate();
		}
	}

	inline void phyOnTcpConnect(PhySocket *sock,void **uptr,bool success)
	{
		_phy.close(sock,true);
	}

	inline void phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from)
	{
		if (!from) {
			_phy.close(sockN,false);
			return;
		} else {
#ifdef ZT_SDK
			// Immediately close new local connections. The intention is to prevent the backplane from being accessed when operating as libzt
			if (!allowHttpBackplaneManagement && ((InetAddress*)from)->ipScope() == InetAddress::IP_SCOPE_LOOPBACK) {
				_phy.close(sockN,false);
				return;
			}
#endif
			TcpConnection *tc = new TcpConnection();
			{
				Mutex::Lock _l(_tcpConnections_m);
				_tcpConnections.push_back(tc);
			}

			tc->type = TcpConnection::TCP_UNCATEGORIZED_INCOMING;
			tc->parent = this;
			tc->sock = sockN;
			tc->remoteAddr = from;
			tc->lastReceive = OSUtils::now();
			http_parser_init(&(tc->parser),HTTP_REQUEST);
			tc->parser.data = (void *)tc;
			tc->messageSize = 0;

			*uptrN = (void *)tc;
		}
	}

	void phyOnTcpClose(PhySocket *sock,void **uptr)
	{
		TcpConnection *tc = (TcpConnection *)*uptr;
		if (tc) {
			{
				Mutex::Lock _l(_tcpConnections_m);
				_tcpConnections.erase(std::remove(_tcpConnections.begin(),_tcpConnections.end(),tc),_tcpConnections.end());
			}
			delete tc;
		}
	}

	void phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len)
	{
		try {
			if (!len) return; // sanity check, should never happen
			TcpConnection *tc = reinterpret_cast<TcpConnection *>(*uptr);
			tc->lastReceive = OSUtils::now();
			switch(tc->type) {

				case TcpConnection::TCP_UNCATEGORIZED_INCOMING:
					switch(reinterpret_cast<uint8_t *>(data)[0]) {
						// HTTP: GET, PUT, POST, HEAD, DELETE
						case 'G':
						case 'P':
						case 'D':
						case 'H': {
							// This is only allowed from IPs permitted to access the management
							// backplane, which is just 127.0.0.1/::1 unless otherwise configured.
							bool allow;
							{
								Mutex::Lock _l(_localConfig_m);
								if (_allowManagementFrom.size() == 0) {
									allow = (tc->remoteAddr.ipScope() == InetAddress::IP_SCOPE_LOOPBACK);
								} else {
									allow = false;
									for(std::vector<InetAddress>::const_iterator i(_allowManagementFrom.begin());i!=_allowManagementFrom.end();++i) {
										if (i->containsAddress(tc->remoteAddr)) {
											allow = true;
											break;
										}
									}
								}
							}
							if (allow) {
								tc->type = TcpConnection::TCP_HTTP_INCOMING;
								phyOnTcpData(sock,uptr,data,len);
							} else {
								_phy.close(sock);
							}
						}	break;

						// Drop unknown protocols
						default:
							_phy.close(sock);
							break;
					}
					return;

				case TcpConnection::TCP_HTTP_INCOMING:
					http_parser_execute(&(tc->parser),&HTTP_PARSER_SETTINGS,(const char *)data,len);
					if ((tc->parser.upgrade)||(tc->parser.http_errno != HPE_OK))
						_phy.close(sock);
					return;

			}
		} catch (std::exception &exc) {
			_phy.close(sock);
		} catch ( ... ) {
			_phy.close(sock);
		}
	}

	inline void phyOnTcpWritable(PhySocket *sock,void **uptr)
	{
		TcpConnection *tc = reinterpret_cast<TcpConnection *>(*uptr);
		bool closeit = false;
		{
			Mutex::Lock _l(tc->writeq_m);
			if (tc->writeq.length() > 0) {
				long sent = (long)_phy.streamSend(sock,tc->writeq.data(),(unsigned long)tc->writeq.length(),true);
				if (sent > 0) {
					if ((unsigned long)sent >= (unsigned long)tc->writeq.length()) {
						tc->writeq.clear();
						_phy.setNotifyWritable(sock,false);

						if (tc->type == TcpConnection::TCP_HTTP_INCOMING)
							closeit = true; // HTTP keep alive not supported
					} else {
						tc->writeq.erase(tc->writeq.begin(),tc->writeq.begin() + sent);
					}
				}
			} else {
				_phy.setNotifyWritable(sock,false);
			}
		}
		if (closeit)
			_phy.close(sock);
	}

	inline void phyOnFileDescriptorActivity(PhySocket *sock,void **uptr,bool readable,bool writable) {}
	inline void phyOnUnixAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN) {}
	inline void phyOnUnixClose(PhySocket *sock,void **uptr) {}
	inline void phyOnUnixData(PhySocket *sock,void **uptr,void *data,unsigned long len) {}
	inline void phyOnUnixWritable(PhySocket *sock,void **uptr) {}

	inline int nodeVirtualNetworkConfigFunction(uint64_t nwid,void **nuptr,enum ZT_VirtualNetworkConfigOperation op,const ZT_VirtualNetworkConfig *nwc)
	{
		Mutex::Lock _l(_nets_m);
		NetworkState &n = _nets[nwid];

		switch(op) {

			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP:
				if (!n.tap) {
					try {
						char friendlyName[128];
						OSUtils::ztsnprintf(friendlyName,sizeof(friendlyName),"ZeroTier One [%.16llx]",nwid);

						n.tap = EthernetTap::newInstance(
							nullptr,
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
						OSUtils::ztsnprintf(nlcpath,sizeof(nlcpath),"%s" ZT_PATH_SEPARATOR_S "networks.d" ZT_PATH_SEPARATOR_S "%.16llx.local.conf",_homePath.c_str(),nwid);
						std::string nlcbuf;
						if (OSUtils::readFile(nlcpath,nlcbuf)) {
							Dictionary<4096> nc;
							nc.load(nlcbuf.c_str());
							Buffer<1024> allowManaged;
							if (nc.get("allowManaged", allowManaged) && allowManaged.size() != 0) {
								std::string addresses (allowManaged.begin(), allowManaged.size());
								if (allowManaged.size() <= 5) { // untidy parsing for backward compatibility
									if (allowManaged[0] == '1' || allowManaged[0] == 't' || allowManaged[0] == 'T') {
										n.settings.allowManaged = true;
									} else {
										n.settings.allowManaged = false;
									}
								} else {
									// this should be a list of IP addresses
									n.settings.allowManaged = true;
									size_t pos = 0;
									while (true) {
										size_t nextPos = addresses.find(',', pos);
										std::string address = addresses.substr(pos, (nextPos == std::string::npos ? addresses.size() : nextPos) - pos);
										n.settings.allowManagedWhitelist.push_back(InetAddress(address.c_str()));
										if (nextPos == std::string::npos) break;
										pos = nextPos + 1;
									}
								}
							} else {
								n.settings.allowManaged = true;
							}
							n.settings.allowGlobal = nc.getB("allowGlobal", false);
							n.settings.allowDefault = nc.getB("allowDefault", false);
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
					} catch (int exc) {
						return -999;
					} catch ( ... ) {
						return -999; // tap init failed
					}
				}
				// After setting up tap, fall through to CONFIG_UPDATE since we also want to do this...

			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE:
				memcpy(&(n.config),nwc,sizeof(ZT_VirtualNetworkConfig));
				if (n.tap) { // sanity check
#if defined(__WINDOWS__) && !defined(ZT_SDK)
					// wait for up to 5 seconds for the WindowsEthernetTap to actually be initialized
					//
					// without WindowsEthernetTap::isInitialized() returning true, the won't actually
					// be online yet and setting managed routes on it will fail.
					const int MAX_SLEEP_COUNT = 500;
					for (int i = 0; !((WindowsEthernetTap *)(n.tap.get()))->isInitialized() && i < MAX_SLEEP_COUNT; i++) {
						Sleep(10);
					}
#endif
					syncManagedStuff(n,true,true);
					n.tap->setMtu(nwc->mtu);
				} else {
					_nets.erase(nwid);
					return -999; // tap init failed
				}
				break;

			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN:
			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY:
				if (n.tap) { // sanity check
#if defined(__WINDOWS__) && !defined(ZT_SDK)
					std::string winInstanceId(((WindowsEthernetTap *)(n.tap.get()))->instanceId());
#endif
					*nuptr = (void *)0;
					n.tap.reset();
					_nets.erase(nwid);
#if defined(__WINDOWS__) && !defined(ZT_SDK)
					if ((op == ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY)&&(winInstanceId.length() > 0))
						WindowsEthernetTap::deletePersistentTapDevice(winInstanceId.c_str());
#endif
					if (op == ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY) {
						char nlcpath[256];
						OSUtils::ztsnprintf(nlcpath,sizeof(nlcpath),"%s" ZT_PATH_SEPARATOR_S "networks.d" ZT_PATH_SEPARATOR_S "%.16llx.local.conf",_homePath.c_str(),nwid);
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

			case ZT_EVENT_TRACE: {
				if (metaData) {
					::fprintf(stderr,"%s" ZT_EOL_S,(const char *)metaData);
					::fflush(stderr);
				}
			}	break;

			case ZT_EVENT_REMOTE_TRACE: {
				// TODO
			}

			default:
				break;

		}
	}

	inline void nodeStatePutFunction(enum ZT_StateObjectType type,const uint64_t id[2],const void *data,int len)
	{
		char p[1024];
		FILE *f;
		bool secure = false;
		char dirname[1024];
		dirname[0] = 0;

		switch(type) {
			case ZT_STATE_OBJECT_IDENTITY_PUBLIC:
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "identity.public",_homePath.c_str());
				break;
			case ZT_STATE_OBJECT_IDENTITY_SECRET:
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "identity.secret",_homePath.c_str());
				secure = true;
				break;
			case ZT_STATE_OBJECT_NETWORK_CONFIG:
				OSUtils::ztsnprintf(dirname,sizeof(dirname),"%s" ZT_PATH_SEPARATOR_S "networks.d",_homePath.c_str());
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "%.16llx.conf",dirname,(unsigned long long)id[0]);
				secure = true;
				break;
			case ZT_STATE_OBJECT_PEER:
				OSUtils::ztsnprintf(dirname,sizeof(dirname),"%s" ZT_PATH_SEPARATOR_S "peers.d",_homePath.c_str());
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "%.10llx.peer",dirname,(unsigned long long)id[0]);
				break;
			case ZT_STATE_OBJECT_ROOT_LIST:
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "roots",_homePath.c_str());
				break;
			default:
				return;
		}

		if ((len >= 0)&&(data)) {
			// Check to see if we've already written this first. This reduces
			// redundant writes and I/O overhead on most platforms and has
			// little effect on others.
			f = fopen(p,"rb");
			if (f) {
				char *const buf = (char *)malloc(len*4);
				if (buf) {
					long l = (long)fread(buf,1,(size_t)(len*4),f);
					fclose(f);
					if ((l == (long)len)&&(memcmp(data,buf,l) == 0)) {
						free(buf);
						return;
					}
					free(buf);
				}
			}

			f = fopen(p,"wb");
			if ((!f)&&(dirname[0])) { // create subdirectory if it does not exist
				OSUtils::mkdir(dirname);
				f = fopen(p,"wb");
			}
			if (f) {
				if (fwrite(data,len,1,f) != 1)
					fprintf(stderr,"WARNING: unable to write to file: %s (I/O error)" ZT_EOL_S,p);
				fclose(f);
				if (secure)
					OSUtils::lockDownFile(p,false);
			} else {
				fprintf(stderr,"WARNING: unable to write to file: %s (unable to open)" ZT_EOL_S,p);
			}
		} else {
			OSUtils::rm(p);
		}
	}

	inline int nodeStateGetFunction(enum ZT_StateObjectType type,const uint64_t id[2],void *data,unsigned int maxlen)
	{
		char p[4096];
		switch(type) {
			case ZT_STATE_OBJECT_IDENTITY_PUBLIC:
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "identity.public",_homePath.c_str());
				break;
			case ZT_STATE_OBJECT_IDENTITY_SECRET:
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "identity.secret",_homePath.c_str());
				break;
			case ZT_STATE_OBJECT_NETWORK_CONFIG:
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "networks.d" ZT_PATH_SEPARATOR_S "%.16llx.conf",_homePath.c_str(),(unsigned long long)id[0]);
				break;
			case ZT_STATE_OBJECT_PEER:
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "peers.d" ZT_PATH_SEPARATOR_S "%.10llx.peer",_homePath.c_str(),(unsigned long long)id[0]);
				break;
			default:
				return -1;
		}
		FILE *f = fopen(p,"rb");
		if (f) {
			int n = (int)fread(data,1,maxlen,f);
			fclose(f);
			if (n >= 0)
				return n;
		}
		return -1;
	}

	inline int nodeWirePacketSendFunction(const int64_t localSocket,const struct sockaddr_storage *addr,const void *data,unsigned int len,unsigned int ttl)
	{
		if ((localSocket != -1)&&(localSocket != 0)&&(_binder.isUdpSocketValid((PhySocket *)((uintptr_t)localSocket)))) {
			if ((ttl)&&(addr->ss_family == AF_INET)) _phy.setIp4UdpTtl((PhySocket *)((uintptr_t)localSocket),ttl);
			const bool r = _phy.udpSend((PhySocket *)((uintptr_t)localSocket),(const struct sockaddr *)addr,data,len);
			if ((ttl)&&(addr->ss_family == AF_INET)) _phy.setIp4UdpTtl((PhySocket *)((uintptr_t)localSocket),255);
			return ((r) ? 0 : -1);
		} else {
			return ((_binder.udpSendAll(_phy,addr,data,len,ttl)) ? 0 : -1);
		}
	}

	inline void nodeVirtualNetworkFrameFunction(uint64_t nwid,void **nuptr,uint64_t sourceMac,uint64_t destMac,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
	{
		NetworkState *n = reinterpret_cast<NetworkState *>(*nuptr);
		if ((!n)||(!n->tap))
			return;
		n->tap->put(MAC(sourceMac),MAC(destMac),etherType,data,len);
	}

	inline int nodePathCheckFunction(uint64_t ztaddr,const int64_t localSocket,const struct sockaddr_storage *remoteAddr)
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
		}
		if (gbl) {
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
			lh = (Utils::random() & 1) ? &_v4Hints : &_v6Hints;
		else if (family == AF_INET)
			lh = &_v4Hints;
		else if (family == AF_INET6)
			lh = &_v6Hints;
		else return 0;
		const std::vector<InetAddress> *l = lh->get(ztaddr);
		if ((l)&&(l->size() > 0)) {
			memcpy(result,&((*l)[(unsigned long)Utils::random() % l->size()]),sizeof(struct sockaddr_storage));
			return 1;
		} else return 0;
	}

	inline void tapFrameHandler(uint64_t nwid,const MAC &from,const MAC &to,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
	{
		_node->processVirtualNetworkFrame((void *)0,OSUtils::now(),nwid,from.toInt(),to.toInt(),etherType,vlanId,data,len,&_nextBackgroundTaskDeadline);
	}

	inline void onHttpRequestToServer(TcpConnection *tc)
	{
		char tmpn[4096];
		std::string data;
		std::string contentType("text/plain"); // default if not changed in handleRequest()
		unsigned int scode = 404;

		// Note that we check allowed IP ranges when HTTP connections are first detected in
		// phyOnTcpData(). If we made it here the source IP is okay.

		try {
			scode = handleControlPlaneHttpRequest(tc->remoteAddr,tc->parser.method,tc->url,tc->headers,tc->readq,data,contentType);
		} catch (std::exception &exc) {
			fprintf(stderr,"WARNING: unexpected exception processing control HTTP request: %s" ZT_EOL_S,exc.what());
			scode = 500;
		} catch ( ... ) {
			fprintf(stderr,"WARNING: unexpected exception processing control HTTP request: unknown exception" ZT_EOL_S);
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

		OSUtils::ztsnprintf(tmpn,sizeof(tmpn),"HTTP/1.1 %.3u %s\r\nCache-Control: no-cache\r\nPragma: no-cache\r\nContent-Type: %s\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
			scode,
			scodestr,
			contentType.c_str(),
			(unsigned long)data.length());
		{
			Mutex::Lock _l(tc->writeq_m);
			tc->writeq = tmpn;
			if (tc->parser.method != HTTP_HEAD)
				tc->writeq.append(data);
		}

		_phy.setNotifyWritable(tc->sock,true);
	}

	inline void onHttpResponseFromClient(TcpConnection *tc)
	{
		_phy.close(tc->sock);
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
		if ((ifname[0] == 'f')&&(ifname[1] == 'e')&&(ifname[2] == 't')&&(ifname[3] == 'h')) return false; // ... as is feth#
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
			// Check global blacklists
			const std::vector<InetAddress> *gbl = (const std::vector<InetAddress> *)0;
			if (ifaddr.ss_family == AF_INET) {
				gbl = &_globalV4Blacklist;
			} else if (ifaddr.ss_family == AF_INET6) {
				gbl = &_globalV6Blacklist;
			}
			if (gbl) {
				Mutex::Lock _l(_localConfig_m);
				for(std::vector<InetAddress>::const_iterator a(gbl->begin());a!=gbl->end();++a) {
					if (a->containsAddress(ifaddr))
						return false;
				}
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

static int SnodeVirtualNetworkConfigFunction(ZT_Node *node,void *uptr,void *tptr,uint64_t nwid,void **nuptr,enum ZT_VirtualNetworkConfigOperation op,const ZT_VirtualNetworkConfig *nwconf)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodeVirtualNetworkConfigFunction(nwid,nuptr,op,nwconf); }
static void SnodeEventCallback(ZT_Node *node,void *uptr,void *tptr,enum ZT_Event event,const void *metaData)
{ reinterpret_cast<OneServiceImpl *>(uptr)->nodeEventCallback(event,metaData); }
static void SnodeStatePutFunction(ZT_Node *node,void *uptr,void *tptr,enum ZT_StateObjectType type,const uint64_t id[2],const void *data,int len)
{ reinterpret_cast<OneServiceImpl *>(uptr)->nodeStatePutFunction(type,id,data,len); }
static int SnodeStateGetFunction(ZT_Node *node,void *uptr,void *tptr,enum ZT_StateObjectType type,const uint64_t id[2],void *data,unsigned int maxlen)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodeStateGetFunction(type,id,data,maxlen); }
static int SnodeWirePacketSendFunction(ZT_Node *node,void *uptr,void *tptr,int64_t localSocket,const struct sockaddr_storage *addr,const void *data,unsigned int len,unsigned int ttl)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodeWirePacketSendFunction(localSocket,addr,data,len,ttl); }
static void SnodeVirtualNetworkFrameFunction(ZT_Node *node,void *uptr,void *tptr,uint64_t nwid,void **nuptr,uint64_t sourceMac,uint64_t destMac,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
{ reinterpret_cast<OneServiceImpl *>(uptr)->nodeVirtualNetworkFrameFunction(nwid,nuptr,sourceMac,destMac,etherType,vlanId,data,len); }
static int SnodePathCheckFunction(ZT_Node *node,void *uptr,void *tptr,uint64_t ztaddr,int64_t localSocket,const struct sockaddr_storage *remoteAddr)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodePathCheckFunction(ztaddr,localSocket,remoteAddr); }
static int SnodePathLookupFunction(ZT_Node *node,void *uptr,void *tptr,uint64_t ztaddr,int family,struct sockaddr_storage *result)
{ return reinterpret_cast<OneServiceImpl *>(uptr)->nodePathLookupFunction(ztaddr,family,result); }
static void StapFrameHandler(void *uptr,void *tptr,uint64_t nwid,const MAC &from,const MAC &to,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
{ reinterpret_cast<OneServiceImpl *>(uptr)->tapFrameHandler(nwid,from,to,etherType,vlanId,data,len); }

static int ShttpOnMessageBegin(http_parser *parser)
{
	TcpConnection *tc = reinterpret_cast<TcpConnection *>(parser->data);
	tc->currentHeaderField = "";
	tc->currentHeaderValue = "";
	tc->messageSize = 0;
	tc->url.clear();
	tc->status.clear();
	tc->headers.clear();
	tc->readq.clear();
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
{ return 0; }
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
	tc->readq.append(ptr,length);
	return 0;
}
static int ShttpOnMessageComplete(http_parser *parser)
{
	TcpConnection *tc = reinterpret_cast<TcpConnection *>(parser->data);
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
