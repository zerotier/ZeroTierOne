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

#include "SqliteNetworkController.hpp"

#include "../node/Node.hpp"
#include "../node/Utils.hpp"
#include "../node/CertificateOfMembership.hpp"
#include "../node/NetworkConfig.hpp"
#include "../node/Dictionary.hpp"
#include "../node/InetAddress.hpp"
#include "../node/MAC.hpp"
#include "../node/Address.hpp"

// offbase includes and builds upon nlohmann::json
using json = nlohmann::json;

// Stored in database as schemaVersion key in Config.
// If not present, database is assumed to be empty and at the current schema version
// and this key/value is added automatically.
//#define ZT_NETCONF_SQLITE_SCHEMA_VERSION 5
//#define ZT_NETCONF_SQLITE_SCHEMA_VERSION_STR "5"

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
		rule.v.port[1] = (uint16_t)(r.value("end",0ULL) & 0xffffULL);
		return true;
	} else if (t == "MATCH_IP_DEST_PORT_RANGE") {
		rule.t |= ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE;
		rule.v.port[0] = (uint16_t)(r.value("start",0ULL) & 0xffffULL);
		rule.v.port[1] = (uint16_t)(r.value("end",0ULL) & 0xffffULL);
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
		rule.v.frameSize[0] = (uint16_t)(Utils::hexStrToU64(r.value("start","0").c_str()) & 0xffffULL);
		rule.v.frameSize[1] = (uint16_t)(Utils::hexStrToU64(r.value("end","0").c_str()) & 0xffffULL);
		return true;
	} else if (t == "MATCH_TAGS_SAMENESS") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_SAMENESS;
		rule.v.tag.id = (uint32_t)(Utils::hexStrToU64(r.value("id","0").c_str()) & 0xffffffffULL);
		rule.v.tag.value = (uint32_t)(Utils::hexStrToU64(r.value("value","0").c_str()) & 0xffffffffULL);
		return true;
	} else if (t == "MATCH_TAGS_BITWISE_AND") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND;
		rule.v.tag.id = (uint32_t)(Utils::hexStrToU64(r.value("id","0").c_str()) & 0xffffffffULL);
		rule.v.tag.value = (uint32_t)(Utils::hexStrToU64(r.value("value","0").c_str()) & 0xffffffffULL);
		return true;
	} else if (t == "MATCH_TAGS_BITWISE_OR") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR;
		rule.v.tag.id = (uint32_t)(Utils::hexStrToU64(r.value("id","0").c_str()) & 0xffffffffULL);
		rule.v.tag.value = (uint32_t)(Utils::hexStrToU64(r.value("value","0").c_str()) & 0xffffffffULL);
		return true;
	} else if (t == "MATCH_TAGS_BITWISE_XOR") {
		rule.t |= ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR;
		rule.v.tag.id = (uint32_t)(Utils::hexStrToU64(r.value("id","0").c_str()) & 0xffffffffULL);
		rule.v.tag.value = (uint32_t)(Utils::hexStrToU64(r.value("value","0").c_str()) & 0xffffffffULL);
		return true;
	}
	return false;
}

SqliteNetworkController::SqliteNetworkController(Node *node,const char *dbPath) :
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

SqliteNetworkController::~SqliteNetworkController()
{
}

NetworkController::ResultCode SqliteNetworkController::doNetworkConfigRequest(const InetAddress &fromAddr,const Identity &signingId,const Identity &identity,uint64_t nwid,const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> &metaData,NetworkConfig &nc)
{
	if (((!signingId)||(!signingId.hasPrivate()))||(signingId.address().toInt() != (nwid >> 24))) {
		return NetworkController::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
	}

	char nwids[24],nodeIds[24];
	Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);
	Utils::snprintf(nodeIds,sizeof(nodeIds),"%.10llx",(unsigned long long)identity.address().toInt());

	const uint64_t now = OSUtils::now();

	/*
	{ // begin lock
		Mutex::Lock _l(_lock);

		// Check rate limit circuit breaker to prevent flooding
		{
			uint64_t &lrt = _lastRequestTime[std::pair<uint64_t,uint64_t>(identity.address().toInt(),nwid)];
			if ((now - lrt) <= ZT_NETCONF_MIN_REQUEST_PERIOD)
				return NetworkController::NETCONF_QUERY_IGNORE;
			lrt = now;
		}

		_backupNeeded = true;

		// Create Node record or do full identity check if we already have one

		sqlite3_reset(_sGetNodeIdentity);
		sqlite3_bind_text(_sGetNodeIdentity,1,member.nodeId,10,SQLITE_STATIC);
		if (sqlite3_step(_sGetNodeIdentity) == SQLITE_ROW) {
			try {
				Identity alreadyKnownIdentity((const char *)sqlite3_column_text(_sGetNodeIdentity,0));
				if (alreadyKnownIdentity != identity)
					return NetworkController::NETCONF_QUERY_ACCESS_DENIED;
			} catch ( ... ) { // identity stored in database is not valid or is NULL
				return NetworkController::NETCONF_QUERY_ACCESS_DENIED;
			}
		} else {
			std::string idstr(identity.toString(false));
			sqlite3_reset(_sCreateOrReplaceNode);
			sqlite3_bind_text(_sCreateOrReplaceNode,1,member.nodeId,10,SQLITE_STATIC);
			sqlite3_bind_text(_sCreateOrReplaceNode,2,idstr.c_str(),-1,SQLITE_STATIC);
			if (sqlite3_step(_sCreateOrReplaceNode) != SQLITE_DONE) {
				return NetworkController::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
			}
		}

		// Fetch Network record

		sqlite3_reset(_sGetNetworkById);
		sqlite3_bind_text(_sGetNetworkById,1,network.id,16,SQLITE_STATIC);
		if (sqlite3_step(_sGetNetworkById) == SQLITE_ROW) {
			network.name = (const char *)sqlite3_column_text(_sGetNetworkById,0);
			network.isPrivate = (sqlite3_column_int(_sGetNetworkById,1) > 0);
			network.enableBroadcast = (sqlite3_column_int(_sGetNetworkById,2) > 0);
			network.allowPassiveBridging = (sqlite3_column_int(_sGetNetworkById,3) > 0);
			network.flags = sqlite3_column_int(_sGetNetworkById,4);
			network.multicastLimit = sqlite3_column_int(_sGetNetworkById,5);
			network.creationTime = (uint64_t)sqlite3_column_int64(_sGetNetworkById,6);
			network.revision = (uint64_t)sqlite3_column_int64(_sGetNetworkById,7);
			network.memberRevisionCounter = (uint64_t)sqlite3_column_int64(_sGetNetworkById,8);
		} else {
			return NetworkController::NETCONF_QUERY_OBJECT_NOT_FOUND;
		}

		// Fetch or create Member record

		sqlite3_reset(_sGetMember);
		sqlite3_bind_text(_sGetMember,1,network.id,16,SQLITE_STATIC);
		sqlite3_bind_text(_sGetMember,2,member.nodeId,10,SQLITE_STATIC);
		if (sqlite3_step(_sGetMember) == SQLITE_ROW) {
			member.rowid = sqlite3_column_int64(_sGetMember,0);
			member.authorized = (sqlite3_column_int(_sGetMember,1) > 0);
			member.activeBridge = (sqlite3_column_int(_sGetMember,2) > 0);
			member.lastRequestTime = (uint64_t)sqlite3_column_int64(_sGetMember,5);
			const char *rhblob = (const char *)sqlite3_column_blob(_sGetMember,6);
			if (rhblob)
				member.recentHistory.fromBlob(rhblob,(unsigned int)sqlite3_column_bytes(_sGetMember,6));
		} else {
			member.authorized = (network.isPrivate ? false : true);
			member.activeBridge = false;
			sqlite3_reset(_sCreateMember);
			sqlite3_bind_text(_sCreateMember,1,network.id,16,SQLITE_STATIC);
			sqlite3_bind_text(_sCreateMember,2,member.nodeId,10,SQLITE_STATIC);
			sqlite3_bind_int(_sCreateMember,3,(member.authorized ? 1 : 0));
			sqlite3_bind_text(_sCreateMember,4,network.id,16,SQLITE_STATIC);
			if (sqlite3_step(_sCreateMember) != SQLITE_DONE) {
				return NetworkController::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
			}
			member.rowid = sqlite3_last_insert_rowid(_db);

			sqlite3_reset(_sIncrementMemberRevisionCounter);
			sqlite3_bind_text(_sIncrementMemberRevisionCounter,1,network.id,16,SQLITE_STATIC);
			sqlite3_step(_sIncrementMemberRevisionCounter);
		}

		// Update Member.history

		{
			char mh[1024];
			Utils::snprintf(mh,sizeof(mh),
				"{\"ts\":%llu,\"authorized\":%s,\"clientMajorVersion\":%u,\"clientMinorVersion\":%u,\"clientRevision\":%u,\"fromAddr\":",
				(unsigned long long)now,
				((member.authorized) ? "true" : "false"),
				metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MAJOR_VERSION,0),
				metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MINOR_VERSION,0),
				metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_REVISION,0));
			member.recentHistory.push_front(std::string(mh));
			if (fromAddr) {
				member.recentHistory.front().push_back('"');
				member.recentHistory.front().append(_jsonEscape(fromAddr.toString()));
				member.recentHistory.front().append("\"}");
			} else {
				member.recentHistory.front().append("null}");
			}

			while (member.recentHistory.size() > ZT_NETCONF_DB_MEMBER_HISTORY_LENGTH)
				member.recentHistory.pop_back();
			std::string rhblob(member.recentHistory.toBlob());

			sqlite3_reset(_sUpdateMemberHistory);
			sqlite3_clear_bindings(_sUpdateMemberHistory);
			sqlite3_bind_int64(_sUpdateMemberHistory,1,(sqlite3_int64)now);
			sqlite3_bind_blob(_sUpdateMemberHistory,2,(const void *)rhblob.data(),(int)rhblob.length(),SQLITE_STATIC);
			sqlite3_bind_int64(_sUpdateMemberHistory,3,member.rowid);
			sqlite3_step(_sUpdateMemberHistory);
		}

		// Don't proceed if member is not authorized! ---------------------------

		if (!member.authorized)
			return NetworkController::NETCONF_QUERY_ACCESS_DENIED;

		// Create network configuration -- we create both legacy and new types and send both for backward compatibility

		// New network config structure
		nc.networkId = Utils::hexStrToU64(network.id);
		nc.type = network.isPrivate ? ZT_NETWORK_TYPE_PRIVATE : ZT_NETWORK_TYPE_PUBLIC;
		nc.timestamp = now;
		nc.revision = network.revision;
		nc.issuedTo = member.nodeId;
		if (network.enableBroadcast) nc.flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_BROADCAST;
		if (network.allowPassiveBridging) nc.flags |= ZT_NETWORKCONFIG_FLAG_ALLOW_PASSIVE_BRIDGING;
		memcpy(nc.name,network.name,std::min((unsigned int)ZT_MAX_NETWORK_SHORT_NAME_LENGTH,(unsigned int)strlen(network.name)));

		{	// TODO: right now only etherTypes are supported in rules
			std::vector<int> allowedEtherTypes;
			sqlite3_reset(_sGetEtherTypesFromRuleTable);
			sqlite3_bind_text(_sGetEtherTypesFromRuleTable,1,network.id,16,SQLITE_STATIC);
			while (sqlite3_step(_sGetEtherTypesFromRuleTable) == SQLITE_ROW) {
				if (sqlite3_column_type(_sGetEtherTypesFromRuleTable,0) == SQLITE_NULL) {
					allowedEtherTypes.clear();
					allowedEtherTypes.push_back(0); // NULL 'allow' matches ANY
					break;
				} else {
					int et = sqlite3_column_int(_sGetEtherTypesFromRuleTable,0);
					if ((et >= 0)&&(et <= 0xffff))
						allowedEtherTypes.push_back(et);
				}
			}
			std::sort(allowedEtherTypes.begin(),allowedEtherTypes.end());
			allowedEtherTypes.erase(std::unique(allowedEtherTypes.begin(),allowedEtherTypes.end()),allowedEtherTypes.end());

			for(long i=0;i<(long)allowedEtherTypes.size();++i) {
				if ((nc.ruleCount + 2) > ZT_MAX_NETWORK_RULES)
					break;
				if (allowedEtherTypes[i] > 0) {
					nc.rules[nc.ruleCount].t = ZT_NETWORK_RULE_MATCH_ETHERTYPE;
					nc.rules[nc.ruleCount].v.etherType = (uint16_t)allowedEtherTypes[i];
					++nc.ruleCount;
				}
				nc.rules[nc.ruleCount++].t = ZT_NETWORK_RULE_ACTION_ACCEPT;
			}
		}

		nc.multicastLimit = network.multicastLimit;

		bool amActiveBridge = false;
		{
			sqlite3_reset(_sGetActiveBridges);
			sqlite3_bind_text(_sGetActiveBridges,1,network.id,16,SQLITE_STATIC);
			while (sqlite3_step(_sGetActiveBridges) == SQLITE_ROW) {
				const char *ab = (const char *)sqlite3_column_text(_sGetActiveBridges,0);
				if ((ab)&&(strlen(ab) == 10)) {
					const uint64_t ab2 = Utils::hexStrToU64(ab);
					nc.addSpecialist(Address(ab2),ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE);
					if (!strcmp(member.nodeId,ab))
						amActiveBridge = true;
				}
			}
		}

		// Do not send relays to 1.1.0 since it had a serious bug in using them
		// 1.1.0 will still work, it'll just fall back to roots instead of using network preferred relays
		if (!((metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MAJOR_VERSION,0) == 1)&&(metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MINOR_VERSION,0) == 1)&&(metaData.getUI(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_REVISION,0) == 0))) {
			sqlite3_reset(_sGetRelays);
			sqlite3_bind_text(_sGetRelays,1,network.id,16,SQLITE_STATIC);
			while (sqlite3_step(_sGetRelays) == SQLITE_ROW) {
				const char *n = (const char *)sqlite3_column_text(_sGetRelays,0);
				const char *a = (const char *)sqlite3_column_text(_sGetRelays,1);
				if ((n)&&(a)) {
					Address node(n);
					InetAddress addr(a);
					if (node)
						nc.addSpecialist(node,ZT_NETWORKCONFIG_SPECIALIST_TYPE_NETWORK_PREFERRED_RELAY);
				}
			}
		}

		sqlite3_reset(_sGetRoutes);
		sqlite3_bind_text(_sGetRoutes,1,network.id,16,SQLITE_STATIC);
		while ((sqlite3_step(_sGetRoutes) == SQLITE_ROW)&&(nc.routeCount < ZT_MAX_NETWORK_ROUTES)) {
			ZT_VirtualNetworkRoute *r = &(nc.routes[nc.routeCount]);
			memset(r,0,sizeof(ZT_VirtualNetworkRoute));
			switch(sqlite3_column_int(_sGetRoutes,3)) { // ipVersion
				case 4:
					*(reinterpret_cast<InetAddress *>(&(r->target))) = InetAddress((const void *)((const char *)sqlite3_column_blob(_sGetRoutes,0) + 12),4,(unsigned int)sqlite3_column_int(_sGetRoutes,2));
					break;
				case 6:
					*(reinterpret_cast<InetAddress *>(&(r->target))) = InetAddress((const void *)sqlite3_column_blob(_sGetRoutes,0),16,(unsigned int)sqlite3_column_int(_sGetRoutes,2));
					break;
				default:
					continue;
			}
			if (sqlite3_column_type(_sGetRoutes,1) != SQLITE_NULL) {
				switch(sqlite3_column_int(_sGetRoutes,3)) { // ipVersion
					case 4:
						*(reinterpret_cast<InetAddress *>(&(r->via))) = InetAddress((const void *)((const char *)sqlite3_column_blob(_sGetRoutes,1) + 12),4,0);
						break;
					case 6:
						*(reinterpret_cast<InetAddress *>(&(r->via))) = InetAddress((const void *)sqlite3_column_blob(_sGetRoutes,1),16,0);
						break;
					default:
						continue;
				}
			}
			r->flags = (uint16_t)sqlite3_column_int(_sGetRoutes,4);
			r->metric = (uint16_t)sqlite3_column_int(_sGetRoutes,5);
			++nc.routeCount;
		}

		// Assign special IPv6 addresses if these are enabled
		if (((network.flags & ZT_DB_NETWORK_FLAG_ZT_MANAGED_V6_RFC4193) != 0)&&(nc.staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)) {
			nc.staticIps[nc.staticIpCount++] = InetAddress::makeIpv6rfc4193(nwid,identity.address().toInt());
			nc.flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION;
		}
		if (((network.flags & ZT_DB_NETWORK_FLAG_ZT_MANAGED_V6_6PLANE) != 0)&&(nc.staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)) {
			nc.staticIps[nc.staticIpCount++] = InetAddress::makeIpv66plane(nwid,identity.address().toInt());
			nc.flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION;
		}

		// Get managed addresses that are assigned to this member
		bool haveManagedIpv4AutoAssignment = false;
		bool haveManagedIpv6AutoAssignment = false; // "special" NDP-emulated address types do not count
		sqlite3_reset(_sGetIpAssignmentsForNode);
		sqlite3_bind_text(_sGetIpAssignmentsForNode,1,network.id,16,SQLITE_STATIC);
		sqlite3_bind_text(_sGetIpAssignmentsForNode,2,member.nodeId,10,SQLITE_STATIC);
		while (sqlite3_step(_sGetIpAssignmentsForNode) == SQLITE_ROW) {
			const unsigned char *const ipbytes = (const unsigned char *)sqlite3_column_blob(_sGetIpAssignmentsForNode,0);
			if ((!ipbytes)||(sqlite3_column_bytes(_sGetIpAssignmentsForNode,0) != 16))
				continue;
			//const int ipNetmaskBits = sqlite3_column_int(_sGetIpAssignmentsForNode,1);
			const int ipVersion = sqlite3_column_int(_sGetIpAssignmentsForNode,2);

			InetAddress ip;
			if (ipVersion == 4)
				ip = InetAddress(ipbytes + 12,4,0);
			else if (ipVersion == 6)
				ip = InetAddress(ipbytes,16,0);
			else continue;

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
				if (ipVersion == 4)
					haveManagedIpv4AutoAssignment = true;
				else if (ipVersion == 6)
					haveManagedIpv6AutoAssignment = true;
			}
		}

		// Auto-assign IPv6 address if auto-assignment is enabled and it's needed
		if ( ((network.flags & ZT_DB_NETWORK_FLAG_ZT_MANAGED_V6_AUTO_ASSIGN) != 0) && (!haveManagedIpv6AutoAssignment) && (!amActiveBridge) ) {
			sqlite3_reset(_sGetIpAssignmentPools);
			sqlite3_bind_text(_sGetIpAssignmentPools,1,network.id,16,SQLITE_STATIC);
			sqlite3_bind_int(_sGetIpAssignmentPools,2,6); // 6 == IPv6
			while (sqlite3_step(_sGetIpAssignmentPools) == SQLITE_ROW) {
				const uint8_t *const ipRangeStartB = reinterpret_cast<const unsigned char *>(sqlite3_column_blob(_sGetIpAssignmentPools,0));
				const uint8_t *const ipRangeEndB = reinterpret_cast<const unsigned char *>(sqlite3_column_blob(_sGetIpAssignmentPools,1));
				if ((!ipRangeStartB)||(!ipRangeEndB)||(sqlite3_column_bytes(_sGetIpAssignmentPools,0) != 16)||(sqlite3_column_bytes(_sGetIpAssignmentPools,1) != 16))
					continue;

				uint64_t s[2],e[2],x[2],xx[2];
				memcpy(s,ipRangeStartB,16);
				memcpy(e,ipRangeEndB,16);
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
					if (routedNetmaskBits > 0) {
						sqlite3_reset(_sCheckIfIpIsAllocated);
						sqlite3_bind_text(_sCheckIfIpIsAllocated,1,network.id,16,SQLITE_STATIC);
						sqlite3_bind_blob(_sCheckIfIpIsAllocated,2,(const void *)ip6.rawIpData(),16,SQLITE_STATIC);
						sqlite3_bind_int(_sCheckIfIpIsAllocated,3,6); // 6 == IPv6
						sqlite3_bind_int(_sCheckIfIpIsAllocated,4,(int)0);
						if (sqlite3_step(_sCheckIfIpIsAllocated) != SQLITE_ROW) {
							// No rows returned, so the IP is available
							sqlite3_reset(_sAllocateIp);
							sqlite3_bind_text(_sAllocateIp,1,network.id,16,SQLITE_STATIC);
							sqlite3_bind_text(_sAllocateIp,2,member.nodeId,10,SQLITE_STATIC);
							sqlite3_bind_int(_sAllocateIp,3,(int)0);
							sqlite3_bind_blob(_sAllocateIp,4,(const void *)ip6.rawIpData(),16,SQLITE_STATIC);
							sqlite3_bind_int(_sAllocateIp,5,routedNetmaskBits); // IP netmask bits from matching route
							sqlite3_bind_int(_sAllocateIp,6,6); // 6 == IPv6
							if (sqlite3_step(_sAllocateIp) == SQLITE_DONE) {
								ip6.setPort(routedNetmaskBits);
								if (nc.staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)
									nc.staticIps[nc.staticIpCount++] = ip6;
								break;
							}
						}
					}
				}
			}
		}

		// Auto-assign IPv4 address if auto-assignment is enabled and it's needed
		if ( ((network.flags & ZT_DB_NETWORK_FLAG_ZT_MANAGED_V4_AUTO_ASSIGN) != 0) && (!haveManagedIpv4AutoAssignment) && (!amActiveBridge) ) {
			sqlite3_reset(_sGetIpAssignmentPools);
			sqlite3_bind_text(_sGetIpAssignmentPools,1,network.id,16,SQLITE_STATIC);
			sqlite3_bind_int(_sGetIpAssignmentPools,2,4); // 4 == IPv4
			while (sqlite3_step(_sGetIpAssignmentPools) == SQLITE_ROW) {
				const unsigned char *ipRangeStartB = reinterpret_cast<const unsigned char *>(sqlite3_column_blob(_sGetIpAssignmentPools,0));
				const unsigned char *ipRangeEndB = reinterpret_cast<const unsigned char *>(sqlite3_column_blob(_sGetIpAssignmentPools,1));
				if ((!ipRangeStartB)||(!ipRangeEndB)||(sqlite3_column_bytes(_sGetIpAssignmentPools,0) != 16)||(sqlite3_column_bytes(_sGetIpAssignmentPools,1) != 16))
					continue;

				uint32_t ipRangeStart = Utils::ntoh(*(reinterpret_cast<const uint32_t *>(ipRangeStartB + 12)));
				uint32_t ipRangeEnd = Utils::ntoh(*(reinterpret_cast<const uint32_t *>(ipRangeEndB + 12)));
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

					// If it's routed, then try to claim and assign it and if successful end loop
					if (routedNetmaskBits > 0) {
						uint32_t ipBlob[4]; // actually a 16-byte blob, we put IPv4s in the last 4 bytes
						ipBlob[0] = 0; ipBlob[1] = 0; ipBlob[2] = 0; ipBlob[3] = Utils::hton(ip);
						sqlite3_reset(_sCheckIfIpIsAllocated);
						sqlite3_bind_text(_sCheckIfIpIsAllocated,1,network.id,16,SQLITE_STATIC);
						sqlite3_bind_blob(_sCheckIfIpIsAllocated,2,(const void *)ipBlob,16,SQLITE_STATIC);
						sqlite3_bind_int(_sCheckIfIpIsAllocated,3,4); // 4 == IPv4
						sqlite3_bind_int(_sCheckIfIpIsAllocated,4,(int)0);
						if (sqlite3_step(_sCheckIfIpIsAllocated) != SQLITE_ROW) {
							// No rows returned, so the IP is available
							sqlite3_reset(_sAllocateIp);
							sqlite3_bind_text(_sAllocateIp,1,network.id,16,SQLITE_STATIC);
							sqlite3_bind_text(_sAllocateIp,2,member.nodeId,10,SQLITE_STATIC);
							sqlite3_bind_int(_sAllocateIp,3,(int)0);
							sqlite3_bind_blob(_sAllocateIp,4,(const void *)ipBlob,16,SQLITE_STATIC);
							sqlite3_bind_int(_sAllocateIp,5,routedNetmaskBits); // IP netmask bits from matching route
							sqlite3_bind_int(_sAllocateIp,6,4); // 4 == IPv4
							if (sqlite3_step(_sAllocateIp) == SQLITE_DONE) {
								if (nc.staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES) {
									struct sockaddr_in *const v4ip = reinterpret_cast<struct sockaddr_in *>(&(nc.staticIps[nc.staticIpCount++]));
									v4ip->sin_family = AF_INET;
									v4ip->sin_port = Utils::hton((uint16_t)routedNetmaskBits);
									v4ip->sin_addr.s_addr = Utils::hton(ip);
								}
								break;
							}
						}
					}
				}
			}
		}
	} // end lock

	// Perform signing outside lock to enable concurrency
	if (network.isPrivate) {
		CertificateOfMembership com(now,ZT_NETWORK_COM_DEFAULT_REVISION_MAX_DELTA,nwid,identity.address());
		if (com.sign(signingId)) {
			nc.com = com;
		} else {
			return NETCONF_QUERY_INTERNAL_SERVER_ERROR;
		}
	}

	return NetworkController::NETCONF_QUERY_OK;
	*/
}

unsigned int SqliteNetworkController::handleControlPlaneHttpGET(
	const std::vector<std::string> &path,
	const std::map<std::string,std::string> &urlArgs,
	const std::map<std::string,std::string> &headers,
	const std::string &body,
	std::string &responseBody,
	std::string &responseContentType)
{
	return _doCPGet(path,urlArgs,headers,body,responseBody,responseContentType);
}

unsigned int SqliteNetworkController::handleControlPlaneHttpPOST(
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
			uint64_t nwid = Utils::hexStrToU64(path[1].c_str());
			char nwids[24];
			Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);

			if (path.size() >= 3) {
				json network(_readJson(_networkJP(nwid,false)));
				if (!network.size())
					return 404;

				if ((path.size() == 4)&&(path[2] == "member")&&(path[3].length() == 10)) {
					uint64_t address = Utils::hexStrToU64(path[3].c_str());

					/*
					int64_t addToNetworkRevision = 0;

					int64_t memberRowId = 0;
					sqlite3_reset(_sGetMember);
					sqlite3_bind_text(_sGetMember,1,nwids,16,SQLITE_STATIC);
					sqlite3_bind_text(_sGetMember,2,addrs,10,SQLITE_STATIC);
					bool memberExists = false;
					if (sqlite3_step(_sGetMember) == SQLITE_ROW) {
						memberExists = true;
						memberRowId = sqlite3_column_int64(_sGetMember,0);
					}

					if (!memberExists) {
						sqlite3_reset(_sCreateMember);
						sqlite3_bind_text(_sCreateMember,1,nwids,16,SQLITE_STATIC);
						sqlite3_bind_text(_sCreateMember,2,addrs,10,SQLITE_STATIC);
						sqlite3_bind_int(_sCreateMember,3,0);
						sqlite3_bind_text(_sCreateMember,4,nwids,16,SQLITE_STATIC);
						if (sqlite3_step(_sCreateMember) != SQLITE_DONE)
							return 500;
						memberRowId = (int64_t)sqlite3_last_insert_rowid(_db);

						sqlite3_reset(_sIncrementMemberRevisionCounter);
						sqlite3_bind_text(_sIncrementMemberRevisionCounter,1,nwids,16,SQLITE_STATIC);
						sqlite3_step(_sIncrementMemberRevisionCounter);
						addToNetworkRevision = 1;
					}

					json_value *j = json_parse(body.c_str(),body.length());
					if (j) {
						if (j->type == json_object) {
							for(unsigned int k=0;k<j->u.object.length;++k) {

								if (!strcmp(j->u.object.values[k].name,"authorized")) {
									if (j->u.object.values[k].value->type == json_boolean) {
										sqlite3_reset(_sUpdateMemberAuthorized);
										sqlite3_bind_int(_sUpdateMemberAuthorized,1,(j->u.object.values[k].value->u.boolean == 0) ? 0 : 1);
										sqlite3_bind_text(_sUpdateMemberAuthorized,2,nwids,16,SQLITE_STATIC);
										sqlite3_bind_int64(_sUpdateMemberAuthorized,3,memberRowId);
										if (sqlite3_step(_sUpdateMemberAuthorized) != SQLITE_DONE)
											return 500;

										sqlite3_reset(_sIncrementMemberRevisionCounter);
										sqlite3_bind_text(_sIncrementMemberRevisionCounter,1,nwids,16,SQLITE_STATIC);
										sqlite3_step(_sIncrementMemberRevisionCounter);
										addToNetworkRevision = 1;
									}
								} else if (!strcmp(j->u.object.values[k].name,"activeBridge")) {
									if (j->u.object.values[k].value->type == json_boolean) {
										sqlite3_reset(_sUpdateMemberActiveBridge);
										sqlite3_bind_int(_sUpdateMemberActiveBridge,1,(j->u.object.values[k].value->u.boolean == 0) ? 0 : 1);
										sqlite3_bind_text(_sUpdateMemberActiveBridge,2,nwids,16,SQLITE_STATIC);
										sqlite3_bind_int64(_sUpdateMemberActiveBridge,3,memberRowId);
										if (sqlite3_step(_sUpdateMemberActiveBridge) != SQLITE_DONE)
											return 500;

										sqlite3_reset(_sIncrementMemberRevisionCounter);
										sqlite3_bind_text(_sIncrementMemberRevisionCounter,1,nwids,16,SQLITE_STATIC);
										sqlite3_step(_sIncrementMemberRevisionCounter);
										addToNetworkRevision = 1;
									}
								} else if (!strcmp(j->u.object.values[k].name,"ipAssignments")) {
									if (j->u.object.values[k].value->type == json_array) {
										sqlite3_reset(_sDeleteIpAllocations);
										sqlite3_bind_text(_sDeleteIpAllocations,1,nwids,16,SQLITE_STATIC);
										sqlite3_bind_text(_sDeleteIpAllocations,2,addrs,10,SQLITE_STATIC);
										sqlite3_bind_int(_sDeleteIpAllocations,3,(int)0);
										if (sqlite3_step(_sDeleteIpAllocations) != SQLITE_DONE)
											return 500;
										for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
											json_value *ipalloc = j->u.object.values[k].value->u.array.values[kk];
											if (ipalloc->type == json_string) {
												InetAddress a(ipalloc->u.string.ptr);
												char ipBlob[16];
												int ipVersion = 0;
												_ipToBlob(a,ipBlob,ipVersion);
												if (ipVersion > 0) {
													sqlite3_reset(_sAllocateIp);
													sqlite3_bind_text(_sAllocateIp,1,nwids,16,SQLITE_STATIC);
													sqlite3_bind_text(_sAllocateIp,2,addrs,10,SQLITE_STATIC);
													sqlite3_bind_int(_sAllocateIp,3,(int)0);
													sqlite3_bind_blob(_sAllocateIp,4,(const void *)ipBlob,16,SQLITE_STATIC);
													sqlite3_bind_int(_sAllocateIp,5,(int)a.netmaskBits()); // NOTE: this field is now ignored but set it anyway
													sqlite3_bind_int(_sAllocateIp,6,ipVersion);
													if (sqlite3_step(_sAllocateIp) != SQLITE_DONE)
														return 500;
												}
											}
										}
										addToNetworkRevision = 1;
									}
								} else if (!strcmp(j->u.object.values[k].name,"identity")) {
									// Identity is technically an immutable field, but if the member's Node has
									// no identity we allow it to be populated. This is primarily for migrating
									// node data from another controller.
									json_value *idstr = j->u.object.values[k].value;
									if (idstr->type == json_string) {
										bool alreadyHaveIdentity = false;

										sqlite3_reset(_sGetNodeIdentity);
										sqlite3_bind_text(_sGetNodeIdentity,1,addrs,10,SQLITE_STATIC);
										if (sqlite3_step(_sGetNodeIdentity) == SQLITE_ROW) {
											const char *tmp2 = (const char *)sqlite3_column_text(_sGetNodeIdentity,0);
											if ((tmp2)&&(tmp2[0]))
												alreadyHaveIdentity = true;
										}

										if (!alreadyHaveIdentity) {
											try {
												Identity id2(idstr->u.string.ptr);
												if (id2) {
													std::string idstr2(id2.toString(false)); // object must persist until after sqlite3_step() for SQLITE_STATIC
													sqlite3_reset(_sCreateOrReplaceNode);
													sqlite3_bind_text(_sCreateOrReplaceNode,1,addrs,10,SQLITE_STATIC);
													sqlite3_bind_text(_sCreateOrReplaceNode,2,idstr2.c_str(),-1,SQLITE_STATIC);
													sqlite3_step(_sCreateOrReplaceNode);
												}
											} catch ( ... ) {} // ignore invalid identities
										}
									}
								}

							}
						}
						json_value_free(j);
					}

					if ((addToNetworkRevision > 0)&&(revision > 0)) {
						sqlite3_reset(_sSetNetworkRevision);
						sqlite3_bind_int64(_sSetNetworkRevision,1,revision + addToNetworkRevision);
						sqlite3_bind_text(_sSetNetworkRevision,2,nwids,16,SQLITE_STATIC);
						sqlite3_step(_sSetNetworkRevision);
					}
					*/

					return _doCPGet(path,urlArgs,headers,body,responseBody,responseContentType);
				} else if ((path.size() == 3)&&(path[2] == "test")) {

					Mutex::Lock _l(_circuitTests_m);

					ZT_CircuitTest *test = (ZT_CircuitTest *)malloc(sizeof(ZT_CircuitTest));
					memset(test,0,sizeof(ZT_CircuitTest));

					Utils::getSecureRandom(&(test->testId),sizeof(test->testId));
					test->credentialNetworkId = nwid;
					test->ptr = (void *)this;

					// TODO TODO
					/*
					json_value *j = json_parse(body.c_str(),body.length());
					if (j) {
						if (j->type == json_object) {
							for(unsigned int k=0;k<j->u.object.length;++k) {

								if (!strcmp(j->u.object.values[k].name,"hops")) {
									if (j->u.object.values[k].value->type == json_array) {
										for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
											json_value *hop = j->u.object.values[k].value->u.array.values[kk];
											if (hop->type == json_array) {
												for(unsigned int kkk=0;kkk<hop->u.array.length;++kkk) {
													if (hop->u.array.values[kkk]->type == json_string) {
														test->hops[test->hopCount].addresses[test->hops[test->hopCount].breadth++] = Utils::hexStrToU64(hop->u.array.values[kkk]->u.string.ptr) & 0xffffffffffULL;
													}
												}
												++test->hopCount;
											}
										}
									}
								} else if (!strcmp(j->u.object.values[k].name,"reportAtEveryHop")) {
									if (j->u.object.values[k].value->type == json_boolean)
										test->reportAtEveryHop = (j->u.object.values[k].value->u.boolean == 0) ? 0 : 1;
								}

							}
						}
						json_value_free(j);
					}
					*/

					if (!test->hopCount) {
						::free((void *)test);
						return 500;
					}

					test->timestamp = OSUtils::now();

					_CircuitTestEntry &te = _circuitTests[test->testId];
					te.test = test;
					te.jsonResults = "";

					_node->circuitTestBegin(test,&(SqliteNetworkController::_circuitTestCallback));

					char json[1024];
					Utils::snprintf(json,sizeof(json),"{\"testId\":\"%.16llx\"}",test->testId);
					responseBody = json;
					responseContentType = "application/json";
					return 200;

				} // else 404

			} else {
				// POST to network ID

				json b;
				try {
					b = json::parse(body);
				} catch ( ... ) {
					return 403;
				}

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
							for(unsigned long i=0;i<rules.size();++i) {
								auto rule = rules[i];
								if (rule.is_object()) {
								}
							}
						}
					}
				} catch ( ... ) {
					// TODO: report?
				}

				if (!network.count("private")) network["private"] = true;
				if (!network.count("creationTime")) network["creationTime"] = OSUtils::now();
				if (!network.count("name")) network["name"] = "";
				if (!network.count("multicastLimit")) network["multicastLimit"] = (uint64_t)32;
				if (!network.count("revision")) network["revision"] = (uint64_t)0;
				if (!network.count("memberRevisionCounter")) network["memberRevisionCounter"] = (uint64_t)0;
				if (!network.count("memberLastModified")) network["memberLastModified"] = (uint64_t)0;
				if (!network.count("v4AssignMode")) network["v4AssignMode"] = "{\"zt\":false}"_json;
				if (!network.count("v6AssignMode")) network["v6AssignMode"] = "{\"rfc4193\":false,\"zt\":false,\"6plane\":false}"_json;

				if (!network.count("rules")) {
				}

				network["lastModified"] = OSUtils::now();
				network["revision"] = network.value("revision",0ULL) + 1ULL;

				_writeJson(_networkJP(nwid,true),network);

				responseBody = network.dump(2);
				responseContentType = "application/json";
				return 200;
			} // else 404

		} // else 404

	} // else 404

	return 404;
}

unsigned int SqliteNetworkController::handleControlPlaneHttpDELETE(
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

unsigned int SqliteNetworkController::_doCPGet(
	const std::vector<std::string> &path,
	const std::map<std::string,std::string> &urlArgs,
	const std::map<std::string,std::string> &headers,
	const std::string &body,
	std::string &responseBody,
	std::string &responseContentType)
{
	// Assumes _lock is locked
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

						json o(member);
						o["nwid"] = nwids;
						o["address"] = addrs;
						o["clock"] = OSUtils::now();
						responseBody = o.dump(2);
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
				o["nwid"] = nwids;
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

void SqliteNetworkController::_circuitTestCallback(ZT_Node *node,ZT_CircuitTest *test,const ZT_CircuitTestReport *report)
{
	char tmp[65535];
	SqliteNetworkController *const self = reinterpret_cast<SqliteNetworkController *>(test->ptr);

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
