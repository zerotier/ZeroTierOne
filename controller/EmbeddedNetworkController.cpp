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
#define ZT_NETCONF_DB_MEMBER_HISTORY_LENGTH 8

// Min duration between requests for an address/nwid combo to prevent floods
#define ZT_NETCONF_MIN_REQUEST_PERIOD 1000

// Nodes are considered active if they've queried in less than this long
#define ZT_NETCONF_NODE_ACTIVE_THRESHOLD ((ZT_NETWORK_AUTOCONF_DELAY * 2) + 5000)

namespace ZeroTier {

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
			r["zt"] = Address(rule.v.zt).toString();
			break;
		case ZT_NETWORK_RULE_ACTION_REDIRECT:
			r["type"] = "ACTION_REDIRECT";
			r["zt"] = Address(rule.v.zt).toString();
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
	std::string t = r["type"];
	memset(&rule,0,sizeof(ZT_VirtualNetworkRule));
	if (r.value("not",false))
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
		rule.v.zt = Utils::hexStrToU64(r.value("zt","0").c_str()) & 0xffffffffffULL;
		return true;
	} else if (t == "ACTION_REDIRECT") {
		rule.t |= ZT_NETWORK_RULE_ACTION_REDIRECT;
		rule.v.zt = Utils::hexStrToU64(r.value("zt","0").c_str()) & 0xffffffffffULL;
		return true;
	} else if (t == "MATCH_SOURCE_ZEROTIER_ADDRESS") {
		rule.t |= ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS;
		rule.v.zt = Utils::hexStrToU64(r.value("zt","0").c_str()) & 0xffffffffffULL;
		return true;
	} else if (t == "MATCH_DEST_ZEROTIER_ADDRESS") {
		rule.t |= ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS;
		rule.v.zt = Utils::hexStrToU64(r.value("zt","0").c_str()) & 0xffffffffffULL;
		return true;
	} else if (t == "MATCH_VLAN_ID") {
		rule.t |= ZT_NETWORK_RULE_MATCH_VLAN_ID;
		rule.v.vlanId = (uint16_t)(r.value("vlanId",0ULL) & 0xffffULL);
		return true;
	} else if (t == "MATCH_VLAN_PCP") {
		rule.t |= ZT_NETWORK_RULE_MATCH_VLAN_PCP;
		rule.v.vlanPcp = (uint8_t)(r.value("vlanPcp",0ULL) & 0xffULL);
		return true;
	} else if (t == "MATCH_VLAN_DEI") {
		rule.t |= ZT_NETWORK_RULE_MATCH_VLAN_DEI;
		rule.v.vlanDei = (uint8_t)(r.value("vlanDei",0ULL) & 0xffULL);
		return true;
	} else if (t == "MATCH_ETHERTYPE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_ETHERTYPE;
		rule.v.etherType = (uint16_t)(r.value("etherType",0ULL) & 0xffffULL);
		return true;
	} else if (t == "MATCH_MAC_SOURCE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_MAC_SOURCE;
		const std::string mac(r.value("mac","0"));
		Utils::unhex(mac.c_str(),(unsigned int)mac.length(),rule.v.mac,6);
		return true;
	} else if (t == "MATCH_MAC_DEST") {
		rule.t |= ZT_NETWORK_RULE_MATCH_MAC_DEST;
		const std::string mac(r.value("mac","0"));
		Utils::unhex(mac.c_str(),(unsigned int)mac.length(),rule.v.mac,6);
		return true;
	} else if (t == "MATCH_IPV4_SOURCE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV4_SOURCE;
		InetAddress ip(r.value("ip","0.0.0.0"));
		rule.v.ipv4.ip = reinterpret_cast<struct sockaddr_in *>(&ip)->sin_addr.s_addr;
		rule.v.ipv4.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in *>(&ip)->sin_port) & 0xff;
		if (rule.v.ipv4.mask > 32) rule.v.ipv4.mask = 32;
		return true;
	} else if (t == "MATCH_IPV4_DEST") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV4_DEST;
		InetAddress ip(r.value("ip","0.0.0.0"));
		rule.v.ipv4.ip = reinterpret_cast<struct sockaddr_in *>(&ip)->sin_addr.s_addr;
		rule.v.ipv4.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in *>(&ip)->sin_port) & 0xff;
		if (rule.v.ipv4.mask > 32) rule.v.ipv4.mask = 32;
		return true;
	} else if (t == "MATCH_IPV6_SOURCE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV6_SOURCE;
		InetAddress ip(r.value("ip","::0"));
		memcpy(rule.v.ipv6.ip,reinterpret_cast<struct sockaddr_in6 *>(&ip)->sin6_addr.s6_addr,16);
		rule.v.ipv6.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in6 *>(&ip)->sin6_port) & 0xff;
		if (rule.v.ipv6.mask > 128) rule.v.ipv6.mask = 128;
		return true;
	} else if (t == "MATCH_IPV6_DEST") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IPV6_DEST;
		InetAddress ip(r.value("ip","::0"));
		memcpy(rule.v.ipv6.ip,reinterpret_cast<struct sockaddr_in6 *>(&ip)->sin6_addr.s6_addr,16);
		rule.v.ipv6.mask = Utils::ntoh(reinterpret_cast<struct sockaddr_in6 *>(&ip)->sin6_port) & 0xff;
		if (rule.v.ipv6.mask > 128) rule.v.ipv6.mask = 128;
		return true;
	} else if (t == "MATCH_IP_TOS") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IP_TOS;
		rule.v.ipTos = (uint8_t)(r.value("ipTos",0ULL) & 0xffULL);
		return true;
	} else if (t == "MATCH_IP_PROTOCOL") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IP_PROTOCOL;
		rule.v.ipProtocol = (uint8_t)(r.value("ipProtocol",0ULL) & 0xffULL);
		return true;
	} else if (t == "MATCH_IP_SOURCE_PORT_RANGE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE;
		rule.v.port[0] = (uint16_t)(r.value("start",0ULL) & 0xffffULL);
		rule.v.port[1] = (uint16_t)(r.value("end",(uint64_t)rule.v.port[0]) & 0xffffULL);
		return true;
	} else if (t == "MATCH_IP_DEST_PORT_RANGE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE;
		rule.v.port[0] = (uint16_t)(r.value("start",0ULL) & 0xffffULL);
		rule.v.port[1] = (uint16_t)(r.value("end",(uint64_t)rule.v.port[0]) & 0xffffULL);
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
		rule.v.frameSize[0] = (uint16_t)(r.value("start",0ULL) & 0xffffULL);
		rule.v.frameSize[1] = (uint16_t)(r.value("end",(uint64_t)rule.v.frameSize[0]) & 0xffffULL);
		return true;
	} else if (t == "MATCH_TAGS_SAMENESS") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_SAMENESS;
		rule.v.tag.id = (uint32_t)(r.value("id",0ULL) & 0xffffffffULL);
		rule.v.tag.value = (uint32_t)(r.value("value",0ULL) & 0xffffffffULL);
		return true;
	} else if (t == "MATCH_TAGS_BITWISE_AND") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND;
		rule.v.tag.id = (uint32_t)(r.value("id",0ULL) & 0xffffffffULL);
		rule.v.tag.value = (uint32_t)(r.value("value",0ULL) & 0xffffffffULL);
		return true;
	} else if (t == "MATCH_TAGS_BITWISE_OR") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR;
		rule.v.tag.id = (uint32_t)(r.value("id",0ULL) & 0xffffffffULL);
		rule.v.tag.value = (uint32_t)(r.value("value",0ULL) & 0xffffffffULL);
		return true;
	} else if (t == "MATCH_TAGS_BITWISE_XOR") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR;
		rule.v.tag.id = (uint32_t)(r.value("id",0ULL) & 0xffffffffULL);
		rule.v.tag.value = (uint32_t)(r.value("value",0ULL) & 0xffffffffULL);
		return true;
	}
	return false;
}

EmbeddedNetworkController::EmbeddedNetworkController(Node *node,const char *dbPath) :
	_node(node),
	_path(dbPath)
{
	OSUtils::mkdir(dbPath);
	/*
	if (sqlite3_open_v2(dbPath,&_db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,(const char *)0) != SQLITE_OK)
		throw std::runtime_error("SqliteNetworkController cannot open database file");
	sqlite3_busy_timeout(_db,10000);

	sqlite3_exec(_db,"PRAGMA synchronous = OFF",0,0,0);
	sqlite3_exec(_db,"PRAGMA journal_mode = MEMORY",0,0,0);

	sqlite3_stmt *s = (sqlite3_stmt *)0;
	if ((sqlite3_prepare_v2(_db,"SELECT v FROM Config WHERE k = 'schemaVersion';",-1,&s,(const char **)0) == SQLITE_OK)&&(s)) {
		int schemaVersion = -1234;
		if (sqlite3_step(s) == SQLITE_ROW) {
			schemaVersion = sqlite3_column_int(s,0);
		}

		sqlite3_finalize(s);

		if (schemaVersion == -1234) {
			sqlite3_close(_db);
			throw std::runtime_error("SqliteNetworkController schemaVersion not found in Config table (init failure?)");
		}

		if (schemaVersion < 2) {
			// Create NodeHistory table to upgrade from version 1 to version 2
			if (sqlite3_exec(_db,
					"CREATE TABLE NodeHistory (\n"
					"  nodeId char(10) NOT NULL REFERENCES Node(id) ON DELETE CASCADE,\n"
					"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"
					"  networkVisitCounter INTEGER NOT NULL DEFAULT(0),\n"
					"  networkRequestAuthorized INTEGER NOT NULL DEFAULT(0),\n"
					"  requestTime INTEGER NOT NULL DEFAULT(0),\n"
					"  clientMajorVersion INTEGER NOT NULL DEFAULT(0),\n"
					"  clientMinorVersion INTEGER NOT NULL DEFAULT(0),\n"
					"  clientRevision INTEGER NOT NULL DEFAULT(0),\n"
					"  networkRequestMetaData VARCHAR(1024),\n"
					"  fromAddress VARCHAR(128)\n"
					");\n"
					"CREATE INDEX NodeHistory_nodeId ON NodeHistory (nodeId);\n"
					"CREATE INDEX NodeHistory_networkId ON NodeHistory (networkId);\n"
					"CREATE INDEX NodeHistory_requestTime ON NodeHistory (requestTime);\n"
					"UPDATE \"Config\" SET \"v\" = 2 WHERE \"k\" = 'schemaVersion';\n"
				,0,0,0) != SQLITE_OK) {
				char err[1024];
				Utils::snprintf(err,sizeof(err),"SqliteNetworkController cannot upgrade the database to version 2: %s",sqlite3_errmsg(_db));
				sqlite3_close(_db);
				throw std::runtime_error(err);
			} else {
				schemaVersion = 2;
			}
		}

		if (schemaVersion < 3) {
			// Create Route table to upgrade from version 2 to version 3 and migrate old
			// data. Also delete obsolete Gateway table that was never actually used, and
			// migrate Network flags to a bitwise flags field instead of ASCII cruft.
			if (sqlite3_exec(_db,
					"DROP TABLE Gateway;\n"
					"CREATE TABLE Route (\n"
					"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"
					"  target blob(16) NOT NULL,\n"
					"  via blob(16),\n"
					"  targetNetmaskBits integer NOT NULL,\n"
					"  ipVersion integer NOT NULL,\n"
					"  flags integer NOT NULL,\n"
					"  metric integer NOT NULL\n"
					");\n"
					"CREATE INDEX Route_networkId ON Route (networkId);\n"
					"INSERT INTO Route SELECT DISTINCT networkId,\"ip\" AS \"target\",NULL AS \"via\",ipNetmaskBits AS targetNetmaskBits,ipVersion,0 AS \"flags\",0 AS \"metric\" FROM IpAssignment WHERE nodeId IS NULL AND \"type\" = 1;\n"
					"ALTER TABLE Network ADD COLUMN \"flags\" integer NOT NULL DEFAULT(0);\n"
					"UPDATE Network SET \"flags\" = (\"flags\" | 1) WHERE v4AssignMode = 'zt';\n"
					"UPDATE Network SET \"flags\" = (\"flags\" | 2) WHERE v6AssignMode = 'rfc4193';\n"
					"UPDATE Network SET \"flags\" = (\"flags\" | 4) WHERE v6AssignMode = '6plane';\n"
					"ALTER TABLE Member ADD COLUMN \"flags\" integer NOT NULL DEFAULT(0);\n"
					"DELETE FROM IpAssignment WHERE nodeId IS NULL AND \"type\" = 1;\n"
					"UPDATE \"Config\" SET \"v\" = 3 WHERE \"k\" = 'schemaVersion';\n"
				,0,0,0) != SQLITE_OK) {
				char err[1024];
				Utils::snprintf(err,sizeof(err),"SqliteNetworkController cannot upgrade the database to version 3: %s",sqlite3_errmsg(_db));
				sqlite3_close(_db);
				throw std::runtime_error(err);
			} else {
				schemaVersion = 3;
			}
		}

		if (schemaVersion < 4) {
			// Turns out this was overkill and a huge performance drag. Will be revisiting this
			// more later but for now a brief snapshot of recent history stored in Member is fine.
			// Also prepare for implementation of proof of work requests.
			if (sqlite3_exec(_db,
					"DROP TABLE NodeHistory;\n"
					"ALTER TABLE Member ADD COLUMN lastRequestTime integer NOT NULL DEFAULT(0);\n"
					"ALTER TABLE Member ADD COLUMN lastPowDifficulty integer NOT NULL DEFAULT(0);\n"
					"ALTER TABLE Member ADD COLUMN lastPowTime integer NOT NULL DEFAULT(0);\n"
					"ALTER TABLE Member ADD COLUMN recentHistory blob;\n"
					"CREATE INDEX Member_networkId_lastRequestTime ON Member(networkId, lastRequestTime);\n"
					"UPDATE \"Config\" SET \"v\" = 4 WHERE \"k\" = 'schemaVersion';\n"
				,0,0,0) != SQLITE_OK) {
				char err[1024];
				Utils::snprintf(err,sizeof(err),"SqliteNetworkController cannot upgrade the database to version 3: %s",sqlite3_errmsg(_db));
				sqlite3_close(_db);
				throw std::runtime_error(err);
			} else {
				schemaVersion = 4;
			}
		}

		if (schemaVersion < 5) {
			// Upgrade old rough draft Rule table to new release format
			if (sqlite3_exec(_db,
					"DROP TABLE Relay;\n"
					"DROP INDEX Rule_networkId_ruleNo;\n"
					"ALTER TABLE \"Rule\" RENAME TO RuleOld;\n"
					"CREATE TABLE Rule (\n"
					"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"
					"  capId integer,\n"
					"  ruleNo integer NOT NULL,\n"
					"  ruleType integer NOT NULL DEFAULT(0),\n"
					"  \"addr\" blob(16),\n"
					"  \"int1\" integer,\n"
					"  \"int2\" integer,\n"
					"  \"int3\" integer,\n"
					"  \"int4\" integer\n"
					");\n"
					"INSERT INTO \"Rule\" SELECT networkId,(ruleNo*2) AS ruleNo,37 AS \"ruleType\",etherType AS \"int1\" FROM RuleOld WHERE RuleOld.etherType IS NOT NULL AND RuleOld.etherType > 0;\n"
					"INSERT INTO \"Rule\" SELECT networkId,((ruleNo*2)+1) AS ruleNo,1 AS \"ruleType\" FROM RuleOld;\n"
					"DROP TABLE RuleOld;\n"
					"CREATE INDEX Rule_networkId_capId ON Rule (networkId,capId);\n"
					"CREATE TABLE MemberTC (\n"
					"  networkId char(16) NOT NULL REFERENCES Network(id) ON DELETE CASCADE,\n"
					"  nodeId char(10) NOT NULL REFERENCES Node(id) ON DELETE CASCADE,\n"
					"  tagId integer,\n"
					"  tagValue integer,\n"
					"  capId integer,\n"
					"  capMaxCustodyChainLength integer NOT NULL DEFAULT(1)\n"
					");\n"
					"CREATE INDEX MemberTC_networkId_nodeId ON MemberTC (networkId,nodeId);\n"
					"UPDATE \"Config\" SET \"v\" = 5 WHERE \"k\" = 'schemaVersion';\n"
				,0,0,0) != SQLITE_OK) {
				char err[1024];
				Utils::snprintf(err,sizeof(err),"SqliteNetworkController cannot upgrade the database to version 3: %s",sqlite3_errmsg(_db));
				sqlite3_close(_db);
				throw std::runtime_error(err);
			} else {
				schemaVersion = 5;
			}
		}

		if (schemaVersion != ZT_NETCONF_SQLITE_SCHEMA_VERSION) {
			sqlite3_close(_db);
			throw std::runtime_error("SqliteNetworkController database schema version mismatch");
		}
	} else {
		// Prepare statement will fail if Config table doesn't exist, which means our DB
		// needs to be initialized.
		if (sqlite3_exec(_db,ZT_NETCONF_SCHEMA_SQL"INSERT INTO Config (k,v) VALUES ('schemaVersion',"ZT_NETCONF_SQLITE_SCHEMA_VERSION_STR");",0,0,0) != SQLITE_OK) {
			char err[1024];
			Utils::snprintf(err,sizeof(err),"SqliteNetworkController cannot initialize database and/or insert schemaVersion into Config table: %s",sqlite3_errmsg(_db));
			sqlite3_close(_db);
			throw std::runtime_error(err);
		}
	}

	if (

			  (sqlite3_prepare_v2(_db,"SELECT name,private,enableBroadcast,allowPassiveBridging,\"flags\",multicastLimit,creationTime,revision,memberRevisionCounter,(SELECT COUNT(1) FROM Member WHERE Member.networkId = Network.id AND Member.authorized > 0) FROM Network WHERE id = ?",-1,&_sGetNetworkById,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT revision FROM Network WHERE id = ?",-1,&_sGetNetworkRevision,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Network SET revision = ? WHERE id = ?",-1,&_sSetNetworkRevision,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Network (id,name,creationTime,revision) VALUES (?,?,?,1)",-1,&_sCreateNetwork,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Network WHERE id = ?",-1,&_sDeleteNetwork,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT id FROM Network ORDER BY id ASC",-1,&_sListNetworks,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Network SET memberRevisionCounter = (memberRevisionCounter + 1) WHERE id = ?",-1,&_sIncrementMemberRevisionCounter,(const char **)0) != SQLITE_OK)

			||(sqlite3_prepare_v2(_db,"SELECT identity FROM Node WHERE id = ?",-1,&_sGetNodeIdentity,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT OR REPLACE INTO Node (id,identity) VALUES (?,?)",-1,&_sCreateOrReplaceNode,(const char **)0) != SQLITE_OK)

			||(sqlite3_prepare_v2(_db,"INSERT INTO Rule (networkId,ruleNo,nodeId,ztSource,ztDest,vlanId,vlanPcp,vlanDei,) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",-1,&_sCreateRule,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT ruleNo,nodeId,sourcePort,destPort,vlanId,vlanPcp,etherType,macSource,macDest,ipSource,ipDest,ipTos,ipProtocol,ipSourcePort,ipDestPort,\"flags\",invFlags,\"action\" FROM Rule WHERE networkId = ? ORDER BY ruleNo ASC",-1,&_sListRules,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Rule WHERE networkId = ?",-1,&_sDeleteRulesForNetwork,(const char **)0) != SQLITE_OK)

			||(sqlite3_prepare_v2(_db,"SELECT ipRangeStart,ipRangeEnd FROM IpAssignmentPool WHERE networkId = ? AND ipVersion = ?",-1,&_sGetIpAssignmentPools,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT ipRangeStart,ipRangeEnd,ipVersion FROM IpAssignmentPool WHERE networkId = ? ORDER BY ipRangeStart ASC",-1,&_sGetIpAssignmentPools2,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO IpAssignmentPool (networkId,ipRangeStart,ipRangeEnd,ipVersion) VALUES (?,?,?,?)",-1,&_sCreateIpAssignmentPool,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM IpAssignmentPool WHERE networkId = ?",-1,&_sDeleteIpAssignmentPoolsForNetwork,(const char **)0) != SQLITE_OK)

			||(sqlite3_prepare_v2(_db,"SELECT ip,ipNetmaskBits,ipVersion FROM IpAssignment WHERE networkId = ? AND nodeId = ? AND \"type\" = 0 ORDER BY ip ASC",-1,&_sGetIpAssignmentsForNode,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT 1 FROM IpAssignment WHERE networkId = ? AND ip = ? AND ipVersion = ? AND \"type\" = ?",-1,&_sCheckIfIpIsAllocated,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO IpAssignment (networkId,nodeId,\"type\",ip,ipNetmaskBits,ipVersion) VALUES (?,?,?,?,?,?)",-1,&_sAllocateIp,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM IpAssignment WHERE networkId = ? AND nodeId = ? AND \"type\" = ?",-1,&_sDeleteIpAllocations,(const char **)0) != SQLITE_OK)

			||(sqlite3_prepare_v2(_db,"SELECT rowid,authorized,activeBridge,memberRevision,\"flags\",lastRequestTime,recentHistory FROM Member WHERE networkId = ? AND nodeId = ?",-1,&_sGetMember,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT m.authorized,m.activeBridge,m.memberRevision,n.identity,m.flags,m.lastRequestTime,m.recentHistory FROM Member AS m LEFT OUTER JOIN Node AS n ON n.id = m.nodeId WHERE m.networkId = ? AND m.nodeId = ?",-1,&_sGetMember2,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Member (networkId,nodeId,authorized,activeBridge,memberRevision) VALUES (?,?,?,0,(SELECT memberRevisionCounter FROM Network WHERE id = ?))",-1,&_sCreateMember,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT nodeId FROM Member WHERE networkId = ? AND activeBridge > 0 AND authorized > 0",-1,&_sGetActiveBridges,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT m.nodeId,m.memberRevision FROM Member AS m WHERE m.networkId = ? ORDER BY m.nodeId ASC",-1,&_sListNetworkMembers,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Member SET authorized = ?,memberRevision = (SELECT memberRevisionCounter FROM Network WHERE id = ?) WHERE rowid = ?",-1,&_sUpdateMemberAuthorized,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Member SET activeBridge = ?,memberRevision = (SELECT memberRevisionCounter FROM Network WHERE id = ?) WHERE rowid = ?",-1,&_sUpdateMemberActiveBridge,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Member SET \"lastRequestTime\" = ?, \"recentHistory\" = ? WHERE rowid = ?",-1,&_sUpdateMemberHistory,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Member WHERE networkId = ? AND nodeId = ?",-1,&_sDeleteMember,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Member WHERE networkId = ?",-1,&_sDeleteAllNetworkMembers,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT nodeId,recentHistory FROM Member WHERE networkId = ? AND lastRequestTime >= ?",-1,&_sGetActiveNodesOnNetwork,(const char **)0) != SQLITE_OK)

			||(sqlite3_prepare_v2(_db,"INSERT INTO Route (networkId,target,via,targetNetmaskBits,ipVersion,flags,metric) VALUES (?,?,?,?,?,?,?)",-1,&_sCreateRoute,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT DISTINCT target,via,targetNetmaskBits,ipVersion,flags,metric FROM \"Route\" WHERE networkId = ? ORDER BY ipVersion,target,via",-1,&_sGetRoutes,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM \"Route\" WHERE networkId = ?",-1,&_sDeleteRoutes,(const char **)0) != SQLITE_OK)

			||(sqlite3_prepare_v2(_db,"SELECT \"v\" FROM \"Config\" WHERE \"k\" = ?",-1,&_sGetConfig,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT OR REPLACE INTO \"Config\" (\"k\",\"v\") VALUES (?,?)",-1,&_sSetConfig,(const char **)0) != SQLITE_OK)

		 ) {
		std::string err(std::string("SqliteNetworkController unable to initialize one or more prepared statements: ") + sqlite3_errmsg(_db));
		sqlite3_close(_db);
		throw std::runtime_error(err);
	}

	sqlite3_reset(_sGetConfig);
	sqlite3_bind_text(_sGetConfig,1,"instanceId",10,SQLITE_STATIC);
	if (sqlite3_step(_sGetConfig) != SQLITE_ROW) {
		unsigned char sr[32];
		Utils::getSecureRandom(sr,32);
		for(unsigned int i=0;i<32;++i)
			_instanceId.push_back("0123456789abcdef"[(unsigned int)sr[i] & 0xf]);

		sqlite3_reset(_sSetConfig);
		sqlite3_bind_text(_sSetConfig,1,"instanceId",10,SQLITE_STATIC);
		sqlite3_bind_text(_sSetConfig,2,_instanceId.c_str(),-1,SQLITE_STATIC);
		if (sqlite3_step(_sSetConfig) != SQLITE_DONE)
			throw std::runtime_error("SqliteNetworkController unable to read or initialize instanceId");
	} else {
		const char *iid = reinterpret_cast<const char *>(sqlite3_column_text(_sGetConfig,0));
		if (!iid)
			throw std::runtime_error("SqliteNetworkController unable to read instanceId (it's NULL)");
		_instanceId = iid;
	}

#ifdef ZT_NETCONF_SQLITE_TRACE
	sqlite3_trace(_db,sqliteTraceFunc,(void *)0);
#endif

	_backupThread = Thread::start(this);
	*/
}

EmbeddedNetworkController::~EmbeddedNetworkController()
{
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

	json network(_readJson(_networkJP(nwid,false)));
	if (!network.size())
		return NetworkController::NETCONF_QUERY_OBJECT_NOT_FOUND;
	const std::string memberJP(_memberJP(nwid,identity.address(),false));
	json member(_readJson(memberJP));

	{
		std::string haveIdStr = member.value("identity","");
		if (haveIdStr.length() > 0) {
			try {
				if (Identity(haveIdStr.c_str()) != identity)
					return NetworkController::NETCONF_QUERY_ACCESS_DENIED;
			} catch ( ... ) {
				return NetworkController::NETCONF_QUERY_ACCESS_DENIED;
			}
		} else {
			member["identity"] = identity.toString(false);
		}
	}

	// Make sure these are always present no matter what, and increment member revision since we will always at least log something
	member["id"] = identity.address().toString();
	member["address"] = member["id"];
	member["nwid"] = network["id"];
	member["memberRevision"] = member.value("memberRevision",0ULL) + 1;

	// Update member log
	{
		json rlEntry = json::object();
		rlEntry["ts"] = now;
		rlEntry["authorized"] = member["authorized"];
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

	// Stop if network is private and member is not authorized
	if ( (network.value("private",true)) && (!member.value("authorized",false)) ) {
		_writeJson(memberJP,member);
		return NetworkController::NETCONF_QUERY_ACCESS_DENIED;
	}
	// Else compose and send network config

	nc.networkId = nwid;
	nc.type = network.value("private",true) ? ZT_NETWORK_TYPE_PRIVATE : ZT_NETWORK_TYPE_PUBLIC;
	nc.timestamp = now;
	nc.revision = network.value("revision",0ULL);
	nc.issuedTo = identity.address();
	if (network.value("enableBroadcast",true)) nc.flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_BROADCAST;
	if (network.value("allowPassiveBridging",false)) nc.flags |= ZT_NETWORKCONFIG_FLAG_ALLOW_PASSIVE_BRIDGING;
	Utils::scopy(nc.name,sizeof(nc.name),network.value("name","").c_str());
	nc.multicastLimit = (unsigned int)network.value("multicastLimit",32ULL);

	bool amActiveBridge = false;
	{
		json ab = network["activeBridges"];
		if (ab.is_array()) {
			for(unsigned long i=0;i<ab.size();++i) {
				std::string a = ab[i];
				if (a.length() == ZT_ADDRESS_LENGTH_HEX) {
					const uint64_t ab2 = Utils::hexStrToU64(a.c_str());
					nc.addSpecialist(Address(ab2),ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE);
					if (identity.address().toInt() == ab2)
						amActiveBridge = true;
				}
			}
		}
	}

	auto v4AssignMode = network["v4AssignMode"];
	auto v6AssignMode = network["v6AssignMode"];
	auto ipAssignmentPools = network["ipAssignmentPools"];
	auto routes = network["routes"];
	auto rules = network["rules"];

	if (v6AssignMode.is_object()) {
		if ((v6AssignMode.value("rfc4193",false))&&(nc.staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)) {
			nc.staticIps[nc.staticIpCount++] = InetAddress::makeIpv6rfc4193(nwid,identity.address().toInt());
			nc.flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION;
		}
		if ((v6AssignMode.value("6plane",false))&&(nc.staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)) {
			nc.staticIps[nc.staticIpCount++] = InetAddress::makeIpv66plane(nwid,identity.address().toInt());
			nc.flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION;
		}
	}

	if (rules.is_array()) {
		for(unsigned long i=0;i<rules.size();++i) {
			if (nc.ruleCount >= ZT_MAX_NETWORK_RULES)
				break;
			auto rule = rules[i];
			if (_parseRule(rule,nc.rules[nc.ruleCount]))
				++nc.ruleCount;
		}
	}

	if (routes.is_array()) {
		for(unsigned long i=0;i<routes.size();++i) {
			if (nc.routeCount >= ZT_MAX_NETWORK_ROUTES)
				break;
			auto route = routes[i];
			InetAddress t(route.value("target",""));
			InetAddress v(route.value("via",""));
			if ((t)&&(v)&&(t.ss_family == v.ss_family)) {
				ZT_VirtualNetworkRoute *r = &(nc.routes[nc.routeCount]);
				*(reinterpret_cast<InetAddress *>(&(r->target))) = t;
				*(reinterpret_cast<InetAddress *>(&(r->via))) = v;
				++nc.routeCount;
			}
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

	std::set<InetAddress> allocatedIps;
	bool allocatedIpsLoaded = false;

	if ( (ipAssignmentPools.is_array()) && ((v6AssignMode.is_object())&&(v6AssignMode.value("zt",false))) && (!haveManagedIpv6AutoAssignment) && (!amActiveBridge) ) {
		if (!allocatedIpsLoaded) allocatedIps = _getAlreadyAllocatedIps(nwid);
		for(unsigned long p=0;((p<ipAssignmentPools.size())&&(!haveManagedIpv6AutoAssignment));++p) {
			auto pool = ipAssignmentPools[p];
			if (pool.is_object()) {
				InetAddress ipRangeStart(std::string(pool.value("ipRangeStart","")));
				InetAddress ipRangeEnd(std::string(pool.value("ipRangeEnd","")));
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
						if ((routedNetmaskBits > 0)&&(!allocatedIps.count(ip6))) {
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

	if ( (ipAssignmentPools.is_array()) && ((v4AssignMode.is_object())&&(v4AssignMode.value("zt",false))) && (!haveManagedIpv4AutoAssignment) && (!amActiveBridge) ) {
		if (!allocatedIpsLoaded) allocatedIps = _getAlreadyAllocatedIps(nwid);
		for(unsigned long p=0;((p<ipAssignmentPools.size())&&(!haveManagedIpv4AutoAssignment));++p) {
			auto pool = ipAssignmentPools[p];
			if (pool.is_object()) {
				InetAddress ipRangeStart(std::string(pool.value("ipRangeStart","")));
				InetAddress ipRangeEnd(std::string(pool.value("ipRangeEnd","")));
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
						if ((routedNetmaskBits > 0)&&(!allocatedIps.count(ip4))) {
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

	if (network.value("private",true)) {
		CertificateOfMembership com(now,ZT_NETWORK_COM_DEFAULT_REVISION_MAX_DELTA,nwid,identity.address());
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
									const std::string rc = member.value("memberRevision","0");
									responseBody.append(rc);
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
									if ((mostRecentLog.is_object())&&((uint64_t)mostRecentLog.value("ts",0ULL) >= threshold)) {
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

				nlohmann::json o(network);
				o["clock"] = OSUtils::now();
				responseBody = o.dump(2);
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

					try {
						if (b.count("authorized")) member["authorized"] = b.value("authorized",false);
						if ((b.count("identity"))&&(!member.count("identity"))) member["identity"] = b.value("identity",""); // allow identity to be populated only if not already known

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
					} catch ( ... ) {
						return 400;
					}

					if (!member.count("authorized")) member["authorized"] = false;
					if (!member.count("ipAssignments")) member["ipAssignments"] = json::array();
					if (!member.count("recentLog")) member["recentLog"] = json::array();

					member["id"] = addrs;
					member["address"] = addrs; // legacy
					member["nwid"] = nwids;
					member["memberRevision"] = member.value("memberRevision",0ULL) + 1;
					member["objtype"] = "member";

					_writeJson(_memberJP(nwid,Address(address),true).c_str(),member);

					member["clock"] = OSUtils::now();
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
					test->reportAtEveryHop = (b.value("reportAtEveryHop",true) ? 1 : 0);

					if (!test->hopCount) {
						::free((void *)test);
						return 400;
					}

					test->timestamp = OSUtils::now();

					_CircuitTestEntry &te = _circuitTests[test->testId];
					te.test = test;
					te.jsonResults = "";

					_node->circuitTestBegin(test,&(EmbeddedNetworkController::_circuitTestCallback));

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

				try {
					if (b.count("name")) network["name"] = b.value("name","");
					if (b.count("private")) network["private"] = b.value("private",true);
					if (b.count("enableBroadcast")) network["enableBroadcast"] = b.value("enableBroadcast",false);
					if (b.count("allowPassiveBridging")) network["allowPassiveBridging"] = b.value("allowPassiveBridging",false);
					if (b.count("multicastLimit")) network["multicastLimit"] = b.value("multicastLimit",32ULL);

					if (b.count("activeBridges")) {
						auto ab = b["activeBridges"];
						if (ab.is_array()) {
							json ab2 = json::array();
							for(unsigned long i=0;i<ab.size();++i) {
								std::string a = ab[i];
								if (a.length() == ZT_ADDRESS_LENGTH_HEX)
									ab2.push_back(a);
							}
							network["activeBridges"] = ab2;
						}
					}

					if (b.count("v4AssignMode")) {
						auto nv4m = network["v4AssignMode"];
						if (!nv4m.is_object()) nv4m = json::object();
						if (b["v4AssignMode"].is_string()) { // backward compatibility
							nv4m["zt"] = (b.value("v4AssignMode","") == "zt");
						} else if (b["v4AssignMode"].is_object()) {
							auto v4m = b["v4AssignMode"];
							if (v4m.count("zt")) nv4m["zt"] = v4m.value("zt",false);
						}
						if (!nv4m.count("zt")) nv4m["zt"] = false;
					}

					if (b.count("v6AssignMode")) {
						auto nv6m = network["v6AssignMode"];
						if (!nv6m.is_object()) nv6m = json::object();
						if (b["v6AssignMode"].is_string()) { // backward compatibility
							std::vector<std::string> v6m(Utils::split(b.value("v6AssignMode","").c_str(),",","",""));
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
							if (v6m.count("rfc4193")) nv6m["rfc4193"] = v6m.value("rfc4193",false);
							if (v6m.count("zt")) nv6m["rfc4193"] = v6m.value("zt",false);
							if (v6m.count("6plane")) nv6m["rfc4193"] = v6m.value("6plane",false);
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
									InetAddress t(rt.value("target",""));
									InetAddress v(rt.value("via",""));
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
									InetAddress f(ip.value("ipRangeStart",""));
									InetAddress t(ip.value("ipRangeEnd",""));
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
				} catch ( ... ) {
					return 400;
				}

				if (!network.count("private")) network["private"] = true;
				if (!network.count("creationTime")) network["creationTime"] = OSUtils::now();
				if (!network.count("name")) network["name"] = "";
				if (!network.count("multicastLimit")) network["multicastLimit"] = (uint64_t)32;
				if (!network.count("v4AssignMode")) network["v4AssignMode"] = "{\"zt\":false}"_json;
				if (!network.count("v6AssignMode")) network["v6AssignMode"] = "{\"rfc4193\":false,\"zt\":false,\"6plane\":false}"_json;
				if (!network.count("activeBridges")) network["activeBridges"] = json::array();

				if (!network.count("rules")) {
					// If unspecified, rules are set to allow anything and behave like a flat L2 segment
					network["rules"] = {
						{ "not",false },
						{ "type","ACTION_ACCEPT" }
					};
				}

				network["id"] = nwids;
				network["nwid"] = nwids; // legacy
				network["revision"] = network.value("revision",0ULL) + 1ULL;
				network["objtype"] = "network";

				_writeJson(_networkJP(nwid,true),network);

				network["clock"] = OSUtils::now();
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

} // namespace ZeroTier
