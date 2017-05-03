/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc->
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

#ifndef _WIN32
#include <sys/time.h>
#endif
#include <sys/types.h>

#include <algorithm>
#include <utility>
#include <stdexcept>
#include <map>
#include <thread>
#include <memory>

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
#define ZT_NETCONF_DB_MEMBER_HISTORY_LENGTH 2

// Min duration between requests for an address/nwid combo to prevent floods
#define ZT_NETCONF_MIN_REQUEST_PERIOD 1000

namespace ZeroTier {

static json _renderRule(ZT_VirtualNetworkRule &rule)
{
	char tmp[128];
	json r = json::object();
	const ZT_VirtualNetworkRuleType rt = (ZT_VirtualNetworkRuleType)(rule.t & 0x3f);

	switch(rt) {
		case ZT_NETWORK_RULE_ACTION_DROP:
			r["type"] = "ACTION_DROP";
			break;
		case ZT_NETWORK_RULE_ACTION_ACCEPT:
			r["type"] = "ACTION_ACCEPT";
			break;
		case ZT_NETWORK_RULE_ACTION_TEE:
			r["type"] = "ACTION_TEE";
			r["address"] = Address(rule.v.fwd.address).toString();
			r["flags"] = (unsigned int)rule.v.fwd.flags;
			r["length"] = (unsigned int)rule.v.fwd.length;
			break;
		case ZT_NETWORK_RULE_ACTION_WATCH:
			r["type"] = "ACTION_WATCH";
			r["address"] = Address(rule.v.fwd.address).toString();
			r["flags"] = (unsigned int)rule.v.fwd.flags;
			r["length"] = (unsigned int)rule.v.fwd.length;
			break;
		case ZT_NETWORK_RULE_ACTION_REDIRECT:
			r["type"] = "ACTION_REDIRECT";
			r["address"] = Address(rule.v.fwd.address).toString();
			r["flags"] = (unsigned int)rule.v.fwd.flags;
			break;
		case ZT_NETWORK_RULE_ACTION_BREAK:
			r["type"] = "ACTION_BREAK";
			break;
		default:
			break;
	}

	if (r.size() == 0) {
		switch(rt) {
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
				r["vlanId"] = (unsigned int)rule.v.vlanId;
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_PCP:
				r["type"] = "MATCH_VLAN_PCP";
				r["vlanPcp"] = (unsigned int)rule.v.vlanPcp;
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_DEI:
				r["type"] = "MATCH_VLAN_DEI";
				r["vlanDei"] = (unsigned int)rule.v.vlanDei;
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
				r["mask"] = (unsigned int)rule.v.ipTos.mask;
				r["start"] = (unsigned int)rule.v.ipTos.value[0];
				r["end"] = (unsigned int)rule.v.ipTos.value[1];
				break;
			case ZT_NETWORK_RULE_MATCH_IP_PROTOCOL:
				r["type"] = "MATCH_IP_PROTOCOL";
				r["ipProtocol"] = (unsigned int)rule.v.ipProtocol;
				break;
			case ZT_NETWORK_RULE_MATCH_ETHERTYPE:
				r["type"] = "MATCH_ETHERTYPE";
				r["etherType"] = (unsigned int)rule.v.etherType;
				break;
			case ZT_NETWORK_RULE_MATCH_ICMP:
				r["type"] = "MATCH_ICMP";
				r["icmpType"] = (unsigned int)rule.v.icmp.type;
				if ((rule.v.icmp.flags & 0x01) != 0)
					r["icmpCode"] = (unsigned int)rule.v.icmp.code;
				else r["icmpCode"] = json();
				break;
			case ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE:
				r["type"] = "MATCH_IP_SOURCE_PORT_RANGE";
				r["start"] = (unsigned int)rule.v.port[0];
				r["end"] = (unsigned int)rule.v.port[1];
				break;
			case ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE:
				r["type"] = "MATCH_IP_DEST_PORT_RANGE";
				r["start"] = (unsigned int)rule.v.port[0];
				r["end"] = (unsigned int)rule.v.port[1];
				break;
			case ZT_NETWORK_RULE_MATCH_CHARACTERISTICS:
				r["type"] = "MATCH_CHARACTERISTICS";
				Utils::snprintf(tmp,sizeof(tmp),"%.16llx",rule.v.characteristics);
				r["mask"] = tmp;
				break;
			case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
				r["type"] = "MATCH_FRAME_SIZE_RANGE";
				r["start"] = (unsigned int)rule.v.frameSize[0];
				r["end"] = (unsigned int)rule.v.frameSize[1];
				break;
			case ZT_NETWORK_RULE_MATCH_RANDOM:
				r["type"] = "MATCH_RANDOM";
				r["probability"] = (unsigned long)rule.v.randomProbability;
				break;
			case ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE:
				r["type"] = "MATCH_TAGS_DIFFERENCE";
				r["id"] = rule.v.tag.id;
				r["value"] = rule.v.tag.value;
				break;
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND:
				r["type"] = "MATCH_TAGS_BITWISE_AND";
				r["id"] = rule.v.tag.id;
				r["value"] = rule.v.tag.value;
				break;
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR:
				r["type"] = "MATCH_TAGS_BITWISE_OR";
				r["id"] = rule.v.tag.id;
				r["value"] = rule.v.tag.value;
				break;
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR:
				r["type"] = "MATCH_TAGS_BITWISE_XOR";
				r["id"] = rule.v.tag.id;
				r["value"] = rule.v.tag.value;
				break;
			case ZT_NETWORK_RULE_MATCH_TAGS_EQUAL:
				r["type"] = "MATCH_TAGS_EQUAL";
				r["id"] = rule.v.tag.id;
				r["value"] = rule.v.tag.value;
				break;
			case ZT_NETWORK_RULE_MATCH_TAG_SENDER:
				r["type"] = "MATCH_TAG_SENDER";
				r["id"] = rule.v.tag.id;
				r["value"] = rule.v.tag.value;
				break;
			case ZT_NETWORK_RULE_MATCH_TAG_RECEIVER:
				r["type"] = "MATCH_TAG_RECEIVER";
				r["id"] = rule.v.tag.id;
				r["value"] = rule.v.tag.value;
				break;
			default:
				break;
		}

		if (r.size() > 0) {
			r["not"] = ((rule.t & 0x80) != 0);
			r["or"] = ((rule.t & 0x40) != 0);
		}
	}

	return r;
}

static bool _parseRule(json &r,ZT_VirtualNetworkRule &rule)
{
	if (!r.is_object())
		return false;

	const std::string t(OSUtils::jsonString(r["type"],""));
	memset(&rule,0,sizeof(ZT_VirtualNetworkRule));

	if (OSUtils::jsonBool(r["not"],false))
		rule.t = 0x80;
	else rule.t = 0x00;
	if (OSUtils::jsonBool(r["or"],false))
		rule.t |= 0x40;

	bool tag = false;
	if (t == "ACTION_DROP") {
		rule.t |= ZT_NETWORK_RULE_ACTION_DROP;
		return true;
	} else if (t == "ACTION_ACCEPT") {
		rule.t |= ZT_NETWORK_RULE_ACTION_ACCEPT;
		return true;
	} else if (t == "ACTION_TEE") {
		rule.t |= ZT_NETWORK_RULE_ACTION_TEE;
		rule.v.fwd.address = Utils::hexStrToU64(OSUtils::jsonString(r["address"],"0").c_str()) & 0xffffffffffULL;
		rule.v.fwd.flags = (uint32_t)(OSUtils::jsonInt(r["flags"],0ULL) & 0xffffffffULL);
		rule.v.fwd.length = (uint16_t)(OSUtils::jsonInt(r["length"],0ULL) & 0xffffULL);
		return true;
	} else if (t == "ACTION_WATCH") {
		rule.t |= ZT_NETWORK_RULE_ACTION_WATCH;
		rule.v.fwd.address = Utils::hexStrToU64(OSUtils::jsonString(r["address"],"0").c_str()) & 0xffffffffffULL;
		rule.v.fwd.flags = (uint32_t)(OSUtils::jsonInt(r["flags"],0ULL) & 0xffffffffULL);
		rule.v.fwd.length = (uint16_t)(OSUtils::jsonInt(r["length"],0ULL) & 0xffffULL);
		return true;
	} else if (t == "ACTION_REDIRECT") {
		rule.t |= ZT_NETWORK_RULE_ACTION_REDIRECT;
		rule.v.fwd.address = Utils::hexStrToU64(OSUtils::jsonString(r["address"],"0").c_str()) & 0xffffffffffULL;
		rule.v.fwd.flags = (uint32_t)(OSUtils::jsonInt(r["flags"],0ULL) & 0xffffffffULL);
		return true;
	} else if (t == "ACTION_BREAK") {
		rule.t |= ZT_NETWORK_RULE_ACTION_BREAK;
		return true;
	} else if (t == "MATCH_SOURCE_ZEROTIER_ADDRESS") {
		rule.t |= ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS;
		rule.v.zt = Utils::hexStrToU64(OSUtils::jsonString(r["zt"],"0").c_str()) & 0xffffffffffULL;
		return true;
	} else if (t == "MATCH_DEST_ZEROTIER_ADDRESS") {
		rule.t |= ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS;
		rule.v.zt = Utils::hexStrToU64(OSUtils::jsonString(r["zt"],"0").c_str()) & 0xffffffffffULL;
		return true;
	} else if (t == "MATCH_VLAN_ID") {
		rule.t |= ZT_NETWORK_RULE_MATCH_VLAN_ID;
		rule.v.vlanId = (uint16_t)(OSUtils::jsonInt(r["vlanId"],0ULL) & 0xffffULL);
		return true;
	} else if (t == "MATCH_VLAN_PCP") {
		rule.t |= ZT_NETWORK_RULE_MATCH_VLAN_PCP;
		rule.v.vlanPcp = (uint8_t)(OSUtils::jsonInt(r["vlanPcp"],0ULL) & 0xffULL);
		return true;
	} else if (t == "MATCH_VLAN_DEI") {
		rule.t |= ZT_NETWORK_RULE_MATCH_VLAN_DEI;
		rule.v.vlanDei = (uint8_t)(OSUtils::jsonInt(r["vlanDei"],0ULL) & 0xffULL);
		return true;
	} else if (t == "MATCH_MAC_SOURCE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_MAC_SOURCE;
		const std::string mac(OSUtils::jsonString(r["mac"],"0"));
		Utils::unhex(mac.c_str(),(unsigned int)mac.length(),rule.v.mac,6);
		return true;
	} else if (t == "MATCH_MAC_DEST") {
		rule.t |= ZT_NETWORK_RULE_MATCH_MAC_DEST;
		const std::string mac(OSUtils::jsonString(r["mac"],"0"));
		Utils::unhex(mac.c_str(),(unsigned int)mac.length(),rule.v.mac,6);
		return true;
	} else if (t == "MATCH_IPV4_SOURCE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV4_SOURCE;
		InetAddress ip(OSUtils::jsonString(r["ip"],"0.0.0.0"));
		rule.v.ipv4.ip = reinterpret_cast<struct sockaddr_in *>(&ip)->sin_addr.s_addr;
		rule.v.ipv4.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in *>(&ip)->sin_port) & 0xff;
		if (rule.v.ipv4.mask > 32) rule.v.ipv4.mask = 32;
		return true;
	} else if (t == "MATCH_IPV4_DEST") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV4_DEST;
		InetAddress ip(OSUtils::jsonString(r["ip"],"0.0.0.0"));
		rule.v.ipv4.ip = reinterpret_cast<struct sockaddr_in *>(&ip)->sin_addr.s_addr;
		rule.v.ipv4.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in *>(&ip)->sin_port) & 0xff;
		if (rule.v.ipv4.mask > 32) rule.v.ipv4.mask = 32;
		return true;
	} else if (t == "MATCH_IPV6_SOURCE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV6_SOURCE;
		InetAddress ip(OSUtils::jsonString(r["ip"],"::0"));
		memcpy(rule.v.ipv6.ip,reinterpret_cast<struct sockaddr_in6 *>(&ip)->sin6_addr.s6_addr,16);
		rule.v.ipv6.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in6 *>(&ip)->sin6_port) & 0xff;
		if (rule.v.ipv6.mask > 128) rule.v.ipv6.mask = 128;
		return true;
	} else if (t == "MATCH_IPV6_DEST") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV6_DEST;
		InetAddress ip(OSUtils::jsonString(r["ip"],"::0"));
		memcpy(rule.v.ipv6.ip,reinterpret_cast<struct sockaddr_in6 *>(&ip)->sin6_addr.s6_addr,16);
		rule.v.ipv6.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in6 *>(&ip)->sin6_port) & 0xff;
		if (rule.v.ipv6.mask > 128) rule.v.ipv6.mask = 128;
		return true;
	} else if (t == "MATCH_IP_TOS") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IP_TOS;
		rule.v.ipTos.mask = (uint8_t)(OSUtils::jsonInt(r["mask"],0ULL) & 0xffULL);
		rule.v.ipTos.value[0] = (uint8_t)(OSUtils::jsonInt(r["start"],0ULL) & 0xffULL);
		rule.v.ipTos.value[1] = (uint8_t)(OSUtils::jsonInt(r["end"],0ULL) & 0xffULL);
		return true;
	} else if (t == "MATCH_IP_PROTOCOL") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IP_PROTOCOL;
		rule.v.ipProtocol = (uint8_t)(OSUtils::jsonInt(r["ipProtocol"],0ULL) & 0xffULL);
		return true;
	} else if (t == "MATCH_ETHERTYPE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_ETHERTYPE;
		rule.v.etherType = (uint16_t)(OSUtils::jsonInt(r["etherType"],0ULL) & 0xffffULL);
		return true;
	} else if (t == "MATCH_ICMP") {
		rule.t |= ZT_NETWORK_RULE_MATCH_ICMP;
		rule.v.icmp.type = (uint8_t)(OSUtils::jsonInt(r["icmpType"],0ULL) & 0xffULL);
		json &code = r["icmpCode"];
		if (code.is_null()) {
			rule.v.icmp.code = 0;
			rule.v.icmp.flags = 0x00;
		} else {
			rule.v.icmp.code = (uint8_t)(OSUtils::jsonInt(code,0ULL) & 0xffULL);
			rule.v.icmp.flags = 0x01;
		}
		return true;
	} else if (t == "MATCH_IP_SOURCE_PORT_RANGE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE;
		rule.v.port[0] = (uint16_t)(OSUtils::jsonInt(r["start"],0ULL) & 0xffffULL);
		rule.v.port[1] = (uint16_t)(OSUtils::jsonInt(r["end"],(uint64_t)rule.v.port[0]) & 0xffffULL);
		return true;
	} else if (t == "MATCH_IP_DEST_PORT_RANGE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE;
		rule.v.port[0] = (uint16_t)(OSUtils::jsonInt(r["start"],0ULL) & 0xffffULL);
		rule.v.port[1] = (uint16_t)(OSUtils::jsonInt(r["end"],(uint64_t)rule.v.port[0]) & 0xffffULL);
		return true;
	} else if (t == "MATCH_CHARACTERISTICS") {
		rule.t |= ZT_NETWORK_RULE_MATCH_CHARACTERISTICS;
		if (r.count("mask")) {
			json &v = r["mask"];
			if (v.is_number()) {
				rule.v.characteristics = v;
			} else {
				std::string tmp = v;
				rule.v.characteristics = Utils::hexStrToU64(tmp.c_str());
			}
		}
		return true;
	} else if (t == "MATCH_FRAME_SIZE_RANGE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE;
		rule.v.frameSize[0] = (uint16_t)(OSUtils::jsonInt(r["start"],0ULL) & 0xffffULL);
		rule.v.frameSize[1] = (uint16_t)(OSUtils::jsonInt(r["end"],(uint64_t)rule.v.frameSize[0]) & 0xffffULL);
		return true;
	} else if (t == "MATCH_RANDOM") {
		rule.t |= ZT_NETWORK_RULE_MATCH_RANDOM;
		rule.v.randomProbability = (uint32_t)(OSUtils::jsonInt(r["probability"],0ULL) & 0xffffffffULL);
		return true;
	} else if (t == "MATCH_TAGS_DIFFERENCE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE;
		tag = true;
	} else if (t == "MATCH_TAGS_BITWISE_AND") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND;
		tag = true;
	} else if (t == "MATCH_TAGS_BITWISE_OR") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR;
		tag = true;
	} else if (t == "MATCH_TAGS_BITWISE_XOR") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR;
		tag = true;
	} else if (t == "MATCH_TAGS_EQUAL") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_EQUAL;
		tag = true;
	} else if (t == "MATCH_TAG_SENDER") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAG_SENDER;
		tag = true;
	} else if (t == "MATCH_TAG_RECEIVER") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAG_RECEIVER;
		tag = true;
	}
	if (tag) {
		rule.v.tag.id = (uint32_t)(OSUtils::jsonInt(r["id"],0ULL) & 0xffffffffULL);
		rule.v.tag.value = (uint32_t)(OSUtils::jsonInt(r["value"],0ULL) & 0xffffffffULL);
		return true;
	}

	return false;
}

EmbeddedNetworkController::EmbeddedNetworkController(Node *node,const char *dbPath) :
	_startTime(OSUtils::now()),
	_running(true),
	_db(dbPath),
	_node(node)
{
}

EmbeddedNetworkController::~EmbeddedNetworkController()
{
	std::vector<Thread> t;
	{
		Mutex::Lock _l(_threads_m);
		_running = false;
		t = _threads;
	}
	if (t.size() > 0) {
		_queue.stop();
		for(std::vector<Thread>::iterator i(t.begin());i!=t.end();++i)
			Thread::join(*i);
	}
}

void EmbeddedNetworkController::init(const Identity &signingId,Sender *sender)
{
	this->_sender = sender;
	this->_signingId = signingId;
}

void EmbeddedNetworkController::request(
	uint64_t nwid,
	const InetAddress &fromAddr,
	uint64_t requestPacketId,
	const Identity &identity,
	const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> &metaData)
{
	if (((!_signingId)||(!_signingId.hasPrivate()))||(_signingId.address().toInt() != (nwid >> 24))||(!_sender))
		return;
	_startThreads();
	_RQEntry *qe = new _RQEntry;
	qe->nwid = nwid;
	qe->requestPacketId = requestPacketId;
	qe->fromAddr = fromAddr;
	qe->identity = identity;
	qe->metaData = metaData;
	qe->type = _RQEntry::RQENTRY_TYPE_REQUEST;
	_queue.post(qe);
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

			json network;
			if (!_db.getNetwork(nwid,network))
				return 404;

			if (path.size() >= 3) {

				if (path[2] == "member") {

					if (path.size() >= 4) {
						const uint64_t address = Utils::hexStrToU64(path[3].c_str());
						json member;
						if (!_db.getNetworkMember(nwid,address,member))
							return 404;
						_addMemberNonPersistedFields(nwid,address,member,OSUtils::now());
						responseBody = OSUtils::jsonDump(member);
						responseContentType = "application/json";
					} else {
						responseBody = "{";
						_db.eachMember(nwid,[&responseBody](uint64_t networkId,uint64_t nodeId,const json &member) {
							if ((member.is_object())&&(member.size() > 0)) {
								responseBody.append((responseBody.length() == 1) ? "\"" : ",\"");
								responseBody.append(OSUtils::jsonString(member["id"],"0"));
								responseBody.append("\":");
								responseBody.append(OSUtils::jsonString(member["revision"],"0"));
							}
						});
						responseBody.push_back('}');
						responseContentType = "application/json";
					}
					return 200;

				} // else 404

			} else {

				const uint64_t now = OSUtils::now();
				JSONDB::NetworkSummaryInfo ns;
				_db.getNetworkSummaryInfo(nwid,ns);
				_addNetworkNonPersistedFields(network,now,ns);
				responseBody = OSUtils::jsonDump(network);
				responseContentType = "application/json";
				return 200;

			}
		} else if (path.size() == 1) {

			std::vector<uint64_t> networkIds(_db.networkIds());
			std::sort(networkIds.begin(),networkIds.end());

			char tmp[64];
			responseBody.push_back('[');
			for(std::vector<uint64_t>::const_iterator i(networkIds.begin());i!=networkIds.end();++i) {
				if (responseBody.length() > 1)
					responseBody.push_back(',');
				Utils::snprintf(tmp,sizeof(tmp),"\"%.16llx\"",(unsigned long long)*i);
				responseBody.append(tmp);
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
		b = OSUtils::jsonParse(body);
		if (!b.is_object()) {
			responseBody = "{ \"message\": \"body is not a JSON object\" }";
			responseContentType = "application/json";
			return 400;
		}
	} catch ( ... ) {
		responseBody = "{ \"message\": \"body JSON is invalid\" }";
		responseContentType = "application/json";
		return 400;
	}
	const uint64_t now = OSUtils::now();

	if (path[0] == "network") {

		if ((path.size() >= 2)&&(path[1].length() == 16)) {
			uint64_t nwid = Utils::hexStrToU64(path[1].c_str());
			char nwids[24];
			Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);

			if (path.size() >= 3) {

				if ((path.size() == 4)&&(path[2] == "member")&&(path[3].length() == 10)) {
					uint64_t address = Utils::hexStrToU64(path[3].c_str());
					char addrs[24];
					Utils::snprintf(addrs,sizeof(addrs),"%.10llx",(unsigned long long)address);

					json member;
					_db.getNetworkMember(nwid,address,member);
					json origMember(member); // for detecting changes
					_initMember(member);

					try {
						if (b.count("activeBridge")) member["activeBridge"] = OSUtils::jsonBool(b["activeBridge"],false);
						if (b.count("noAutoAssignIps")) member["noAutoAssignIps"] = OSUtils::jsonBool(b["noAutoAssignIps"],false);

						if (b.count("authorized")) {
							const bool newAuth = OSUtils::jsonBool(b["authorized"],false);
							if (newAuth != OSUtils::jsonBool(member["authorized"],false)) {
								member["authorized"] = newAuth;
								member[((newAuth) ? "lastAuthorizedTime" : "lastDeauthorizedTime")] = now;

								json ah;
								ah["a"] = newAuth;
								ah["by"] = "api";
								ah["ts"] = now;
								ah["ct"] = json();
								ah["c"] = json();
								member["authHistory"].push_back(ah);

								// Member is being de-authorized, so spray Revocation objects to all online members
								if (!newAuth) {
									Revocation rev((uint32_t)_node->prng(),nwid,0,now,ZT_REVOCATION_FLAG_FAST_PROPAGATE,Address(address),Revocation::CREDENTIAL_TYPE_COM);
									rev.sign(_signingId);

									Mutex::Lock _l(_memberStatus_m);
									for(auto i=_memberStatus.begin();i!=_memberStatus.end();++i) {
										if ((i->first.networkId == nwid)&&(i->second.online(now)))
											_node->ncSendRevocation(Address(i->first.nodeId),rev);
									}
								}
							}
						}

						if (b.count("ipAssignments")) {
							json &ipa = b["ipAssignments"];
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
							json &tags = b["tags"];
							if (tags.is_array()) {
								std::map<uint64_t,uint64_t> mtags;
								for(unsigned long i=0;i<tags.size();++i) {
									json &tag = tags[i];
									if ((tag.is_array())&&(tag.size() == 2))
										mtags[OSUtils::jsonInt(tag[0],0ULL) & 0xffffffffULL] = OSUtils::jsonInt(tag[1],0ULL) & 0xffffffffULL;
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
							json &capabilities = b["capabilities"];
							if (capabilities.is_array()) {
								json mcaps = json::array();
								for(unsigned long i=0;i<capabilities.size();++i) {
									mcaps.push_back(OSUtils::jsonInt(capabilities[i],0ULL));
								}
								std::sort(mcaps.begin(),mcaps.end());
								mcaps.erase(std::unique(mcaps.begin(),mcaps.end()),mcaps.end());
								member["capabilities"] = mcaps;
							}
						}
					} catch ( ... ) {
						responseBody = "{ \"message\": \"exception while processing parameters in JSON body\" }";
						responseContentType = "application/json";
						return 400;
					}

					member["id"] = addrs;
					member["address"] = addrs; // legacy
					member["nwid"] = nwids;

					_removeMemberNonPersistedFields(member);
					if (member != origMember) {
						json &revj = member["revision"];
						member["revision"] = (revj.is_number() ? ((uint64_t)revj + 1ULL) : 1ULL);
						_db.saveNetworkMember(nwid,address,member);

						// Push update to member if online
						try {
							Mutex::Lock _l(_memberStatus_m);
							_MemberStatus &ms = _memberStatus[_MemberStatusKey(nwid,address)];
							if ((ms.online(now))&&(ms.lastRequestMetaData))
								request(nwid,InetAddress(),0,ms.identity,ms.lastRequestMetaData);
						} catch ( ... ) {}
					}

					_addMemberNonPersistedFields(nwid,address,member,now);
					responseBody = OSUtils::jsonDump(member);
					responseContentType = "application/json";

					return 200;
				} else if ((path.size() == 3)&&(path[2] == "test")) {

					Mutex::Lock _l(_tests_m);

					_tests.push_back(ZT_CircuitTest());
					ZT_CircuitTest *const test = &(_tests.back());
					memset(test,0,sizeof(ZT_CircuitTest));

					Utils::getSecureRandom(&(test->testId),sizeof(test->testId));
					test->credentialNetworkId = nwid;
					test->ptr = (void *)this;
					json hops = b["hops"];
					if (hops.is_array()) {
						for(unsigned long i=0;i<hops.size();++i) {
							json &hops2 = hops[i];
							if (hops2.is_array()) {
								for(unsigned long j=0;j<hops2.size();++j) {
									std::string s = hops2[j];
									test->hops[test->hopCount].addresses[test->hops[test->hopCount].breadth++] = Utils::hexStrToU64(s.c_str()) & 0xffffffffffULL;
								}
								++test->hopCount;
							} else if (hops2.is_string()) {
								std::string s = hops2;
								test->hops[test->hopCount].addresses[test->hops[test->hopCount].breadth++] = Utils::hexStrToU64(s.c_str()) & 0xffffffffffULL;
								++test->hopCount;
							}
						}
					}
					test->reportAtEveryHop = (OSUtils::jsonBool(b["reportAtEveryHop"],true) ? 1 : 0);

					if (!test->hopCount) {
						_tests.pop_back();
						responseBody = "{ \"message\": \"a test must contain at least one hop\" }";
						responseContentType = "application/json";
						return 400;
					}

					test->timestamp = OSUtils::now();

					if (_node) {
						_node->circuitTestBegin((void *)0,test,&(EmbeddedNetworkController::_circuitTestCallback));
					} else {
						_tests.pop_back();
						return 500;
					}

					char json[512];
					Utils::snprintf(json,sizeof(json),"{\"testId\":\"%.16llx\",\"timestamp\":%llu}",test->testId,test->timestamp);
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
						if (!_db.hasNetwork(tryNwid)) {
							nwid = tryNwid;
							break;
						}
					}
					if (!nwid)
						return 503;
				}
				Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);

				json network;
				_db.getNetwork(nwid,network);
				json origNetwork(network); // for detecting changes
				_initNetwork(network);

				try {
					if (b.count("name")) network["name"] = OSUtils::jsonString(b["name"],"");
					if (b.count("private")) network["private"] = OSUtils::jsonBool(b["private"],true);
					if (b.count("enableBroadcast")) network["enableBroadcast"] = OSUtils::jsonBool(b["enableBroadcast"],false);
					if (b.count("allowPassiveBridging")) network["allowPassiveBridging"] = OSUtils::jsonBool(b["allowPassiveBridging"],false);
					if (b.count("multicastLimit")) network["multicastLimit"] = OSUtils::jsonInt(b["multicastLimit"],32ULL);

					if (b.count("v4AssignMode")) {
						json nv4m;
						json &v4m = b["v4AssignMode"];
						if (v4m.is_string()) { // backward compatibility
							nv4m["zt"] = (OSUtils::jsonString(v4m,"") == "zt");
						} else if (v4m.is_object()) {
							nv4m["zt"] = OSUtils::jsonBool(v4m["zt"],false);
						} else nv4m["zt"] = false;
						network["v4AssignMode"] = nv4m;
					}

					if (b.count("v6AssignMode")) {
						json nv6m;
						json &v6m = b["v6AssignMode"];
						if (!nv6m.is_object()) nv6m = json::object();
						if (v6m.is_string()) { // backward compatibility
							std::vector<std::string> v6ms(OSUtils::split(OSUtils::jsonString(v6m,"").c_str(),",","",""));
							std::sort(v6ms.begin(),v6ms.end());
							v6ms.erase(std::unique(v6ms.begin(),v6ms.end()),v6ms.end());
							nv6m["rfc4193"] = false;
							nv6m["zt"] = false;
							nv6m["6plane"] = false;
							for(std::vector<std::string>::iterator i(v6ms.begin());i!=v6ms.end();++i) {
								if (*i == "rfc4193")
									nv6m["rfc4193"] = true;
								else if (*i == "zt")
									nv6m["zt"] = true;
								else if (*i == "6plane")
									nv6m["6plane"] = true;
							}
						} else if (v6m.is_object()) {
							if (v6m.count("rfc4193")) nv6m["rfc4193"] = OSUtils::jsonBool(v6m["rfc4193"],false);
							if (v6m.count("zt")) nv6m["zt"] = OSUtils::jsonBool(v6m["zt"],false);
							if (v6m.count("6plane")) nv6m["6plane"] = OSUtils::jsonBool(v6m["6plane"],false);
						} else {
							nv6m["rfc4193"] = false;
							nv6m["zt"] = false;
							nv6m["6plane"] = false;
						}
						network["v6AssignMode"] = nv6m;
					}

					if (b.count("routes")) {
						json &rts = b["routes"];
						if (rts.is_array()) {
							json nrts = json::array();
							for(unsigned long i=0;i<rts.size();++i) {
								json &rt = rts[i];
								if (rt.is_object()) {
									json &target = rt["target"];
									json &via = rt["via"];
									if (target.is_string()) {
										InetAddress t(target.get<std::string>());
										InetAddress v;
										if (via.is_string()) v.fromString(via.get<std::string>());
										if ( ((t.ss_family == AF_INET)||(t.ss_family == AF_INET6)) && (t.netmaskBitsValid()) ) {
											json tmp;
											tmp["target"] = t.toString();
											if (v.ss_family == t.ss_family)
												tmp["via"] = v.toIpString();
											else tmp["via"] = json();
											nrts.push_back(tmp);
										}
									}
								}
							}
							network["routes"] = nrts;
						}
					}

					if (b.count("ipAssignmentPools")) {
						json &ipp = b["ipAssignmentPools"];
						if (ipp.is_array()) {
							json nipp = json::array();
							for(unsigned long i=0;i<ipp.size();++i) {
								json &ip = ipp[i];
								if ((ip.is_object())&&(ip.count("ipRangeStart"))&&(ip.count("ipRangeEnd"))) {
									InetAddress f(OSUtils::jsonString(ip["ipRangeStart"],""));
									InetAddress t(OSUtils::jsonString(ip["ipRangeEnd"],""));
									if ( ((f.ss_family == AF_INET)||(f.ss_family == AF_INET6)) && (f.ss_family == t.ss_family) ) {
										json tmp = json::object();
										tmp["ipRangeStart"] = f.toIpString();
										tmp["ipRangeEnd"] = t.toIpString();
										nipp.push_back(tmp);
									}
								}
							}
							network["ipAssignmentPools"] = nipp;
						}
					}

					if (b.count("rules")) {
						json &rules = b["rules"];
						if (rules.is_array()) {
							json nrules = json::array();
							for(unsigned long i=0;i<rules.size();++i) {
								json &rule = rules[i];
								if (rule.is_object()) {
									ZT_VirtualNetworkRule ztr;
									if (_parseRule(rule,ztr))
										nrules.push_back(_renderRule(ztr));
								}
							}
							network["rules"] = nrules;
						}
					}

					if (b.count("authTokens")) {
						json &authTokens = b["authTokens"];
						if (authTokens.is_array()) {
							json nat = json::array();
							for(unsigned long i=0;i<authTokens.size();++i) {
								json &token = authTokens[i];
								if (token.is_object()) {
									std::string tstr = token["token"];
									if (tstr.length() > 0) {
										json t = json::object();
										t["token"] = tstr;
										t["expires"] = OSUtils::jsonInt(token["expires"],0ULL);
										t["maxUsesPerMember"] = OSUtils::jsonInt(token["maxUsesPerMember"],0ULL);
										nat.push_back(t);
									}
								}
							}
							network["authTokens"] = nat;
						}
					}

					if (b.count("capabilities")) {
						json &capabilities = b["capabilities"];
						if (capabilities.is_array()) {
							std::map< uint64_t,json > ncaps;
							for(unsigned long i=0;i<capabilities.size();++i) {
								json &cap = capabilities[i];
								if (cap.is_object()) {
									json ncap = json::object();
									const uint64_t capId = OSUtils::jsonInt(cap["id"],0ULL);
									ncap["id"] = capId;
									ncap["default"] = OSUtils::jsonBool(cap["default"],false);

									json &rules = cap["rules"];
									json nrules = json::array();
									if (rules.is_array()) {
										for(unsigned long i=0;i<rules.size();++i) {
											json &rule = rules[i];
											if (rule.is_object()) {
												ZT_VirtualNetworkRule ztr;
												if (_parseRule(rule,ztr))
													nrules.push_back(_renderRule(ztr));
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

					if (b.count("tags")) {
						json &tags = b["tags"];
						if (tags.is_array()) {
							std::map< uint64_t,json > ntags;
							for(unsigned long i=0;i<tags.size();++i) {
								json &tag = tags[i];
								if (tag.is_object()) {
									json ntag = json::object();
									const uint64_t tagId = OSUtils::jsonInt(tag["id"],0ULL);
									ntag["id"] = tagId;
									json &dfl = tag["default"];
									if (dfl.is_null())
										ntag["default"] = dfl;
									else ntag["default"] = OSUtils::jsonInt(dfl,0ULL);
									ntags[tagId] = ntag;
								}
							}

							json ntagsa = json::array();
							for(std::map< uint64_t,json >::iterator t(ntags.begin());t!=ntags.end();++t)
								ntagsa.push_back(t->second);
							network["tags"] = ntagsa;
						}
					}

				} catch ( ... ) {
					responseBody = "{ \"message\": \"exception occurred while parsing body variables\" }";
					responseContentType = "application/json";
					return 400;
				}

				network["id"] = nwids;
				network["nwid"] = nwids; // legacy

				_removeNetworkNonPersistedFields(network);
				if (network != origNetwork) {
					json &revj = network["revision"];
					network["revision"] = (revj.is_number() ? ((uint64_t)revj + 1ULL) : 1ULL);
					_db.saveNetwork(nwid,network);

					// Send an update to all members of the network that are online
					Mutex::Lock _l(_memberStatus_m);
					for(auto i=_memberStatus.begin();i!=_memberStatus.end();++i) {
						if ((i->first.networkId == nwid)&&(i->second.online(now))&&(i->second.lastRequestMetaData))
							request(nwid,InetAddress(),0,i->second.identity,i->second.lastRequestMetaData);
					}
				}

				JSONDB::NetworkSummaryInfo ns;
				_db.getNetworkSummaryInfo(nwid,ns);
				_addNetworkNonPersistedFields(network,now,ns);

				responseBody = OSUtils::jsonDump(network);
				responseContentType = "application/json";
				return 200;
			} // else 404

		} // else 404

	} else if (path[0] == "ping") {

		_startThreads();
		_RQEntry *qe = new _RQEntry;
		qe->type = _RQEntry::RQENTRY_TYPE_PING;
		_queue.post(qe);

		char tmp[64];
		Utils::snprintf(tmp,sizeof(tmp),"{\"clock\":%llu,\"ping\":%s}",(unsigned long long)now,OSUtils::jsonDump(b).c_str());
		responseBody = tmp;
		responseContentType = "application/json";

		return 200;

	}

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
			if (path.size() >= 3) {
				if ((path.size() == 4)&&(path[2] == "member")&&(path[3].length() == 10)) {
					const uint64_t address = Utils::hexStrToU64(path[3].c_str());

					json member = _db.eraseNetworkMember(nwid,address);

					{
						Mutex::Lock _l(_memberStatus_m);
						_memberStatus.erase(_MemberStatusKey(nwid,address));
					}

					if (!member.size())
						return 404;
					responseBody = OSUtils::jsonDump(member);
					responseContentType = "application/json";
					return 200;
				}
			} else {
				json network = _db.eraseNetwork(nwid);

				{
					Mutex::Lock _l(_memberStatus_m);
					for(auto i=_memberStatus.begin();i!=_memberStatus.end();) {
						if (i->first.networkId == nwid)
							_memberStatus.erase(i++);
						else ++i;
					}
				}

				if (!network.size())
					return 404;
				responseBody = OSUtils::jsonDump(network);
				responseContentType = "application/json";
				return 200;
			}
		} // else 404

	} // else 404

	return 404;
}

void EmbeddedNetworkController::threadMain()
	throw()
{
	uint64_t lastCircuitTestCheck = 0;
	_RQEntry *qe = (_RQEntry *)0;
	while ((_running)&&(_queue.get(qe))) {
		try {
			if (qe->type == _RQEntry::RQENTRY_TYPE_REQUEST) {
				_request(qe->nwid,qe->fromAddr,qe->requestPacketId,qe->identity,qe->metaData);
			} else if (qe->type == _RQEntry::RQENTRY_TYPE_PING) {
				const uint64_t now = OSUtils::now();
				bool first = true;
				std::string pong("{\"memberStatus\":{");
				{
					Mutex::Lock _l(_memberStatus_m);
					pong.reserve(64 * _memberStatus.size());
					_db.eachId([this,&pong,&now,&first](uint64_t networkId,uint64_t nodeId) {
						char tmp[64];
						uint64_t lrt = 0ULL;
						auto ms = this->_memberStatus.find(_MemberStatusKey(networkId,nodeId));
						if (ms != _memberStatus.end())
							lrt = ms->second.lastRequestTime;
						Utils::snprintf(tmp,sizeof(tmp),"%s\"%.16llx-%.10llx\":%llu",
							(first) ? "" : ",",
							(unsigned long long)networkId,
							(unsigned long long)nodeId,
							(unsigned long long)lrt);
						pong.append(tmp);
						first = false;
					});
				}
				char tmp2[256];
				Utils::snprintf(tmp2,sizeof(tmp2),"},\"clock\":%llu,\"startTime\":%llu}",(unsigned long long)now,(unsigned long long)_startTime);
				pong.append(tmp2);
				_db.writeRaw("pong",pong);
			}
		} catch ( ... ) {}
		delete qe;

		if (_running) {
			uint64_t now = OSUtils::now();
			if ((now - lastCircuitTestCheck) > ZT_EMBEDDEDNETWORKCONTROLLER_CIRCUIT_TEST_EXPIRATION) {
				lastCircuitTestCheck = now;
				Mutex::Lock _l(_tests_m);
				for(std::list< ZT_CircuitTest >::iterator i(_tests.begin());i!=_tests.end();) {
					if ((now - i->timestamp) > ZT_EMBEDDEDNETWORKCONTROLLER_CIRCUIT_TEST_EXPIRATION) {
						_node->circuitTestEnd(&(*i));
						_tests.erase(i++);
					} else ++i;
				}
			}
		}
	}
}

void EmbeddedNetworkController::_circuitTestCallback(ZT_Node *node,ZT_CircuitTest *test,const ZT_CircuitTestReport *report)
{
	char tmp[2048],id[128];
	EmbeddedNetworkController *const self = reinterpret_cast<EmbeddedNetworkController *>(test->ptr);

	if ((!test)||(!report)||(!test->credentialNetworkId)) return; // sanity check

	const uint64_t now = OSUtils::now();
	Utils::snprintf(id,sizeof(id),"network/%.16llx/test/%.16llx-%.16llx-%.10llx-%.10llx",test->credentialNetworkId,test->testId,now,report->upstream,report->current);
	Utils::snprintf(tmp,sizeof(tmp),
		"{\"id\": \"%s\","
		"\"objtype\": \"circuit_test\","
		"\"timestamp\": %llu,"
		"\"networkId\": \"%.16llx\","
		"\"testId\": \"%.16llx\","
		"\"upstream\": \"%.10llx\","
		"\"current\": \"%.10llx\","
		"\"receivedTimestamp\": %llu,"
		"\"sourcePacketId\": \"%.16llx\","
		"\"flags\": %llu,"
		"\"sourcePacketHopCount\": %u,"
		"\"errorCode\": %u,"
		"\"vendor\": %d,"
		"\"protocolVersion\": %u,"
		"\"majorVersion\": %u,"
		"\"minorVersion\": %u,"
		"\"revision\": %u,"
		"\"platform\": %d,"
		"\"architecture\": %d,"
		"\"receivedOnLocalAddress\": \"%s\","
		"\"receivedFromRemoteAddress\": \"%s\","
		"\"receivedFromLinkQuality\": %f}",
		id + 30, // last bit only, not leading path
		(unsigned long long)test->timestamp,
		(unsigned long long)test->credentialNetworkId,
		(unsigned long long)test->testId,
		(unsigned long long)report->upstream,
		(unsigned long long)report->current,
		(unsigned long long)now,
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
		reinterpret_cast<const InetAddress *>(&(report->receivedFromRemoteAddress))->toString().c_str(),
		((double)report->receivedFromLinkQuality / (double)ZT_PATH_LINK_QUALITY_MAX));

	self->_db.writeRaw(id,std::string(tmp));
}

void EmbeddedNetworkController::_request(
	uint64_t nwid,
	const InetAddress &fromAddr,
	uint64_t requestPacketId,
	const Identity &identity,
	const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> &metaData)
{
	char nwids[24];
	JSONDB::NetworkSummaryInfo ns;
	json network,member,origMember;

	if (((!_signingId)||(!_signingId.hasPrivate()))||(_signingId.address().toInt() != (nwid >> 24))||(!_sender))
		return;

	const uint64_t now = OSUtils::now();

	if (requestPacketId) {
		Mutex::Lock _l(_memberStatus_m);
		_MemberStatus &ms = _memberStatus[_MemberStatusKey(nwid,identity.address().toInt())];
		if ((now - ms.lastRequestTime) <= ZT_NETCONF_MIN_REQUEST_PERIOD)
			return;
		ms.lastRequestTime = now;
	}

	Utils::snprintf(nwids,sizeof(nwids),"%.16llx",nwid);
	if (!_db.getNetworkAndMember(nwid,identity.address().toInt(),network,member,ns)) {
		_sender->ncSendError(nwid,requestPacketId,identity.address(),NetworkController::NC_ERROR_OBJECT_NOT_FOUND);
		return;
	}
	origMember = member;
	const bool newMember = ((!member.is_object())||(member.size() == 0));
	_initMember(member);

	{
		std::string haveIdStr(OSUtils::jsonString(member["identity"],""));
		if (haveIdStr.length() > 0) {
			// If we already know this member's identity perform a full compare. This prevents
			// a "collision" from being able to auth onto our network in place of an already
			// known member.
			try {
				if (Identity(haveIdStr.c_str()) != identity) {
					_sender->ncSendError(nwid,requestPacketId,identity.address(),NetworkController::NC_ERROR_ACCESS_DENIED);
					return;
				}
			} catch ( ... ) {
				_sender->ncSendError(nwid,requestPacketId,identity.address(),NetworkController::NC_ERROR_ACCESS_DENIED);
				return;
			}
		} else {
			// If we do not yet know this member's identity, learn it.
			member["identity"] = identity.toString(false);
		}
	}

	// These are always the same, but make sure they are set
	{
		const std::string addrs(identity.address().toString());
		member["id"] = addrs;
		member["address"] = addrs;
		member["nwid"] = nwids;
	}

	// Determine whether and how member is authorized
	const char *authorizedBy = (const char *)0;
	bool autoAuthorized = false;
	json autoAuthCredentialType,autoAuthCredential;
	if (OSUtils::jsonBool(member["authorized"],false)) {
		authorizedBy = "memberIsAuthorized";
	} else if (!OSUtils::jsonBool(network["private"],true)) {
		authorizedBy = "networkIsPublic";
		json &ahist = member["authHistory"];
		if ((!ahist.is_array())||(ahist.size() == 0))
			autoAuthorized = true;
	} else {
		char presentedAuth[512];
		if (metaData.get(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_AUTH,presentedAuth,sizeof(presentedAuth)) > 0) {
			presentedAuth[511] = (char)0; // sanity check

			// Check for bearer token presented by member
			if ((strlen(presentedAuth) > 6)&&(!strncmp(presentedAuth,"token:",6))) {
				const char *const presentedToken = presentedAuth + 6;

				json &authTokens = network["authTokens"];
				if (authTokens.is_array()) {
					for(unsigned long i=0;i<authTokens.size();++i) {
						json &token = authTokens[i];
						if (token.is_object()) {
							const uint64_t expires = OSUtils::jsonInt(token["expires"],0ULL);
							const uint64_t maxUses = OSUtils::jsonInt(token["maxUsesPerMember"],0ULL);
							std::string tstr = OSUtils::jsonString(token["token"],"");

							if (((expires == 0ULL)||(expires > now))&&(tstr == presentedToken)) {
								bool usable = (maxUses == 0);
								if (!usable) {
									uint64_t useCount = 0;
									json &ahist = member["authHistory"];
									if (ahist.is_array()) {
										for(unsigned long j=0;j<ahist.size();++j) {
											json &ah = ahist[j];
											if ((OSUtils::jsonString(ah["ct"],"") == "token")&&(OSUtils::jsonString(ah["c"],"") == tstr)&&(OSUtils::jsonBool(ah["a"],false)))
												++useCount;
										}
									}
									usable = (useCount < maxUses);
								}
								if (usable) {
									authorizedBy = "token";
									autoAuthorized = true;
									autoAuthCredentialType = "token";
									autoAuthCredential = tstr;
								}
							}
						}
					}
				}
			}
		}
	}

	// If we auto-authorized, update member record
	if ((autoAuthorized)&&(authorizedBy)) {
		member["authorized"] = true;
		member["lastAuthorizedTime"] = now;

		json ah;
		ah["a"] = true;
		ah["by"] = authorizedBy;
		ah["ts"] = now;
		ah["ct"] = autoAuthCredentialType;
		ah["c"] = autoAuthCredential;
		member["authHistory"].push_back(ah);

		json &revj = member["revision"];
		member["revision"] = (revj.is_number() ? ((uint64_t)revj + 1ULL) : 1ULL);
	}

	if (authorizedBy) {
		// Update version info and meta-data if authorized and if this is a genuine request
		if (requestPacketId) {
			const uint64_t vMajor = metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MAJOR_VERSION,0);
			const uint64_t vMinor = metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MINOR_VERSION,0);
			const uint64_t vRev = metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_REVISION,0);
			const uint64_t vProto = metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_PROTOCOL_VERSION,0);

			member["vMajor"] = vMajor;
			member["vMinor"] = vMinor;
			member["vRev"] = vRev;
			member["vProto"] = vProto;

			{
				Mutex::Lock _l(_memberStatus_m);
				_MemberStatus &ms = _memberStatus[_MemberStatusKey(nwid,identity.address().toInt())];

				ms.vMajor = (int)vMajor;
				ms.vMinor = (int)vMinor;
				ms.vRev = (int)vRev;
				ms.vProto = (int)vProto;
				ms.lastRequestMetaData = metaData;
				ms.identity = identity;

				if (fromAddr)
					ms.physicalAddr = fromAddr;
				if (ms.physicalAddr)
					member["physicalAddr"] = ms.physicalAddr.toString();
			}
		}
	} else {
		// If they are not authorized, STOP!
		_removeMemberNonPersistedFields(member);
		if (origMember != member)
			_db.saveNetworkMember(nwid,identity.address().toInt(),member);
		_sender->ncSendError(nwid,requestPacketId,identity.address(),NetworkController::NC_ERROR_ACCESS_DENIED);
		return;
	}

	// -------------------------------------------------------------------------
	// If we made it this far, they are authorized.
	// -------------------------------------------------------------------------

	uint64_t credentialtmd = ZT_NETWORKCONFIG_DEFAULT_CREDENTIAL_TIME_MAX_MAX_DELTA;
	if (now > ns.mostRecentDeauthTime) {
		// If we recently de-authorized a member, shrink credential TTL/max delta to
		// be below the threshold required to exclude it. Cap this to a min/max to
		// prevent jitter or absurdly large values.
		const uint64_t deauthWindow = now - ns.mostRecentDeauthTime;
		if (deauthWindow < ZT_NETWORKCONFIG_DEFAULT_CREDENTIAL_TIME_MIN_MAX_DELTA) {
			credentialtmd = ZT_NETWORKCONFIG_DEFAULT_CREDENTIAL_TIME_MIN_MAX_DELTA;
		} else if (deauthWindow < (ZT_NETWORKCONFIG_DEFAULT_CREDENTIAL_TIME_MAX_MAX_DELTA + 5000ULL)) {
			credentialtmd = deauthWindow - 5000ULL;
		}
	}

	std::auto_ptr<NetworkConfig> nc(new NetworkConfig());

	nc->networkId = nwid;
	nc->type = OSUtils::jsonBool(network["private"],true) ? ZT_NETWORK_TYPE_PRIVATE : ZT_NETWORK_TYPE_PUBLIC;
	nc->timestamp = now;
	nc->credentialTimeMaxDelta = credentialtmd;
	nc->revision = OSUtils::jsonInt(network["revision"],0ULL);
	nc->issuedTo = identity.address();
	if (OSUtils::jsonBool(network["enableBroadcast"],true)) nc->flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_BROADCAST;
	if (OSUtils::jsonBool(network["allowPassiveBridging"],false)) nc->flags |= ZT_NETWORKCONFIG_FLAG_ALLOW_PASSIVE_BRIDGING;
	Utils::scopy(nc->name,sizeof(nc->name),OSUtils::jsonString(network["name"],"").c_str());
	nc->multicastLimit = (unsigned int)OSUtils::jsonInt(network["multicastLimit"],32ULL);

	for(std::vector<Address>::const_iterator ab(ns.activeBridges.begin());ab!=ns.activeBridges.end();++ab)
		nc->addSpecialist(*ab,ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE);

	json &v4AssignMode = network["v4AssignMode"];
	json &v6AssignMode = network["v6AssignMode"];
	json &ipAssignmentPools = network["ipAssignmentPools"];
	json &routes = network["routes"];
	json &rules = network["rules"];
	json &capabilities = network["capabilities"];
	json &tags = network["tags"];
	json &memberCapabilities = member["capabilities"];
	json &memberTags = member["tags"];

	if (metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_RULES_ENGINE_REV,0) <= 0) {
		// Old versions with no rules engine support get an allow everything rule.
		// Since rules are enforced bidirectionally, newer versions *will* still
		// enforce rules on the inbound side.
		nc->ruleCount = 1;
		nc->rules[0].t = ZT_NETWORK_RULE_ACTION_ACCEPT;
	} else {
		if (rules.is_array()) {
			for(unsigned long i=0;i<rules.size();++i) {
				if (nc->ruleCount >= ZT_MAX_NETWORK_RULES)
					break;
				if (_parseRule(rules[i],nc->rules[nc->ruleCount]))
					++nc->ruleCount;
			}
		}

		std::map< uint64_t,json * > capsById;
		if (!memberCapabilities.is_array())
			memberCapabilities = json::array();
		if (capabilities.is_array()) {
			for(unsigned long i=0;i<capabilities.size();++i) {
				json &cap = capabilities[i];
				if (cap.is_object()) {
					const uint64_t id = OSUtils::jsonInt(cap["id"],0ULL) & 0xffffffffULL;
					capsById[id] = &cap;
					if ((newMember)&&(OSUtils::jsonBool(cap["default"],false))) {
						bool have = false;
						for(unsigned long i=0;i<memberCapabilities.size();++i) {
							if (id == (OSUtils::jsonInt(memberCapabilities[i],0ULL) & 0xffffffffULL)) {
								have = true;
								break;
							}
						}
						if (!have)
							memberCapabilities.push_back(id);
					}
				}
			}
		}
		for(unsigned long i=0;i<memberCapabilities.size();++i) {
			const uint64_t capId = OSUtils::jsonInt(memberCapabilities[i],0ULL) & 0xffffffffULL;
			std::map< uint64_t,json * >::const_iterator ctmp = capsById.find(capId);
			if (ctmp != capsById.end()) {
				json *cap = ctmp->second;
				if ((cap)&&(cap->is_object())&&(cap->size() > 0)) {
					ZT_VirtualNetworkRule capr[ZT_MAX_CAPABILITY_RULES];
					unsigned int caprc = 0;
					json &caprj = (*cap)["rules"];
					if ((caprj.is_array())&&(caprj.size() > 0)) {
						for(unsigned long j=0;j<caprj.size();++j) {
							if (caprc >= ZT_MAX_CAPABILITY_RULES)
								break;
							if (_parseRule(caprj[j],capr[caprc]))
								++caprc;
						}
					}
					nc->capabilities[nc->capabilityCount] = Capability((uint32_t)capId,nwid,now,1,capr,caprc);
					if (nc->capabilities[nc->capabilityCount].sign(_signingId,identity.address()))
						++nc->capabilityCount;
					if (nc->capabilityCount >= ZT_MAX_NETWORK_CAPABILITIES)
						break;
				}
			}
		}

		std::map< uint32_t,uint32_t > memberTagsById;
		if (memberTags.is_array()) {
			for(unsigned long i=0;i<memberTags.size();++i) {
				json &t = memberTags[i];
				if ((t.is_array())&&(t.size() == 2))
					memberTagsById[(uint32_t)(OSUtils::jsonInt(t[0],0ULL) & 0xffffffffULL)] = (uint32_t)(OSUtils::jsonInt(t[1],0ULL) & 0xffffffffULL);
			}
		}
		if (tags.is_array()) { // check network tags array for defaults that are not present in member tags
			for(unsigned long i=0;i<tags.size();++i) {
				json &t = tags[i];
				if (t.is_object()) {
					const uint32_t id = (uint32_t)(OSUtils::jsonInt(t["id"],0) & 0xffffffffULL);
					json &dfl = t["default"];
					if ((dfl.is_number())&&(memberTagsById.find(id) == memberTagsById.end())) {
						memberTagsById[id] = (uint32_t)(OSUtils::jsonInt(dfl,0) & 0xffffffffULL);
						json mt = json::array();
						mt.push_back(id);
						mt.push_back(dfl);
						memberTags.push_back(mt); // add default to member tags if not present
					}
				}
			}
		}
		for(std::map< uint32_t,uint32_t >::const_iterator t(memberTagsById.begin());t!=memberTagsById.end();++t) {
			if (nc->tagCount >= ZT_MAX_NETWORK_TAGS)
				break;
			nc->tags[nc->tagCount] = Tag(nwid,now,identity.address(),t->first,t->second);
			if (nc->tags[nc->tagCount].sign(_signingId))
				++nc->tagCount;
		}
	}

	if (routes.is_array()) {
		for(unsigned long i=0;i<routes.size();++i) {
			if (nc->routeCount >= ZT_MAX_NETWORK_ROUTES)
				break;
			json &route = routes[i];
			json &target = route["target"];
			json &via = route["via"];
			if (target.is_string()) {
				const InetAddress t(target.get<std::string>());
				InetAddress v;
				if (via.is_string()) v.fromString(via.get<std::string>());
				if ((t.ss_family == AF_INET)||(t.ss_family == AF_INET6)) {
					ZT_VirtualNetworkRoute *r = &(nc->routes[nc->routeCount]);
					*(reinterpret_cast<InetAddress *>(&(r->target))) = t;
					if (v.ss_family == t.ss_family)
						*(reinterpret_cast<InetAddress *>(&(r->via))) = v;
					++nc->routeCount;
				}
			}
		}
	}

	const bool noAutoAssignIps = OSUtils::jsonBool(member["noAutoAssignIps"],false);

	if ((v6AssignMode.is_object())&&(!noAutoAssignIps)) {
		if ((OSUtils::jsonBool(v6AssignMode["rfc4193"],false))&&(nc->staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)) {
			nc->staticIps[nc->staticIpCount++] = InetAddress::makeIpv6rfc4193(nwid,identity.address().toInt());
			nc->flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION;
		}
		if ((OSUtils::jsonBool(v6AssignMode["6plane"],false))&&(nc->staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)) {
			nc->staticIps[nc->staticIpCount++] = InetAddress::makeIpv66plane(nwid,identity.address().toInt());
			nc->flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION;
		}
	}

	bool haveManagedIpv4AutoAssignment = false;
	bool haveManagedIpv6AutoAssignment = false; // "special" NDP-emulated address types do not count
	json ipAssignments = member["ipAssignments"]; // we want to make a copy
	if (ipAssignments.is_array()) {
		for(unsigned long i=0;i<ipAssignments.size();++i) {
			if (!ipAssignments[i].is_string())
				continue;
			std::string ips = ipAssignments[i];
			InetAddress ip(ips);

			// IP assignments are only pushed if there is a corresponding local route. We also now get the netmask bits from
			// this route, ignoring the netmask bits field of the assigned IP itself. Using that was worthless and a source
			// of user error / poor UX.
			int routedNetmaskBits = 0;
			for(unsigned int rk=0;rk<nc->routeCount;++rk) {
				if ( (!nc->routes[rk].via.ss_family) && (reinterpret_cast<const InetAddress *>(&(nc->routes[rk].target))->containsAddress(ip)) )
					routedNetmaskBits = reinterpret_cast<const InetAddress *>(&(nc->routes[rk].target))->netmaskBits();
			}

			if (routedNetmaskBits > 0) {
				if (nc->staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES) {
					ip.setPort(routedNetmaskBits);
					nc->staticIps[nc->staticIpCount++] = ip;
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

	if ( (ipAssignmentPools.is_array()) && ((v6AssignMode.is_object())&&(OSUtils::jsonBool(v6AssignMode["zt"],false))) && (!haveManagedIpv6AutoAssignment) && (!noAutoAssignIps) ) {
		for(unsigned long p=0;((p<ipAssignmentPools.size())&&(!haveManagedIpv6AutoAssignment));++p) {
			json &pool = ipAssignmentPools[p];
			if (pool.is_object()) {
				InetAddress ipRangeStart(OSUtils::jsonString(pool["ipRangeStart"],""));
				InetAddress ipRangeEnd(OSUtils::jsonString(pool["ipRangeEnd"],""));
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
						for(unsigned int rk=0;rk<nc->routeCount;++rk) {
							if ( (!nc->routes[rk].via.ss_family) && (nc->routes[rk].target.ss_family == AF_INET6) && (reinterpret_cast<const InetAddress *>(&(nc->routes[rk].target))->containsAddress(ip6)) )
								routedNetmaskBits = reinterpret_cast<const InetAddress *>(&(nc->routes[rk].target))->netmaskBits();
						}

						// If it's routed, then try to claim and assign it and if successful end loop
						if ( (routedNetmaskBits > 0) && (!std::binary_search(ns.allocatedIps.begin(),ns.allocatedIps.end(),ip6)) ) {
							ipAssignments.push_back(ip6.toIpString());
							member["ipAssignments"] = ipAssignments;
							ip6.setPort((unsigned int)routedNetmaskBits);
							if (nc->staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)
								nc->staticIps[nc->staticIpCount++] = ip6;
							haveManagedIpv6AutoAssignment = true;
							break;
						}
					}
				}
			}
		}
	}

	if ( (ipAssignmentPools.is_array()) && ((v4AssignMode.is_object())&&(OSUtils::jsonBool(v4AssignMode["zt"],false))) && (!haveManagedIpv4AutoAssignment) && (!noAutoAssignIps) ) {
		for(unsigned long p=0;((p<ipAssignmentPools.size())&&(!haveManagedIpv4AutoAssignment));++p) {
			json &pool = ipAssignmentPools[p];
			if (pool.is_object()) {
				InetAddress ipRangeStartIA(OSUtils::jsonString(pool["ipRangeStart"],""));
				InetAddress ipRangeEndIA(OSUtils::jsonString(pool["ipRangeEnd"],""));
				if ( (ipRangeStartIA.ss_family == AF_INET) && (ipRangeEndIA.ss_family == AF_INET) ) {
					uint32_t ipRangeStart = Utils::ntoh((uint32_t)(reinterpret_cast<struct sockaddr_in *>(&ipRangeStartIA)->sin_addr.s_addr));
					uint32_t ipRangeEnd = Utils::ntoh((uint32_t)(reinterpret_cast<struct sockaddr_in *>(&ipRangeEndIA)->sin_addr.s_addr));
					if ((ipRangeEnd < ipRangeStart)||(ipRangeStart == 0))
						continue;
					uint32_t ipRangeLen = ipRangeEnd - ipRangeStart;

					// Start with the LSB of the member's address
					uint32_t ipTrialCounter = (uint32_t)(identity.address().toInt() & 0xffffffff);

					for(uint32_t k=ipRangeStart,trialCount=0;((k<=ipRangeEnd)&&(trialCount < 1000));++k,++trialCount) {
						uint32_t ip = (ipRangeLen > 0) ? (ipRangeStart + (ipTrialCounter % ipRangeLen)) : ipRangeStart;
						++ipTrialCounter;
						if ((ip & 0x000000ff) == 0x000000ff)
							continue; // don't allow addresses that end in .255

						// Check if this IP is within a local-to-Ethernet routed network
						int routedNetmaskBits = -1;
						for(unsigned int rk=0;rk<nc->routeCount;++rk) {
							if (nc->routes[rk].target.ss_family == AF_INET) {
								uint32_t targetIp = Utils::ntoh((uint32_t)(reinterpret_cast<const struct sockaddr_in *>(&(nc->routes[rk].target))->sin_addr.s_addr));
								int targetBits = Utils::ntoh((uint16_t)(reinterpret_cast<const struct sockaddr_in *>(&(nc->routes[rk].target))->sin_port));
								if ((ip & (0xffffffff << (32 - targetBits))) == targetIp) {
									routedNetmaskBits = targetBits;
									break;
								}
							}
						}

						// If it's routed, then try to claim and assign it and if successful end loop
						const InetAddress ip4(Utils::hton(ip),0);
						if ( (routedNetmaskBits > 0) && (!std::binary_search(ns.allocatedIps.begin(),ns.allocatedIps.end(),ip4)) ) {
							ipAssignments.push_back(ip4.toIpString());
							member["ipAssignments"] = ipAssignments;
							if (nc->staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES) {
								struct sockaddr_in *const v4ip = reinterpret_cast<struct sockaddr_in *>(&(nc->staticIps[nc->staticIpCount++]));
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

	// Issue a certificate of ownership for all static IPs
	if (nc->staticIpCount) {
		nc->certificatesOfOwnership[0] = CertificateOfOwnership(nwid,now,identity.address(),1);
		for(unsigned int i=0;i<nc->staticIpCount;++i)
			nc->certificatesOfOwnership[0].addThing(nc->staticIps[i]);
		nc->certificatesOfOwnership[0].sign(_signingId);
		nc->certificateOfOwnershipCount = 1;
	}

	CertificateOfMembership com(now,credentialtmd,nwid,identity.address());
	if (com.sign(_signingId)) {
		nc->com = com;
	} else {
		_sender->ncSendError(nwid,requestPacketId,identity.address(),NetworkController::NC_ERROR_INTERNAL_SERVER_ERROR);
		return;
	}

	_removeMemberNonPersistedFields(member);
	if (member != origMember)
		_db.saveNetworkMember(nwid,identity.address().toInt(),member);

	_sender->ncSendConfig(nwid,requestPacketId,identity.address(),*(nc.get()),metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_VERSION,0) < 6);
}

} // namespace ZeroTier
