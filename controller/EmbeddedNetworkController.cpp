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
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#include <algorithm>
#include <utility>
#include <stdexcept>
#include <set>
#include <map>

#include "../include/ZeroTierOne.h"
#include "../node/Constants.hpp"

#include "EmbeddedNetworkController.hpp"

#include "../node/Node.hpp"
#include "../node/Utils.hpp"
#include "../node/CertificateOfMembership.hpp"
#include "../node/NetworkConfig.hpp"
#include "../node/Dictionary.hpp"
#include "../node/InetAddress.hpp"
#include "../node/MAC.hpp"
#include "../node/Address.hpp"

using json = nlohmann::json;

// API version reported via JSON control plane
#define ZT_NETCONF_CONTROLLER_API_VERSION 3

// Number of requests to remember in member history
#define ZT_NETCONF_DB_MEMBER_HISTORY_LENGTH 16

// Min duration between requests for an address/nwid combo to prevent floods
#define ZT_NETCONF_MIN_REQUEST_PERIOD 1000

// Nodes are considered active if they've queried in less than this long
#define ZT_NETCONF_NODE_ACTIVE_THRESHOLD ((ZT_NETWORK_AUTOCONF_DELAY * 2) + 5000)

namespace ZeroTier {

// JSON blob I/O
static json _readJson(const std::string &path)
{
	std::string buf;
	if (OSUtils::readFile(path.c_str(),buf)) {
		try {
			return json::parse(buf);
		} catch ( ... ) {}
	}
	return json::object();
}
static bool _writeJson(const std::string &path,const json &obj)
{
	return OSUtils::writeFile(path.c_str(),obj.dump(2));
}

// Get JSON values as unsigned integers, strings, or booleans, doing type conversion if possible
static uint64_t _jI(const json &jv,const uint64_t dfl)
{
	if (jv.is_number()) {
		return (uint64_t)jv;
	} else if (jv.is_string()) {
		std::string s = jv;
		return Utils::strToU64(s.c_str());
	} else if (jv.is_boolean()) {
		return ((bool)jv ? 1ULL : 0ULL);
	}
	return dfl;
}
static bool _jB(const json &jv,const bool dfl)
{
	if (jv.is_boolean()) {
		return (bool)jv;
	} else if (jv.is_number()) {
		return ((uint64_t)jv > 0ULL);
	} else if (jv.is_string()) {
		std::string s = jv;
		if (s.length() > 0) {
			switch(s[0]) {
				case 't':
				case 'T':
				case '1':
					return true;
			}
		}
		return false;
	}
	return dfl;
}
static std::string _jS(const json &jv,const char *dfl)
{
	if (jv.is_string()) {
		return jv;
	} else if (jv.is_number()) {
		char tmp[64];
		Utils::snprintf(tmp,sizeof(tmp),"%llu",(uint64_t)jv);
		return tmp;
	} else if (jv.is_boolean()) {
		return ((bool)jv ? std::string("1") : std::string("0"));
	}
	return std::string((dfl) ? dfl : "");
}

static json _renderRule(ZT_VirtualNetworkRule &rule)
{
	char tmp[128];
	json r = json::object();
	r["not"] = ((rule.t & 0x80) != 0);
	switch((rule.t) & 0x7f) {
		case ZT_NETWORK_RULE_ACTION_DROP:
			r["type"] = "ACTION_DROP";
			break;
		case ZT_NETWORK_RULE_ACTION_ACCEPT:
			r["type"] = "ACTION_ACCEPT";
			break;
		case ZT_NETWORK_RULE_ACTION_TEE:
			r["type"] = "ACTION_TEE";
			r["address"] = Address(rule.v.fwd.address).toString();
			r["flags"] = (uint64_t)rule.v.fwd.flags;
			r["length"] = (uint64_t)rule.v.fwd.length;
			break;
		case ZT_NETWORK_RULE_ACTION_REDIRECT:
			r["type"] = "ACTION_REDIRECT";
			r["address"] = Address(rule.v.fwd.address).toString();
			r["flags"] = (uint64_t)rule.v.fwd.flags;
			r["length"] = (uint64_t)rule.v.fwd.length;
			break;
		case ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS:
			r["type"] = "MATCH_SOURCE_ZEROTIER_ADDRESS";
			r["zt"] = Address(rule.v.zt).toString();
			break;
		case ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS:
			r["type"] = "MATCH_DEST_ZEROTIER_ADDRESS";
			r["zt"] = Address(rule.v.zt).toString();
			break;
		case ZT_NETWORK_RULE_MATCH_VLAN_ID:
			r["type"] = "MATCH_VLAN_ID";
			r["vlanId"] = (uint64_t)rule.v.vlanId;
			break;
		case ZT_NETWORK_RULE_MATCH_VLAN_PCP:
			r["type"] = "MATCH_VLAN_PCP";
			r["vlanPcp"] = (uint64_t)rule.v.vlanPcp;
			break;
		case ZT_NETWORK_RULE_MATCH_VLAN_DEI:
			r["type"] = "MATCH_VLAN_DEI";
			r["vlanDei"] = (uint64_t)rule.v.vlanDei;
			break;
		case ZT_NETWORK_RULE_MATCH_ETHERTYPE:
			r["type"] = "MATCH_ETHERTYPE";
			r["etherType"] = (uint64_t)rule.v.etherType;
			break;
		case ZT_NETWORK_RULE_MATCH_MAC_SOURCE:
			r["type"] = "MATCH_MAC_SOURCE";
			Utils::snprintf(tmp,sizeof(tmp),"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",(unsigned int)rule.v.mac[0],(unsigned int)rule.v.mac[1],(unsigned int)rule.v.mac[2],(unsigned int)rule.v.mac[3],(unsigned int)rule.v.mac[4],(unsigned int)rule.v.mac[5]);
			r["mac"] = tmp;
			break;
		case ZT_NETWORK_RULE_MATCH_MAC_DEST:
			r["type"] = "MATCH_MAC_DEST";
			Utils::snprintf(tmp,sizeof(tmp),"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",(unsigned int)rule.v.mac[0],(unsigned int)rule.v.mac[1],(unsigned int)rule.v.mac[2],(unsigned int)rule.v.mac[3],(unsigned int)rule.v.mac[4],(unsigned int)rule.v.mac[5]);
			r["mac"] = tmp;
			break;
		case ZT_NETWORK_RULE_MATCH_IPV4_SOURCE:
			r["type"] = "MATCH_IPV4_SOURCE";
			r["ip"] = InetAddress(&(rule.v.ipv4.ip),4,(unsigned int)rule.v.ipv4.mask).toString();
			break;
		case ZT_NETWORK_RULE_MATCH_IPV4_DEST:
			r["type"] = "MATCH_IPV4_DEST";
			r["ip"] = InetAddress(&(rule.v.ipv4.ip),4,(unsigned int)rule.v.ipv4.mask).toString();
			break;
		case ZT_NETWORK_RULE_MATCH_IPV6_SOURCE:
			r["type"] = "MATCH_IPV6_SOURCE";
			r["ip"] = InetAddress(rule.v.ipv6.ip,16,(unsigned int)rule.v.ipv6.mask).toString();
			break;
		case ZT_NETWORK_RULE_MATCH_IPV6_DEST:
			r["type"] = "MATCH_IPV6_DEST";
			r["ip"] = InetAddress(rule.v.ipv6.ip,16,(unsigned int)rule.v.ipv6.mask).toString();
			break;
		case ZT_NETWORK_RULE_MATCH_IP_TOS:
			r["type"] = "MATCH_IP_TOS";
			r["ipTos"] = (uint64_t)rule.v.ipTos;
			break;
		case ZT_NETWORK_RULE_MATCH_IP_PROTOCOL:
			r["type"] = "MATCH_IP_PROTOCOL";
			r["ipProtocol"] = (uint64_t)rule.v.ipProtocol;
			break;
		case ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE:
			r["type"] = "MATCH_IP_SOURCE_PORT_RANGE";
			r["start"] = (uint64_t)rule.v.port[0];
			r["end"] = (uint64_t)rule.v.port[1];
			break;
		case ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE:
			r["type"] = "MATCH_IP_DEST_PORT_RANGE";
			r["start"] = (uint64_t)rule.v.port[0];
			r["end"] = (uint64_t)rule.v.port[1];
			break;
		case ZT_NETWORK_RULE_MATCH_CHARACTERISTICS:
			r["type"] = "MATCH_CHARACTERISTICS";
			Utils::snprintf(tmp,sizeof(tmp),"%.16llx",rule.v.characteristics[0]);
			r["mask"] = tmp;
			Utils::snprintf(tmp,sizeof(tmp),"%.16llx",rule.v.characteristics[1]);
			r["value"] = tmp;
			break;
		case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
			r["type"] = "MATCH_FRAME_SIZE_RANGE";
			r["start"] = (uint64_t)rule.v.frameSize[0];
			r["end"] = (uint64_t)rule.v.frameSize[1];
			break;
		case ZT_NETWORK_RULE_MATCH_TAGS_SAMENESS:
			r["type"] = "MATCH_TAGS_SAMENESS";
			r["id"] = (uint64_t)rule.v.tag.id;
			r["value"] = (uint64_t)rule.v.tag.value;
			break;
		case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND:
			r["type"] = "MATCH_TAGS_BITWISE_AND";
			r["id"] = (uint64_t)rule.v.tag.id;
			r["value"] = (uint64_t)rule.v.tag.value;
			break;
		case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR:
			r["type"] = "MATCH_TAGS_BITWISE_OR";
			r["id"] = (uint64_t)rule.v.tag.id;
			r["value"] = (uint64_t)rule.v.tag.value;
			break;
		case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR:
			r["type"] = "MATCH_TAGS_BITWISE_XOR";
			r["id"] = (uint64_t)rule.v.tag.id;
			r["value"] = (uint64_t)rule.v.tag.value;
			break;
	}
	return r;
}

static bool _parseRule(const json &r,ZT_VirtualNetworkRule &rule)
{
	if (r.is_object())
		return false;
	const std::string t(_jS(r["type"],""));
	memset(&rule,0,sizeof(ZT_VirtualNetworkRule));
	if (_jB(r["not"],false))
		rule.t = 0x80;
	else rule.t = 0x00;
	if (t == "ACTION_DROP") {
		rule.t |= ZT_NETWORK_RULE_ACTION_DROP;
		return true;
	} else if (t == "ACTION_ACCEPT") {
		rule.t |= ZT_NETWORK_RULE_ACTION_ACCEPT;
		return true;
	} else if (t == "ACTION_TEE") {
		rule.t |= ZT_NETWORK_RULE_ACTION_TEE;
		rule.v.fwd.address = Utils::hexStrToU64(_jS(r["address"],"0").c_str()) & 0xffffffffffULL;
		rule.v.fwd.flags = (uint32_t)(_jI(r["flags"],0ULL) & 0xffffffffULL);
		rule.v.fwd.length = (uint16_t)(_jI(r["length"],0ULL) & 0xffffULL);
		return true;
	} else if (t == "ACTION_REDIRECT") {
		rule.t |= ZT_NETWORK_RULE_ACTION_REDIRECT;
		rule.v.fwd.address = Utils::hexStrToU64(_jS(r["zt"],"0").c_str()) & 0xffffffffffULL;
		rule.v.fwd.flags = (uint32_t)(_jI(r["flags"],0ULL) & 0xffffffffULL);
		rule.v.fwd.length = (uint16_t)(_jI(r["length"],0ULL) & 0xffffULL);
		return true;
	} else if (t == "MATCH_SOURCE_ZEROTIER_ADDRESS") {
		rule.t |= ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS;
		rule.v.zt = Utils::hexStrToU64(_jS(r["zt"],"0").c_str()) & 0xffffffffffULL;
		return true;
	} else if (t == "MATCH_DEST_ZEROTIER_ADDRESS") {
		rule.t |= ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS;
		rule.v.zt = Utils::hexStrToU64(_jS(r["zt"],"0").c_str()) & 0xffffffffffULL;
		return true;
	} else if (t == "MATCH_VLAN_ID") {
		rule.t |= ZT_NETWORK_RULE_MATCH_VLAN_ID;
		rule.v.vlanId = (uint16_t)(_jI(r["vlanId"],0ULL) & 0xffffULL);
		return true;
	} else if (t == "MATCH_VLAN_PCP") {
		rule.t |= ZT_NETWORK_RULE_MATCH_VLAN_PCP;
		rule.v.vlanPcp = (uint8_t)(_jI(r["vlanPcp"],0ULL) & 0xffULL);
		return true;
	} else if (t == "MATCH_VLAN_DEI") {
		rule.t |= ZT_NETWORK_RULE_MATCH_VLAN_DEI;
		rule.v.vlanDei = (uint8_t)(_jI(r["vlanDei"],0ULL) & 0xffULL);
		return true;
	} else if (t == "MATCH_ETHERTYPE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_ETHERTYPE;
		rule.v.etherType = (uint16_t)(_jI(r["etherType"],0ULL) & 0xffffULL);
		return true;
	} else if (t == "MATCH_MAC_SOURCE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_MAC_SOURCE;
		const std::string mac(_jS(r["mac"],"0"));
		Utils::unhex(mac.c_str(),(unsigned int)mac.length(),rule.v.mac,6);
		return true;
	} else if (t == "MATCH_MAC_DEST") {
		rule.t |= ZT_NETWORK_RULE_MATCH_MAC_DEST;
		const std::string mac(_jS(r["mac"],"0"));
		Utils::unhex(mac.c_str(),(unsigned int)mac.length(),rule.v.mac,6);
		return true;
	} else if (t == "MATCH_IPV4_SOURCE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV4_SOURCE;
		InetAddress ip(_jS(r["ip"],"0.0.0.0"));
		rule.v.ipv4.ip = reinterpret_cast<struct sockaddr_in *>(&ip)->sin_addr.s_addr;
		rule.v.ipv4.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in *>(&ip)->sin_port) & 0xff;
		if (rule.v.ipv4.mask > 32) rule.v.ipv4.mask = 32;
		return true;
	} else if (t == "MATCH_IPV4_DEST") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV4_DEST;
		InetAddress ip(_jS(r["ip"],"0.0.0.0"));
		rule.v.ipv4.ip = reinterpret_cast<struct sockaddr_in *>(&ip)->sin_addr.s_addr;
		rule.v.ipv4.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in *>(&ip)->sin_port) & 0xff;
		if (rule.v.ipv4.mask > 32) rule.v.ipv4.mask = 32;
		return true;
	} else if (t == "MATCH_IPV6_SOURCE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV6_SOURCE;
		InetAddress ip(_jS(r["ip"],"::0"));
		memcpy(rule.v.ipv6.ip,reinterpret_cast<struct sockaddr_in6 *>(&ip)->sin6_addr.s6_addr,16);
		rule.v.ipv6.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in6 *>(&ip)->sin6_port) & 0xff;
		if (rule.v.ipv6.mask > 128) rule.v.ipv6.mask = 128;
		return true;
	} else if (t == "MATCH_IPV6_DEST") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV6_DEST;
		InetAddress ip(_jS(r["ip"],"::0"));
		memcpy(rule.v.ipv6.ip,reinterpret_cast<struct sockaddr_in6 *>(&ip)->sin6_addr.s6_addr,16);
		rule.v.ipv6.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in6 *>(&ip)->sin6_port) & 0xff;
		if (rule.v.ipv6.mask > 128) rule.v.ipv6.mask = 128;
		return true;
	} else if (t == "MATCH_IP_TOS") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IP_TOS;
		rule.v.ipTos = (uint8_t)(_jI(r["ipTos"],0ULL) & 0xffULL);
		return true;
	} else if (t == "MATCH_IP_PROTOCOL") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IP_PROTOCOL;
		rule.v.ipProtocol = (uint8_t)(_jI(r["ipProtocol"],0ULL) & 0xffULL);
		return true;
	} else if (t == "MATCH_IP_SOURCE_PORT_RANGE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE;
		rule.v.port[0] = (uint16_t)(_jI(r["start"],0ULL) & 0xffffULL);
		rule.v.port[1] = (uint16_t)(_jI(r["end"],(uint64_t)rule.v.port[0]) & 0xffffULL);
		return true;
	} else if (t == "MATCH_IP_DEST_PORT_RANGE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE;
		rule.v.port[0] = (uint16_t)(_jI(r["start"],0ULL) & 0xffffULL);
		rule.v.port[1] = (uint16_t)(_jI(r["end"],(uint64_t)rule.v.port[0]) & 0xffffULL);
		return true;
	} else if (t == "MATCH_CHARACTERISTICS") {
		rule.t |= ZT_NETWORK_RULE_MATCH_CHARACTERISTICS;
		if (r.count("mask")) {
			auto v = r["mask"];
			if (v.is_number()) {
				rule.v.characteristics[0] = v;
			} else {
				std::string tmp = v;
				rule.v.characteristics[0] = Utils::hexStrToU64(tmp.c_str());
			}
		}
		if (r.count("value")) {
			auto v = r["value"];
			if (v.is_number()) {
				rule.v.characteristics[1] = v;
			} else {
				std::string tmp = v;
				rule.v.characteristics[1] = Utils::hexStrToU64(tmp.c_str());
			}
		}
		return true;
	} else if (t == "MATCH_FRAME_SIZE_RANGE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE;
		rule.v.frameSize[0] = (uint16_t)(_jI(r["start"],0ULL) & 0xffffULL);
		rule.v.frameSize[1] = (uint16_t)(_jI(r["end"],(uint64_t)rule.v.frameSize[0]) & 0xffffULL);
		return true;
	} else if (t == "MATCH_TAGS_SAMENESS") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_SAMENESS;
		rule.v.tag.id = (uint32_t)(_jI(r["id"],0ULL) & 0xffffffffULL);
		rule.v.tag.value = (uint32_t)(_jI(r["value"],0ULL) & 0xffffffffULL);
		return true;
	} else if (t == "MATCH_TAGS_BITWISE_AND") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND;
		rule.v.tag.id = (uint32_t)(_jI(r["id"],0ULL) & 0xffffffffULL);
		rule.v.tag.value = (uint32_t)(_jI(r["value"],0ULL) & 0xffffffffULL);
		return true;
	} else if (t == "MATCH_TAGS_BITWISE_OR") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR;
		rule.v.tag.id = (uint32_t)(_jI(r["id"],0ULL) & 0xffffffffULL);
		rule.v.tag.value = (uint32_t)(_jI(r["value"],0ULL) & 0xffffffffULL);
		return true;
	} else if (t == "MATCH_TAGS_BITWISE_XOR") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR;
		rule.v.tag.id = (uint32_t)(_jI(r["id"],0ULL) & 0xffffffffULL);
		rule.v.tag.value = (uint32_t)(_jI(r["value"],0ULL) & 0xffffffffULL);
		return true;
	}
	return false;
}

EmbeddedNetworkController::EmbeddedNetworkController(Node *node,const char *dbPath) :
	_node(node),
	_path(dbPath),
	_daemonRun(true)
{
	OSUtils::mkdir(dbPath);
	OSUtils::lockDownFile(dbPath,true); // networks might contain auth tokens, etc., so restrict directory permissions
	_daemon = Thread::start(this);
}

EmbeddedNetworkController::~EmbeddedNetworkController()
{
}

void EmbeddedNetworkController::threadMain()
	throw()
{
	uint64_t lastUpdatedNetworkMemberCache = 0;
	while (_daemonRun) {
		// Every 60 seconds we rescan the filesystem for network members and rebuild our cache
		if ((OSUtils::now() - lastUpdatedNetworkMemberCache) >= 60000) {
			const std::vector<std::string> networks(OSUtils::listSubdirectories((_path + ZT_PATH_SEPARATOR_S + "network").c_str()));
			for(auto n=networks.begin();n!=networks.end();++n) {
				if (n->length() == 16) {
					const std::vector<std::string> members(OSUtils::listSubdirectories((*n + ZT_PATH_SEPARATOR_S + "member").c_str()));
					std::map<Address,nlohmann::json> newCache;
					for(auto m=members.begin();m!=members.end();++m) {
						if (m->length() == ZT_ADDRESS_LENGTH_HEX) {
							const Address maddr(*m);
							try {
								const json mj(_readJson((_path + ZT_PATH_SEPARATOR_S + "network" + ZT_PATH_SEPARATOR_S + *n + ZT_PATH_SEPARATOR_S + "member" + ZT_PATH_SEPARATOR_S + *m + ZT_PATH_SEPARATOR_S + "config.json")));
								if ((mj.is_object())&&(mj.size() > 0)) {
									newCache[maddr] = mj;
								}
							} catch ( ... ) {}
						}
					}
					{
						Mutex::Lock _l(_networkMemberCache_m);
						_networkMemberCache[Utils::hexStrToU64(n->c_str())] = newCache;
					}
				}
			}
			lastUpdatedNetworkMemberCache = OSUtils::now();
		}

		{	// Every 25ms we push up to 50 network refreshes, which amounts to a max of about 300-500kb/sec
			unsigned int count = 0;
			Mutex::Lock _l(_refreshQueue_m);
			while (_refreshQueue.size() > 0) {
				_Refresh &r = _refreshQueue.front();
				if (_node)
					_node->pushNetworkRefresh(r.dest,r.nwid,r.blacklistAddresses,r.blacklistThresholds,r.numBlacklistEntries);
				_refreshQueue.pop_front();
				if (++count >= 50)
					break;
			}
		}

		Thread::sleep(25);
	}
}

NetworkController::ResultCode EmbeddedNetworkController::doNetworkConfigRequest(const InetAddress &fromAddr,const Identity &signingId,const Identity &identity,uint64_t nwid,const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> &metaData,NetworkConfig &nc)
{
	if (((!signingId)||(!signingId.hasPrivate()))||(signingId.address().toInt() != (nwid >> 24))) {
		return NetworkController::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
	}

	const uint64_t now = OSUtils::now();

	// Check rate limit circuit breaker to prevent flooding
	{
		Mutex::Lock _l(_lastRequestTime_m);
		uint64_t &lrt = _lastRequestTime[std::pair<uint64_t,uint64_t>(identity.address().toInt(),nwid)];
		if ((now - lrt) <= ZT_NETCONF_MIN_REQUEST_PERIOD)
			return NetworkController::NETCONF_QUERY_IGNORE;
		lrt = now;
	}

	const json network(_readJson(_networkJP(nwid,false)));
	if (!network.size())
		return NetworkController::NETCONF_QUERY_OBJECT_NOT_FOUND;

	const std::string memberJP(_memberJP(nwid,identity.address(),true));
	json member(_readJson(memberJP));
	_initMember(member);

	{
		std::string haveIdStr(_jS(member["identity"],""));
		if (haveIdStr.length() > 0) {
			// If we already know this member's identity perform a full compare. This prevents
			// a "collision" from being able to auth onto our network in place of an already
			// known member.
			try {
				if (Identity(haveIdStr.c_str()) != identity)
					return NetworkController::NETCONF_QUERY_ACCESS_DENIED;
			} catch ( ... ) {
				return NetworkController::NETCONF_QUERY_ACCESS_DENIED;
			}
		} else {
			// If we do not yet know this member's identity, learn it.
			member["identity"] = identity.toString(false);
		}
	}

	// These are always the same, but make sure they are set
	member["id"] = identity.address().toString();
	member["address"] = member["id"];
	member["nwid"] = network["id"];

	// Determine whether and how member is authorized
	const char *authorizedBy = (const char *)0;
	if (!_jB(network["private"],true)) {
		authorizedBy = "networkIsPublic";
		// If member already has an authorized field, leave it alone. That way its state is
		// preserved if the user toggles the network back to private. Otherwise set it to
		// true by default for new members of public nets.
		if (!member.count("authorized")) {
			member["authorized"] = true;
			member["lastAuthorizedTime"] = now;
			member["lastAuthorizedBy"] = authorizedBy;
			member["lastModified"] = now;
			auto revj = member["revision"];
			member["revision"] = (revj.is_number() ? ((uint64_t)revj + 1ULL) : 1ULL);
		}
	} else if (_jB(member["authorized"],false)) {
		authorizedBy = "memberIsAuthorized";
	} else {
		char atok[256];
		if (metaData.get(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_AUTH_TOKEN,atok,sizeof(atok)) > 0) {
			atok[255] = (char)0; // not necessary but YDIFLO
			if (strlen(atok) > 0) { // extra sanity check since we never want to compare a null token on either side
				auto authTokens = network["authTokens"];
				if (authTokens.is_array()) {
					for(unsigned long i=0;i<authTokens.size();++i) {
						auto at = authTokens[i];
						if (at.is_object()) {
							const uint64_t expires = _jI(at["expires"],0ULL);
							std::string tok = _jS(at["token"],"");
							if ( ((expires == 0ULL)||(expires > now)) && (tok.length() > 0) && (tok == atok) ) {
								authorizedBy = "token";
								member["authorized"] = true; // tokens actually change member authorization state
								member["lastAuthorizedTime"] = now;
								member["lastAuthorizedBy"] = authorizedBy;
								member["lastModified"] = now;
								auto revj = member["revision"];
								member["revision"] = (revj.is_number() ? ((uint64_t)revj + 1ULL) : 1ULL);
								break;
							}
						}
					}
				}
			}
		}
	}

	// Log this request
	{
		json rlEntry = json::object();
		rlEntry["ts"] = now;
		rlEntry["authorized"] = (authorizedBy) ? true : false;
		rlEntry["authorizedBy"] = (authorizedBy) ? authorizedBy : "";
		rlEntry["clientMajorVersion"] = metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MAJOR_VERSION,0);
		rlEntry["clientMinorVersion"] = metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MINOR_VERSION,0);
		rlEntry["clientRevision"] = metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_REVISION,0);
		rlEntry["clientProtocolVersion"] = metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_PROTOCOL_VERSION,0);
		if (fromAddr)
			rlEntry["fromAddr"] = fromAddr.toString();

		json recentLog = json::array();
		recentLog.push_back(rlEntry);
		auto oldLog = member["recentLog"];
		if (oldLog.is_array()) {
			for(unsigned long i=0;i<oldLog.size();++i) {
				recentLog.push_back(oldLog[i]);
				if (recentLog.size() >= ZT_NETCONF_DB_MEMBER_HISTORY_LENGTH)
					break;
			}
		}
		member["recentLog"] = recentLog;
	}

	// If they are not authorized, STOP!
	if (!authorizedBy) {
		_writeJson(memberJP,member);
		return NetworkController::NETCONF_QUERY_ACCESS_DENIED;
	}

	// -------------------------------------------------------------------------
	// If we made it this far, they are authorized.
	// -------------------------------------------------------------------------

	_NetworkMemberInfo nmi;
	_getNetworkMemberInfo(now,nwid,nmi);

	// Compute credential TTL. This is the "moving window" for COM agreement and
	// the global TTL for Capability and Tag objects. (The same value is used
	// for both.) This is computed by reference to the last time we deauthorized
	// a member, since within the time period since this event any temporal
	// differences are not particularly relevant.
	uint64_t credentialTtl = ZT_NETWORKCONFIG_DEFAULT_MIN_CREDENTIAL_TTL;
	if (now > nmi.mostRecentDeauthTime)
		credentialTtl += (now - nmi.mostRecentDeauthTime);
	if (credentialTtl > ZT_NETWORKCONFIG_DEFAULT_MAX_CREDENTIAL_TTL)
		credentialTtl = ZT_NETWORKCONFIG_DEFAULT_MAX_CREDENTIAL_TTL;

	nc.networkId = nwid;
	nc.type = _jB(network["private"],true) ? ZT_NETWORK_TYPE_PRIVATE : ZT_NETWORK_TYPE_PUBLIC;
	nc.timestamp = now;
	nc.credentialTimeToLive = credentialTtl;
	nc.revision = _jI(network["revision"],0ULL);
	nc.issuedTo = identity.address();
	if (_jB(network["enableBroadcast"],true)) nc.flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_BROADCAST;
	if (_jB(network["allowPassiveBridging"],false)) nc.flags |= ZT_NETWORKCONFIG_FLAG_ALLOW_PASSIVE_BRIDGING;
	Utils::scopy(nc.name,sizeof(nc.name),_jS(network["name"],"").c_str());
	nc.multicastLimit = (unsigned int)_jI(network["multicastLimit"],32ULL);

	for(std::set<Address>::const_iterator ab(nmi.activeBridges.begin());ab!=nmi.activeBridges.end();++ab)
		nc.addSpecialist(*ab,ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE);

	const json &v4AssignMode = network["v4AssignMode"];
	const json &v6AssignMode = network["v6AssignMode"];
	const json &ipAssignmentPools = network["ipAssignmentPools"];
	const json &routes = network["routes"];
	const json &rules = network["rules"];
	const json &capabilities = network["capabilities"];
	const json &memberCapabilities = member["capabilities"];
	const json &memberTags = member["tags"];

	if (rules.is_array()) {
		for(unsigned long i=0;i<rules.size();++i) {
			if (nc.ruleCount >= ZT_MAX_NETWORK_RULES)
				break;
			if (_parseRule(rules[i],nc.rules[nc.ruleCount]))
				++nc.ruleCount;
		}
	}

	if ((memberCapabilities.is_array())&&(memberCapabilities.size() > 0)&&(capabilities.is_array())) {
		std::map< uint64_t,const json * > capsById;
		for(unsigned long i=0;i<capabilities.size();++i) {
			const json &cap = capabilities[i];
			if (cap.is_object())
				capsById[_jI(cap["id"],0ULL) & 0xffffffffULL] = &cap;
		}

		for(unsigned long i=0;i<memberCapabilities.size();++i) {
			const uint64_t capId = _jI(memberCapabilities[i],0ULL) & 0xffffffffULL;
			const json *cap = capsById[capId];
			if ((cap->is_object())&&(cap->size() > 0)) {
				ZT_VirtualNetworkRule capr[ZT_MAX_CAPABILITY_RULES];
				unsigned int caprc = 0;
				auto caprj = (*cap)["rules"];
				if ((caprj.is_array())&&(caprj.size() > 0)) {
					for(unsigned long j=0;j<caprj.size();++j) {
						if (caprc >= ZT_MAX_CAPABILITY_RULES)
							break;
						if (_parseRule(caprj[j],capr[caprc]))
							++caprc;
					}
				}
				nc.capabilities[nc.capabilityCount] = Capability((uint32_t)capId,nwid,now,1,capr,caprc);
				if (nc.capabilities[nc.capabilityCount].sign(signingId,identity.address()))
					++nc.capabilityCount;
				if (nc.capabilityCount >= ZT_MAX_NETWORK_CAPABILITIES)
					break;
			}
		}
	}

	if (memberTags.is_array()) {
		std::map< uint32_t,uint32_t > tagsById;
		for(unsigned long i=0;i<memberTags.size();++i) {
			auto t = memberTags[i];
			if ((t.is_array())&&(t.size() == 2))
				tagsById[(uint32_t)(_jI(t[0],0ULL) & 0xffffffffULL)] = (uint32_t)(_jI(t[1],0ULL) & 0xffffffffULL);
		}
		for(std::map< uint32_t,uint32_t >::const_iterator t(tagsById.begin());t!=tagsById.end();++t) {
			if (nc.tagCount >= ZT_MAX_NETWORK_TAGS)
				break;
			nc.tags[nc.tagCount] = Tag(nwid,now,identity.address(),t->first,t->second);
			if (nc.tags[nc.tagCount].sign(signingId))
				++nc.tagCount;
		}
	}

	if (routes.is_array()) {
		for(unsigned long i=0;i<routes.size();++i) {
			if (nc.routeCount >= ZT_MAX_NETWORK_ROUTES)
				break;
			auto route = routes[i];
			InetAddress t(_jS(route["target"],""));
			InetAddress v(_jS(route["via"],""));
			if ((t)&&(v)&&(t.ss_family == v.ss_family)) {
				ZT_VirtualNetworkRoute *r = &(nc.routes[nc.routeCount]);
				*(reinterpret_cast<InetAddress *>(&(r->target))) = t;
				*(reinterpret_cast<InetAddress *>(&(r->via))) = v;
				++nc.routeCount;
			}
		}
	}

	if (v6AssignMode.is_object()) {
		if ((_jB(v6AssignMode["rfc4193"],false))&&(nc.staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)) {
			nc.staticIps[nc.staticIpCount++] = InetAddress::makeIpv6rfc4193(nwid,identity.address().toInt());
			nc.flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION;
		}
		if ((_jB(v6AssignMode["6plane"],false))&&(nc.staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)) {
			nc.staticIps[nc.staticIpCount++] = InetAddress::makeIpv66plane(nwid,identity.address().toInt());
			nc.flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION;
		}
	}

	bool haveManagedIpv4AutoAssignment = false;
	bool haveManagedIpv6AutoAssignment = false; // "special" NDP-emulated address types do not count
	json ipAssignments = member["ipAssignments"];
	if (ipAssignments.is_array()) {
		for(unsigned long i=0;i<ipAssignments.size();++i) {
			std::string ips = ipAssignments[i];
			InetAddress ip(ips);

			// IP assignments are only pushed if there is a corresponding local route. We also now get the netmask bits from
			// this route, ignoring the netmask bits field of the assigned IP itself. Using that was worthless and a source
			// of user error / poor UX.
			int routedNetmaskBits = 0;
			for(unsigned int rk=0;rk<nc.routeCount;++rk) {
				if ( (!nc.routes[rk].via.ss_family) && (reinterpret_cast<const InetAddress *>(&(nc.routes[rk].target))->containsAddress(ip)) )
					routedNetmaskBits = reinterpret_cast<const InetAddress *>(&(nc.routes[rk].target))->netmaskBits();
			}

			if (routedNetmaskBits > 0) {
				if (nc.staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES) {
					ip.setPort(routedNetmaskBits);
					nc.staticIps[nc.staticIpCount++] = ip;
				}
				if (ip.ss_family == AF_INET)
					haveManagedIpv4AutoAssignment = true;
				else if (ip.ss_family == AF_INET6)
					haveManagedIpv6AutoAssignment = true;
			}
		}
	} else {
		ipAssignments = json::array();
	}

	if ( (ipAssignmentPools.is_array()) && ((v6AssignMode.is_object())&&(_jB(v6AssignMode["zt"],false))) && (!haveManagedIpv6AutoAssignment) && (!_jB(member["noAutoAssignIps"],false)) ) {
		for(unsigned long p=0;((p<ipAssignmentPools.size())&&(!haveManagedIpv6AutoAssignment));++p) {
			auto pool = ipAssignmentPools[p];
			if (pool.is_object()) {
				InetAddress ipRangeStart(_jS(pool["ipRangeStart"],""));
				InetAddress ipRangeEnd(_jS(pool["ipRangeEnd"],""));
				if ( (ipRangeStart.ss_family == AF_INET6) && (ipRangeEnd.ss_family == AF_INET6) ) {
					uint64_t s[2],e[2],x[2],xx[2];
					memcpy(s,ipRangeStart.rawIpData(),16);
					memcpy(e,ipRangeEnd.rawIpData(),16);
					s[0] = Utils::ntoh(s[0]);
					s[1] = Utils::ntoh(s[1]);
					e[0] = Utils::ntoh(e[0]);
					e[1] = Utils::ntoh(e[1]);
					x[0] = s[0];
					x[1] = s[1];

					for(unsigned int trialCount=0;trialCount<1000;++trialCount) {
						if ((trialCount == 0)&&(e[1] > s[1])&&((e[1] - s[1]) >= 0xffffffffffULL)) {
							// First see if we can just cram a ZeroTier ID into the higher 64 bits. If so do that.
							xx[0] = Utils::hton(x[0]);
							xx[1] = Utils::hton(x[1] + identity.address().toInt());
						} else {
							// Otherwise pick random addresses -- this technically doesn't explore the whole range if the lower 64 bit range is >= 1 but that won't matter since that would be huge anyway
							Utils::getSecureRandom((void *)xx,16);
							if ((e[0] > s[0]))
								xx[0] %= (e[0] - s[0]);
							else xx[0] = 0;
							if ((e[1] > s[1]))
								xx[1] %= (e[1] - s[1]);
							else xx[1] = 0;
							xx[0] = Utils::hton(x[0] + xx[0]);
							xx[1] = Utils::hton(x[1] + xx[1]);
						}

						InetAddress ip6((const void *)xx,16,0);

						// Check if this IP is within a local-to-Ethernet routed network
						int routedNetmaskBits = 0;
						for(unsigned int rk=0;rk<nc.routeCount;++rk) {
							if ( (!nc.routes[rk].via.ss_family) && (nc.routes[rk].target.ss_family == AF_INET6) && (reinterpret_cast<const InetAddress *>(&(nc.routes[rk].target))->containsAddress(ip6)) )
								routedNetmaskBits = reinterpret_cast<const InetAddress *>(&(nc.routes[rk].target))->netmaskBits();
						}

						// If it's routed, then try to claim and assign it and if successful end loop
						if ((routedNetmaskBits > 0)&&(!nmi.allocatedIps.count(ip6))) {
							ipAssignments.push_back(ip6.toIpString());
							member["ipAssignments"] = ipAssignments;
							ip6.setPort((unsigned int)routedNetmaskBits);
							if (nc.staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)
								nc.staticIps[nc.staticIpCount++] = ip6;
							haveManagedIpv4AutoAssignment = true;
							break;
						}
					}
				}
			}
		}
	}

	if ( (ipAssignmentPools.is_array()) && ((v4AssignMode.is_object())&&(_jB(v4AssignMode["zt"],false))) && (!haveManagedIpv4AutoAssignment) && (!_jB(member["noAutoAssignIps"],false)) ) {
		for(unsigned long p=0;((p<ipAssignmentPools.size())&&(!haveManagedIpv4AutoAssignment));++p) {
			auto pool = ipAssignmentPools[p];
			if (pool.is_object()) {
				InetAddress ipRangeStart(_jS(pool["ipRangeStart"],""));
				InetAddress ipRangeEnd(_jS(pool["ipRangeEnd"],""));
				if ( (ipRangeStart.ss_family == AF_INET) && (ipRangeEnd.ss_family == AF_INET) ) {
					uint32_t ipRangeStart = Utils::ntoh((uint32_t)(reinterpret_cast<struct sockaddr_in *>(&ipRangeStart)->sin_addr.s_addr));
					uint32_t ipRangeEnd = Utils::ntoh((uint32_t)(reinterpret_cast<struct sockaddr_in *>(&ipRangeEnd)->sin_addr.s_addr));
					if ((ipRangeEnd <= ipRangeStart)||(ipRangeStart == 0))
						continue;
					uint32_t ipRangeLen = ipRangeEnd - ipRangeStart;

					// Start with the LSB of the member's address
					uint32_t ipTrialCounter = (uint32_t)(identity.address().toInt() & 0xffffffff);

					for(uint32_t k=ipRangeStart,trialCount=0;(k<=ipRangeEnd)&&(trialCount < 1000);++k,++trialCount) {
						uint32_t ip = (ipRangeLen > 0) ? (ipRangeStart + (ipTrialCounter % ipRangeLen)) : ipRangeStart;
						++ipTrialCounter;
						if ((ip & 0x000000ff) == 0x000000ff)
							continue; // don't allow addresses that end in .255

						// Check if this IP is within a local-to-Ethernet routed network
						int routedNetmaskBits = 0;
						for(unsigned int rk=0;rk<nc.routeCount;++rk) {
							if ((!nc.routes[rk].via.ss_family)&&(nc.routes[rk].target.ss_family == AF_INET)) {
								uint32_t targetIp = Utils::ntoh((uint32_t)(reinterpret_cast<const struct sockaddr_in *>(&(nc.routes[rk].target))->sin_addr.s_addr));
								int targetBits = Utils::ntoh((uint16_t)(reinterpret_cast<const struct sockaddr_in *>(&(nc.routes[rk].target))->sin_port));
								if ((ip & (0xffffffff << (32 - targetBits))) == targetIp) {
									routedNetmaskBits = targetBits;
									break;
								}
							}
						}

						InetAddress ip4(Utils::hton(ip),0);

						// If it's routed, then try to claim and assign it and if successful end loop
						if ((routedNetmaskBits > 0)&&(!nmi.allocatedIps.count(ip4))) {
							ipAssignments.push_back(ip4.toIpString());
							member["ipAssignments"] = ipAssignments;
							if (nc.staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES) {
								struct sockaddr_in *const v4ip = reinterpret_cast<struct sockaddr_in *>(&(nc.staticIps[nc.staticIpCount++]));
								v4ip->sin_family = AF_INET;
								v4ip->sin_port = Utils::hton((uint16_t)routedNetmaskBits);
								v4ip->sin_addr.s_addr = Utils::hton(ip);
							}
							haveManagedIpv4AutoAssignment = true;
							break;
						}
					}
				}
			}
		}
	}

	if (_jB(network["private"],true)) {
		CertificateOfMembership com(now,credentialTtl,nwid,identity.address());
		if (com.sign(signingId)) {
			nc.com = com;
		} else {
			return NETCONF_QUERY_INTERNAL_SERVER_ERROR;
		}
	}

	_writeJson(memberJP,member);
	return NetworkController::NETCONF_QUERY_OK;
}

unsigned int EmbeddedNetworkController::handleControlPlaneHttpGET(
	const std::vector<std::string> &path,
	const std::map<std::string,std::string> &urlArgs,
	const std::map<std::string,std::string> &headers,
	const std::string &body,
	std::string &responseBody,
	std::string &responseContentType)
{
	if ((path.size() > 0)&&(path[0] == "network")) {

		if ((path.size() >= 2)&&(path[1].length() == 16)) {
			const uint64_t nwid = Utils::hexStrToU64(path[1].c_str());
			char nwids[24];
			Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);

			json network(_readJson(_networkJP(nwid,false)));
			if (!network.size())
				return 404;

			if (path.size() >= 3) {

				if (path[2] == "member") {

					if (path.size() >= 4) {
						const uint64_t address = Utils::hexStrToU64(path[3].c_str());

						json member(_readJson(_memberJP(nwid,Address(address),false)));
						if (!member.size())
							return 404;

						char addrs[24];
						Utils::snprintf(addrs,sizeof(addrs),"%.10llx",address);

						// Add non-persisted fields
						member["clock"] = OSUtils::now();

						responseBody = member.dump(2);
						responseContentType = "application/json";

						return 200;
					} else {

						responseBody = "{";
						std::vector<std::string> members(OSUtils::listSubdirectories((_networkBP(nwid,false) + ZT_PATH_SEPARATOR_S + "member").c_str()));
						for(std::vector<std::string>::iterator i(members.begin());i!=members.end();++i) {
							if (i->length() == ZT_ADDRESS_LENGTH_HEX) {
								json member(_readJson(_memberJP(nwid,Address(Utils::hexStrToU64(i->c_str())),false)));
								if (member.size()) {
									responseBody.append((responseBody.length() == 1) ? "\"" : ",\"");
									responseBody.append(*i);
									responseBody.append("\":");
									responseBody.append(_jS(member["revision"],"0"));
								}
							}
						}
						responseBody.push_back('}');
						responseContentType = "application/json";

						return 200;
					}

				} else if ((path[2] == "active")&&(path.size() == 3)) {

					responseBody = "{";
					std::vector<std::string> members(OSUtils::listSubdirectories((_networkBP(nwid,false) + ZT_PATH_SEPARATOR_S + "member").c_str()));
					const uint64_t threshold = OSUtils::now() - ZT_NETCONF_NODE_ACTIVE_THRESHOLD;
					for(std::vector<std::string>::iterator i(members.begin());i!=members.end();++i) {
						if (i->length() == ZT_ADDRESS_LENGTH_HEX) {
							json member(_readJson(_memberJP(nwid,Address(Utils::hexStrToU64(i->c_str())),false)));
							if (member.size()) {
								auto recentLog = member["recentLog"];
								if ((recentLog.is_array())&&(recentLog.size() > 0)) {
									auto mostRecentLog = recentLog[0];
									if ((mostRecentLog.is_object())&&(_jI(mostRecentLog["ts"],0ULL) >= threshold)) {
										responseBody.append((responseBody.length() == 1) ? "\"" : ",\"");
										responseBody.append(*i);
										responseBody.append("\":");
										responseBody.append(mostRecentLog.dump());
									}
								}
							}
						}
					}
					responseBody.push_back('}');
					responseContentType = "application/json";
					return 200;

				} else if ((path[2] == "test")&&(path.size() >= 4)) {

					Mutex::Lock _l(_circuitTests_m);
					std::map< uint64_t,_CircuitTestEntry >::iterator cte(_circuitTests.find(Utils::hexStrToU64(path[3].c_str())));
					if ((cte != _circuitTests.end())&&(cte->second.test)) {

						responseBody = "[";
						responseBody.append(cte->second.jsonResults);
						responseBody.push_back(']');
						responseContentType = "application/json";

						return 200;

					} // else 404

				} // else 404

			} else {

				const uint64_t now = OSUtils::now();
				_NetworkMemberInfo nmi;
				_getNetworkMemberInfo(now,nwid,nmi);
				_addNetworkNonPersistedFields(network,now,nmi);
				responseBody = network.dump(2);
				responseContentType = "application/json";
				return 200;

			}
		} else if (path.size() == 1) {

			responseBody = "[";
			std::vector<std::string> networks(OSUtils::listSubdirectories((_path + ZT_PATH_SEPARATOR_S + "network").c_str()));
			for(auto i(networks.begin());i!=networks.end();++i) {
				if (i->length() == 16) {
					responseBody.append((responseBody.length() == 1) ? "\"" : ",\"");
					responseBody.append(*i);
					responseBody.append("\"");
				}
			}
			responseBody.push_back(']');
			responseContentType = "application/json";
			return 200;

		} // else 404

	} else {

		char tmp[4096];
		Utils::snprintf(tmp,sizeof(tmp),"{\n\t\"controller\": true,\n\t\"apiVersion\": %d,\n\t\"clock\": %llu\n}\n",ZT_NETCONF_CONTROLLER_API_VERSION,(unsigned long long)OSUtils::now());
		responseBody = tmp;
		responseContentType = "application/json";
		return 200;

	}

	return 404;
}

unsigned int EmbeddedNetworkController::handleControlPlaneHttpPOST(
	const std::vector<std::string> &path,
	const std::map<std::string,std::string> &urlArgs,
	const std::map<std::string,std::string> &headers,
	const std::string &body,
	std::string &responseBody,
	std::string &responseContentType)
{
	if (path.empty())
		return 404;

	json b;
	try {
		b = json::parse(body);
		if (!b.is_object())
			return 400;
	} catch ( ... ) {
		return 400;
	}
	const uint64_t now = OSUtils::now();

	if (path[0] == "network") {

		if ((path.size() >= 2)&&(path[1].length() == 16)) {
			uint64_t nwid = Utils::hexStrToU64(path[1].c_str());
			char nwids[24];
			Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);

			if (path.size() >= 3) {
				json network(_readJson(_networkJP(nwid,false)));
				if (!network.size())
					return 404;

				if ((path.size() == 4)&&(path[2] == "member")&&(path[3].length() == 10)) {
					uint64_t address = Utils::hexStrToU64(path[3].c_str());
					char addrs[24];
					Utils::snprintf(addrs,sizeof(addrs),"%.10llx",(unsigned long long)address);

					json member(_readJson(_memberJP(nwid,Address(address),true)));
					_initMember(member);

					try {
						if (b.count("activeBridge")) member["activeBridge"] = _jB(b["activeBridge"],false);
						if (b.count("noAutoAssignIps")) member["noAutoAssignIps"] = _jB(b["noAutoAssignIps"],false);
						if ((b.count("identity"))&&(!member.count("identity"))) member["identity"] = _jS(b["identity"],""); // allow identity to be populated only if not already known

						if (b.count("authorized")) {
							const bool newAuth = _jB(b["authorized"],false);
							const bool oldAuth = _jB(member["authorized"],false);
							if (newAuth != oldAuth) {
								if (newAuth) {
									member["authorized"] = true;
									member["lastAuthorizedTime"] = now;
									member["lastAuthorizedBy"] = "user";
								} else {
									member["authorized"] = false;
									member["lastDeauthorizedTime"] = now;
								}
							}
						}

						if (b.count("ipAssignments")) {
							auto ipa = b["ipAssignments"];
							if (ipa.is_array()) {
								json mipa(json::array());
								for(unsigned long i=0;i<ipa.size();++i) {
									std::string ips = ipa[i];
									InetAddress ip(ips);
									if ((ip.ss_family == AF_INET)||(ip.ss_family == AF_INET6)) {
										mipa.push_back(ip.toIpString());
									}
								}
								member["ipAssignments"] = mipa;
							}
						}

						if (b.count("tags")) {
							auto tags = b["tags"];
							if (tags.is_array()) {
								std::map<uint64_t,uint64_t> mtags;
								for(unsigned long i=0;i<tags.size();++i) {
									auto tag = tags[i];
									if ((tag.is_array())&&(tag.size() == 2))
										mtags[_jI(tag[0],0ULL) & 0xffffffffULL] = _jI(tag[1],0ULL) & 0xffffffffULL;
								}
								json mtagsa = json::array();
								for(std::map<uint64_t,uint64_t>::iterator t(mtags.begin());t!=mtags.end();++t) {
									json ta = json::array();
									ta.push_back(t->first);
									ta.push_back(t->second);
									mtagsa.push_back(ta);
								}
								member["tags"] = mtagsa;
							}
						}

						if (b.count("capabilities")) {
							auto capabilities = b["capabilities"];
							if (capabilities.is_array()) {
								json mcaps = json::array();
								for(unsigned long i=0;i<capabilities.size();++i) {
									mcaps.push_back(_jI(capabilities[i],0ULL));
								}
								std::sort(mcaps.begin(),mcaps.end());
								mcaps.erase(std::unique(mcaps.begin(),mcaps.end()),mcaps.end());
								member["capabilities"] = mcaps;
							}
						}
					} catch ( ... ) {
						return 400;
					}

					member["id"] = addrs;
					member["address"] = addrs; // legacy
					member["nwid"] = nwids;
					member["lastModified"] = now;
					auto revj = member["revision"];
					member["revision"] = (revj.is_number() ? ((uint64_t)revj + 1ULL) : 1ULL);

					_writeJson(_memberJP(nwid,Address(address),true).c_str(),member);

					{
						Mutex::Lock _l(_networkMemberCache_m);
						_networkMemberCache[nwid][Address(address)] = member;
					}

					{
						Mutex::Lock _l(_refreshQueue_m);
						_refreshQueue.push_back(_Refresh());
						_Refresh &r = _refreshQueue.back();
						r.dest = Address(address);
						r.nwid = nwid;
						r.numBlacklistEntries = 0;
					}

					// Add non-persisted fields
					member["clock"] = now;

					responseBody = member.dump(2);
					responseContentType = "application/json";
					return 200;
				} else if ((path.size() == 3)&&(path[2] == "test")) {

					Mutex::Lock _l(_circuitTests_m);

					ZT_CircuitTest *test = (ZT_CircuitTest *)malloc(sizeof(ZT_CircuitTest));
					memset(test,0,sizeof(ZT_CircuitTest));

					Utils::getSecureRandom(&(test->testId),sizeof(test->testId));
					test->credentialNetworkId = nwid;
					test->ptr = (void *)this;
					json hops = b["hops"];
					if (hops.is_array()) {
						for(unsigned long i=0;i<hops.size();++i) {
							auto hops2 = hops[i];
							if (hops2.is_array()) {
								for(unsigned long j=0;j<hops2.size();++j) {
									std::string s = hops2[j];
									test->hops[test->hopCount].addresses[test->hops[test->hopCount].breadth++] = Utils::hexStrToU64(s.c_str()) & 0xffffffffffULL;
								}
							} else if (hops2.is_string()) {
								std::string s = hops2;
								test->hops[test->hopCount].addresses[test->hops[test->hopCount].breadth++] = Utils::hexStrToU64(s.c_str()) & 0xffffffffffULL;
							}
						}
					}
					test->reportAtEveryHop = (_jB(b["reportAtEveryHop"],true) ? 1 : 0);

					if (!test->hopCount) {
						::free((void *)test);
						return 400;
					}

					test->timestamp = OSUtils::now();

					_CircuitTestEntry &te = _circuitTests[test->testId];
					te.test = test;
					te.jsonResults = "";

					if (_node)
						_node->circuitTestBegin(test,&(EmbeddedNetworkController::_circuitTestCallback));
					else return 500;

					char json[1024];
					Utils::snprintf(json,sizeof(json),"{\"testId\":\"%.16llx\"}",test->testId);
					responseBody = json;
					responseContentType = "application/json";
					return 200;

				} // else 404

			} else {
				// POST to network ID

				// Magic ID ending with ______ picks a random unused network ID
				if (path[1].substr(10) == "______") {
					nwid = 0;
					uint64_t nwidPrefix = (Utils::hexStrToU64(path[1].substr(0,10).c_str()) << 24) & 0xffffffffff000000ULL;
					uint64_t nwidPostfix = 0;
					for(unsigned long k=0;k<100000;++k) { // sanity limit on trials
						Utils::getSecureRandom(&nwidPostfix,sizeof(nwidPostfix));
						uint64_t tryNwid = nwidPrefix | (nwidPostfix & 0xffffffULL);
						if ((tryNwid & 0xffffffULL) == 0ULL) tryNwid |= 1ULL;
						Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)tryNwid);
						if (!OSUtils::fileExists(_networkJP(tryNwid,false).c_str())) {
							nwid = tryNwid;
							break;
						}
					}
					if (!nwid)
						return 503;
				}

				json network(_readJson(_networkJP(nwid,true)));
				_initNetwork(network);

				try {
					if (b.count("name")) network["name"] = _jS(b["name"],"");
					if (b.count("private")) network["private"] = _jB(b["private"],true);
					if (b.count("enableBroadcast")) network["enableBroadcast"] = _jB(b["enableBroadcast"],false);
					if (b.count("allowPassiveBridging")) network["allowPassiveBridging"] = _jB(b["allowPassiveBridging"],false);
					if (b.count("multicastLimit")) network["multicastLimit"] = _jI(b["multicastLimit"],32ULL);

					if (b.count("v4AssignMode")) {
						auto nv4m = network["v4AssignMode"];
						if (!nv4m.is_object()) nv4m = json::object();
						if (b["v4AssignMode"].is_string()) { // backward compatibility
							nv4m["zt"] = (_jS(b["v4AssignMode"],"") == "zt");
						} else if (b["v4AssignMode"].is_object()) {
							auto v4m = b["v4AssignMode"];
							if (v4m.count("zt")) nv4m["zt"] = _jB(v4m["zt"],false);
						}
						if (!nv4m.count("zt")) nv4m["zt"] = false;
					}

					if (b.count("v6AssignMode")) {
						auto nv6m = network["v6AssignMode"];
						if (!nv6m.is_object()) nv6m = json::object();
						if (b["v6AssignMode"].is_string()) { // backward compatibility
							std::vector<std::string> v6m(Utils::split(_jS(b["v6AssignMode"],"").c_str(),",","",""));
							std::sort(v6m.begin(),v6m.end());
							v6m.erase(std::unique(v6m.begin(),v6m.end()),v6m.end());
							for(std::vector<std::string>::iterator i(v6m.begin());i!=v6m.end();++i) {
								if (*i == "rfc4193")
									nv6m["rfc4193"] = true;
								else if (*i == "zt")
									nv6m["zt"] = true;
								else if (*i == "6plane")
									nv6m["6plane"] = true;
							}
						} else if (b["v6AssignMode"].is_object()) {
							auto v6m = b["v6AssignMode"];
							if (v6m.count("rfc4193")) nv6m["rfc4193"] = _jB(v6m["rfc4193"],false);
							if (v6m.count("zt")) nv6m["rfc4193"] = _jB(v6m["zt"],false);
							if (v6m.count("6plane")) nv6m["rfc4193"] = _jB(v6m["6plane"],false);
						}
						if (!nv6m.count("rfc4193")) nv6m["rfc4193"] = false;
						if (!nv6m.count("zt")) nv6m["zt"] = false;
						if (!nv6m.count("6plane")) nv6m["6plane"] = false;
					}

					if (b.count("routes")) {
						auto rts = b["routes"];
						if (rts.is_array()) {
							for(unsigned long i=0;i<rts.size();++i) {
								auto rt = rts[i];
								if ((rt.is_object())&&(rt.count("target"))&&(rt.count("via"))) {
									InetAddress t(_jS(rt["target"],""));
									InetAddress v(_jS(rt["via"],""));
									if ( ((t.ss_family == AF_INET)||(t.ss_family == AF_INET6)) && (t.ss_family == v.ss_family) && (t.netmaskBitsValid()) ) {
										auto nrts = network["routes"];
										if (!nrts.is_array()) nrts = json::array();
										json tmp;
										tmp["target"] = t.toString();
										tmp["via"] = v.toIpString();
										nrts.push_back(tmp);
									}
								}
							}
						}
					}

					if (b.count("ipAssignmentPools")) {
						auto ipp = b["ipAssignmentPools"];
						if (ipp.is_array()) {
							for(unsigned long i=0;i<ipp.size();++i) {
								auto ip = ipp[i];
								if ((ip.is_object())&&(ip.count("ipRangeStart"))&&(ip.count("ipRangeEnd"))) {
									InetAddress f(_jS(ip["ipRangeStart"],""));
									InetAddress t(_jS(ip["ipRangeEnd"],""));
									if ( ((f.ss_family == AF_INET)||(f.ss_family == AF_INET6)) && (f.ss_family == t.ss_family) ) {
										auto nipp = network["ipAssignmentPools"];
										if (!nipp.is_array()) nipp = json::array();
										json tmp;
										tmp["ipRangeStart"] = f.toIpString();
										tmp["ipRangeEnd"] = t.toIpString();
										nipp.push_back(tmp);
									}
								}
							}
						}
					}

					if (b.count("rules")) {
						auto rules = b["rules"];
						if (rules.is_array()) {
							json nrules = json::array();
							for(unsigned long i=0;i<rules.size();++i) {
								json rule = rules[i];
								if (rule.is_object()) {
									ZT_VirtualNetworkRule ztr;
									if (_parseRule(rule,ztr)) {
										rule = _renderRule(ztr);
										if ((rule.is_object())&&(rule.count("type"))) {
											nrules.push_back(rule);
										}
									}
								}
							}
							network["rules"] = nrules;
						}
					}

					if (b.count("authTokens")) {
						auto authTokens = b["authTokens"];
						if (authTokens.is_array()) {
							json nat = json::array();
							for(unsigned long i=0;i<authTokens.size();++i) {
								auto token = authTokens[i];
								if (token.is_object()) {
									std::string tstr = token["token"];
									if (tstr.length() > 0) {
										json t = json::object();
										t["token"] = tstr;
										t["expires"] = _jI(token["expires"],0ULL);
										nat.push_back(t);
									}
								}
							}
							network["authTokens"] = nat;
						}
					}

					if (b.count("capabilities")) {
						auto capabilities = b["capabilities"];
						if (capabilities.is_array()) {
							std::map< uint64_t,json > ncaps;
							for(unsigned long i=0;i<capabilities.size();++i) {
								auto cap = capabilities[i];
								if (cap.is_object()) {
									json ncap = json::object();
									const uint64_t capId = _jI(cap["id"],0ULL);
									ncap["id"] = capId;

									auto rules = cap["rules"];
									json nrules = json::array();
									if (rules.is_array()) {
										for(unsigned long i=0;i<rules.size();++i) {
											json rule = rules[i];
											if (rule.is_object()) {
												ZT_VirtualNetworkRule ztr;
												if (_parseRule(rule,ztr)) {
													rule = _renderRule(ztr);
													if ((rule.is_object())&&(rule.count("type"))) {
														nrules.push_back(rule);
													}
												}
											}
										}
									}
									ncap["rules"] = nrules;

									ncaps[capId] = ncap;
								}
							}

							json ncapsa = json::array();
							for(std::map< uint64_t,json >::iterator c(ncaps.begin());c!=ncaps.end();++c)
								ncapsa.push_back(c->second);
							network["capabilities"] = ncapsa;
						}
					}
				} catch ( ... ) {
					return 400;
				}

				network["id"] = nwids;
				network["nwid"] = nwids; // legacy
				auto rev = network["revision"];
				network["revision"] = (rev.is_number() ? ((uint64_t)rev + 1ULL) : 1ULL);
				network["lastModified"] = now;

				_writeJson(_networkJP(nwid,true),network);

				_NetworkMemberInfo nmi;
				_getNetworkMemberInfo(now,nwid,nmi);
				_addNetworkNonPersistedFields(network,now,nmi);

				responseBody = network.dump(2);
				responseContentType = "application/json";
				return 200;
			} // else 404

		} // else 404

	} // else 404

	return 404;
}

unsigned int EmbeddedNetworkController::handleControlPlaneHttpDELETE(
	const std::vector<std::string> &path,
	const std::map<std::string,std::string> &urlArgs,
	const std::map<std::string,std::string> &headers,
	const std::string &body,
	std::string &responseBody,
	std::string &responseContentType)
{
	if (path.empty())
		return 404;

	if (path[0] == "network") {
		if ((path.size() >= 2)&&(path[1].length() == 16)) {
			const uint64_t nwid = Utils::hexStrToU64(path[1].c_str());

			json network(_readJson(_networkJP(nwid,false)));
			if (!network.size())
				return 404;

			if (path.size() >= 3) {
				if ((path.size() == 4)&&(path[2] == "member")&&(path[3].length() == 10)) {
					const uint64_t address = Utils::hexStrToU64(path[3].c_str());

					json member(_readJson(_memberJP(nwid,Address(address),false)));
					if (!member.size())
						return 404;

					OSUtils::rmDashRf(_memberBP(nwid,Address(address),false).c_str());

					responseBody = member.dump(2);
					responseContentType = "application/json";
					return 200;
				}
			} else {
				OSUtils::rmDashRf(_networkBP(nwid,false).c_str());
				{
					Mutex::Lock _l(_networkMemberCache_m);
					_networkMemberCache.erase(nwid);
				}
				responseBody = network.dump(2);
				responseContentType = "application/json";
				return 200;
			}
		} // else 404

	} // else 404

	return 404;
}

void EmbeddedNetworkController::_circuitTestCallback(ZT_Node *node,ZT_CircuitTest *test,const ZT_CircuitTestReport *report)
{
	char tmp[65535];
	EmbeddedNetworkController *const self = reinterpret_cast<EmbeddedNetworkController *>(test->ptr);

	if (!test)
		return;
	if (!report)
		return;

	Mutex::Lock _l(self->_circuitTests_m);
	std::map< uint64_t,_CircuitTestEntry >::iterator cte(self->_circuitTests.find(test->testId));

	if (cte == self->_circuitTests.end()) { // sanity check: a circuit test we didn't launch?
		self->_node->circuitTestEnd(test);
		::free((void *)test);
		return;
	}

	Utils::snprintf(tmp,sizeof(tmp),
		"%s{\n"
		"\t\"timestamp\": %llu," ZT_EOL_S
		"\t\"testId\": \"%.16llx\"," ZT_EOL_S
		"\t\"upstream\": \"%.10llx\"," ZT_EOL_S
		"\t\"current\": \"%.10llx\"," ZT_EOL_S
		"\t\"receivedTimestamp\": %llu," ZT_EOL_S
		"\t\"remoteTimestamp\": %llu," ZT_EOL_S
		"\t\"sourcePacketId\": \"%.16llx\"," ZT_EOL_S
		"\t\"flags\": %llu," ZT_EOL_S
		"\t\"sourcePacketHopCount\": %u," ZT_EOL_S
		"\t\"errorCode\": %u," ZT_EOL_S
		"\t\"vendor\": %d," ZT_EOL_S
		"\t\"protocolVersion\": %u," ZT_EOL_S
		"\t\"majorVersion\": %u," ZT_EOL_S
		"\t\"minorVersion\": %u," ZT_EOL_S
		"\t\"revision\": %u," ZT_EOL_S
		"\t\"platform\": %d," ZT_EOL_S
		"\t\"architecture\": %d," ZT_EOL_S
		"\t\"receivedOnLocalAddress\": \"%s\"," ZT_EOL_S
		"\t\"receivedFromRemoteAddress\": \"%s\"" ZT_EOL_S
		"}",
		((cte->second.jsonResults.length() > 0) ? ",\n" : ""),
		(unsigned long long)report->timestamp,
		(unsigned long long)test->testId,
		(unsigned long long)report->upstream,
		(unsigned long long)report->current,
		(unsigned long long)OSUtils::now(),
		(unsigned long long)report->remoteTimestamp,
		(unsigned long long)report->sourcePacketId,
		(unsigned long long)report->flags,
		report->sourcePacketHopCount,
		report->errorCode,
		(int)report->vendor,
		report->protocolVersion,
		report->majorVersion,
		report->minorVersion,
		report->revision,
		(int)report->platform,
		(int)report->architecture,
		reinterpret_cast<const InetAddress *>(&(report->receivedOnLocalAddress))->toString().c_str(),
		reinterpret_cast<const InetAddress *>(&(report->receivedFromRemoteAddress))->toString().c_str());

	cte->second.jsonResults.append(tmp);
}

void EmbeddedNetworkController::_getNetworkMemberInfo(uint64_t now,uint64_t nwid,_NetworkMemberInfo &nmi)
{
	Mutex::Lock _mcl(_networkMemberCache_m);
	auto memberCacheEntry = _networkMemberCache[nwid];
	nmi.totalMemberCount = memberCacheEntry.size();
	for(std::map< Address,nlohmann::json >::const_iterator nm(memberCacheEntry.begin());nm!=memberCacheEntry.end();++nm) {
		if (_jB(nm->second["authorized"],false)) {
			++nmi.authorizedMemberCount;

			auto mlog = nm->second["recentLog"];
			if ((mlog.is_array())&&(mlog.size() > 0)) {
				auto mlog1 = mlog[0];
				if (mlog1.is_object()) {
					if ((now - _jI(mlog1["ts"],0ULL)) < ZT_NETCONF_NODE_ACTIVE_THRESHOLD)
						++nmi.activeMemberCount;
				}
			}

			if (_jB(nm->second["activeBridge"],false)) {
				nmi.activeBridges.insert(nm->first);
			}

			auto mips = nm->second["ipAssignments"];
			if (mips.is_array()) {
				for(unsigned long i=0;i<mips.size();++i) {
					InetAddress mip(_jS(mips[i],""));
					if ((mip.ss_family == AF_INET)||(mip.ss_family == AF_INET6))
						nmi.allocatedIps.insert(mip);
				}
			}
		} else {
			nmi.mostRecentDeauthTime = std::max(nmi.mostRecentDeauthTime,_jI(nm->second["lastDeauthorizedTime"],0ULL));
		}
	}
}

} // namespace ZeroTier
