/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
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

#include "../version.h"
#include "../include/ZeroTierOne.h"

#include "../node/Constants.hpp"
#include "../node/Mutex.hpp"
#include "../node/Node.hpp"
#include "../node/Utils.hpp"
#include "../node/InetAddress.hpp"
#include "../node/MAC.hpp"
#include "../node/Identity.hpp"
#include "../node/World.hpp"
#include "../node/Salsa20.hpp"
#include "../node/Poly1305.hpp"
#include "../node/SHA512.hpp"
#include "../node/Bond.hpp"
#include "../node/Peer.hpp"

#include "../osdep/Phy.hpp"
#include "../osdep/OSUtils.hpp"
#include "../osdep/Http.hpp"
#include "../osdep/PortMapper.hpp"
#include "../osdep/Binder.hpp"
#include "../osdep/ManagedRoute.hpp"
#include "../osdep/BlockingQueue.hpp"

#include "OneService.hpp"
#include "SoftwareUpdater.hpp"

#if ZT_SSO_ENABLED
#include <zeroidc.h>
#endif

#ifdef __WINDOWS__
#include <winsock2.h>
#include <windows.h>
#include <shlobj.h>
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

#ifdef __APPLE__
#include "../osdep/MacDNSHelper.hpp"
#elif defined(__WINDOWS__)
#include "../osdep/WinDNSHelper.hpp"
#endif

#ifdef ZT_USE_SYSTEM_HTTP_PARSER
#include <http_parser.h>
#else
#include "../ext/http-parser/http_parser.h"
#endif

#if ZT_VAULT_SUPPORT
extern "C" {
#include <curl/curl.h>
}
#endif

#include "../ext/json/json.hpp"

using json = nlohmann::json;

#include "../controller/EmbeddedNetworkController.hpp"
#include "../controller/PostgreSQL.hpp"
#include "../controller/Redis.hpp"
#include "../osdep/EthernetTap.hpp"
#ifdef __WINDOWS__
#include "../osdep/WindowsEthernetTap.hpp"
#endif

#ifndef ZT_SOFTWARE_UPDATE_DEFAULT
#define ZT_SOFTWARE_UPDATE_DEFAULT "disable"
#endif

// Sanity limits for HTTP
#define ZT_MAX_HTTP_MESSAGE_SIZE (1024 * 1024 * 64)
#define ZT_MAX_HTTP_CONNECTIONS 65536

// Interface metric for ZeroTier taps -- this ensures that if we are on WiFi and also
// bridged via ZeroTier to the same LAN traffic will (if the OS is sane) prefer WiFi.
#define ZT_IF_METRIC 5000

// How often to check for new multicast subscriptions on a tap device
#define ZT_TAP_CHECK_MULTICAST_INTERVAL 5000

// TCP fallback relay (run by ZeroTier, Inc. -- this will eventually go away)
#ifndef ZT_SDK
#define ZT_TCP_FALLBACK_RELAY "204.80.128.1/443"
#endif

// Frequency at which we re-resolve the TCP fallback relay
#define ZT_TCP_FALLBACK_RERESOLVE_DELAY 86400000

// Attempt to engage TCP fallback after this many ms of no reply to packets sent to global-scope IPs
#define ZT_TCP_FALLBACK_AFTER 60000

// How often to check for local interface addresses
#define ZT_LOCAL_INTERFACE_CHECK_INTERVAL 60000

// Maximum write buffer size for outgoing TCP connections (sanity limit)
#define ZT_TCP_MAX_WRITEQ_SIZE 33554432

// TCP activity timeout
#define ZT_TCP_ACTIVITY_TIMEOUT 60000

#if ZT_VAULT_SUPPORT
size_t curlResponseWrite(void *ptr, size_t size, size_t nmemb, std::string *data)
{
	data->append((char*)ptr, size * nmemb);
	return size * nmemb;
}
#endif


namespace ZeroTier {

const char *ssoResponseTemplate = "<html>\
<head>\
<style type=\"text/css\">\
html,body {\
	background: #eeeeee;\
	margin: 0;\
	padding: 0;\
	font-family: \"Helvetica\";\
	font-weight: bold;\
	font-size: 12pt;\
	height: 100%;\
	width: 100%;\
}\
div.icon {\
	background: #ffb354;\
	color: #000000;\
	font-size: 120pt;\
	border-radius: 2.5rem;\
	display: inline-block;\
	width: 1.3em;\
	height: 1.3em;\
	padding: 0;\
	margin: 15;\
	line-height: 1.4em;\
	vertical-align: middle;\
	text-align: center;\
}\
</style>\
</head>\
<body>\
<br><br><br><br><br><br>\
<center>\
<div class=\"icon\">&#x23c1;</div>\
<div class=\"text\">%s</div>\
</center>\
</body>\
</html>";

// Configured networks
class NetworkState
{
public:
	NetworkState() 
		: _webPort(9993)
		, _tap((EthernetTap *)0)
#if ZT_SSO_ENABLED
		, _idc(nullptr)
#endif
	{
		// Real defaults are in network 'up' code in network event handler
		_settings.allowManaged = true;
		_settings.allowGlobal = false;
		_settings.allowDefault = false;
		_settings.allowDNS = false;
		memset(&_config, 0, sizeof(ZT_VirtualNetworkConfig));
	}

	~NetworkState()
	{
		this->_managedRoutes.clear();
		this->_tap.reset();

#if ZT_SSO_ENABLED
		if (_idc) {
			zeroidc::zeroidc_stop(_idc);
			zeroidc::zeroidc_delete(_idc);
			_idc = nullptr;
		}
#endif
	}

	void setWebPort(unsigned int port) {
		_webPort = port;
	}

	void setTap(std::shared_ptr<EthernetTap> tap) {
		this->_tap = tap;
	}

	std::shared_ptr<EthernetTap> tap() const {
		return _tap;
	}

	OneService::NetworkSettings settings() const {
		return _settings;
	}

	void setSettings(const OneService::NetworkSettings &settings) {
		_settings = settings;
	}

	void setAllowManaged(bool allow) {
		_settings.allowManaged = allow;
	}

	bool allowManaged() const {
		return _settings.allowManaged;
	}

	void setAllowGlobal(bool allow) {
		_settings.allowGlobal = allow;
	}

	bool allowGlobal() const {
		return _settings.allowGlobal;
	}

	void setAllowDefault(bool allow) {
		_settings.allowDefault = allow;
	}

	bool allowDefault() const {
		return _settings.allowDefault;
	}

	void setAllowDNS(bool allow) {
		_settings.allowDNS = allow;
	}

	bool allowDNS() const {
		return _settings.allowDNS;
	}
	
	std::vector<InetAddress> allowManagedWhitelist() const {
		return _settings.allowManagedWhitelist;
	}

	void addToAllowManagedWhiteList(const InetAddress& addr) {
		_settings.allowManagedWhitelist.push_back(addr);
	}

	const ZT_VirtualNetworkConfig& config() {
		return _config;
	}

	void setConfig(const ZT_VirtualNetworkConfig *nwc) {
		char nwbuf[17] = {};
		const char* nwid = Utils::hex(nwc->nwid, nwbuf);
		// fprintf(stderr, "NetworkState::setConfig(%s)\n", nwid);

		memcpy(&_config, nwc, sizeof(ZT_VirtualNetworkConfig));
		// fprintf(stderr, "ssoEnabled: %s, ssoVersion: %d\n", 
		// 	_config.ssoEnabled ? "true" : "false", _config.ssoVersion);

		if (_config.ssoEnabled && _config.ssoVersion == 1) {
			//  fprintf(stderr, "ssoEnabled for %s\n", nwid);
#if ZT_SSO_ENABLED
			if (_idc == nullptr)
			{
				assert(_config.issuerURL != nullptr);
				assert(_config.ssoClientID != nullptr);
				assert(_config.centralAuthURL != nullptr);

				// fprintf(stderr, "Issuer URL: %s\n", _config.issuerURL);
				// fprintf(stderr, "Client ID: %s\n", _config.ssoClientID);
				// fprintf(stderr, "Central Auth URL: %s\n", _config.centralAuthURL);
				
				_idc = zeroidc::zeroidc_new(
					_config.issuerURL,
					_config.ssoClientID,
					_config.centralAuthURL,
					_webPort
				);

				if (_idc == nullptr) {
					fprintf(stderr, "idc is null\n");
					return;
				}

				// fprintf(stderr, "idc created (%s, %s, %s)\n", _config.issuerURL, _config.ssoClientID, _config.centralAuthURL);
			}

			zeroidc::zeroidc_set_nonce_and_csrf(
				_idc,
				_config.ssoState,
				_config.ssoNonce
			);

			char* url = zeroidc::zeroidc_get_auth_url(_idc);
			memcpy(_config.authenticationURL, url, strlen(url));
			_config.authenticationURL[strlen(url)] = 0;
			zeroidc::free_cstr(url);

			if (zeroidc::zeroidc_is_running(_idc) && nwc->status == ZT_NETWORK_STATUS_AUTHENTICATION_REQUIRED) {
				// TODO: kick the refresh thread
				zeroidc::zeroidc_kick_refresh_thread(_idc);
			}
#endif
		}
	}

	std::vector<InetAddress>& managedIps()  {
		return _managedIps;
	}

	void setManagedIps(const std::vector<InetAddress> &managedIps) {
		_managedIps = managedIps;
	}

	std::map< InetAddress, SharedPtr<ManagedRoute> >& managedRoutes() {
		return _managedRoutes;
	}

	const char* getAuthURL() {
#if ZT_SSO_ENABLED
		if (_idc != nullptr) {
			return zeroidc::zeroidc_get_auth_url(_idc);
		}
		fprintf(stderr, "_idc is null\n");
#endif
		return "";
	}

	char* doTokenExchange(const char *code) {
#if ZT_SSO_ENABLED
		if (_idc == nullptr) {
			fprintf(stderr, "ainfo or idc null\n");
			return "";
		}

		char *ret = zeroidc::zeroidc_token_exchange(_idc, code);
		zeroidc::zeroidc_set_nonce_and_csrf(
			_idc,
			_config.ssoState,
			_config.ssoNonce
		);

		char* url = zeroidc::zeroidc_get_auth_url(_idc);
		memcpy(_config.authenticationURL, url, strlen(url));
		_config.authenticationURL[strlen(url)] = 0;
		zeroidc::free_cstr(url);
		
		return ret;
#else
		return "";
#endif
	}

	uint64_t getExpiryTime() {
#if ZT_SSO_ENABLED
		if (_idc == nullptr) {
			fprintf(stderr, "idc is null\n");
			return 0;
		}
		return zeroidc::zeroidc_get_exp_time(_idc);
#else
		return 0;
#endif
	}

private:
	unsigned int _webPort;
	std::shared_ptr<EthernetTap> _tap;
	ZT_VirtualNetworkConfig _config; // memcpy() of raw config from core
	std::vector<InetAddress> _managedIps;
	std::map< InetAddress, SharedPtr<ManagedRoute> > _managedRoutes;
	OneService::NetworkSettings _settings;
#if ZT_SSO_ENABLED
	zeroidc::ZeroIDC *_idc;
#endif
};

namespace {

static const InetAddress NULL_INET_ADDR;

// Fake TLS hello for TCP tunnel outgoing connections (TUNNELED mode)
static const char ZT_TCP_TUNNEL_HELLO[9] = { 0x17,0x03,0x03,0x00,0x04,(char)ZEROTIER_ONE_VERSION_MAJOR,(char)ZEROTIER_ONE_VERSION_MINOR,(char)((ZEROTIER_ONE_VERSION_REVISION >> 8) & 0xff),(char)(ZEROTIER_ONE_VERSION_REVISION & 0xff) };

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

static void _networkToJson(nlohmann::json &nj,NetworkState &ns)
{
	char tmp[256];

	const char *nstatus = "",*ntype = "";
	switch(ns.config().status) {
		case ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION: nstatus = "REQUESTING_CONFIGURATION"; break;
		case ZT_NETWORK_STATUS_OK:                       nstatus = "OK"; break;
		case ZT_NETWORK_STATUS_ACCESS_DENIED:            nstatus = "ACCESS_DENIED"; break;
		case ZT_NETWORK_STATUS_NOT_FOUND:                nstatus = "NOT_FOUND"; break;
		case ZT_NETWORK_STATUS_PORT_ERROR:               nstatus = "PORT_ERROR"; break;
		case ZT_NETWORK_STATUS_CLIENT_TOO_OLD:           nstatus = "CLIENT_TOO_OLD"; break;
		case ZT_NETWORK_STATUS_AUTHENTICATION_REQUIRED:  nstatus = "AUTHENTICATION_REQUIRED"; break;
	}
	switch(ns.config().type) {
		case ZT_NETWORK_TYPE_PRIVATE:                    ntype = "PRIVATE"; break;
		case ZT_NETWORK_TYPE_PUBLIC:                     ntype = "PUBLIC"; break;
	}

	OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.16llx",ns.config().nwid);
	nj["id"] = tmp;
	nj["nwid"] = tmp;
	OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",(unsigned int)((ns.config().mac >> 40) & 0xff),(unsigned int)((ns.config().mac >> 32) & 0xff),(unsigned int)((ns.config().mac >> 24) & 0xff),(unsigned int)((ns.config().mac >> 16) & 0xff),(unsigned int)((ns.config().mac >> 8) & 0xff),(unsigned int)(ns.config().mac & 0xff));
	nj["mac"] = tmp;
	nj["name"] = ns.config().name;
	nj["status"] = nstatus;
	nj["type"] = ntype;
	nj["mtu"] = ns.config().mtu;
	nj["dhcp"] = (bool)(ns.config().dhcp != 0);
	nj["bridge"] = (bool)(ns.config().bridge != 0);
	nj["broadcastEnabled"] = (bool)(ns.config().broadcastEnabled != 0);
	nj["portError"] = ns.config().portError;
	nj["netconfRevision"] = ns.config().netconfRevision;
	nj["portDeviceName"] = ns.tap()->deviceName();

	OneService::NetworkSettings localSettings = ns.settings();

	nj["allowManaged"] = localSettings.allowManaged;
	nj["allowGlobal"] = localSettings.allowGlobal;
	nj["allowDefault"] = localSettings.allowDefault;
	nj["allowDNS"] = localSettings.allowDNS;

	nlohmann::json aa = nlohmann::json::array();
	for(unsigned int i=0;i<ns.config().assignedAddressCount;++i) {
		aa.push_back(reinterpret_cast<const InetAddress *>(&(ns.config().assignedAddresses[i]))->toString(tmp));
	}
	nj["assignedAddresses"] = aa;

	nlohmann::json ra = nlohmann::json::array();
	for(unsigned int i=0;i<ns.config().routeCount;++i) {
		nlohmann::json rj;
		rj["target"] = reinterpret_cast<const InetAddress *>(&(ns.config().routes[i].target))->toString(tmp);
		if (ns.config().routes[i].via.ss_family == ns.config().routes[i].target.ss_family)
			rj["via"] = reinterpret_cast<const InetAddress *>(&(ns.config().routes[i].via))->toIpString(tmp);
		else rj["via"] = nlohmann::json();
		rj["flags"] = (int)ns.config().routes[i].flags;
		rj["metric"] = (int)ns.config().routes[i].metric;
		ra.push_back(rj);
	}
	nj["routes"] = ra;

	nlohmann::json mca = nlohmann::json::array();
	for(unsigned int i=0;i<ns.config().multicastSubscriptionCount;++i) {
		nlohmann::json m;
		m["mac"] = MAC(ns.config().multicastSubscriptions[i].mac).toString(tmp);
		m["adi"] = ns.config().multicastSubscriptions[i].adi;
		mca.push_back(m);
	}
	nj["multicastSubscriptions"] = mca;

	nlohmann::json m;
	m["domain"] = ns.config().dns.domain;
	m["servers"] = nlohmann::json::array();
	for(int j=0;j<ZT_MAX_DNS_SERVERS;++j) {

		InetAddress a(ns.config().dns.server_addr[j]);
		if (a.isV4() || a.isV6()) {
			char buf[256];
			m["servers"].push_back(a.toIpString(buf));
		}
	}
	nj["dns"] = m;
	if (ns.config().ssoEnabled) {
		const char* authURL = ns.getAuthURL();
		//fprintf(stderr, "Auth URL: %s\n", authURL);
		nj["authenticationURL"] = authURL;
		nj["authenticationExpiryTime"] = (ns.getExpiryTime()*1000);
		nj["ssoEnabled"] = ns.config().ssoEnabled;
	}
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
	pj["isBonded"] = peer->isBonded;
	if (peer->isBonded) {
		pj["bondingPolicy"] = peer->bondingPolicy;
		pj["numAliveLinks"] = peer->numAliveLinks;
		pj["numTotalLinks"] = peer->numTotalLinks;
	}

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

static void _bondToJson(nlohmann::json &pj, SharedPtr<Bond> &bond)
{
	uint64_t now = OSUtils::now();

	int bondingPolicy = bond->policy();
	pj["bondingPolicy"] = Bond::getPolicyStrByCode(bondingPolicy);
	if (bondingPolicy == ZT_BOND_POLICY_NONE) {
		return;
	}

	pj["numAliveLinks"] = bond->getNumAliveLinks();
	pj["numTotalLinks"] = bond->getNumTotalLinks();
	pj["failoverInterval"] = bond->getFailoverInterval();
	pj["downDelay"] = bond->getDownDelay();
	pj["upDelay"] = bond->getUpDelay();
	if (bondingPolicy == ZT_BOND_POLICY_BALANCE_RR) {
		pj["packetsPerLink"] = bond->getPacketsPerLink();
	}
	if (bondingPolicy == ZT_BOND_POLICY_ACTIVE_BACKUP) {
		pj["linkSelectMethod"] = bond->getLinkSelectMethod();
	}

	nlohmann::json pa = nlohmann::json::array();
	std::vector< SharedPtr<Path> > paths = bond->paths(now);

	for(unsigned int i=0;i<paths.size();++i) {
		char pathStr[128];
		paths[i]->address().toString(pathStr);

		nlohmann::json j;
		j["ifname"] = bond->getLink(paths[i])->ifname();
		j["path"] = pathStr;
		/*
		j["alive"] = paths[i]->alive(now,true);
		j["bonded"] = paths[i]->bonded();
		j["latencyMean"] = paths[i]->latencyMean();
		j["latencyVariance"] = paths[i]->latencyVariance();
		j["packetLossRatio"] = paths[i]->packetLossRatio();
		j["packetErrorRatio"] = paths[i]->packetErrorRatio();
		j["givenLinkSpeed"] = 1000;
		j["allocation"] = paths[i]->allocation();
		*/
		pa.push_back(j);
	}
	pj["links"] = pa;
}

static void _moonToJson(nlohmann::json &mj,const World &world)
{
	char tmp[4096];
	OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.16llx",world.id());
	mj["id"] = tmp;
	mj["timestamp"] = world.timestamp();
	mj["signature"] = Utils::hex(world.signature().data,ZT_C25519_SIGNATURE_LEN,tmp);
	mj["updatesMustBeSignedBy"] = Utils::hex(world.updatesMustBeSignedBy().data,ZT_C25519_PUBLIC_KEY_LEN,tmp);
	nlohmann::json ra = nlohmann::json::array();
	for(std::vector<World::Root>::const_iterator r(world.roots().begin());r!=world.roots().end();++r) {
		nlohmann::json rj;
		rj["identity"] = r->identity.toString(false,tmp);
		nlohmann::json eps = nlohmann::json::array();
		for(std::vector<InetAddress>::const_iterator a(r->stableEndpoints.begin());a!=r->stableEndpoints.end();++a)
			eps.push_back(a->toString(tmp));
		rj["stableEndpoints"] = eps;
		ra.push_back(rj);
	}
	mj["roots"] = ra;
	mj["waiting"] = false;
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
		TCP_HTTP_OUTGOING,
		TCP_TUNNEL_OUTGOING // TUNNELED mode proxy outbound connection
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

struct OneServiceIncomingPacket
{
	uint64_t now;
	int64_t sock;
	struct sockaddr_storage from;
	unsigned int size;
	uint8_t data[ZT_MAX_MTU];
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
	SoftwareUpdater *_updater;
	PhySocket *_localControlSocket4;
	PhySocket *_localControlSocket6;
	bool _updateAutoApply;
	bool _allowTcpFallbackRelay;
	bool _allowSecondaryPort;

	unsigned int _primaryPort;
	unsigned int _secondaryPort;
	unsigned int _tertiaryPort;
	volatile unsigned int _udpPortPickerCounter;

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

	std::vector<InetAddress> explicitBind;

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
#ifdef ZT_TCP_FALLBACK_RELAY
	InetAddress _fallbackRelayAddress;
	uint64_t _lastSendToGlobalV4;
#endif

	// Last potential sleep/wake event
	uint64_t _lastRestart;

	// Deadline for the next background task service function
	volatile int64_t _nextBackgroundTaskDeadline;

	

	std::map<uint64_t,NetworkState> _nets;
	Mutex _nets_m;

	// Active TCP/IP connections
	std::vector< TcpConnection * > _tcpConnections;
	Mutex _tcpConnections_m;
	TcpConnection *_tcpFallbackTunnel;

	// Termination status information
	ReasonForTermination _termReason;
	std::string _fatalErrorMessage;
	Mutex _termReason_m;

	// uPnP/NAT-PMP port mapper if enabled
	bool _portMappingEnabled; // local.conf settings
#ifdef ZT_USE_MINIUPNPC
	PortMapper *_portMapper;
#endif

	// HashiCorp Vault Settings
#if ZT_VAULT_SUPPORT
	bool _vaultEnabled;
	std::string _vaultURL;
	std::string _vaultToken;
	std::string _vaultPath; // defaults to cubbyhole/zerotier/identity.secret for per-access key storage
#endif

	// Set to false to force service to stop
	volatile bool _run;
	Mutex _run_m;

	RedisConfig *_rc;
	std::string _ssoRedirectURL;

	// end member variables ----------------------------------------------------

	OneServiceImpl(const char *hp,unsigned int port) :
		_homePath((hp) ? hp : ".")
		,_controllerDbPath(_homePath + ZT_PATH_SEPARATOR_S "controller.d")
		,_networksPath(_homePath + ZT_PATH_SEPARATOR_S "networks.d")
		,_moonsPath(_homePath + ZT_PATH_SEPARATOR_S "moons.d")
		,_controller((EmbeddedNetworkController *)0)
		,_phy(this,false,true)
		,_node((Node *)0)
		,_updater((SoftwareUpdater *)0)
		,_localControlSocket4((PhySocket *)0)
		,_localControlSocket6((PhySocket *)0)
		,_updateAutoApply(false)
		,_primaryPort(port)
		,_udpPortPickerCounter(0)
		,_lastDirectReceiveFromGlobal(0)
#ifdef ZT_TCP_FALLBACK_RELAY
		, _fallbackRelayAddress(ZT_TCP_FALLBACK_RELAY)
		,_lastSendToGlobalV4(0)
#endif
		,_lastRestart(0)
		,_nextBackgroundTaskDeadline(0)
		,_tcpFallbackTunnel((TcpConnection *)0)
		,_termReason(ONE_STILL_RUNNING)
		,_portMappingEnabled(true)
#ifdef ZT_USE_MINIUPNPC
		,_portMapper((PortMapper *)0)
#endif
#ifdef ZT_VAULT_SUPPORT
		,_vaultEnabled(false)
		,_vaultURL()
		,_vaultToken()
		,_vaultPath("cubbyhole/zerotier")
#endif
		,_run(true)
		,_rc(NULL)
		,_ssoRedirectURL()
	{
		_ports[0] = 0;
		_ports[1] = 0;
		_ports[2] = 0;

#if ZT_VAULT_SUPPORT
		curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
	}

	virtual ~OneServiceImpl()
	{
		_binder.closeAll(_phy);
		_phy.close(_localControlSocket4);
		_phy.close(_localControlSocket6);

#if ZT_VAULT_SUPPORT
		curl_global_cleanup();
#endif

#ifdef ZT_USE_MINIUPNPC
		delete _portMapper;
#endif
		delete _controller;
		delete _rc;
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
				_authToken = _trimString(_authToken);
			}

			{
				struct ZT_Node_Callbacks cb;
				cb.version = 0;
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

			// Save original port number to show it if bind error
			const int _configuredPort = _primaryPort;

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
				_fatalErrorMessage = std::string("cannot bind to local control interface port ")+std::to_string(_configuredPort);
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

			// Attempt to bind to a secondary port.
			// This exists because there are buggy NATs out there that fail if more
			// than one device behind the same NAT tries to use the same internal
			// private address port number. Buggy NATs are a running theme.
			//
			// This used to pick the secondary port based on the node ID until we
			// discovered another problem: buggy routers and malicious traffic 
			// "detection".  A lot of routers have such things built in these days
			// and mis-detect ZeroTier traffic as malicious and block it resulting
			// in a node that appears to be in a coma.  Secondary ports are now 
			// randomized on startup.
			if (_allowSecondaryPort) {
				if (_secondaryPort) {
					_ports[1] = _secondaryPort;
				} else {
					_ports[1] = _getRandomPort();
				}
			}
#ifdef ZT_USE_MINIUPNPC
			if (_portMappingEnabled) {
				// If we're running uPnP/NAT-PMP, bind a *third* port for that. We can't
				// use the other two ports for that because some NATs do really funky
				// stuff with ports that are explicitly mapped that breaks things.
				if (_tertiaryPort) {
					_ports[2] = _tertiaryPort;
				} else {
					_ports[2] = _getRandomPort();
				}

				if (_ports[2]) {
					char uniqueName[64];
					OSUtils::ztsnprintf(uniqueName,sizeof(uniqueName),"ZeroTier/%.10llx@%u",_node->address(),_ports[2]);
					_portMapper = new PortMapper(_ports[2],uniqueName);
				}
			}
#endif

			// Delete legacy iddb.d if present (cleanup)
			OSUtils::rmDashRf((_homePath + ZT_PATH_SEPARATOR_S "iddb.d").c_str());

			// Network controller is now enabled by default for desktop and server
			_controller = new EmbeddedNetworkController(_node,_homePath.c_str(),_controllerDbPath.c_str(),_ports[0], _rc);
			if (!_ssoRedirectURL.empty()) {
				_controller->setSSORedirectURL(_ssoRedirectURL);
			}
			_node->setNetconfMaster((void *)_controller);

			// Join existing networks in networks.d
			{
				std::vector<std::string> networksDotD(OSUtils::listDirectory((_homePath + ZT_PATH_SEPARATOR_S "networks.d").c_str()));
				for(std::vector<std::string>::iterator f(networksDotD.begin());f!=networksDotD.end();++f) {
					std::size_t dot = f->find_last_of('.');
					if ((dot == 16)&&(f->substr(16) == ".conf"))
						_node->join(Utils::hexStrToU64(f->substr(0,dot).c_str()),(void *)0,(void *)0);
				}
			}

			// Orbit existing moons in moons.d
			{
				std::vector<std::string> moonsDotD(OSUtils::listDirectory((_homePath + ZT_PATH_SEPARATOR_S "moons.d").c_str()));
				for(std::vector<std::string>::iterator f(moonsDotD.begin());f!=moonsDotD.end();++f) {
					std::size_t dot = f->find_last_of('.');
					if ((dot == 16)&&(f->substr(16) == ".moon"))
						_node->orbit((void *)0,Utils::hexStrToU64(f->substr(0,dot).c_str()),0);
				}
			}

			// Main I/O loop
			_nextBackgroundTaskDeadline = 0;
			int64_t clockShouldBe = OSUtils::now();
			_lastRestart = clockShouldBe;
			int64_t lastTapMulticastGroupCheck = 0;
			int64_t lastBindRefresh = 0;
			int64_t lastUpdateCheck = clockShouldBe;
			int64_t lastCleanedPeersDb = 0;
			int64_t lastLocalInterfaceAddressCheck = (clockShouldBe - ZT_LOCAL_INTERFACE_CHECK_INTERVAL) + 15000; // do this in 15s to give portmapper time to configure and other things time to settle
			int64_t lastLocalConfFileCheck = OSUtils::now();
			int64_t lastOnline = lastLocalConfFileCheck;
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

				// Check for updates (if enabled)
				if ((_updater)&&((now - lastUpdateCheck) > 10000)) {
					lastUpdateCheck = now;
					if (_updater->check(now) && _updateAutoApply)
						_updater->apply();
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

				// If secondary port is not configured to a constant value and we've been offline for a while,
				// bind a new secondary port. This is a workaround for a "coma" issue caused by buggy NATs that stop
				// working on one port after a while.
				if (_node->online()) {
					lastOnline = now;
				} else if ((_secondaryPort == 0)&&((now - lastOnline) > ZT_PATH_HEARTBEAT_PERIOD)) {
					_secondaryPort = _getRandomPort();
					lastBindRefresh = 0;
				}

				// Refresh bindings in case device's interfaces have changed, and also sync routes to update any shadow routes (e.g. shadow default)
				if (((now - lastBindRefresh) >= (_node->bondController()->inUse() ? ZT_BINDER_REFRESH_PERIOD / 4 : ZT_BINDER_REFRESH_PERIOD))||(restarted)) {
					lastBindRefresh = now;
					unsigned int p[3];
					unsigned int pc = 0;
					for(int i=0;i<3;++i) {
						if (_ports[i])
							p[pc++] = _ports[i];
					}
					_binder.refresh(_phy,p,pc,explicitBind,*this);
					{
						Mutex::Lock _l(_nets_m);
						for(std::map<uint64_t,NetworkState>::iterator n(_nets.begin());n!=_nets.end();++n) {
							if (n->second.tap())
								syncManagedStuff(n->second,false,true,false);
						}
					}
				}

				// Run background task processor in core if it's time to do so
				int64_t dl = _nextBackgroundTaskDeadline;
				if (dl <= now) {
					_node->processBackgroundTasks((void *)0,now,&_nextBackgroundTaskDeadline);
					dl = _nextBackgroundTaskDeadline;
				}

				// Close TCP fallback tunnel if we have direct UDP
				if ((_tcpFallbackTunnel)&&((now - _lastDirectReceiveFromGlobal) < (ZT_TCP_FALLBACK_AFTER / 2)))
					_phy.close(_tcpFallbackTunnel->sock);

				// Sync multicast group memberships
				if ((now - lastTapMulticastGroupCheck) >= ZT_TAP_CHECK_MULTICAST_INTERVAL) {
					lastTapMulticastGroupCheck = now;
					std::vector< std::pair< uint64_t,std::pair< std::vector<MulticastGroup>,std::vector<MulticastGroup> > > > mgChanges;
					{
						Mutex::Lock _l(_nets_m);
						mgChanges.reserve(_nets.size() + 1);
						for(std::map<uint64_t,NetworkState>::const_iterator n(_nets.begin());n!=_nets.end();++n) {
							if (n->second.tap()) {
								mgChanges.push_back(std::pair< uint64_t,std::pair< std::vector<MulticastGroup>,std::vector<MulticastGroup> > >(n->first,std::pair< std::vector<MulticastGroup>,std::vector<MulticastGroup> >()));
								n->second.tap()->scanMulticastGroups(mgChanges.back().second.first,mgChanges.back().second.second);
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
				if ((now - lastLocalInterfaceAddressCheck) >= (_node->bondController()->inUse() ? ZT_LOCAL_INTERFACE_CHECK_INTERVAL / 8 : ZT_LOCAL_INTERFACE_CHECK_INTERVAL)) {
					lastLocalInterfaceAddressCheck = now;

					_node->clearLocalInterfaceAddresses();

#ifdef ZT_USE_MINIUPNPC
					if (_portMapper) {
						std::vector<InetAddress> mappedAddresses(_portMapper->get());
						for(std::vector<InetAddress>::const_iterator ext(mappedAddresses.begin());ext!=mappedAddresses.end();++ext)
							_node->addLocalInterfaceAddress(reinterpret_cast<const struct sockaddr_storage *>(&(*ext)));
					}
#endif

					std::vector<InetAddress> boundAddrs(_binder.allBoundLocalInterfaceAddresses());
					for(std::vector<InetAddress>::const_iterator i(boundAddrs.begin());i!=boundAddrs.end();++i) {
						_node->addLocalInterfaceAddress(reinterpret_cast<const struct sockaddr_storage *>(&(*i)));
					}
				}

				// Clean peers.d periodically
				if ((now - lastCleanedPeersDb) >= 3600000) {
					lastCleanedPeersDb = now;
					OSUtils::cleanDirectory((_homePath + ZT_PATH_SEPARATOR_S "peers.d").c_str(),now - 2592000000LL); // delete older than 30 days
				}

				const unsigned long delay = (dl > now) ? (unsigned long)(dl - now) : 500;
				clockShouldBe = now + (int64_t)delay;
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

		delete _updater;
		_updater = (SoftwareUpdater *)0;
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

		// Make a copy so lookups don't modify in place;
		json lc(_localConfig);

		// Get any trusted paths in local.conf (we'll parse the rest of physical[] elsewhere)
		json &physical = lc["physical"];
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

		json &settings = lc["settings"];
		if (settings.is_object()) {
			// Allow controller DB path to be put somewhere else
			const std::string cdbp(OSUtils::jsonString(settings["controllerDbPath"],""));
			if (cdbp.length() > 0)
				_controllerDbPath = cdbp;

			_ssoRedirectURL = OSUtils::jsonString(settings["ssoRedirectURL"], "");

#ifdef ZT_CONTROLLER_USE_LIBPQ
			// TODO:  Redis config
			json &redis = settings["redis"];
			if (redis.is_object() && _rc == NULL) {
				_rc = new RedisConfig;
				_rc->hostname = OSUtils::jsonString(redis["hostname"],"");
				_rc->port = OSUtils::jsonInt(redis["port"],0);
				_rc->password = OSUtils::jsonString(redis["password"],"");
				_rc->clusterMode = OSUtils::jsonBool(redis["clusterMode"], false);
			}
#endif

			// Bind to wildcard instead of to specific interfaces (disables full tunnel capability)
			json &bind = settings["bind"];
			if (bind.is_array()) {
				for(unsigned long i=0;i<bind.size();++i) {
					const std::string ips(OSUtils::jsonString(bind[i],""));
					if (ips.length() > 0) {
						InetAddress ip(ips.c_str());
						if ((ip.ss_family == AF_INET)||(ip.ss_family == AF_INET6))
							explicitBind.push_back(ip);
					}
				}
			}
		}

		// Set trusted paths if there are any
		if (!ppc.empty()) {
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
		if ((n != _nets.end())&&(n->second.tap()))
			return n->second.tap()->deviceName();
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
		*numRoutes = *numRoutes < n.config().routeCount ? *numRoutes : n.config().routeCount;
		for(unsigned int i=0; i<*numRoutes; i++) {
			ZT_VirtualNetworkRoute *vnr = (ZT_VirtualNetworkRoute*)routeArray;
			memcpy(&vnr[i], &(n.config().routes[i]), sizeof(ZT_VirtualNetworkRoute));
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
		settings = n->second.settings();
		return true;
	}

	virtual bool setNetworkSettings(const uint64_t nwid,const NetworkSettings &settings)
	{
		char nlcpath[4096];
		OSUtils::ztsnprintf(nlcpath,sizeof(nlcpath),"%s" ZT_PATH_SEPARATOR_S "%.16llx.local.conf",_networksPath.c_str(),nwid);
		FILE *out = fopen(nlcpath,"w");
		if (out) {
			fprintf(out,"allowManaged=%d\n",(int)settings.allowManaged);
			fprintf(out,"allowGlobal=%d\n",(int)settings.allowGlobal);
			fprintf(out,"allowDefault=%d\n",(int)settings.allowDefault);
			fprintf(out,"allowDNS=%d\n",(int)settings.allowDNS);
			fclose(out);
		}


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
		if (!ps.empty()) {
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
			int argpos = path.find('?');
			if(synotoken_pos != std::string::npos && argpos != std::string::npos) {
				std::string cookie = path.substr(argpos+1, synotoken_pos-(argpos+1));
				std::string synotoken = path.substr(synotoken_pos);
				std::string cookie_val = cookie.substr(cookie.find('=')+1);
				std::string synotoken_val = synotoken.substr(synotoken.find('=')+1);
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
				if (ps[0] == "bond") {
					if (_node->bondController()->inUse()) {
						if (ps.size() == 3) {
							//fprintf(stderr, "ps[0]=%s\nps[1]=%s\nps[2]=%s\n", ps[0].c_str(), ps[1].c_str(), ps[2].c_str());
							if (ps[2].length() == 10) {
								// check if hex string
								const uint64_t id = Utils::hexStrToU64(ps[2].c_str());
								if (ps[1] == "show") {
									SharedPtr<Bond> bond = _node->bondController()->getBondByPeerId(id);
									if (bond) {
										_bondToJson(res,bond);
										scode = 200;
									} else {
										fprintf(stderr, "unable to find bond to peer %llx\n", (unsigned long long)id);
										scode = 400;
									}
								}
								if (ps[1] == "flows") {
									fprintf(stderr, "displaying flows\n");
								}
							}
						}
					} else {
						scode = 400; /* bond controller is not enabled */
					}
				} else if (ps[0] == "config") {
					Mutex::Lock lc(_localConfig_m);
					res = _localConfig;
					scode = 200;
				} else if (ps[0] == "status") {
					ZT_NodeStatus status;
					_node->status(&status);

					OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.10llx",status.address);
					res["address"] = tmp;
					res["publicIdentity"] = status.publicIdentity;
					res["online"] = (bool)(status.online != 0);
					res["tcpFallbackActive"] = (_tcpFallbackTunnel != (TcpConnection *)0);
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
					settings["allowTcpFallbackRelay"] = OSUtils::jsonBool(settings["allowTcpFallbackRelay"],_allowTcpFallbackRelay);
					settings["primaryPort"] = OSUtils::jsonInt(settings["primaryPort"],(uint64_t)_primaryPort) & 0xffff;
					settings["secondaryPort"] = OSUtils::jsonInt(settings["secondaryPort"],(uint64_t)_secondaryPort) & 0xffff;
					settings["tertiaryPort"] = OSUtils::jsonInt(settings["tertiaryPort"],(uint64_t)_tertiaryPort) & 0xffff;
					// Enumerate all external listening address/port pairs
					std::vector<InetAddress> boundAddrs(_binder.allBoundLocalInterfaceAddresses());
					auto boundAddrArray = json::array();
					for (int i = 0; i < boundAddrs.size(); i++) {
						char ipBuf[64] = { 0 };
						boundAddrs[i].toString(ipBuf);
						boundAddrArray.push_back(ipBuf);
					}
					settings["listeningOn"] = boundAddrArray;

#ifdef ZT_USE_MINIUPNPC
					settings["portMappingEnabled"] = OSUtils::jsonBool(settings["portMappingEnabled"],true);
#else
					settings["portMappingEnabled"] = false; // not supported in build
#endif
#ifndef ZT_SDK
					settings["softwareUpdate"] = OSUtils::jsonString(settings["softwareUpdate"],ZT_SOFTWARE_UPDATE_DEFAULT);
					settings["softwareUpdateChannel"] = OSUtils::jsonString(settings["softwareUpdateChannel"],ZT_SOFTWARE_UPDATE_DEFAULT_CHANNEL);
#endif
					const World planet(_node->planet());
					res["planetWorldId"] = planet.id();
					res["planetWorldTimestamp"] = planet.timestamp();

					scode = 200;
				} else if (ps[0] == "moon") {
					std::vector<World> moons(_node->moons());
					if (ps.size() == 1) {
						// Return [array] of all moons

						res = json::array();
						for(std::vector<World>::const_iterator m(moons.begin());m!=moons.end();++m) {
							json mj;
							_moonToJson(mj,*m);
							res.push_back(mj);
						}

						scode = 200;
					} else {
						// Return a single moon by ID

						const uint64_t id = Utils::hexStrToU64(ps[1].c_str());
						for(std::vector<World>::const_iterator m(moons.begin());m!=moons.end();++m) {
							if (m->id() == id) {
								_moonToJson(res,*m);
								scode = 200;
								break;
							}
						}

					}
				} else if (ps[0] == "network") {
					Mutex::Lock _l(_nets_m);
					if (ps.size() == 1) {
						// Return [array] of all networks

						res = nlohmann::json::array();

						for (auto it = _nets.begin(); it != _nets.end(); ++it) {
							NetworkState &ns = it->second;
							nlohmann::json nj;
							_networkToJson(nj, ns);
							res.push_back(nj);
						}

						scode = 200;
					} else if (ps.size() == 2) {
						// Return a single network by ID or 404 if not found

						const uint64_t wantnw = Utils::hexStrToU64(ps[1].c_str());
						if (_nets.find(wantnw) != _nets.end()) {
							res = json::object();
							NetworkState& ns = _nets[wantnw];
							_networkToJson(res, ns);
							scode = 200;
						}
					} else {
						scode = 404;
					}
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
				} else if (ps[0] == "bonds") {
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
#if ZT_SSO_ENABLED
			} else if (ps[0] == "sso") {
				char resBuf[4096] = {0};
				const char *error = zeroidc::zeroidc_get_url_param_value("error", path.c_str());
				if (error != nullptr) {
					const char *desc = zeroidc::zeroidc_get_url_param_value("error_description", path.c_str());
					scode = 500;
					char errBuff[256] = {0};
					sprintf(errBuff, "ERROR %s: %s", error, desc);
					sprintf(resBuf, ssoResponseTemplate, errBuff);
					responseBody = std::string(resBuf);
					responseContentType = "text/html";
					return scode;
				} 

				// SSO redirect handling
				char* state = zeroidc::zeroidc_get_url_param_value("state", path.c_str());
				char* nwid = zeroidc::zeroidc_network_id_from_state(state);
				
				const uint64_t id = Utils::hexStrToU64(nwid);
				
				zeroidc::free_cstr(nwid);
				zeroidc::free_cstr(state);

				Mutex::Lock l(_nets_m);
				if (_nets.find(id) != _nets.end()) {
					NetworkState& ns = _nets[id];
					char* code = zeroidc::zeroidc_get_url_param_value("code", path.c_str());
					char *ret = ns.doTokenExchange(code);
					scode = 200;
					sprintf(resBuf, ssoResponseTemplate, "Authentication Successful. You may now access the network.");
					responseBody = std::string(resBuf);

					zeroidc::free_cstr(code);
					zeroidc::free_cstr(ret);
					
					responseContentType = "text/html";
					return scode;
				} else {
					scode = 404;
				}
#endif
			} else {
				scode = 401; // isAuth == false && !sso
			}
		} else if ((httpMethod == HTTP_POST)||(httpMethod == HTTP_PUT)) {
 			if (isAuth) {
				if (ps[0] == "bond") {
					if (_node->bondController()->inUse()) {
						if (ps.size() == 3) {
							//fprintf(stderr, "ps[0]=%s\nps[1]=%s\nps[2]=%s\n", ps[0].c_str(), ps[1].c_str(), ps[2].c_str());
							if (ps[2].length() == 10) {
								// check if hex string
								const uint64_t id = Utils::hexStrToU64(ps[2].c_str());
								if (ps[1] == "rotate") {
									SharedPtr<Bond> bond = _node->bondController()->getBondByPeerId(id);
									if (bond) {
										scode = bond->abForciblyRotateLink() ? 200 : 400;
									} else {
										fprintf(stderr, "unable to find bond to peer %llx\n", (unsigned long long)id);
										scode = 400;
									}
								}
								if (ps[1] == "enable") {
									fprintf(stderr, "enabling bond\n");
								}
							}
						}
					} else {
						scode = 400; /* bond controller is not enabled */
					}
				} else if (ps[0] == "config") {
					// Right now we only support writing the things the UI supports changing.
					if (ps.size() == 2) {
						if (ps[1] == "settings") {
							try {
								json j(OSUtils::jsonParse(body));
								if (j.is_object()) {
									Mutex::Lock lcl(_localConfig_m);
									json lc(_localConfig);
									for(json::const_iterator s(j.begin());s!=j.end();++s) {
										lc["settings"][s.key()] = s.value();
									}
									std::string lcStr = OSUtils::jsonDump(lc, 4);
									if (OSUtils::writeFile((_homePath + ZT_PATH_SEPARATOR_S "local.conf").c_str(), lcStr)) {
										_localConfig = lc;
									}
								} else {
									scode = 400;
								}
							} catch ( ... ) {
								scode = 400;
							}
						} else {
							scode = 404;
						}
					} else {
						scode = 404;
					}
				} else if (ps[0] == "moon") {
					if (ps.size() == 2) {

						uint64_t seed = 0;
						try {
							json j(OSUtils::jsonParse(body));
							if (j.is_object()) {
								seed = Utils::hexStrToU64(OSUtils::jsonString(j["seed"],"0").c_str());
							}
						} catch ( ... ) {
							// discard invalid JSON
						}

						std::vector<World> moons(_node->moons());
						const uint64_t id = Utils::hexStrToU64(ps[1].c_str());
						for(std::vector<World>::const_iterator m(moons.begin());m!=moons.end();++m) {
							if (m->id() == id) {
								_moonToJson(res,*m);
								scode = 200;
								break;
							}
						}

						if ((scode != 200)&&(seed != 0)) {
							char tmp[64];
							OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.16llx",id);
							res["id"] = tmp;
							res["roots"] = json::array();
							res["timestamp"] = 0;
							res["signature"] = json();
							res["updatesMustBeSignedBy"] = json();
							res["waiting"] = true;
							_node->orbit((void *)0,id,seed);
							scode = 200;
						}

					} else scode = 404;
				} else if (ps[0] == "network") {
					if (ps.size() == 2) {

						uint64_t wantnw = Utils::hexStrToU64(ps[1].c_str());
						_node->join(wantnw,(void *)0,(void *)0); // does nothing if we are a member
						Mutex::Lock l(_nets_m);
						if (!_nets.empty()) {
							if (_nets.find(wantnw) != _nets.end()) {
								NetworkState& ns = _nets[wantnw];
								try {
									json j(OSUtils::jsonParse(body));
								
									json &allowManaged = j["allowManaged"];
									if (allowManaged.is_boolean()) {
										ns.setAllowManaged((bool)allowManaged);
									}
									json& allowGlobal = j["allowGlobal"];
									if (allowGlobal.is_boolean()) {
										ns.setAllowGlobal((bool)allowGlobal);
									}
									json& allowDefault = j["allowDefault"];
									if (allowDefault.is_boolean()) {
										ns.setAllowDefault((bool)allowDefault);
									}
									json& allowDNS = j["allowDNS"];
									if (allowDNS.is_boolean()) {
										ns.setAllowDNS((bool)allowDNS);
									}
								} catch (...) {
									// discard invalid JSON
								}
								setNetworkSettings(wantnw, ns.settings());
								if (ns.tap()) {
									syncManagedStuff(ns,true,true,true);
								}

								_networkToJson(res, ns);

								scode = 200;
							}
						} else scode = 500;

					} else scode = 404;
				} else {
					if (_controller)
						scode = _controller->handleControlPlaneHttpPOST(std::vector<std::string>(ps.begin()+1,ps.end()),urlArgs,headers,body,responseBody,responseContentType);
					else scode = 404;
				}
			}
			else {
				scode = 401; // isAuth == false
			}
		} else if (httpMethod == HTTP_DELETE) {
			if (isAuth) {

				if (ps[0] == "moon") {
					if (ps.size() == 2) {
						_node->deorbit((void *)0,Utils::hexStrToU64(ps[1].c_str()));
						res["result"] = true;
						scode = 200;
					} // else 404
				} else if (ps[0] == "network") {
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

		if (!_node->bondController()->inUse()) {
			// defaultBondingPolicy
			std::string defaultBondingPolicyStr(OSUtils::jsonString(settings["defaultBondingPolicy"],""));
			int defaultBondingPolicy = _node->bondController()->getPolicyCodeByStr(defaultBondingPolicyStr);
			_node->bondController()->setBondingLayerDefaultPolicy(defaultBondingPolicy);
			_node->bondController()->setBondingLayerDefaultPolicyStr(defaultBondingPolicyStr); // Used if custom policy
			// Custom Policies
			json &customBondingPolicies = settings["policies"];
			for (json::iterator policyItr = customBondingPolicies.begin(); policyItr != customBondingPolicies.end();++policyItr) {
				// Custom Policy
				std::string customPolicyStr(policyItr.key());
				json &customPolicy = policyItr.value();
				std::string basePolicyStr(OSUtils::jsonString(customPolicy["basePolicy"],""));
				if (basePolicyStr.empty()) {
					fprintf(stderr, "error: no base policy was specified for custom policy (%s)\n", customPolicyStr.c_str());
				}
				int basePolicyCode = _node->bondController()->getPolicyCodeByStr(basePolicyStr);
				if (basePolicyCode == ZT_BOND_POLICY_NONE) {
					fprintf(stderr, "error: custom policy (%s) is invalid, unknown base policy (%s).\n",
						customPolicyStr.c_str(), basePolicyStr.c_str());
					continue;
				} if (_node->bondController()->getPolicyCodeByStr(customPolicyStr) != ZT_BOND_POLICY_NONE) {
					fprintf(stderr, "error: custom policy (%s) will be ignored, cannot use standard policy names for custom policies.\n",
						customPolicyStr.c_str());
					continue;
				}
				// New bond, used as a copy template for new instances
				SharedPtr<Bond> newTemplateBond = new Bond(NULL, basePolicyStr, customPolicyStr, SharedPtr<Peer>());
				// Acceptable ranges
				newTemplateBond->setPolicy(basePolicyCode);
				newTemplateBond->setMaxAcceptableLatency(OSUtils::jsonInt(customPolicy["maxAcceptableLatency"],-1));
				newTemplateBond->setMaxAcceptableMeanLatency(OSUtils::jsonInt(customPolicy["maxAcceptableMeanLatency"],-1));
				newTemplateBond->setMaxAcceptablePacketDelayVariance(OSUtils::jsonInt(customPolicy["maxAcceptablePacketDelayVariance"],-1));
				newTemplateBond->setMaxAcceptablePacketLossRatio((float)OSUtils::jsonDouble(customPolicy["maxAcceptablePacketLossRatio"],-1));
				newTemplateBond->setMaxAcceptablePacketErrorRatio((float)OSUtils::jsonDouble(customPolicy["maxAcceptablePacketErrorRatio"],-1));
				newTemplateBond->setMinAcceptableAllocation((float)OSUtils::jsonDouble(customPolicy["minAcceptableAllocation"],0));
				// Quality weights
				json &qualityWeights = customPolicy["qualityWeights"];
				if (qualityWeights.size() == ZT_QOS_WEIGHT_SIZE) { // TODO: Generalize this
					float weights[ZT_QOS_WEIGHT_SIZE];
					weights[ZT_QOS_LAT_IDX] = (float)OSUtils::jsonDouble(qualityWeights["lat"],0.0);
					weights[ZT_QOS_LTM_IDX] = (float)OSUtils::jsonDouble(qualityWeights["ltm"],0.0);
					weights[ZT_QOS_PDV_IDX] = (float)OSUtils::jsonDouble(qualityWeights["pdv"],0.0);
					weights[ZT_QOS_PLR_IDX] = (float)OSUtils::jsonDouble(qualityWeights["plr"],0.0);
					weights[ZT_QOS_PER_IDX] = (float)OSUtils::jsonDouble(qualityWeights["per"],0.0);
					weights[ZT_QOS_THR_IDX] = (float)OSUtils::jsonDouble(qualityWeights["thr"],0.0);
					weights[ZT_QOS_THM_IDX] = (float)OSUtils::jsonDouble(qualityWeights["thm"],0.0);
					weights[ZT_QOS_THV_IDX] = (float)OSUtils::jsonDouble(qualityWeights["thv"],0.0);
					newTemplateBond->setUserQualityWeights(weights,ZT_QOS_WEIGHT_SIZE);
				}
				// Bond-specific properties
				newTemplateBond->setUpDelay(OSUtils::jsonInt(customPolicy["upDelay"],-1));
				newTemplateBond->setDownDelay(OSUtils::jsonInt(customPolicy["downDelay"],-1));
				newTemplateBond->setFlowRebalanceStrategy(OSUtils::jsonInt(customPolicy["flowRebalanceStrategy"],(uint64_t)0));
				newTemplateBond->setFailoverInterval(OSUtils::jsonInt(customPolicy["failoverInterval"],ZT_BOND_FAILOVER_DEFAULT_INTERVAL));
				newTemplateBond->setPacketsPerLink(OSUtils::jsonInt(customPolicy["packetsPerLink"],-1));

				// Policy-Specific link set
				json &links = customPolicy["links"];
				for (json::iterator linkItr = links.begin(); linkItr != links.end();++linkItr) {
					std::string linkNameStr(linkItr.key());
					json &link = linkItr.value();

					bool enabled = OSUtils::jsonInt(link["enabled"],true);
					uint32_t speed = OSUtils::jsonInt(link["speed"],0);
					float alloc = (float)OSUtils::jsonDouble(link["alloc"],0);

					if (speed && alloc) {
						fprintf(stderr, "error: cannot specify both speed (%d) and alloc (%f) for link (%s), pick one, link disabled.\n",
							speed, alloc, linkNameStr.c_str());
						enabled = false;
					}
					uint8_t ipvPref = OSUtils::jsonInt(link["ipvPref"],0);
					std::string failoverToStr(OSUtils::jsonString(link["failoverTo"],""));
					// Mode
					std::string linkModeStr(OSUtils::jsonString(link["mode"],"spare"));
					uint8_t linkMode = ZT_BOND_SLAVE_MODE_SPARE;
					if (linkModeStr == "primary") { linkMode = ZT_BOND_SLAVE_MODE_PRIMARY; }
					if (linkModeStr == "spare") { linkMode = ZT_BOND_SLAVE_MODE_SPARE; }
					// ipvPref
					if ((ipvPref != 0) && (ipvPref != 4) && (ipvPref != 6) && (ipvPref != 46) && (ipvPref != 64)) {
						fprintf(stderr, "error: invalid ipvPref value (%d), link disabled.\n", ipvPref);
						enabled = false;
					}
					if (linkMode == ZT_BOND_SLAVE_MODE_SPARE && failoverToStr.length()) {
						fprintf(stderr, "error: cannot specify failover links for spares, link disabled.\n");
						failoverToStr = "";
						enabled = false;
					}
					_node->bondController()->addCustomLink(customPolicyStr, new Link(linkNameStr,ipvPref,speed,enabled,linkMode,failoverToStr,alloc));
				}
				std::string linkSelectMethodStr(OSUtils::jsonString(customPolicy["activeReselect"],"optimize"));
				if (linkSelectMethodStr == "always") {
					newTemplateBond->setLinkSelectMethod(ZT_BOND_RESELECTION_POLICY_ALWAYS);
				}
				if (linkSelectMethodStr == "better") {
					newTemplateBond->setLinkSelectMethod(ZT_BOND_RESELECTION_POLICY_BETTER);
				}
				if (linkSelectMethodStr == "failure") {
					newTemplateBond->setLinkSelectMethod(ZT_BOND_RESELECTION_POLICY_FAILURE);
				}
				if (linkSelectMethodStr == "optimize") {
					newTemplateBond->setLinkSelectMethod(ZT_BOND_RESELECTION_POLICY_OPTIMIZE);
				}
				if (newTemplateBond->getLinkSelectMethod() < 0 || newTemplateBond->getLinkSelectMethod() > 3) {
					fprintf(stderr, "warning: invalid value (%s) for linkSelectMethod, assuming mode: always\n", linkSelectMethodStr.c_str());
				}
				/*
				newBond->setPolicy(_node->bondController()->getPolicyCodeByStr(basePolicyStr));
				newBond->setFlowHashing((bool)OSUtils::jsonInt(userSpecifiedBondingPolicies[i]["allowFlowHashing"],(bool)allowFlowHashing));
				newBond->setBondMonitorInterval((unsigned int)OSUtils::jsonInt(userSpecifiedBondingPolicies[i]["monitorInterval"],(uint64_t)0));
				newBond->setAllowPathNegotiation((bool)OSUtils::jsonInt(userSpecifiedBondingPolicies[i]["allowPathNegotiation"],(bool)false));
				*/
				if (!_node->bondController()->addCustomPolicy(newTemplateBond)) {
					fprintf(stderr, "error: a custom policy of this name (%s) already exists.\n", customPolicyStr.c_str());
				}
			}
			// Peer-specific bonding
			json &peerSpecificBonds = settings["peerSpecificBonds"];
			for (json::iterator peerItr = peerSpecificBonds.begin(); peerItr != peerSpecificBonds.end();++peerItr) {
				_node->bondController()->assignBondingPolicyToPeer(std::stoull(peerItr.key(),0,16), peerItr.value());
			}
			// Check settings
			if (defaultBondingPolicyStr.length() && !defaultBondingPolicy && !_node->bondController()->inUse()) {
				fprintf(stderr, "error: unknown policy (%s) specified by defaultBondingPolicy, bond disabled.\n", defaultBondingPolicyStr.c_str());
			}
		}

		// bondingPolicy cannot be used with allowTcpFallbackRelay
		_allowTcpFallbackRelay = OSUtils::jsonBool(settings["allowTcpFallbackRelay"],true);
#ifdef ZT_TCP_FALLBACK_RELAY
		_fallbackRelayAddress = InetAddress(OSUtils::jsonString("tcpFallbackRelay", ZT_TCP_FALLBACK_RELAY).c_str());
#endif
		_primaryPort = (unsigned int)OSUtils::jsonInt(settings["primaryPort"],(uint64_t)_primaryPort) & 0xffff;
		_allowSecondaryPort = OSUtils::jsonBool(settings["allowSecondaryPort"],true);
		_secondaryPort = (unsigned int)OSUtils::jsonInt(settings["secondaryPort"],0);
		_tertiaryPort = (unsigned int)OSUtils::jsonInt(settings["tertiaryPort"],0);
		if (_secondaryPort != 0 || _tertiaryPort != 0) {
			fprintf(stderr,"WARNING: using manually-specified secondary and/or tertiary ports. This can cause NAT issues." ZT_EOL_S);
		}
		_portMappingEnabled = OSUtils::jsonBool(settings["portMappingEnabled"],true);

#ifndef ZT_SDK
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
#endif

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

#if ZT_VAULT_SUPPORT
		json &vault = settings["vault"];
		if (vault.is_object()) {
			const std::string url(OSUtils::jsonString(vault["vaultURL"], "").c_str());
			if (!url.empty()) {
				_vaultURL = url;
			}

			const std::string token(OSUtils::jsonString(vault["vaultToken"], "").c_str());
			if (!token.empty()) {
				_vaultToken = token;
			}

			const std::string path(OSUtils::jsonString(vault["vaultPath"], "").c_str());
			if (!path.empty()) {
				_vaultPath = path;
			}
		}

		// also check environment variables for values.  Environment variables
		// will override local.conf variables
		const std::string envURL(getenv("VAULT_ADDR"));
		if (!envURL.empty()) {
			_vaultURL = envURL;
		}

		const std::string envToken(getenv("VAULT_TOKEN"));
		if (!envToken.empty()) {
			_vaultToken = envToken;
		}

		const std::string envPath(getenv("VAULT_PATH"));
		if (!envPath.empty()) {
			_vaultPath = envPath;
		}

		if (!_vaultURL.empty() && !_vaultToken.empty()) {
			_vaultEnabled = true;
		}
#endif

	// Checks if a managed IP or route target is allowed
	bool checkIfManagedIsAllowed(const NetworkState &n,const InetAddress &target)
	{
		if (!n.allowManaged())
			return false;

		if (!n.allowManagedWhitelist().empty()) {
			bool allowed = false;
			for (InetAddress addr : n.allowManagedWhitelist()) {
				if (addr.containsAddress(target) && addr.netmaskBits() <= target.netmaskBits()) {
					allowed = true;
					break;
				}
			}
			if (!allowed) return false;
		}

		if (target.isDefaultRoute())
			return n.allowDefault();
		switch(target.ipScope()) {
			case InetAddress::IP_SCOPE_NONE:
			case InetAddress::IP_SCOPE_MULTICAST:
			case InetAddress::IP_SCOPE_LOOPBACK:
			case InetAddress::IP_SCOPE_LINK_LOCAL:
				return false;
			case InetAddress::IP_SCOPE_GLOBAL:
				return n.allowGlobal();
			default:
				return true;
		}
	}

	// Match only an IP from a vector of IPs -- used in syncManagedStuff()
	inline bool matchIpOnly(const std::set<InetAddress> &ips,const InetAddress &ip) const
	{
		for(std::set<InetAddress>::const_iterator i(ips.begin());i!=ips.end();++i) {
			if (i->ipsEqual(ip))
				return true;
		}
		return false;
	}

	// Apply or update managed IPs for a configured network (be sure n.tap exists)
	void syncManagedStuff(NetworkState &n,bool syncIps,bool syncRoutes, bool syncDns)
	{
		char ipbuf[64];

		// assumes _nets_m is locked
		if (syncIps) {
			std::vector<InetAddress> newManagedIps;
			newManagedIps.reserve(n.config().assignedAddressCount);
			for(unsigned int i=0;i<n.config().assignedAddressCount;++i) {
				const InetAddress *ii = reinterpret_cast<const InetAddress *>(&(n.config().assignedAddresses[i]));
				if (checkIfManagedIsAllowed(n,*ii))
					newManagedIps.push_back(*ii);
			}
			std::sort(newManagedIps.begin(),newManagedIps.end());
			newManagedIps.erase(std::unique(newManagedIps.begin(),newManagedIps.end()),newManagedIps.end());

			for(std::vector<InetAddress>::iterator ip(n.managedIps().begin());ip!=n.managedIps().end();++ip) {
				if (std::find(newManagedIps.begin(),newManagedIps.end(),*ip) == newManagedIps.end()) {
					if (!n.tap()->removeIp(*ip))
						fprintf(stderr,"ERROR: unable to remove ip address %s" ZT_EOL_S, ip->toString(ipbuf));
				}
			}

			for(std::vector<InetAddress>::iterator ip(newManagedIps.begin());ip!=newManagedIps.end();++ip) {
				if (std::find(n.managedIps().begin(),n.managedIps().end(),*ip) == n.managedIps().end()) {
					if (!n.tap()->addIp(*ip))
						fprintf(stderr,"ERROR: unable to add ip address %s" ZT_EOL_S, ip->toString(ipbuf));
				}
			}

#ifdef __APPLE__
			if (!MacDNSHelper::addIps(n.config().nwid, n.config().mac, n.tap()->deviceName().c_str(), newManagedIps))
				fprintf(stderr, "ERROR: unable to add v6 addresses to system configuration" ZT_EOL_S);
#endif
			n.setManagedIps(newManagedIps);
		}

		if (syncRoutes) {
			// Get tap device name (use LUID in hex on Windows) and IP addresses.
#if defined(__WINDOWS__) && !defined(ZT_SDK)
			char tapdevbuf[64];
			OSUtils::ztsnprintf(tapdevbuf,sizeof(tapdevbuf),"%.16llx",(unsigned long long)((WindowsEthernetTap *)(n.tap().get()))->luid().Value);
			std::string tapdev(tapdevbuf);
#else
			std::string tapdev(n.tap()->deviceName());
#endif

			std::vector<InetAddress> tapIps(n.tap()->ips());
			std::set<InetAddress> myIps(tapIps.begin(), tapIps.end());
			for(unsigned int i=0;i<n.config().assignedAddressCount;++i)
				myIps.insert(InetAddress(n.config().assignedAddresses[i]));

			std::set<InetAddress> haveRouteTargets;
			for(unsigned int i=0;i<n.config().routeCount;++i) {
				const InetAddress *const target = reinterpret_cast<const InetAddress *>(&(n.config().routes[i].target));
				const InetAddress *const via = reinterpret_cast<const InetAddress *>(&(n.config().routes[i].via));

				// Make sure we are allowed to set this managed route, and that 'via' is not our IP. The latter
				// avoids setting routes via the router on the router.
				if ( (!checkIfManagedIsAllowed(n,*target)) || ((via->ss_family == target->ss_family)&&(matchIpOnly(myIps,*via))) )
					continue;

				// Find an IP on the interface that can be a source IP, abort if no IPs assigned.
				const InetAddress *src = nullptr;
				unsigned int mostMatchingPrefixBits = 0;
				for(std::set<InetAddress>::const_iterator i(myIps.begin());i!=myIps.end();++i) {
					const unsigned int matchingPrefixBits = i->matchingPrefixBits(*target);
					if (matchingPrefixBits >= mostMatchingPrefixBits && ((target->isV4() && i->isV4()) || (target->isV6() && i->isV6()))) {
						mostMatchingPrefixBits = matchingPrefixBits;
						src = &(*i);
					}
				}
				if (!src)
					continue;

				// Ignore routes implied by local managed IPs since adding the IP adds the route.
				// Apple on the other hand seems to need this at least on some versions.
#ifndef __APPLE__
				bool haveRoute = false;
				for(std::vector<InetAddress>::iterator ip(n.managedIps().begin());ip!=n.managedIps().end();++ip) {
					if ((target->netmaskBits() == ip->netmaskBits())&&(target->containsAddress(*ip))) {
						haveRoute = true;
						break;
					}
				}
				if (haveRoute)
					continue;
#endif

				haveRouteTargets.insert(*target);

#ifndef ZT_SDK
				SharedPtr<ManagedRoute> &mr = n.managedRoutes()[*target];
				if (!mr)
					mr.set(new ManagedRoute(*target, *via, *src, tapdev.c_str()));
#endif
			}

			for(std::map< InetAddress, SharedPtr<ManagedRoute> >::iterator r(n.managedRoutes().begin());r!=n.managedRoutes().end();) {
				if (haveRouteTargets.find(r->first) == haveRouteTargets.end())
					n.managedRoutes().erase(r++);
				else ++r;
			}

			// Sync device-local managed routes first, then indirect results. That way
			// we don't get destination unreachable for routes that are via things
			// that do not yet have routes in the system.
			for(std::map< InetAddress, SharedPtr<ManagedRoute> >::iterator r(n.managedRoutes().begin());r!=n.managedRoutes().end();++r) {
				if (!r->second->via())
					r->second->sync();
			}
			for(std::map< InetAddress, SharedPtr<ManagedRoute> >::iterator r(n.managedRoutes().begin());r!=n.managedRoutes().end();++r) {
				if (r->second->via())
					r->second->sync();
			}
		}

		if (syncDns) {
			if (n.allowDNS()) {
				if (strlen(n.config().dns.domain) != 0) {
					std::vector<InetAddress> servers;
					for (int j = 0; j < ZT_MAX_DNS_SERVERS; ++j) {
						InetAddress a(n.config().dns.server_addr[j]);
						if (a.isV4() || a.isV6()) {
							servers.push_back(a);
						}
					}
					n.tap()->setDns(n.config().dns.domain, servers);
				}
			} else {
#ifdef __APPLE__
				MacDNSHelper::removeDNS(n.config().nwid);
#elif defined(__WINDOWS__)
				WinDNSHelper::removeDNS(n.config().nwid);
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
		if (!success) {
			phyOnTcpClose(sock,uptr);
			return;
		}

		TcpConnection *const tc = reinterpret_cast<TcpConnection *>(*uptr);
		if (!tc) { // sanity check
			_phy.close(sock,true);
			return;
		}
		tc->sock = sock;

		if (tc->type == TcpConnection::TCP_TUNNEL_OUTGOING) {
			if (_tcpFallbackTunnel)
				_phy.close(_tcpFallbackTunnel->sock);
			_tcpFallbackTunnel = tc;
			_phy.streamSend(sock,ZT_TCP_TUNNEL_HELLO,sizeof(ZT_TCP_TUNNEL_HELLO));
		} else {
			_phy.close(sock,true);
		}
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
			if (tc == _tcpFallbackTunnel) {
				_tcpFallbackTunnel = (TcpConnection *)0;
			}
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
								if (_allowManagementFrom.empty()) {
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
				case TcpConnection::TCP_HTTP_OUTGOING:
					http_parser_execute(&(tc->parser),&HTTP_PARSER_SETTINGS,(const char *)data,len);
					if ((tc->parser.upgrade)||(tc->parser.http_errno != HPE_OK))
						_phy.close(sock);
					return;

				case TcpConnection::TCP_TUNNEL_OUTGOING:
					tc->readq.append((const char *)data,len);
					while (tc->readq.length() >= 5) {
						const char *data = tc->readq.data();
						const unsigned long mlen = ( ((((unsigned long)data[3]) & 0xff) << 8) | (((unsigned long)data[4]) & 0xff) );
						if (tc->readq.length() >= (mlen + 5)) {
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
										(void *)0,
										OSUtils::now(),
										-1,
										reinterpret_cast<struct sockaddr_storage *>(&from),
										data,
										plen,
										&_nextBackgroundTaskDeadline);
									if (ZT_ResultCode_isFatal(rc)) {
										char tmp[256];
										OSUtils::ztsnprintf(tmp,sizeof(tmp),"fatal error code from processWirePacket: %d",(int)rc);
										Mutex::Lock _l(_termReason_m);
										_termReason = ONE_UNRECOVERABLE_ERROR;
										_fatalErrorMessage = tmp;
										this->terminate();
										_phy.close(sock);
										return;
									}
								}
							}

							if (tc->readq.length() > (mlen + 5))
								tc->readq.erase(tc->readq.begin(),tc->readq.begin() + (mlen + 5));
							else tc->readq.clear();
						} else break;
					}
					return;

			}
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
		n.setWebPort(_primaryPort);

		switch (op) {
			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP:
				if (!n.tap()) {
					try {
						char friendlyName[128];
						OSUtils::ztsnprintf(friendlyName,sizeof(friendlyName),"ZeroTier One [%.16llx]",nwid);

						n.setTap(EthernetTap::newInstance(
							nullptr,
							_homePath.c_str(),
							MAC(nwc->mac),
							nwc->mtu,
							(unsigned int)ZT_IF_METRIC,
							nwid,
							friendlyName,
							StapFrameHandler,
							(void *)this));
						*nuptr = (void *)&n;

						char nlcpath[256];
						OSUtils::ztsnprintf(nlcpath,sizeof(nlcpath),"%s" ZT_PATH_SEPARATOR_S "networks.d" ZT_PATH_SEPARATOR_S "%.16llx.local.conf",_homePath.c_str(),nwid);
						std::string nlcbuf;
						if (OSUtils::readFile(nlcpath,nlcbuf)) {
							Dictionary<4096> nc;
							nc.load(nlcbuf.c_str());
							Buffer<1024> allowManaged;
							if (nc.get("allowManaged", allowManaged) && allowManaged.size() > 0) {
								std::string addresses (allowManaged.begin(), allowManaged.size());
								if (allowManaged.size() <= 5) { // untidy parsing for backward compatibility
									if (allowManaged[0] == '1' || allowManaged[0] == 't' || allowManaged[0] == 'T') {
										n.setAllowManaged(true);
									} else {
										n.setAllowManaged(false);
									}
								} else {
									// this should be a list of IP addresses
									n.setAllowManaged(true);
									size_t pos = 0;
									while (true) {
										size_t nextPos = addresses.find(',', pos);
										std::string address = addresses.substr(pos, (nextPos == std::string::npos ? addresses.size() : nextPos) - pos);
										n.addToAllowManagedWhiteList(InetAddress(address.c_str()));
										if (nextPos == std::string::npos) break;
										pos = nextPos + 1;
									}
								}
							} else {
								n.setAllowManaged(true);
							}
							n.setAllowGlobal(nc.getB("allowGlobal", false));
							n.setAllowDefault(nc.getB("allowDefault", false));
							n.setAllowDNS(nc.getB("allowDNS", false));
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
				n.setConfig(nwc);

				if (n.tap()) { // sanity check
#if defined(__WINDOWS__) && !defined(ZT_SDK)
					// wait for up to 5 seconds for the WindowsEthernetTap to actually be initialized
					//
					// without WindowsEthernetTap::isInitialized() returning true, the won't actually
					// be online yet and setting managed routes on it will fail.
					const int MAX_SLEEP_COUNT = 500;
					for (int i = 0; !((WindowsEthernetTap *)(n.tap().get()))->isInitialized() && i < MAX_SLEEP_COUNT; i++) {
						Sleep(10);
					}
#endif
					syncManagedStuff(n,true,true,true);
					n.tap()->setMtu(nwc->mtu);
				} else {
					_nets.erase(nwid);
					return -999; // tap init failed
				}
				break;

			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN:
			case ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY:
				if (n.tap()) { // sanity check
#if defined(__WINDOWS__) && !defined(ZT_SDK)
					std::string winInstanceId(((WindowsEthernetTap *)(n.tap().get()))->instanceId());
#endif
					*nuptr = (void *)0;
					n.tap().reset();
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

			case ZT_EVENT_REMOTE_TRACE: {
				const ZT_RemoteTrace *rt = reinterpret_cast<const ZT_RemoteTrace *>(metaData);
				if ((rt)&&(rt->len > 0)&&(rt->len <= ZT_MAX_REMOTE_TRACE_SIZE)&&(rt->data))
					_controller->handleRemoteTrace(*rt);
			}

			default:
				break;
		}
	}

#if ZT_VAULT_SUPPORT
	inline bool nodeVaultPutIdentity(enum ZT_StateObjectType type, const void *data, int len)
	{
		bool retval = false;
		if (type != ZT_STATE_OBJECT_IDENTITY_PUBLIC && type != ZT_STATE_OBJECT_IDENTITY_SECRET) {
			return retval;
		}

		CURL *curl = curl_easy_init();
		if (curl) {
			char token[512] = { 0 };
			snprintf(token, sizeof(token), "X-Vault-Token: %s", _vaultToken.c_str());

			struct curl_slist *chunk = NULL;
			chunk = curl_slist_append(chunk, token);


			char content_type[512] = { 0 };
			snprintf(content_type, sizeof(content_type), "Content-Type: application/json");

			chunk = curl_slist_append(chunk, content_type);

			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

			char url[2048] = { 0 };
			snprintf(url, sizeof(url), "%s/v1/%s", _vaultURL.c_str(), _vaultPath.c_str());

			curl_easy_setopt(curl, CURLOPT_URL, url);

			json d = json::object();
			if (type == ZT_STATE_OBJECT_IDENTITY_PUBLIC) {
				std::string key((const char*)data, len);
				d["public"] = key;
			}
			else if (type == ZT_STATE_OBJECT_IDENTITY_SECRET) {
				std::string key((const char*)data, len);
				d["secret"] = key;
			}

			if (!d.empty()) {
				std::string post = d.dump();

				if (!post.empty()) {
					curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.c_str());
					curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post.length());

#ifndef NDEBUG
					curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif

					CURLcode res = curl_easy_perform(curl);
					if (res == CURLE_OK) {
						long response_code = 0;
						curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
						if (response_code == 200 || response_code == 204) {
							retval = true;
						}
					}
				}
			}

			curl_easy_cleanup(curl);
			curl = NULL;
			curl_slist_free_all(chunk);
			chunk = NULL;
		}

		return retval;
	}
#endif

	inline void nodeStatePutFunction(enum ZT_StateObjectType type,const uint64_t id[2],const void *data,int len)
	{
#if ZT_VAULT_SUPPORT
		if (_vaultEnabled && (type == ZT_STATE_OBJECT_IDENTITY_SECRET || type == ZT_STATE_OBJECT_IDENTITY_PUBLIC)) {
			if (nodeVaultPutIdentity(type, data, len)) {
				// value successfully written to Vault
				return;
			}
			// else fallback to disk
		}
#endif
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
			case ZT_STATE_OBJECT_PLANET:
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "planet",_homePath.c_str());
				break;
			case ZT_STATE_OBJECT_MOON:
				OSUtils::ztsnprintf(dirname,sizeof(dirname),"%s" ZT_PATH_SEPARATOR_S "moons.d",_homePath.c_str());
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "%.16llx.moon",dirname,(unsigned long long)id[0]);
				break;
			case ZT_STATE_OBJECT_NETWORK_CONFIG:
				OSUtils::ztsnprintf(dirname,sizeof(dirname),"%s" ZT_PATH_SEPARATOR_S "networks.d",_homePath.c_str());
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "%.16llx.conf",dirname,(unsigned long long)id[0]);
				break;
			case ZT_STATE_OBJECT_PEER:
				OSUtils::ztsnprintf(dirname,sizeof(dirname),"%s" ZT_PATH_SEPARATOR_S "peers.d",_homePath.c_str());
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "%.10llx.peer",dirname,(unsigned long long)id[0]);
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

#if ZT_VAULT_SUPPORT
	inline int nodeVaultGetIdentity(enum ZT_StateObjectType type, void *data, unsigned int maxlen)
	{
		if (type != ZT_STATE_OBJECT_IDENTITY_SECRET && type != ZT_STATE_OBJECT_IDENTITY_PUBLIC) {
			return -1;
		}

		int ret = -1;
		CURL *curl = curl_easy_init();
		if (curl) {
			char token[512] = { 0 };
			snprintf(token, sizeof(token), "X-Vault-Token: %s", _vaultToken.c_str());

			struct curl_slist *chunk = NULL;
			chunk = curl_slist_append(chunk, token);
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

			char url[2048] = { 0 };
			snprintf(url, sizeof(url), "%s/v1/%s", _vaultURL.c_str(), _vaultPath.c_str());

			curl_easy_setopt(curl, CURLOPT_URL, url);

			std::string response;
			std::string res_headers;

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curlResponseWrite);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &res_headers);

#ifndef NDEBUG
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif

			CURLcode res = curl_easy_perform(curl);

			if (res == CURLE_OK) {
				long response_code = 0;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
				if (response_code == 200) {
					try {
						json payload = json::parse(response);
						if (!payload["data"].is_null()) {
							json &d = payload["data"];
							if (type == ZT_STATE_OBJECT_IDENTITY_SECRET) {
								std::string secret = OSUtils::jsonString(d["secret"],"");

								if (!secret.empty()) {
									ret = (int)secret.length();
									memcpy(data, secret.c_str(), ret);
								}
							}
							else if (type == ZT_STATE_OBJECT_IDENTITY_PUBLIC) {
								std::string pub = OSUtils::jsonString(d["public"],"");

								if (!pub.empty()) {
									ret = (int)pub.length();
									memcpy(data, pub.c_str(), ret);
								}
							}
						}
					}
					catch (...) {
						ret = -1;
					}
				}
			}

			curl_easy_cleanup(curl);
			curl = NULL;
			curl_slist_free_all(chunk);
			chunk = NULL;
		}
		return ret;
	}
#endif

	inline int nodeStateGetFunction(enum ZT_StateObjectType type,const uint64_t id[2],void *data,unsigned int maxlen)
	{
#if ZT_VAULT_SUPPORT
		if (_vaultEnabled && (type == ZT_STATE_OBJECT_IDENTITY_SECRET || type == ZT_STATE_OBJECT_IDENTITY_PUBLIC) ) {
			int retval = nodeVaultGetIdentity(type, data, maxlen);
			if (retval >= 0)
				return retval;

			// else continue file based lookup
		}
#endif
		char p[4096];
		switch(type) {
			case ZT_STATE_OBJECT_IDENTITY_PUBLIC:
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "identity.public",_homePath.c_str());
				break;
			case ZT_STATE_OBJECT_IDENTITY_SECRET:
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "identity.secret",_homePath.c_str());
				break;
			case ZT_STATE_OBJECT_PLANET:
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "planet",_homePath.c_str());
				break;
			case ZT_STATE_OBJECT_MOON:
				OSUtils::ztsnprintf(p,sizeof(p),"%s" ZT_PATH_SEPARATOR_S "moons.d" ZT_PATH_SEPARATOR_S "%.16llx.moon",_homePath.c_str(),(unsigned long long)id[0]);
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
#if ZT_VAULT_SUPPORT
			if (_vaultEnabled && (type == ZT_STATE_OBJECT_IDENTITY_SECRET || type == ZT_STATE_OBJECT_IDENTITY_PUBLIC)) {
				// If we've gotten here while Vault is enabled, Vault does not know the key and it's been
				// read from disk instead.
				//
				// We should put the value in Vault and remove the local file.
				if (nodeVaultPutIdentity(type, data, n)) {
					unlink(p);
				}
			}
#endif
			if (n >= 0)
				return n;
		}
		return -1;
	}

	inline int nodeWirePacketSendFunction(const int64_t localSocket,const struct sockaddr_storage *addr,const void *data,unsigned int len,unsigned int ttl)
	{
#ifdef ZT_TCP_FALLBACK_RELAY
		if(_allowTcpFallbackRelay) {
			if (addr->ss_family == AF_INET) {
				// TCP fallback tunnel support, currently IPv4 only
				if ((len >= 16)&&(reinterpret_cast<const InetAddress *>(addr)->ipScope() == InetAddress::IP_SCOPE_GLOBAL)) {
					// Engage TCP tunnel fallback if we haven't received anything valid from a global
					// IP address in ZT_TCP_FALLBACK_AFTER milliseconds. If we do start getting
					// valid direct traffic we'll stop using it and close the socket after a while.
					const int64_t now = OSUtils::now();
					if (((now - _lastDirectReceiveFromGlobal) > ZT_TCP_FALLBACK_AFTER)&&((now - _lastRestart) > ZT_TCP_FALLBACK_AFTER)) {
						if (_tcpFallbackTunnel) {
							bool flushNow = false;
							{
								Mutex::Lock _l(_tcpFallbackTunnel->writeq_m);
								if (_tcpFallbackTunnel->writeq.size() < (1024 * 64)) {
									if (_tcpFallbackTunnel->writeq.length() == 0) {
										_phy.setNotifyWritable(_tcpFallbackTunnel->sock,true);
										flushNow = true;
									}
									const unsigned long mlen = len + 7;
									_tcpFallbackTunnel->writeq.push_back((char)0x17);
									_tcpFallbackTunnel->writeq.push_back((char)0x03);
									_tcpFallbackTunnel->writeq.push_back((char)0x03); // fake TLS 1.2 header
									_tcpFallbackTunnel->writeq.push_back((char)((mlen >> 8) & 0xff));
									_tcpFallbackTunnel->writeq.push_back((char)(mlen & 0xff));
									_tcpFallbackTunnel->writeq.push_back((char)4); // IPv4
									_tcpFallbackTunnel->writeq.append(reinterpret_cast<const char *>(reinterpret_cast<const void *>(&(reinterpret_cast<const struct sockaddr_in *>(addr)->sin_addr.s_addr))),4);
									_tcpFallbackTunnel->writeq.append(reinterpret_cast<const char *>(reinterpret_cast<const void *>(&(reinterpret_cast<const struct sockaddr_in *>(addr)->sin_port))),2);
									_tcpFallbackTunnel->writeq.append((const char *)data,len);
								}
							}
							if (flushNow) {
								void *tmpptr = (void *)_tcpFallbackTunnel;
								phyOnTcpWritable(_tcpFallbackTunnel->sock,&tmpptr);
							}
						} else if (((now - _lastSendToGlobalV4) < ZT_TCP_FALLBACK_AFTER)&&((now - _lastSendToGlobalV4) > (ZT_PING_CHECK_INVERVAL / 2))) {
							const InetAddress addr(_fallbackRelayAddress);
							TcpConnection *tc = new TcpConnection();
							{
								Mutex::Lock _l(_tcpConnections_m);
								_tcpConnections.push_back(tc);
							}
							tc->type = TcpConnection::TCP_TUNNEL_OUTGOING;
							tc->remoteAddr = addr;
							tc->lastReceive = OSUtils::now();
							tc->parent = this;
							tc->sock = (PhySocket *)0; // set in connect handler
							tc->messageSize = 0;
							bool connected = false;
							_phy.tcpConnect(reinterpret_cast<const struct sockaddr *>(&addr),connected,(void *)tc,true);
						}
					}
					_lastSendToGlobalV4 = now;
				}
			}
		}
#endif // ZT_TCP_FALLBACK_RELAY

		// Even when relaying we still send via UDP. This way if UDP starts
		// working we can instantly "fail forward" to it and stop using TCP
		// proxy fallback, which is slow.

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
		if ((!n)||(!n->tap()))
			return;
		n->tap()->put(MAC(sourceMac),MAC(destMac),etherType,data,len);
	}

	inline int nodePathCheckFunction(uint64_t ztaddr,const int64_t localSocket,const struct sockaddr_storage *remoteAddr)
	{
		// Make sure we're not trying to do ZeroTier-over-ZeroTier
		{
			Mutex::Lock _l(_nets_m);
			for(std::map<uint64_t,NetworkState>::const_iterator n(_nets.begin());n!=_nets.end();++n) {
				if (n->second.tap()) {
					std::vector<InetAddress> ips(n->second.tap()->ips());
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

	inline int nodePathLookupFunction(uint64_t ztaddr, int family, struct sockaddr_storage* result)
	{
		const Hashtable< uint64_t, std::vector<InetAddress> >* lh = (const Hashtable< uint64_t, std::vector<InetAddress> > *)0;
		if (family < 0)
			lh = (_node->prng() & 1) ? &_v4Hints : &_v6Hints;
		else if (family == AF_INET)
			lh = &_v4Hints;
		else if (family == AF_INET6)
			lh = &_v6Hints;
		else return 0;
		const std::vector<InetAddress>* l = lh->get(ztaddr);
		if ((l) && (!l->empty())) {
			memcpy(result, &((*l)[(unsigned long)_node->prng() % l->size()]), sizeof(struct sockaddr_storage));
			return 1;
		}
		else return 0;
	}

	inline void tapFrameHandler(uint64_t nwid, const MAC& from, const MAC& to, unsigned int etherType, unsigned int vlanId, const void* data, unsigned int len)
	{
		_node->processVirtualNetworkFrame((void*)0, OSUtils::now(), nwid, from.toInt(), to.toInt(), etherType, vlanId, data, len, &_nextBackgroundTaskDeadline);
	}

	inline void onHttpRequestToServer(TcpConnection* tc)
	{
		char tmpn[4096];
		std::string data;
		std::string contentType("text/plain"); // default if not changed in handleRequest()
		unsigned int scode = 404;

		// Note that we check allowed IP ranges when HTTP connections are first detected in
		// phyOnTcpData(). If we made it here the source IP is okay.

		try {
			scode = handleControlPlaneHttpRequest(tc->remoteAddr, tc->parser.method, tc->url, tc->headers, tc->readq, data, contentType);
		}
		catch (std::exception& exc) {
			fprintf(stderr, "WARNING: unexpected exception processing control HTTP request: %s" ZT_EOL_S, exc.what());
			scode = 500;
		}
		catch (...) {
			fprintf(stderr, "WARNING: unexpected exception processing control HTTP request: unknown exception" ZT_EOL_S);
			scode = 500;
		}

		const char* scodestr;
		switch (scode) {
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

		OSUtils::ztsnprintf(tmpn, sizeof(tmpn), "HTTP/1.1 %.3u %s\r\nCache-Control: no-cache\r\nPragma: no-cache\r\nContent-Type: %s\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
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

		_phy.setNotifyWritable(tc->sock, true);
	}

	inline void onHttpResponseFromClient(TcpConnection* tc)
	{
		_phy.close(tc->sock);
	}

	bool shouldBindInterface(const char* ifname, const InetAddress& ifaddr)
	{
#if defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)
		if ((ifname[0] == 'l') && (ifname[1] == 'o')) return false; // loopback
		if ((ifname[0] == 'z') && (ifname[1] == 't')) return false; // sanity check: zt#
		if ((ifname[0] == 't') && (ifname[1] == 'u') && (ifname[2] == 'n')) return false; // tun# is probably an OpenVPN tunnel or similar
		if ((ifname[0] == 't') && (ifname[1] == 'a') && (ifname[2] == 'p')) return false; // tap# is probably an OpenVPN tunnel or similar
#endif

#ifdef __APPLE__
		if ((ifname[0] == 'f') && (ifname[1] == 'e') && (ifname[2] == 't') && (ifname[3] == 'h')) return false; // ... as is feth#
		if ((ifname[0] == 'l') && (ifname[1] == 'o')) return false; // loopback
		if ((ifname[0] == 'z') && (ifname[1] == 't')) return false; // sanity check: zt#
		if ((ifname[0] == 't') && (ifname[1] == 'u') && (ifname[2] == 'n')) return false; // tun# is probably an OpenVPN tunnel or similar
		if ((ifname[0] == 't') && (ifname[1] == 'a') && (ifname[2] == 'p')) return false; // tap# is probably an OpenVPN tunnel or similar
		if ((ifname[0] == 'u') && (ifname[1] == 't') && (ifname[2] == 'u') && (ifname[3] == 'n')) return false; // ... as is utun#
#endif

#ifdef __FreeBSD__
		if ((ifname[0] == 'l') && (ifname[1] == 'o')) return false; // loopback
		if ((ifname[0] == 'z') && (ifname[1] == 't')) return false; // sanity check: zt#
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
				if (n->second.tap()) {
					std::vector<InetAddress> ips(n->second.tap()->ips());
					for(std::vector<InetAddress>::const_iterator i(ips.begin());i!=ips.end();++i) {
						if (i->ipsEqual(ifaddr))
							return false;
					}
#ifdef _WIN32
					if (n->second.tap()->friendlyName() == ifname)
						return false;
#endif
				}
			}
		}

		return true;
	}

	unsigned int _getRandomPort()
	{
		unsigned int randp = 0;
		Utils::getSecureRandom(&randp,sizeof(randp));
		randp = 20000 + (randp % 45500);
		for(int i=0;;++i) {
			if (i > 1000) {
				return 0;
			} else if (++randp >= 65536) {
				randp = 20000;
			}
			if (_trialBind(randp))
				break;
		}
		return randp;
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
