/*
 * Copyright (c)2019 ZeroTier, Inc.
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <type_traits>

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
#include <iomanip>
#include <sstream>
#include <cctype>

#include "../include/ZeroTierOne.h"
#include "../version.h"

#include "EmbeddedNetworkController.hpp"
#include "LFDB.hpp"
#include "FileDB.hpp"
#ifdef ZT_CONTROLLER_USE_LIBPQ
#include "PostgreSQL.hpp"
#endif

#include "../node/Node.hpp"
#include "../node/CertificateOfMembership.hpp"
#include "../node/NetworkConfig.hpp"
#include "../node/Dictionary.hpp"
#include "../node/MAC.hpp"

using json = nlohmann::json;

// API version reported via JSON control plane
#define ZT_NETCONF_CONTROLLER_API_VERSION 4

// Min duration between requests for an address/nwid combo to prevent floods
#define ZT_NETCONF_MIN_REQUEST_PERIOD 1000

// Global maximum size of arrays in JSON objects
#define ZT_CONTROLLER_MAX_ARRAY_SIZE 16384

namespace ZeroTier {

namespace {

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
			r["address"] = Address(rule.v.fwd.address).toString(tmp);
			r["flags"] = (unsigned int)rule.v.fwd.flags;
			r["length"] = (unsigned int)rule.v.fwd.length;
			break;
		case ZT_NETWORK_RULE_ACTION_WATCH:
			r["type"] = "ACTION_WATCH";
			r["address"] = Address(rule.v.fwd.address).toString(tmp);
			r["flags"] = (unsigned int)rule.v.fwd.flags;
			r["length"] = (unsigned int)rule.v.fwd.length;
			break;
		case ZT_NETWORK_RULE_ACTION_REDIRECT:
			r["type"] = "ACTION_REDIRECT";
			r["address"] = Address(rule.v.fwd.address).toString(tmp);
			r["flags"] = (unsigned int)rule.v.fwd.flags;
			break;
		case ZT_NETWORK_RULE_ACTION_BREAK:
			r["type"] = "ACTION_BREAK";
			break;
		default:
			break;
	}

	if (r.empty()) {
		switch(rt) {
			case ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS:
				r["type"] = "MATCH_SOURCE_ZEROTIER_ADDRESS";
				r["zt"] = Address(rule.v.zt).toString(tmp);
				break;
			case ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS:
				r["type"] = "MATCH_DEST_ZEROTIER_ADDRESS";
				r["zt"] = Address(rule.v.zt).toString(tmp);
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
				OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",(unsigned int)rule.v.mac[0],(unsigned int)rule.v.mac[1],(unsigned int)rule.v.mac[2],(unsigned int)rule.v.mac[3],(unsigned int)rule.v.mac[4],(unsigned int)rule.v.mac[5]);
				r["mac"] = tmp;
				break;
			case ZT_NETWORK_RULE_MATCH_MAC_DEST:
				r["type"] = "MATCH_MAC_DEST";
				OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",(unsigned int)rule.v.mac[0],(unsigned int)rule.v.mac[1],(unsigned int)rule.v.mac[2],(unsigned int)rule.v.mac[3],(unsigned int)rule.v.mac[4],(unsigned int)rule.v.mac[5]);
				r["mac"] = tmp;
				break;
			case ZT_NETWORK_RULE_MATCH_IPV4_SOURCE:
				r["type"] = "MATCH_IPV4_SOURCE";
				r["ip"] = InetAddress(&(rule.v.ipv4.ip),4,(unsigned int)rule.v.ipv4.mask).toString(tmp);
				break;
			case ZT_NETWORK_RULE_MATCH_IPV4_DEST:
				r["type"] = "MATCH_IPV4_DEST";
				r["ip"] = InetAddress(&(rule.v.ipv4.ip),4,(unsigned int)rule.v.ipv4.mask).toString(tmp);
				break;
			case ZT_NETWORK_RULE_MATCH_IPV6_SOURCE:
				r["type"] = "MATCH_IPV6_SOURCE";
				r["ip"] = InetAddress(rule.v.ipv6.ip,16,(unsigned int)rule.v.ipv6.mask).toString(tmp);
				break;
			case ZT_NETWORK_RULE_MATCH_IPV6_DEST:
				r["type"] = "MATCH_IPV6_DEST";
				r["ip"] = InetAddress(rule.v.ipv6.ip,16,(unsigned int)rule.v.ipv6.mask).toString(tmp);
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
				OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.16llx",rule.v.characteristics);
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
			case ZT_NETWORK_RULE_MATCH_INTEGER_RANGE:
				r["type"] = "INTEGER_RANGE";
				OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.16llx",rule.v.intRange.start);
				r["start"] = tmp;
				OSUtils::ztsnprintf(tmp,sizeof(tmp),"%.16llx",rule.v.intRange.start + (uint64_t)rule.v.intRange.end);
				r["end"] = tmp;
				r["idx"] = rule.v.intRange.idx;
				r["little"] = ((rule.v.intRange.format & 0x80) != 0);
				r["bits"] = (rule.v.intRange.format & 63) + 1;
				break;
			default:
				break;
		}

		if (!r.empty()) {
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
		const std::string mac(Utils::cleanMac(OSUtils::jsonString(r["mac"],"0")));
		Utils::unhex(mac.c_str(),(unsigned int)mac.length(),rule.v.mac,6);
		return true;
	} else if (t == "MATCH_MAC_DEST") {
		rule.t |= ZT_NETWORK_RULE_MATCH_MAC_DEST;
		const std::string mac(Utils::cleanMac(OSUtils::jsonString(r["mac"],"0")));
		Utils::unhex(mac.c_str(),(unsigned int)mac.length(),rule.v.mac,6);
		return true;
	} else if (t == "MATCH_IPV4_SOURCE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV4_SOURCE;
		InetAddress ip(OSUtils::jsonString(r["ip"],"0.0.0.0").c_str());
		rule.v.ipv4.ip = reinterpret_cast<struct sockaddr_in *>(&ip)->sin_addr.s_addr;
		rule.v.ipv4.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in *>(&ip)->sin_port) & 0xff;
		if (rule.v.ipv4.mask > 32) rule.v.ipv4.mask = 32;
		return true;
	} else if (t == "MATCH_IPV4_DEST") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV4_DEST;
		InetAddress ip(OSUtils::jsonString(r["ip"],"0.0.0.0").c_str());
		rule.v.ipv4.ip = reinterpret_cast<struct sockaddr_in *>(&ip)->sin_addr.s_addr;
		rule.v.ipv4.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in *>(&ip)->sin_port) & 0xff;
		if (rule.v.ipv4.mask > 32) rule.v.ipv4.mask = 32;
		return true;
	} else if (t == "MATCH_IPV6_SOURCE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV6_SOURCE;
		InetAddress ip(OSUtils::jsonString(r["ip"],"::0").c_str());
		memcpy(rule.v.ipv6.ip,reinterpret_cast<struct sockaddr_in6 *>(&ip)->sin6_addr.s6_addr,16);
		rule.v.ipv6.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in6 *>(&ip)->sin6_port) & 0xff;
		if (rule.v.ipv6.mask > 128) rule.v.ipv6.mask = 128;
		return true;
	} else if (t == "MATCH_IPV6_DEST") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV6_DEST;
		InetAddress ip(OSUtils::jsonString(r["ip"],"::0").c_str());
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
	} else if (t == "INTEGER_RANGE") {
		json &s = r["start"];
		if (s.is_string()) {
			std::string tmp = s;
			rule.v.intRange.start = Utils::hexStrToU64(tmp.c_str());
		} else {
			rule.v.intRange.start = OSUtils::jsonInt(s,0ULL);
		}
		json &e = r["end"];
		if (e.is_string()) {
			std::string tmp = e;
			rule.v.intRange.end = (uint32_t)(Utils::hexStrToU64(tmp.c_str()) - rule.v.intRange.start);
		} else {
			rule.v.intRange.end = (uint32_t)(OSUtils::jsonInt(e,0ULL) - rule.v.intRange.start);
		}
		rule.v.intRange.idx = (uint16_t)OSUtils::jsonInt(r["idx"],0ULL);
		rule.v.intRange.format = (OSUtils::jsonBool(r["little"],false)) ? 0x80 : 0x00;
		rule.v.intRange.format |= (uint8_t)((OSUtils::jsonInt(r["bits"],1ULL) - 1) & 63);
	}

	if (tag) {
		rule.v.tag.id = (uint32_t)(OSUtils::jsonInt(r["id"],0ULL) & 0xffffffffULL);
		rule.v.tag.value = (uint32_t)(OSUtils::jsonInt(r["value"],0ULL) & 0xffffffffULL);
		return true;
	}

	return false;
}

} // anonymous namespace

EmbeddedNetworkController::EmbeddedNetworkController(Node *node,const char *ztPath,const char *dbPath, int listenPort, RedisConfig *rc) 
	: _startTime(OSUtils::now())
	, _listenPort(listenPort)
	, _node(node)
	, _ztPath(ztPath)
	, _path(dbPath)
	, _signingId()
	, _signingIdAddressString()
	, _sender((NetworkController::Sender *)0)
	, _db(this)
	, _queue()
	, _threads()
	, _threads_l()
	, _memberStatus()
	, _memberStatus_l()
	, _expiringSoon()
	, _expiringSoon_l()
	, _rc(rc)
	, _ssoExpiryRunning(true)
	, _ssoExpiry(std::thread(&EmbeddedNetworkController::_ssoExpiryThread, this))

#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK	
	, _member_status_lookup{"nc_member_status_lookup",""}
	, _member_status_lookup_count{"nc_member_status_lookup_count",""}
	, _node_is_online{"nc_node_is_online",""}
	, _node_is_online_count{"nc_node_is_online_count",""}
	, _get_and_init_member{"nc_get_and_init_member",""}
	, _get_and_init_member_count{"nc_get_and_init_member_count",""}
	, _have_identity{"nc_have_identity",""}
	, _have_identity_count{"nc_have_identity_count",""}
	, _determine_auth{"nc_determine_auth",""}
	, _determine_auth_count{"nc_determine_auth_count",""}
	, _sso_check{"nc_sso_check",""}
	, _sso_check_count{"nc_sso_check_count",""}
	, _auth_check{"nc_auth_check",""}
	, _auth_check_count{"nc_auth_check_count",""}
	, _json_schlep{"nc_json_schlep",""}
	, _json_schlep_count{"nc_json_schlep_count",""}
	, _issue_certificate{"nc_issue_certificate", ""}
	, _issue_certificate_count{"nc_issue_certificate_count",""}
	, _save_member{"nc_save_member",""}
	, _save_member_count{"nc_save_member_count",""}
	, _send_netconf{"nc_send_netconf2",""}
	, _send_netconf_count{"nc_send_netconf2_count",""}
#endif
{
}

EmbeddedNetworkController::~EmbeddedNetworkController()
{
	std::lock_guard<std::mutex> l(_threads_l);
	_queue.stop();
	for(auto t=_threads.begin();t!=_threads.end();++t) {
		t->join();
	}
	_ssoExpiryRunning = false;
	_ssoExpiry.join();
}

void EmbeddedNetworkController::setSSORedirectURL(const std::string &url) {
	_ssoRedirectURL = url;
}

void EmbeddedNetworkController::init(const Identity &signingId,Sender *sender)
{
	char tmp[64];
	_signingId = signingId;
	_sender = sender;
	_signingIdAddressString = signingId.address().toString(tmp);

#ifdef ZT_CONTROLLER_USE_LIBPQ
	if ((_path.length() > 9)&&(_path.substr(0,9) == "postgres:")) {
		_db.addDB(std::shared_ptr<DB>(new PostgreSQL(_signingId,_path.substr(9).c_str(), _listenPort, _rc)));
	} else {
#endif
		_db.addDB(std::shared_ptr<DB>(new FileDB(_path.c_str())));
#ifdef ZT_CONTROLLER_USE_LIBPQ
	}
#endif

	std::string lfJSON;
	OSUtils::readFile((_ztPath + ZT_PATH_SEPARATOR_S "local.conf").c_str(),lfJSON);
	if (lfJSON.length() > 0) {
		nlohmann::json lfConfig(OSUtils::jsonParse(lfJSON));
		nlohmann::json &settings = lfConfig["settings"];
		if (settings.is_object()) {
			nlohmann::json &controllerDb = settings["controllerDb"];
			if (controllerDb.is_object()) {
				std::string type = controllerDb["type"];
				if (type == "lf") {
					std::string lfOwner = controllerDb["owner"];
					std::string lfHost = controllerDb["host"];
					int lfPort = controllerDb["port"];
					bool storeOnlineState = controllerDb["storeOnlineState"];
					if ((lfOwner.length())&&(lfHost.length())&&(lfPort > 0)&&(lfPort < 65536)) {
						std::size_t pubHdrLoc = lfOwner.find("Public: ");
						if ((pubHdrLoc > 0)&&((pubHdrLoc + 8) < lfOwner.length())) {
							std::string lfOwnerPublic = lfOwner.substr(pubHdrLoc + 8);
							std::size_t pubHdrEnd = lfOwnerPublic.find_first_of("\n\r\t ");
							if (pubHdrEnd != std::string::npos) {
								lfOwnerPublic = lfOwnerPublic.substr(0,pubHdrEnd);
								_db.addDB(std::shared_ptr<DB>(new LFDB(_signingId,_path.c_str(),lfOwner.c_str(),lfOwnerPublic.c_str(),lfHost.c_str(),lfPort,storeOnlineState)));
							}
						}
					}
				}
			}
		}
	}

	_db.waitForReady();
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

	const int64_t now = OSUtils::now();

	if (requestPacketId) {
		std::lock_guard<std::mutex> l(_memberStatus_l);
		_MemberStatus &ms = _memberStatus[_MemberStatusKey(nwid,identity.address().toInt())];
		if ((now - ms.lastRequestTime) <= ZT_NETCONF_MIN_REQUEST_PERIOD) {
			return;
		}
		ms.lastRequestTime = now;
	}

	_RQEntry *qe = new _RQEntry;
	qe->nwid = nwid;
	qe->requestPacketId = requestPacketId;
	qe->fromAddr = fromAddr;
	qe->identity = identity;
	qe->metaData = metaData;
	qe->type = _RQEntry::RQENTRY_TYPE_REQUEST;
	_queue.post(qe);
}

std::string EmbeddedNetworkController::networkUpdateFromPostData(uint64_t networkID, const std::string &body)
{
	json b = OSUtils::jsonParse(body);

	char nwids[24];
	OSUtils::ztsnprintf(nwids, sizeof(nwids), "%.16llx", networkID);

	json network;
	_db.get(networkID, network);
	DB::initNetwork(network);
	if (b.count("name")) network["name"] = OSUtils::jsonString(b["name"],"");
	if (b.count("private")) network["private"] = OSUtils::jsonBool(b["private"],true);
	if (b.count("enableBroadcast")) network["enableBroadcast"] = OSUtils::jsonBool(b["enableBroadcast"],false);
	if (b.count("multicastLimit")) network["multicastLimit"] = OSUtils::jsonInt(b["multicastLimit"],32ULL);
	if (b.count("mtu")) network["mtu"] = std::max(std::min((unsigned int)OSUtils::jsonInt(b["mtu"],ZT_DEFAULT_MTU),(unsigned int)ZT_MAX_MTU),(unsigned int)ZT_MIN_MTU);

	if (b.count("remoteTraceTarget")) {
		const std::string rtt(OSUtils::jsonString(b["remoteTraceTarget"],""));
		if (rtt.length() == 10) {
			network["remoteTraceTarget"] = rtt;
		} else {
			network["remoteTraceTarget"] = json();
		}
	}
	if (b.count("remoteTraceLevel")) network["remoteTraceLevel"] = OSUtils::jsonInt(b["remoteTraceLevel"],0ULL);

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
						InetAddress t(target.get<std::string>().c_str());
						InetAddress v;
						if (via.is_string()) v.fromString(via.get<std::string>().c_str());
						if ( ((t.ss_family == AF_INET)||(t.ss_family == AF_INET6)) && (t.netmaskBitsValid()) ) {
							json tmp;
							char tmp2[64];
							tmp["target"] = t.toString(tmp2);
							if (v.ss_family == t.ss_family)
								tmp["via"] = v.toIpString(tmp2);
							else tmp["via"] = json();
							nrts.push_back(tmp);
							if (nrts.size() >= ZT_CONTROLLER_MAX_ARRAY_SIZE)
								break;
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
					InetAddress f(OSUtils::jsonString(ip["ipRangeStart"],"").c_str());
					InetAddress t(OSUtils::jsonString(ip["ipRangeEnd"],"").c_str());
					if ( ((f.ss_family == AF_INET)||(f.ss_family == AF_INET6)) && (f.ss_family == t.ss_family) ) {
						json tmp = json::object();
						char tmp2[64];
						tmp["ipRangeStart"] = f.toIpString(tmp2);
						tmp["ipRangeEnd"] = t.toIpString(tmp2);
						nipp.push_back(tmp);
						if (nipp.size() >= ZT_CONTROLLER_MAX_ARRAY_SIZE)
							break;
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
					if (_parseRule(rule,ztr)) {
						nrules.push_back(_renderRule(ztr));
						if (nrules.size() >= ZT_CONTROLLER_MAX_ARRAY_SIZE)
							break;
					}
				}
			}
			network["rules"] = nrules;
		}
	}

	if (b.count("authTokens")) {
		json &authTokens = b["authTokens"];
		if (authTokens.is_object()) {
			json nat;
			for(json::iterator t(authTokens.begin());t!=authTokens.end();++t) {
				if ((t.value().is_number())&&(t.value() >= 0))
					nat[t.key()] = t.value();
			}
			network["authTokens"] = nat;
		} else {
			network["authTokens"] = {{}};
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
								if (_parseRule(rule,ztr)) {
									nrules.push_back(_renderRule(ztr));
									if (nrules.size() >= ZT_CONTROLLER_MAX_ARRAY_SIZE)
										break;
								}
							}
						}
					}
					ncap["rules"] = nrules;

					ncaps[capId] = ncap;
				}
			}

			json ncapsa = json::array();
			for(std::map< uint64_t,json >::iterator c(ncaps.begin());c!=ncaps.end();++c) {
				ncapsa.push_back(c->second);
				if (ncapsa.size() >= ZT_CONTROLLER_MAX_ARRAY_SIZE)
					break;
			}
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
			for(std::map< uint64_t,json >::iterator t(ntags.begin());t!=ntags.end();++t) {
				ntagsa.push_back(t->second);
				if (ntagsa.size() >= ZT_CONTROLLER_MAX_ARRAY_SIZE)
					break;
			}
			network["tags"] = ntagsa;
		}
	}

	if (b.count("dns")) {
		json &dns = b["dns"];
		if (dns.is_object()) {
			json nd;

			nd["domain"] = dns["domain"];

			json &srv = dns["servers"];
			if (srv.is_array()) {
				json ns = json::array();
				for(unsigned int i=0;i<srv.size();++i) {
					ns.push_back(srv[i]);
				}
				nd["servers"] = ns;
			}

			network["dns"] = nd;
		}
	}

	network["id"] = nwids;
	network["nwid"] = nwids;

	DB::cleanNetwork(network);
	_db.save(network, true);

	return network.dump();
}

void EmbeddedNetworkController::configureHTTPControlPlane(
	httplib::Server &s,
	httplib::Server &sv6,
	const std::function<void(const httplib::Request&, httplib::Response&, std::string)> setContent)
{
	// Control plane Endpoints
	std::string controllerPath = "/controller";
	std::string networkListPath = "/controller/network";
	std::string networkListPath2 = "/unstable/controller/network";
	std::string networkPath = "/controller/network/([0-9a-fA-F]{16})";
	std::string oldAndBustedNetworkCreatePath = "/controller/network/([0-9a-fA-F]{10})______";
	std::string memberListPath = "/controller/network/([0-9a-fA-F]{16})/member";
	std::string memberListPath2 = "/unstable/controller/network/([0-9a-fA-F]{16})/member";
	std::string memberPath = "/controller/network/([0-9a-fA-F]{16})/member/([0-9a-fA-F]{10})";

	auto controllerGet = [&, setContent](const httplib::Request &req, httplib::Response &res) {
		char tmp[4096];
		const bool dbOk = _db.isReady();
		OSUtils::ztsnprintf(
			tmp,
			sizeof(tmp),
			"{\n\t\"controller\": true,\n\t\"apiVersion\": %d,\n\t\"clock\": %llu,\n\t\"databaseReady\": %s\n}\n",
			ZT_NETCONF_CONTROLLER_API_VERSION,
			(unsigned long long)OSUtils::now(),
			dbOk ? "true" : "false");

			if (!dbOk) {
				res.status = 503;
			}

			setContent(req, res, tmp);
	};
	s.Get(controllerPath, controllerGet);
	sv6.Get(controllerPath, controllerGet);

	auto networkListGet = [&, setContent](const httplib::Request &req, httplib::Response &res) {
		std::set<uint64_t> networkIds;
		_db.networks(networkIds);
		char tmp[64];

		auto out = json::array();
		for(std::set<uint64_t>::const_iterator i(networkIds.begin()); i != networkIds.end(); ++i) {
			OSUtils::ztsnprintf(tmp, sizeof(tmp), "%.16llx", *i);
			out.push_back(tmp);
		}

		setContent(req, res, out.dump());
	};
	s.Get(networkListPath, networkListGet);
	sv6.Get(networkListPath, networkListGet);

	auto networkListGet2 = [&, setContent](const httplib::Request &req, httplib::Response &res) {
		std::set<uint64_t> networkIds;
		_db.networks(networkIds);

		auto meta = json::object();
		auto data = json::array();
		uint64_t networkCount = 0;

		for(std::set<uint64_t>::const_iterator nwid(networkIds.begin()); nwid != networkIds.end(); ++nwid) {
			json network;
			if (!_db.get(*nwid, network)) {
				continue;
			}

			std::vector<json> memTmp;
			if (_db.get(*nwid, network, memTmp)) {
				if (!network.is_null()) {
					uint64_t authorizedCount = 0;
					uint64_t totalCount = memTmp.size();
					networkCount++;

					for (auto m = memTmp.begin(); m != memTmp.end(); ++m) {
						bool a = OSUtils::jsonBool((*m)["authorized"], 0);
						if (a) { authorizedCount++; }
					}

					auto nwMeta = json::object();
					nwMeta["totalMemberCount"] = totalCount;
					nwMeta["authorizedMemberCount"] = authorizedCount;
					network["meta"] = nwMeta;

					data.push_back(network);
				}
			}
		}
		meta["networkCount"] = networkCount;

		auto out = json::object();
		out["data"] = data;
		out["meta"] = meta;

		setContent(req, res, out.dump());
	};
	s.Get(networkListPath2, networkListGet2);
	sv6.Get(networkListPath2, networkListGet2);

	auto networkGet = [&, setContent](const httplib::Request &req, httplib::Response &res) {
		auto networkID = req.matches[1];
		uint64_t nwid = Utils::hexStrToU64(networkID.str().c_str());
		json network;
		if (!_db.get(nwid, network)) {
			res.status = 404;
			return;
		}

		setContent(req, res, network.dump());
	};
	s.Get(networkPath, networkGet);
	sv6.Get(networkPath, networkGet);

	auto createNewNetwork = [&, setContent](const httplib::Request &req, httplib::Response &res) {
		// fprintf(stderr, "creating new network (new style)\n");
		uint64_t nwid = 0;
		uint64_t nwidPrefix = (Utils::hexStrToU64(_signingIdAddressString.c_str()) << 24) & 0xffffffffff000000ULL;
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
		if (!nwid) {
			res.status = 503;
			return;
		}

		setContent(req, res, networkUpdateFromPostData(nwid, req.body));
	};
	s.Put(networkListPath, createNewNetwork);
	s.Post(networkListPath, createNewNetwork);
	sv6.Put(networkListPath, createNewNetwork);
	sv6.Post(networkListPath, createNewNetwork);

	auto createNewNetworkOldAndBusted = [&, setContent](const httplib::Request &req, httplib::Response &res) {
		auto inID = req.matches[1].str();

		if (inID != _signingIdAddressString) {
			res.status = 400;
			return;
		}

		uint64_t nwid = 0;
		uint64_t nwidPrefix = (Utils::hexStrToU64(inID.c_str()) << 24) & 0xffffffffff000000ULL;
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
		if (!nwid) {
			res.status = 503;
			return;
		}
		setContent(req, res, networkUpdateFromPostData(nwid, req.body));
	};
	s.Put(oldAndBustedNetworkCreatePath, createNewNetworkOldAndBusted);
	s.Post(oldAndBustedNetworkCreatePath, createNewNetworkOldAndBusted);
	sv6.Put(oldAndBustedNetworkCreatePath, createNewNetworkOldAndBusted);
	sv6.Post(oldAndBustedNetworkCreatePath, createNewNetworkOldAndBusted);

	auto networkPost = [&, setContent](const httplib::Request &req, httplib::Response &res) {
		auto networkID = req.matches[1].str();
		uint64_t nwid = Utils::hexStrToU64(networkID.c_str());

		res.status = 200;
		setContent(req, res, networkUpdateFromPostData(nwid, req.body));
	};
	s.Put(networkPath, networkPost);
	s.Post(networkPath, networkPost);
	sv6.Put(networkPath, networkPost);
	sv6.Post(networkPath, networkPost);

	auto networkDelete = [&, setContent](const httplib::Request &req, httplib::Response &res) {
		auto networkID = req.matches[1].str();
		uint64_t nwid = Utils::hexStrToU64(networkID.c_str());

		json network;
		if (!_db.get(nwid,network)) {
			res.status = 404;
			return;
		}

		_db.eraseNetwork(nwid);
		setContent(req, res, network.dump());
	};
	s.Delete(networkPath, networkDelete);
	sv6.Delete(networkPath, networkDelete);

	auto memberListGet = [&, setContent](const httplib::Request &req, httplib::Response &res) {
		auto networkID = req.matches[1];
		uint64_t nwid = Utils::hexStrToU64(networkID.str().c_str());
		json network;
		if (!_db.get(nwid, network)) {
			res.status = 404;
			return;
		}

		json out = json::object();
		std::vector<json> memTmp;
		if (_db.get(nwid, network, memTmp)) {
			for (auto m = memTmp.begin(); m != memTmp.end(); ++m) {
				int revision = OSUtils::jsonInt((*m)["revision"], 0);
				std::string id = OSUtils::jsonString((*m)["id"], "");
				if (id.length() == 10) {
					out[id] = revision;
				}
			}
		}

		setContent(req, res, out.dump());
	};
	s.Get(memberListPath, memberListGet);
	sv6.Get(memberListPath, memberListGet);

	auto memberListGet2 = [&, setContent](const httplib::Request &req, httplib::Response &res) {
		auto networkID = req.matches[1];
		uint64_t nwid = Utils::hexStrToU64(networkID.str().c_str());
		json network;
		if (!_db.get(nwid, network)) {
			res.status = 404;
			return;
		}

		auto out = nlohmann::json::object();
		auto meta = nlohmann::json::object();
		std::vector<json> memTmp;
		if (_db.get(nwid, network, memTmp)) {
			uint64_t authorizedCount = 0;
			uint64_t totalCount = memTmp.size();
			for (auto m = memTmp.begin(); m != memTmp.end(); ++m) {
				bool a = OSUtils::jsonBool((*m)["authorized"], 0);
				if (a) { authorizedCount++; }
			}

			meta["totalCount"] = totalCount;
			meta["authorizedCount"] = authorizedCount;

			out["data"] = memTmp;
			out["meta"] = meta;

			setContent(req, res, out.dump());
		} else {
			res.status = 404;
			return;
		}
	};
	s.Get(memberListPath2, memberListGet2);
	sv6.Get(memberListPath2, memberListGet2);

	auto memberGet = [&, setContent](const httplib::Request &req, httplib::Response &res) {
		auto networkID = req.matches[1];
		auto memberID = req.matches[2];
		uint64_t nwid = Utils::hexStrToU64(networkID.str().c_str());
		uint64_t memid = Utils::hexStrToU64(memberID.str().c_str());
		json network;
		json member;
		if (!_db.get(nwid, network, memid, member)) {
			res.status = 404;
			return;
		}

		setContent(req, res, member.dump());
	};
	s.Get(memberPath, memberGet);
	sv6.Get(memberPath, memberGet);

	auto memberPost = [&, setContent](const httplib::Request &req, httplib::Response &res) {
		auto networkID = req.matches[1].str();
		auto memberID = req.matches[2].str();
		uint64_t nwid = Utils::hexStrToU64(networkID.c_str());
		uint64_t memid = Utils::hexStrToU64(memberID.c_str());

		if (!_db.hasNetwork(nwid)) {
			res.status = 404;
			return;
		}

		json network;
		json member;
		_db.get(nwid, network, memid, member);
		DB::initMember(member);

		json b = OSUtils::jsonParse(req.body);

		if (b.count("activeBridge")) member["activeBridge"] = OSUtils::jsonBool(b["activeBridge"], false);
		if (b.count("noAutoAssignIps")) member["noAutoAssignIps"] = OSUtils::jsonBool(b["noAutoAssignIps"], false);
		if (b.count("authenticationExpiryTime")) member["authenticationExpiryTime"] = (uint64_t)OSUtils::jsonInt(b["authenticationExpiryTime"], 0ULL);
		if (b.count("authenticationURL")) member["authenticationURL"] = OSUtils::jsonString(b["authenticationURL"], "");
		if (b.count("name")) member["name"] = OSUtils::jsonString(b["name"], "");

		if (b.count("remoteTraceTarget")) {
			const std::string rtt(OSUtils::jsonString(b["remoteTraceTarget"],""));
			if (rtt.length() == 10) {
				member["remoteTraceTarget"] = rtt;
			} else {
				member["remoteTraceTarget"] = json();
			}
		}
		if (b.count("remoteTraceLevel")) member["remoteTraceLevel"] = OSUtils::jsonInt(b["remoteTraceLevel"],0ULL);

		if (b.count("authorized")) {
			const bool newAuth = OSUtils::jsonBool(b["authorized"],false);
			if (newAuth != OSUtils::jsonBool(member["authorized"],false)) {
				member["authorized"] = newAuth;
				member[((newAuth) ? "lastAuthorizedTime" : "lastDeauthorizedTime")] = OSUtils::now();
				if (newAuth) {
					member["lastAuthorizedCredentialType"] = "api";
					member["lastAuthorizedCredential"] = json();
				}
			}
		}

		if (b.count("ipAssignments")) {
			json &ipa = b["ipAssignments"];
			if (ipa.is_array()) {
				json mipa(json::array());
				for(unsigned long i=0;i<ipa.size();++i) {
					std::string ips = ipa[i];
					InetAddress ip(ips.c_str());
					if ((ip.ss_family == AF_INET)||(ip.ss_family == AF_INET6)) {
						char tmpip[64];
						mipa.push_back(ip.toIpString(tmpip));
						if (mipa.size() >= ZT_CONTROLLER_MAX_ARRAY_SIZE)
							break;
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
					if (mtagsa.size() >= ZT_CONTROLLER_MAX_ARRAY_SIZE)
						break;
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
					if (mcaps.size() >= ZT_CONTROLLER_MAX_ARRAY_SIZE)
						break;
				}
				std::sort(mcaps.begin(),mcaps.end());
				mcaps.erase(std::unique(mcaps.begin(),mcaps.end()),mcaps.end());
				member["capabilities"] = mcaps;
			}
		}

		member["id"] = memberID;
		member["address"] = memberID;
		member["nwid"] = networkID;

		DB::cleanMember(member);
		_db.save(member, true);

		setContent(req, res, member.dump());
	};
	s.Put(memberPath, memberPost);
	s.Post(memberPath, memberPost);
	sv6.Put(memberPath, memberPost);
	sv6.Post(memberPath, memberPost);

	auto memberDelete = [&, setContent](const httplib::Request &req, httplib::Response &res) {
		auto networkID = req.matches[1].str();
		auto memberID = req.matches[2].str();

		uint64_t nwid = Utils::hexStrToU64(networkID.c_str());
		uint64_t address = Utils::hexStrToU64(memberID.c_str());
		json network, member;

		if (!_db.get(nwid, network, address, member)) {
			res.status = 404;
			return;
		}

		if (!member.size()) {
			res.status = 404;
			return;
		}

		_db.eraseMember(nwid, address);

		setContent(req, res, member.dump());
	};
	s.Delete(memberPath, memberDelete);
	sv6.Delete(memberPath, memberDelete);
}

void EmbeddedNetworkController::handleRemoteTrace(const ZT_RemoteTrace &rt)
{
	static volatile unsigned long idCounter = 0;
	char id[128],tmp[128];
	std::string k,v;

	try {
		// Convert Dictionary into JSON object
		json d;
		char *saveptr = (char *)0;
		for(char *l=Utils::stok(rt.data,"\n",&saveptr);(l);l=Utils::stok((char *)0,"\n",&saveptr)) {
			char *eq = strchr(l,'=');
			if (eq > l) {
				k.assign(l,(unsigned long)(eq - l));
				v.clear();
				++eq;
				while (*eq) {
					if (*eq == '\\') {
						++eq;
						if (*eq) {
							switch(*eq) {
								case 'r': v.push_back('\r'); break;
								case 'n': v.push_back('\n'); break;
								case '0': v.push_back((char)0); break;
								case 'e': v.push_back('='); break;
								default: v.push_back(*eq); break;
							}
							++eq;
						}
					} else {
						v.push_back(*(eq++));
					}
				}
				if ((k.length() > 0)&&(v.length() > 0))
					d[k] = v;
			}
		}

		const int64_t now = OSUtils::now();
		OSUtils::ztsnprintf(id,sizeof(id),"%.10llx-%.16llx-%.10llx-%.4x",_signingId.address().toInt(),now,rt.origin,(unsigned int)(idCounter++ & 0xffff));
		d["id"] = id;
		d["objtype"] = "trace";
		d["ts"] = now;
		d["nodeId"] = Utils::hex10(rt.origin,tmp);
		_db.save(d,true);
	} catch ( ... ) {
		// drop invalid trace messages if an error occurs
	}
}

void EmbeddedNetworkController::onNetworkUpdate(const void *db,uint64_t networkId,const nlohmann::json &network)
{
	// Send an update to all members of the network that are online
	const int64_t now = OSUtils::now();
	std::lock_guard<std::mutex> l(_memberStatus_l);
	for(auto i=_memberStatus.begin();i!=_memberStatus.end();++i) {
		if ((i->first.networkId == networkId)&&(i->second.online(now))&&(i->second.lastRequestMetaData))
			request(networkId,InetAddress(),0,i->second.identity,i->second.lastRequestMetaData);
	}
}

void EmbeddedNetworkController::onNetworkMemberUpdate(const void *db,uint64_t networkId,uint64_t memberId,const nlohmann::json &member)
{
	// Push update to member if online
	try {
		std::lock_guard<std::mutex> l(_memberStatus_l);
		_MemberStatus &ms = _memberStatus[_MemberStatusKey(networkId,memberId)];
		if ((ms.online(OSUtils::now()))&&(ms.lastRequestMetaData))
			request(networkId,InetAddress(),0,ms.identity,ms.lastRequestMetaData);
	} catch ( ... ) {}
}

void EmbeddedNetworkController::onNetworkMemberDeauthorize(const void *db,uint64_t networkId,uint64_t memberId)
{
	const int64_t now = OSUtils::now();
	Revocation rev((uint32_t)_node->prng(),networkId,0,now,ZT_REVOCATION_FLAG_FAST_PROPAGATE,Address(memberId),Revocation::CREDENTIAL_TYPE_COM);
	rev.sign(_signingId);
	{
		std::lock_guard<std::mutex> l(_memberStatus_l);
		for(auto i=_memberStatus.begin();i!=_memberStatus.end();++i) {
			if ((i->first.networkId == networkId)&&(i->second.online(now)))
				_node->ncSendRevocation(Address(i->first.nodeId),rev);
		}
	}
}

void EmbeddedNetworkController::_request(
	uint64_t nwid,
	const InetAddress &fromAddr,
	uint64_t requestPacketId,
	const Identity &identity,
	const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> &metaData)
{
	Metrics::network_config_request++;
	auto tid = std::this_thread::get_id();
	std::stringstream ss; ss << tid;
	std::string threadID = ss.str();
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	auto b1 = _member_status_lookup.Add({{"thread", threadID}});
	auto c1 = _member_status_lookup_count.Add({{"thread", threadID}});
	c1++;
	b1.start();
#endif

	char nwids[24];
	DB::NetworkSummaryInfo ns;
	json network,member;

	if (((!_signingId)||(!_signingId.hasPrivate()))||(_signingId.address().toInt() != (nwid >> 24))||(!_sender)) {
		return;
	}

	const int64_t now = OSUtils::now();

#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	b1.stop();
	auto b2 = _node_is_online.Add({{"thread",threadID}});
	auto c2 = _node_is_online_count.Add({{"thread",threadID}});
	c2++;
	b2.start();
#endif
	_db.nodeIsOnline(nwid,identity.address().toInt(),fromAddr);
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	b2.stop();

	auto b3 = _get_and_init_member.Add({{"thread", threadID}});
	auto c3 = _get_and_init_member_count.Add({{"thread",threadID}});
	c3++;
	b3.start();
#endif
	Utils::hex(nwid,nwids);
	_db.get(nwid,network,identity.address().toInt(),member,ns);
	if ((!network.is_object())||(network.empty())) {
		_sender->ncSendError(nwid,requestPacketId,identity.address(),NetworkController::NC_ERROR_OBJECT_NOT_FOUND, nullptr, 0);
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
		b3.stop();
#endif
		return;
	}
	const bool newMember = ((!member.is_object())||(member.empty()));
	DB::initMember(member);
	_MemberStatusKey msk(nwid,identity.address().toInt());
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	b3.stop();
#endif

	{
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
		auto b4 = _have_identity.Add({{"thread",threadID}});
		auto c4 = _have_identity_count.Add({{"thread",threadID}});
		c4++;
		b4.start();
#endif
		const std::string haveIdStr(OSUtils::jsonString(member["identity"],""));
		if (haveIdStr.length() > 0) {
			// If we already know this member's identity perform a full compare. This prevents
			// a "collision" from being able to auth onto our network in place of an already
			// known member.
			try {
				if (Identity(haveIdStr.c_str()) != identity) {
					_sender->ncSendError(nwid,requestPacketId,identity.address(),NetworkController::NC_ERROR_ACCESS_DENIED, nullptr, 0);
					#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
					b4.stop();
					#endif
					return;
				}
			} catch ( ... ) {
				_sender->ncSendError(nwid,requestPacketId,identity.address(),NetworkController::NC_ERROR_ACCESS_DENIED, nullptr, 0);
				#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
				b4.stop();
				#endif
				return;
			}
		} else {
			// If we do not yet know this member's identity, learn it.
			char idtmp[1024];
			member["identity"] = identity.toString(false,idtmp);
		}
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
		b4.stop();
#endif
	}

	// These are always the same, but make sure they are set
	{
		char tmpid[128];
		const std::string addrs(identity.address().toString(tmpid));
		member["id"] = addrs;
		member["address"] = addrs;
		member["nwid"] = nwids;
	}

	// Determine whether and how member is authorized
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	auto b5 = _determine_auth.Add({{"thread",threadID}});
	auto c5 = _determine_auth_count.Add({{"thread",threadID}});
	c5++;
	b5.start();
#endif
	bool authorized = false;
	bool autoAuthorized = false;
	json autoAuthCredentialType,autoAuthCredential;
	if (OSUtils::jsonBool(member["authorized"],false)) {
		authorized = true;
	} else if (!OSUtils::jsonBool(network["private"],true)) {
		authorized = true;
		autoAuthorized = true;
		autoAuthCredentialType = "public";
	} else {
		char presentedAuth[512];
		if (metaData.get(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_AUTH,presentedAuth,sizeof(presentedAuth)) > 0) {
			presentedAuth[511] = (char)0; // sanity check
			if ((strlen(presentedAuth) > 6)&&(!strncmp(presentedAuth,"token:",6))) {
				const char *const presentedToken = presentedAuth + 6;
				json authTokens(network["authTokens"]);
				json &tokenExpires = authTokens[presentedToken];
				if (tokenExpires.is_number()) {
					if ((tokenExpires == 0)||(tokenExpires > now)) {
						authorized = true;
						autoAuthorized = true;
						autoAuthCredentialType = "token";
						autoAuthCredential = presentedToken;
					}
				}
			}
		}
	}

	// If we auto-authorized, update member record
	if ((autoAuthorized)&&(authorized)) {
		member["authorized"] = true;
		member["lastAuthorizedTime"] = now;
		member["lastAuthorizedCredentialType"] = autoAuthCredentialType;
		member["lastAuthorizedCredential"] = autoAuthCredential;
	}
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	b5.stop();
#endif

	// Should we check SSO Stuff?
	// If network is configured with SSO, and the member is not marked exempt: yes
	// Otherwise no, we use standard auth logic.
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	auto b6 = _sso_check.Add({{"thread",threadID}});
	auto c6 = _sso_check_count.Add({{"thread",threadID}});
	c6++;
	b6.start();
#endif
	AuthInfo info;
	int64_t authenticationExpiryTime = -1;
	bool networkSSOEnabled = OSUtils::jsonBool(network["ssoEnabled"], false);
	bool memberSSOExempt = OSUtils::jsonBool(member["ssoExempt"], false);
	if (networkSSOEnabled && !memberSSOExempt) {
		authenticationExpiryTime = (int64_t)OSUtils::jsonInt(member["authenticationExpiryTime"], 0);
		info = _db.getSSOAuthInfo(member, _ssoRedirectURL);
		assert(info.enabled == networkSSOEnabled);
		if (authenticationExpiryTime <= now) {
			if (info.version == 0) {
				Dictionary<4096> authInfo;
				authInfo.add(ZT_AUTHINFO_DICT_KEY_VERSION, (uint64_t)0ULL);
				authInfo.add(ZT_AUTHINFO_DICT_KEY_AUTHENTICATION_URL, info.authenticationURL.c_str());
				_sender->ncSendError(nwid,requestPacketId,identity.address(),NetworkController::NC_ERROR_AUTHENTICATION_REQUIRED, authInfo.data(), authInfo.sizeBytes());
			} else if (info.version == 1) {
				Dictionary<8192> authInfo;
				authInfo.add(ZT_AUTHINFO_DICT_KEY_VERSION, info.version);
				authInfo.add(ZT_AUTHINFO_DICT_KEY_ISSUER_URL, info.issuerURL.c_str());
				authInfo.add(ZT_AUTHINFO_DICT_KEY_CENTRAL_ENDPOINT_URL, info.centralAuthURL.c_str());
				authInfo.add(ZT_AUTHINFO_DICT_KEY_NONCE, info.ssoNonce.c_str());
				authInfo.add(ZT_AUTHINFO_DICT_KEY_STATE, info.ssoState.c_str());
				authInfo.add(ZT_AUTHINFO_DICT_KEY_CLIENT_ID, info.ssoClientID.c_str());
				_sender->ncSendError(nwid,requestPacketId,identity.address(),NetworkController::NC_ERROR_AUTHENTICATION_REQUIRED, authInfo.data(), authInfo.sizeBytes());
			}
			DB::cleanMember(member);
			_db.save(member,true);
			#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
			b6.stop();
			#endif
			return;
		}
	}
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	b6.stop();

	auto b7 = _auth_check.Add({{"thread",threadID}});
	auto c7 = _auth_check_count.Add({{"thread",threadID}});
	c7++;
	b7.start();
#endif
	if (authorized) {
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
				std::lock_guard<std::mutex> l(_memberStatus_l);
				_MemberStatus &ms = _memberStatus[msk];
				ms.authenticationExpiryTime = authenticationExpiryTime;
				ms.vMajor = (int)vMajor;
				ms.vMinor = (int)vMinor;
				ms.vRev = (int)vRev;
				ms.vProto = (int)vProto;
				ms.lastRequestMetaData = metaData;
				ms.identity = identity;
			}

			if (authenticationExpiryTime > 0) {
				std::lock_guard<std::mutex> l(_expiringSoon_l);
				_expiringSoon.insert(std::pair<int64_t, _MemberStatusKey>(authenticationExpiryTime, msk));
			}
		}
	} else {
		// If they are not authorized, STOP!
		DB::cleanMember(member);
		_db.save(member,true);
		_sender->ncSendError(nwid,requestPacketId,identity.address(),NetworkController::NC_ERROR_ACCESS_DENIED, nullptr, 0);
		#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
		b7.stop();
		#endif
		return;
	}
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	b7.stop();
#endif

	// -------------------------------------------------------------------------
	// If we made it this far, they are authorized (and authenticated).
	// -------------------------------------------------------------------------

	// Default timeout: 15 minutes. Maximum: two hours. Can be specified by an optional field in the network config
	// if something longer than 15 minutes is desired. Minimum is 5 minutes since shorter than that would be flaky.
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	auto b8 = _json_schlep.Add({{"thread",threadID}});
	auto c8 = _json_schlep_count.Add({{"thread", threadID}});
	c8++;
	b8.start();
#endif
	int64_t credentialtmd = ZT_NETWORKCONFIG_DEFAULT_CREDENTIAL_TIME_DFL_MAX_DELTA;
	if (network.contains("certificateTimeoutWindowSize")) {
		credentialtmd = (int64_t)network["certificateTimeoutWindowSize"];
	}
	credentialtmd = std::max(std::min(credentialtmd, ZT_NETWORKCONFIG_DEFAULT_CREDENTIAL_TIME_MAX_MAX_DELTA), ZT_NETWORKCONFIG_DEFAULT_CREDENTIAL_TIME_MIN_MAX_DELTA);

	std::unique_ptr<NetworkConfig> nc(new NetworkConfig());

	nc->networkId = nwid;
	nc->type = OSUtils::jsonBool(network["private"],true) ? ZT_NETWORK_TYPE_PRIVATE : ZT_NETWORK_TYPE_PUBLIC;
	nc->timestamp = now;
	nc->credentialTimeMaxDelta = credentialtmd;
	nc->revision = OSUtils::jsonInt(network["revision"],0ULL);
	nc->issuedTo = identity.address();
	if (OSUtils::jsonBool(network["enableBroadcast"],true)) nc->flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_BROADCAST;
	Utils::scopy(nc->name,sizeof(nc->name),OSUtils::jsonString(network["name"],"").c_str());
	nc->mtu = std::max(std::min((unsigned int)OSUtils::jsonInt(network["mtu"],ZT_DEFAULT_MTU),(unsigned int)ZT_MAX_MTU),(unsigned int)ZT_MIN_MTU);
	nc->multicastLimit = (unsigned int)OSUtils::jsonInt(network["multicastLimit"],32ULL);

	nc->ssoEnabled = networkSSOEnabled; //OSUtils::jsonBool(network["ssoEnabled"], false);
	nc->ssoVersion = info.version;

	if (info.version == 0) {
		nc->authenticationExpiryTime = OSUtils::jsonInt(member["authenticationExpiryTime"], 0LL);
		if (!info.authenticationURL.empty()) {
			Utils::scopy(nc->authenticationURL, sizeof(nc->authenticationURL), info.authenticationURL.c_str());
		}
	}
	else if (info.version == 1) {
		nc->authenticationExpiryTime = OSUtils::jsonInt(member["authenticationExpiryTime"], 0LL);
		if (!info.authenticationURL.empty()) {
			Utils::scopy(nc->authenticationURL, sizeof(nc->authenticationURL), info.authenticationURL.c_str());
		}
		if (!info.centralAuthURL.empty()) {
			Utils::scopy(nc->centralAuthURL, sizeof(nc->centralAuthURL), info.centralAuthURL.c_str());
		}
		if (!info.issuerURL.empty()) {
			#ifdef ZT_DEBUG
			fprintf(stderr, "copying issuerURL to nc: %s\n", info.issuerURL.c_str());
			#endif
			Utils::scopy(nc->issuerURL, sizeof(nc->issuerURL), info.issuerURL.c_str());
		}
		if (!info.ssoNonce.empty()) {
			Utils::scopy(nc->ssoNonce, sizeof(nc->ssoNonce), info.ssoNonce.c_str());
		}
		if (!info.ssoState.empty()) {
			Utils::scopy(nc->ssoState, sizeof(nc->ssoState), info.ssoState.c_str());
		}
		if (!info.ssoClientID.empty()) {
			Utils::scopy(nc->ssoClientID, sizeof(nc->ssoClientID), info.ssoClientID.c_str());
		}
	}

	std::string rtt(OSUtils::jsonString(member["remoteTraceTarget"],""));
	if (rtt.length() == 10) {
		nc->remoteTraceTarget = Address(Utils::hexStrToU64(rtt.c_str()));
		nc->remoteTraceLevel = (Trace::Level)OSUtils::jsonInt(member["remoteTraceLevel"],0ULL);
	} else {
		rtt = OSUtils::jsonString(network["remoteTraceTarget"],"");
		if (rtt.length() == 10) {
			nc->remoteTraceTarget = Address(Utils::hexStrToU64(rtt.c_str()));
		} else {
			nc->remoteTraceTarget.zero();
		}
		nc->remoteTraceLevel = (Trace::Level)OSUtils::jsonInt(network["remoteTraceLevel"],0ULL);
	}

	for(std::vector<Address>::const_iterator ab(ns.activeBridges.begin());ab!=ns.activeBridges.end();++ab) {
		nc->addSpecialist(*ab,ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE);
	}

	json &v4AssignMode = network["v4AssignMode"];
	json &v6AssignMode = network["v6AssignMode"];
	json &ipAssignmentPools = network["ipAssignmentPools"];
	json &routes = network["routes"];
	json &rules = network["rules"];
	json &capabilities = network["capabilities"];
	json &tags = network["tags"];
	json &memberCapabilities = member["capabilities"];
	json &memberTags = member["tags"];
	json &dns = network["dns"];

	//fprintf(stderr, "IP Assignment Pools for Network %s: %s\n", nwids, OSUtils::jsonDump(ipAssignmentPools, 2).c_str());

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
				if ((cap)&&(cap->is_object())&&(!cap->empty())) {
					ZT_VirtualNetworkRule capr[ZT_MAX_CAPABILITY_RULES];
					unsigned int caprc = 0;
					json &caprj = (*cap)["rules"];
					if ((caprj.is_array())&&(!caprj.empty())) {
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
				const InetAddress t(target.get<std::string>().c_str());
				InetAddress v;
				if (via.is_string()) v.fromString(via.get<std::string>().c_str());
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
			if (ipAssignments[i].is_string()) {
				const std::string ips = ipAssignments[i];
				InetAddress ip(ips.c_str());

				int routedNetmaskBits = -1;
				for(unsigned int rk=0;rk<nc->routeCount;++rk) {
					if (reinterpret_cast<const InetAddress *>(&(nc->routes[rk].target))->containsAddress(ip)) {
						const int nb = (int)(reinterpret_cast<const InetAddress *>(&(nc->routes[rk].target))->netmaskBits());
						if (nb > routedNetmaskBits)
							routedNetmaskBits = nb;
					}
				}

				if (routedNetmaskBits >= 0) {
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
		}
	} else {
		ipAssignments = json::array();
	}

	if ( (ipAssignmentPools.is_array()) && ((v6AssignMode.is_object())&&(OSUtils::jsonBool(v6AssignMode["zt"],false))) && (!haveManagedIpv6AutoAssignment) && (!noAutoAssignIps) ) {
		for(unsigned long p=0;((p<ipAssignmentPools.size())&&(!haveManagedIpv6AutoAssignment));++p) {
			json &pool = ipAssignmentPools[p];
			if (pool.is_object()) {
				InetAddress ipRangeStart(OSUtils::jsonString(pool["ipRangeStart"],"").c_str());
				InetAddress ipRangeEnd(OSUtils::jsonString(pool["ipRangeEnd"],"").c_str());
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
							char tmpip[64];
							const std::string ipStr(ip6.toIpString(tmpip));
							if (std::find(ipAssignments.begin(),ipAssignments.end(),ipStr) == ipAssignments.end()) {
								ipAssignments.push_back(ipStr);
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
	}

	if ( (ipAssignmentPools.is_array()) && ((v4AssignMode.is_object())&&(OSUtils::jsonBool(v4AssignMode["zt"],false))) && (!haveManagedIpv4AutoAssignment) && (!noAutoAssignIps) ) {
		for(unsigned long p=0;((p<ipAssignmentPools.size())&&(!haveManagedIpv4AutoAssignment));++p) {
			json &pool = ipAssignmentPools[p];
			if (pool.is_object()) {
				InetAddress ipRangeStartIA(OSUtils::jsonString(pool["ipRangeStart"],"").c_str());
				InetAddress ipRangeEndIA(OSUtils::jsonString(pool["ipRangeEnd"],"").c_str());
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
						if ((ip & 0x000000ff) == 0x000000ff) {
							continue; // don't allow addresses that end in .255
						}

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
							char tmpip[64];
							const std::string ipStr(ip4.toIpString(tmpip));
							if (std::find(ipAssignments.begin(),ipAssignments.end(),ipStr) == ipAssignments.end()) {
								ipAssignments.push_back(ipStr);
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
	}
	
	if(dns.is_object()) {
		std::string domain = OSUtils::jsonString(dns["domain"],"");
		memcpy(nc->dns.domain, domain.c_str(), domain.size());
		json &addrArray = dns["servers"];
		if (addrArray.is_array()) {
			for(unsigned int j = 0; j < addrArray.size() && j < ZT_MAX_DNS_SERVERS; ++j) {
				json &addr = addrArray[j];
				nc->dns.server_addr[j] = InetAddress(OSUtils::jsonString(addr,"").c_str());
			}
		}
	} else {
		dns = json::object();
	}
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	b8.stop();
#endif

	// Issue a certificate of ownership for all static IPs
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	auto b9 = _issue_certificate.Add({{"thread",threadID}});
	auto c9 = _issue_certificate_count.Add({{"thread",threadID}});
	c9++;
	b9.start();
#endif
	if (nc->staticIpCount) {
		nc->certificatesOfOwnership[0] = CertificateOfOwnership(nwid,now,identity.address(),1);
		for(unsigned int i=0;i<nc->staticIpCount;++i) {
			nc->certificatesOfOwnership[0].addThing(nc->staticIps[i]);
		}
		nc->certificatesOfOwnership[0].sign(_signingId);
		nc->certificateOfOwnershipCount = 1;
	}

	CertificateOfMembership com(now,credentialtmd,nwid,identity);
	if (com.sign(_signingId)) {
		nc->com = com;
	} else {
		_sender->ncSendError(nwid,requestPacketId,identity.address(),NetworkController::NC_ERROR_INTERNAL_SERVER_ERROR, nullptr, 0);
		#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
		b9.stop();
		#endif
		return;
	}
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	b9.stop();

	auto b10 = _save_member.Add({{"thread",threadID}});
	auto c10 = _save_member_count.Add({{"thread",threadID}});
	c10++;
	b10.start();
#endif
	DB::cleanMember(member);
	_db.save(member,true);
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	b10.stop();

	auto b11 = _send_netconf.Add({{"thread",threadID}});
	auto c11 = _send_netconf_count.Add({{"thread",threadID}});
	c11++;
	b11.start();
#endif
	_sender->ncSendConfig(nwid,requestPacketId,identity.address(),*(nc.get()),metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_VERSION,0) < 6);
#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	b11.stop();
#endif
}

void EmbeddedNetworkController::_startThreads()
{
	std::lock_guard<std::mutex> l(_threads_l);
	if (!_threads.empty()) {
		return;
	}
	const long hwc = std::max((long)std::thread::hardware_concurrency(),(long)1);
	for(long t=0;t<hwc;++t) {
		_threads.emplace_back([this]() {
			Metrics::network_config_request_threads++;
			for(;;) {
				_RQEntry *qe = (_RQEntry *)0;
				Metrics::network_config_request_queue_size = _queue.size();
				auto timedWaitResult = _queue.get(qe, 1000);
				if (timedWaitResult == BlockingQueue<_RQEntry *>::STOP) {
					break;
				} else if (timedWaitResult == BlockingQueue<_RQEntry *>::OK) {
					if (qe) {
						try {
							_request(qe->nwid,qe->fromAddr,qe->requestPacketId,qe->identity,qe->metaData);
						} catch (std::exception &e) {
							fprintf(stderr,"ERROR: exception in controller request handling thread: %s" ZT_EOL_S,e.what());
						} catch ( ... ) {
							fprintf(stderr,"ERROR: exception in controller request handling thread: unknown exception" ZT_EOL_S);
						}
						delete qe;
						qe = nullptr;
					}
				}
			}
			Metrics::network_config_request_threads--;
		});
	}
}

void EmbeddedNetworkController::_ssoExpiryThread() {
	while(_ssoExpiryRunning) {
		std::vector<_MemberStatusKey> expired;
		nlohmann::json network, member;
		int64_t now = OSUtils::now();
		{
			std::lock_guard<std::mutex> l(_expiringSoon_l);
			for(auto s=_expiringSoon.begin();s!=_expiringSoon.end();) {
				Metrics::sso_expiration_checks++;
				const int64_t when = s->first;
				if (when <= now) {
					// The user may have re-authorized, so we must actually look it up and check.
					network.clear();
					member.clear();
					if (_db.get(s->second.networkId, network, s->second.nodeId, member)) {
						int64_t authenticationExpiryTime = (int64_t)OSUtils::jsonInt(member["authenticationExpiryTime"], 0);
						if (authenticationExpiryTime <= now) {
							expired.push_back(s->second);
						}
					}
					s = _expiringSoon.erase(s);
				} else {
					// Don't bother going further into the future than necessary.
					break;
				}
			}
		}
		for(auto e=expired.begin();e!=expired.end();++e) {
			Metrics::sso_member_deauth++;
			onNetworkMemberDeauthorize(nullptr, e->networkId, e->nodeId);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

} // namespace ZeroTier
