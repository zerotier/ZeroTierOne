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

#include "../include/ZeroTierOne.h"
#include "../node/Constants.hpp"

#include "../ext/json-parser/json.h"

#include "SqliteNetworkController.hpp"
#include "../node/Utils.hpp"
#include "../node/CertificateOfMembership.hpp"
#include "../node/NetworkConfig.hpp"
#include "../node/InetAddress.hpp"
#include "../node/MAC.hpp"
#include "../node/Address.hpp"
#include "../osdep/OSUtils.hpp"

// Include ZT_NETCONF_SCHEMA_SQL constant to init database
#include "schema.sql.c"

// Stored in database as schemaVersion key in Config.
// If not present, database is assumed to be empty and at the current schema version
// and this key/value is added automatically.
#define ZT_NETCONF_SQLITE_SCHEMA_VERSION 1
#define ZT_NETCONF_SQLITE_SCHEMA_VERSION_STR "1"

// Maximum age in ms for a cached netconf before we regenerate anyway (one hour)
#define ZT_CACHED_NETCONF_MAX_AGE (60 * 60 * 1000)

namespace ZeroTier {

namespace {

static std::string _jsonEscape(const char *s)
{
	if (!s)
		return std::string();
	std::string buf;
	for(const char *p=s;(*p);++p) {
		switch(*p) {
			case '\t': buf.append("\\t");  break;
			case '\b': buf.append("\\b");  break;
			case '\r': buf.append("\\r");  break;
			case '\n': buf.append("\\n");  break;
			case '\f': buf.append("\\f");  break;
			case '"':  buf.append("\\\""); break;
			case '\\': buf.append("\\\\"); break;
			case '/':  buf.append("\\/");  break;
			default:   buf.push_back(*p);  break;
		}
	}
	return buf;
}
static std::string _jsonEscape(const std::string &s) { return _jsonEscape(s.c_str()); }

struct MemberRecord {
	int64_t rowid;
	char nodeId[16];
	int cachedNetconfBytes;
	const void *cachedNetconf;
	uint64_t cachedNetconfRevision;
	uint64_t cachedNetconfTimestamp;
	uint64_t clientReportedRevision;
	bool authorized;
	bool activeBridge;
};

struct NetworkRecord {
	char id[24];
	const char *name;
	const char *v4AssignMode;
	const char *v6AssignMode;
	bool isPrivate;
	bool enableBroadcast;
	bool allowPassiveBridging;
	int multicastLimit;
	uint64_t creationTime;
	uint64_t revision;
};

} // anonymous namespace

SqliteNetworkController::SqliteNetworkController(const char *dbPath) :
	_dbPath(dbPath),
	_db((sqlite3 *)0)
{
	if (sqlite3_open_v2(dbPath,&_db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,(const char *)0) != SQLITE_OK)
		throw std::runtime_error("SqliteNetworkController cannot open database file");
	sqlite3_busy_timeout(_db,10000);

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
		} else if (schemaVersion != ZT_NETCONF_SQLITE_SCHEMA_VERSION) {
			// Note -- this will eventually run auto-upgrades so this isn't how it'll work going forward
			sqlite3_close(_db);
			throw std::runtime_error("SqliteNetworkController database schema version mismatch");
		}
	} else {
		// Prepare statement will fail if Config table doesn't exist, which means our DB
		// needs to be initialized.
		if (sqlite3_exec(_db,ZT_NETCONF_SCHEMA_SQL"INSERT INTO Config (k,v) VALUES ('schemaVersion',"ZT_NETCONF_SQLITE_SCHEMA_VERSION_STR");",0,0,0) != SQLITE_OK) {
			sqlite3_close(_db);
			throw std::runtime_error("SqliteNetworkController cannot initialize database and/or insert schemaVersion into Config table");
		}
	}

	if (
			  (sqlite3_prepare_v2(_db,"SELECT name,private,enableBroadcast,allowPassiveBridging,v4AssignMode,v6AssignMode,multicastLimit,creationTime,revision FROM Network WHERE id = ?",-1,&_sGetNetworkById,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT rowid,cachedNetconf,cachedNetconfRevision,cachedNetconfTimestamp,clientReportedRevision,authorized,activeBridge FROM Member WHERE networkId = ? AND nodeId = ?",-1,&_sGetMember,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Member (networkId,nodeId,cachedNetconfRevision,clientReportedRevision,authorized,activeBridge) VALUES (?,?,0,0,?,0)",-1,&_sCreateMember,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT identity FROM Node WHERE id = ?",-1,&_sGetNodeIdentity,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Node (id,identity,lastAt,lastSeen,firstSeen) VALUES (?,?,?,?,?)",-1,&_sCreateNode,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Node SET lastAt = ?,lastSeen = ? WHERE id = ?",-1,&_sUpdateNode,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Node SET lastSeen = ? WHERE id = ?",-1,&_sUpdateNode2,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Member SET clientReportedRevision = ? WHERE rowid = ?",-1,&_sUpdateMemberClientReportedRevision,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT etherType FROM Rule WHERE networkId = ? AND \"action\" = 'accept'",-1,&_sGetEtherTypesFromRuleTable,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT mgMac,mgAdi,preload,maxBalance,accrual FROM MulticastRate WHERE networkId = ?",-1,&_sGetMulticastRates,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT nodeId FROM Member WHERE networkId = ? AND activeBridge > 0 AND authorized > 0",-1,&_sGetActiveBridges,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT ip,ipNetmaskBits FROM IpAssignment WHERE networkId = ? AND nodeId = ? AND ipVersion = ?",-1,&_sGetIpAssignmentsForNode,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT ipNetwork,ipNetmaskBits FROM IpAssignmentPool WHERE networkId = ? AND ipVersion = ? AND active > 0",-1,&_sGetIpAssignmentPools,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT 1 FROM IpAssignment WHERE networkId = ? AND ip = ? AND ipVersion = ?",-1,&_sCheckIfIpIsAllocated,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO IpAssignment (networkId,nodeId,ip,ipNetmaskBits,ipVersion) VALUES (?,?,?,?,?)",-1,&_sAllocateIp,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Member SET cachedNetconf = ?,cachedNetconfRevision = ? WHERE rowid = ?",-1,&_sCacheNetconf,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT nodeId,phyAddress FROM Relay WHERE networkId = ?",-1,&_sGetRelays,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT id FROM Network ORDER BY id ASC",-1,&_sListNetworks,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT m.authorized,m.activeBridge,n.id,n.lastAt,n.lastSeen,n.firstSeen FROM Member AS m,Node AS n WHERE m.networkId = ? AND n.id = m.nodeId ORDER BY n.id ASC",-1,&_sListNetworkMembers,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT m.authorized,m.activeBridge,n.identity,n.lastAt,n.lastSeen,n.firstSeen FROM Member AS m,Node AS n WHERE m.networkId = ? AND m.nodeId = ?",-1,&_sGetMember2,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT ipNetwork,ipNetmaskBits,ipVersion,active FROM IpAssignmentPool WHERE networkId = ? ORDER BY ipNetwork ASC",-1,&_sGetIpAssignmentPools2,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT ruleId,nodeId,vlanId,vlanPcp,etherType,macSource,macDest,ipSource,ipDest,ipTos,ipProtocol,ipSourcePort,ipDestPort,\"action\" FROM Rule WHERE networkId = ? ORDER BY ruleId ASC",-1,&_sListRules,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Rule WHERE networkId = ? AND ruleId = ?",-1,&_sDeleteRule,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Rule (networkId,ruleId,nodeId,vlanId,vlanPcP,etherType,macSource,macDest,ipSource,ipDest,ipTos,ipProtocol,ipSourcePort,ipDestPort,\"action\") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",-1,&_sCreateRule,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Network (networkId,name,creationTime,revision) VALUES (?,?,?,1)",-1,&_sCreateNetwork,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Network SET ? = ? WHERE networkId = ?",-1,&_sUpdateNetworkField,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT revision FROM Network WHERE id = ?",-1,&_sGetNetworkRevision,(const char **)0) != SQLITE_OK)
		 ) {
		sqlite3_close(_db);
		throw std::runtime_error("SqliteNetworkController unable to initialize one or more prepared statements");
	}
}

SqliteNetworkController::~SqliteNetworkController()
{
	Mutex::Lock _l(_lock);
	if (_db) {
		sqlite3_finalize(_sGetNetworkById);
		sqlite3_finalize(_sGetMember);
		sqlite3_finalize(_sCreateMember);
		sqlite3_finalize(_sGetNodeIdentity);
		sqlite3_finalize(_sCreateNode);
		sqlite3_finalize(_sUpdateNode);
		sqlite3_finalize(_sUpdateNode2);
		sqlite3_finalize(_sUpdateMemberClientReportedRevision);
		sqlite3_finalize(_sGetEtherTypesFromRuleTable);
		sqlite3_finalize(_sGetMulticastRates);
		sqlite3_finalize(_sGetActiveBridges);
		sqlite3_finalize(_sGetIpAssignmentsForNode);
		sqlite3_finalize(_sGetIpAssignmentPools);
		sqlite3_finalize(_sCheckIfIpIsAllocated);
		sqlite3_finalize(_sAllocateIp);
		sqlite3_finalize(_sCacheNetconf);
		sqlite3_finalize(_sGetRelays);
		sqlite3_finalize(_sListNetworks);
		sqlite3_finalize(_sListNetworkMembers);
		sqlite3_finalize(_sGetMember2);
		sqlite3_finalize(_sGetIpAssignmentPools2);
		sqlite3_finalize(_sListRules);
		sqlite3_finalize(_sDeleteRule);
		sqlite3_finalize(_sCreateRule);
		sqlite3_finalize(_sCreateNetwork);
		sqlite3_finalize(_sUpdateNetworkField);
		sqlite3_finalize(_sGetNetworkRevision);
		sqlite3_close(_db);
	}
}

NetworkController::ResultCode SqliteNetworkController::doNetworkConfigRequest(const InetAddress &fromAddr,const Identity &signingId,const Identity &identity,uint64_t nwid,const Dictionary &metaData,uint64_t haveRevision,Dictionary &netconf)
{
	Mutex::Lock _l(_lock);

	// Note: we can't reuse prepared statements that return const char * pointers without
	// making our own copy in e.g. a std::string first.

	if ((!signingId)||(!signingId.hasPrivate())) {
		netconf["error"] = "signing identity invalid or lacks private key";
		return NetworkController::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
	}

	NetworkRecord network;
	memset(&network,0,sizeof(network));
	Utils::snprintf(network.id,sizeof(network.id),"%.16llx",(unsigned long long)nwid);

	MemberRecord member;
	memset(&member,0,sizeof(member));
	Utils::snprintf(member.nodeId,sizeof(member.nodeId),"%.10llx",(unsigned long long)identity.address().toInt());

	// Create/update Node record and check identity fully -- identities are first-come-first-claim

	sqlite3_reset(_sGetNodeIdentity);
	sqlite3_bind_text(_sGetNodeIdentity,1,member.nodeId,10,SQLITE_STATIC);
	if (sqlite3_step(_sGetNodeIdentity) == SQLITE_ROW) {
		try {
			Identity alreadyKnownIdentity((const char *)sqlite3_column_text(_sGetNodeIdentity,0));
			if (alreadyKnownIdentity == identity) {
				char lastSeen[64];
				Utils::snprintf(lastSeen,sizeof(lastSeen),"%llu",(unsigned long long)OSUtils::now());
				if (fromAddr) {
					std::string lastAt(fromAddr.toString());
					sqlite3_reset(_sUpdateNode);
					sqlite3_bind_text(_sUpdateNode,1,lastAt.c_str(),-1,SQLITE_STATIC);
					sqlite3_bind_text(_sUpdateNode,2,lastSeen,-1,SQLITE_STATIC);
					sqlite3_bind_text(_sUpdateNode,3,member.nodeId,10,SQLITE_STATIC);
					sqlite3_step(_sUpdateNode);
				} else { // fromAddr is empty, which means this was a relayed packet -- so don't update lastAt
					sqlite3_reset(_sUpdateNode2);
					sqlite3_bind_text(_sUpdateNode2,1,lastSeen,-1,SQLITE_STATIC);
					sqlite3_bind_text(_sUpdateNode2,2,member.nodeId,10,SQLITE_STATIC);
					sqlite3_step(_sUpdateNode2);
				}
			} else {
				return NetworkController::NETCONF_QUERY_ACCESS_DENIED;
			}
		} catch ( ... ) { // identity stored in database is not valid or is NULL
			return NetworkController::NETCONF_QUERY_ACCESS_DENIED;
		}
	} else {
		std::string idstr(identity.toString(false));
		std::string lastAt;
		if (fromAddr)
			lastAt = fromAddr.toString();
		char lastSeen[64];
		Utils::snprintf(lastSeen,sizeof(lastSeen),"%llu",(unsigned long long)OSUtils::now());
		sqlite3_reset(_sCreateNode);
		sqlite3_bind_text(_sCreateNode,1,member.nodeId,10,SQLITE_STATIC);
		sqlite3_bind_text(_sCreateNode,2,idstr.c_str(),-1,SQLITE_STATIC);
		sqlite3_bind_text(_sCreateNode,3,lastAt.c_str(),-1,SQLITE_STATIC);
		sqlite3_bind_text(_sCreateNode,4,lastSeen,-1,SQLITE_STATIC);
		sqlite3_bind_text(_sCreateNode,5,lastSeen,-1,SQLITE_STATIC);
		if (sqlite3_step(_sCreateNode) != SQLITE_DONE) {
			netconf["error"] = "unable to create new node record";
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
		network.v4AssignMode = (const char *)sqlite3_column_text(_sGetNetworkById,4);
		network.v6AssignMode = (const char *)sqlite3_column_text(_sGetNetworkById,5);
		network.multicastLimit = sqlite3_column_int(_sGetNetworkById,6);
		network.creationTime = (uint64_t)sqlite3_column_int64(_sGetNetworkById,7);
		network.revision = (uint64_t)sqlite3_column_int64(_sGetNetworkById,8);
	} else return NetworkController::NETCONF_QUERY_OBJECT_NOT_FOUND;

	// Fetch Member record

	bool foundMember = false;
	sqlite3_reset(_sGetMember);
	sqlite3_bind_text(_sGetMember,1,network.id,16,SQLITE_STATIC);
	sqlite3_bind_text(_sGetMember,2,member.nodeId,10,SQLITE_STATIC);
	if (sqlite3_step(_sGetMember) == SQLITE_ROW) {
		foundMember = true;
		member.rowid = (int64_t)sqlite3_column_int64(_sGetMember,0);
		member.cachedNetconfBytes = sqlite3_column_bytes(_sGetMember,1);
		member.cachedNetconf = sqlite3_column_blob(_sGetMember,1);
		member.cachedNetconfRevision = (uint64_t)sqlite3_column_int64(_sGetMember,2);
		member.cachedNetconfTimestamp = (uint64_t)sqlite3_column_int64(_sGetMember,3);
		member.clientReportedRevision = (uint64_t)sqlite3_column_int64(_sGetMember,4);
		member.authorized = (sqlite3_column_int(_sGetMember,5) > 0);
		member.activeBridge = (sqlite3_column_int(_sGetMember,6) > 0);
	}

	// Create Member record for unknown nodes, auto-authorizing if network is public

	if (!foundMember) {
		member.cachedNetconfBytes = 0;
		member.cachedNetconfRevision = 0;
		member.clientReportedRevision = 0;
		member.authorized = (network.isPrivate ? false : true);
		member.activeBridge = false;
		sqlite3_reset(_sCreateMember);
		sqlite3_bind_text(_sCreateMember,1,network.id,16,SQLITE_STATIC);
		sqlite3_bind_text(_sCreateMember,2,member.nodeId,10,SQLITE_STATIC);
		sqlite3_bind_int(_sCreateMember,3,(member.authorized ? 0 : 1));
		if ( (sqlite3_step(_sCreateMember) != SQLITE_DONE) && ((member.rowid = (int64_t)sqlite3_last_insert_rowid(_db)) > 0) ) {
			netconf["error"] = "unable to create new member record";
			return NetworkController::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
		}
	}

	// Check member authorization

	if (!member.authorized)
		return NetworkController::NETCONF_QUERY_ACCESS_DENIED;

	// Update client's currently reported haveRevision in Member record

	if (member.rowid > 0) {
		sqlite3_reset(_sUpdateMemberClientReportedRevision);
		sqlite3_bind_int64(_sUpdateMemberClientReportedRevision,1,(sqlite3_int64)haveRevision);
		sqlite3_bind_int64(_sUpdateMemberClientReportedRevision,2,member.rowid);
		sqlite3_step(_sUpdateMemberClientReportedRevision);
	}

	// If netconf is unchanged from client reported revision, just tell client they're up to date

	if ((haveRevision > 0)&&(haveRevision == network.revision))
		return NetworkController::NETCONF_QUERY_OK_BUT_NOT_NEWER;

	// Generate or retrieve cached netconf

	netconf.clear();
	if ( (member.cachedNetconfBytes > 0)&&
		   (member.cachedNetconfRevision == network.revision)&&
		   ((OSUtils::now() - member.cachedNetconfTimestamp) < ZT_CACHED_NETCONF_MAX_AGE) ) {
		// Use cached copy
		std::string tmp((const char *)member.cachedNetconf,member.cachedNetconfBytes);
		netconf.fromString(tmp);
	} else {
		// Create and sign a new netconf, and save in database to re-use in the future

		char tss[24],rs[24];
		Utils::snprintf(tss,sizeof(tss),"%.16llx",(unsigned long long)OSUtils::now());
		Utils::snprintf(rs,sizeof(rs),"%.16llx",(unsigned long long)network.revision);
		netconf[ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP] = tss;
		netconf[ZT_NETWORKCONFIG_DICT_KEY_REVISION] = rs;
		netconf[ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID] = network.id;
		netconf[ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO] = member.nodeId;
		netconf[ZT_NETWORKCONFIG_DICT_KEY_PRIVATE] = network.isPrivate ? "1" : "0";
		netconf[ZT_NETWORKCONFIG_DICT_KEY_NAME] = (network.name) ? network.name : "";
		netconf[ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST] = network.enableBroadcast ? "1" : "0";
		netconf[ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING] = network.allowPassiveBridging ? "1" : "0";

		{
			std::vector<int> allowedEtherTypes;
			sqlite3_reset(_sGetEtherTypesFromRuleTable);
			sqlite3_bind_text(_sGetEtherTypesFromRuleTable,1,network.id,16,SQLITE_STATIC);
			while (sqlite3_step(_sGetEtherTypesFromRuleTable) == SQLITE_ROW) {
				int et = sqlite3_column_int(_sGetEtherTypesFromRuleTable,0);
				if ((et >= 0)&&(et <= 0xffff))
					allowedEtherTypes.push_back(et);
			}
			std::sort(allowedEtherTypes.begin(),allowedEtherTypes.end());
			std::unique(allowedEtherTypes.begin(),allowedEtherTypes.end());
			std::string allowedEtherTypesCsv;
			for(std::vector<int>::const_iterator i(allowedEtherTypes.begin());i!=allowedEtherTypes.end();++i) {
				if (allowedEtherTypesCsv.length())
					allowedEtherTypesCsv.push_back(',');
				char tmp[16];
				Utils::snprintf(tmp,sizeof(tmp),"%.4x",(unsigned int)*i);
				allowedEtherTypesCsv.append(tmp);
			}
			netconf[ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES] = allowedEtherTypesCsv;
		}

		{
			std::string multicastRates;
			sqlite3_reset(_sGetMulticastRates);
			sqlite3_bind_text(_sGetMulticastRates,1,network.id,16,SQLITE_STATIC);
			while (sqlite3_step(_sGetMulticastRates) == SQLITE_ROW) {
				const char *mac = (const char *)sqlite3_column_text(_sGetMulticastRates,0);
				if ((mac)&&(strlen(mac) == 12)) {
					unsigned long adi = ((unsigned long)sqlite3_column_int64(_sGetMulticastRates,1)) & 0xffffffff;
					char tmp[256];
					Utils::snprintf(tmp,sizeof(tmp),"%s/%.4lx=%x,%x,%x\n",mac,adi,sqlite3_column_int(_sGetMulticastRates,2),sqlite3_column_int(_sGetMulticastRates,3),sqlite3_column_int(_sGetMulticastRates,4));
					multicastRates.append(tmp);
				}
			}
			if (multicastRates.length() > 0)
				netconf[ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_RATES] = multicastRates;
			if (network.multicastLimit > 0) {
				char ml[16];
				Utils::snprintf(ml,sizeof(ml),"%lx",(unsigned long)network.multicastLimit);
				netconf[ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT] = ml;
			}
		}

		{
			std::string activeBridges;
			sqlite3_reset(_sGetActiveBridges);
			sqlite3_bind_text(_sGetActiveBridges,1,network.id,16,SQLITE_STATIC);
			while (sqlite3_step(_sGetActiveBridges) == SQLITE_ROW) {
				const char *ab = (const char *)sqlite3_column_text(_sGetActiveBridges,0);
				if ((ab)&&(strlen(ab) == 10)) {
					if (activeBridges.length())
						activeBridges.push_back(',');
					activeBridges.append(ab);
				}
				if (activeBridges.length() > 1024) // sanity check -- you can't have too many active bridges at the moment
					break;
			}
			if (activeBridges.length())
				netconf[ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES] = activeBridges;
		}

		{
			std::string relays;
			sqlite3_reset(_sGetRelays);
			sqlite3_bind_text(_sGetRelays,1,network.id,16,SQLITE_STATIC);
			while (sqlite3_step(_sGetRelays) == SQLITE_ROW) {
				const char *n = (const char *)sqlite3_column_text(_sGetRelays,0);
				const char *a = (const char *)sqlite3_column_text(_sGetRelays,1);
				if ((n)&&(a)) {
					Address node(n);
					InetAddress addr(a);
					if ((node)&&(addr)) {
						if (relays.length())
							relays.push_back(',');
						relays.append(node.toString());
						relays.push_back(';');
						relays.append(addr.toString());
					}
				}
			}
			if (relays.length())
				netconf[ZT_NETWORKCONFIG_DICT_KEY_RELAYS] = relays;
		}

		if ((network.v4AssignMode)&&(!strcmp(network.v4AssignMode,"zt"))) {
			std::string v4s;

			sqlite3_reset(_sGetIpAssignmentsForNode);
			sqlite3_bind_text(_sGetIpAssignmentsForNode,1,network.id,16,SQLITE_STATIC);
			sqlite3_bind_text(_sGetIpAssignmentsForNode,2,member.nodeId,10,SQLITE_STATIC);
			sqlite3_bind_int(_sGetIpAssignmentsForNode,3,4); // 4 == IPv4
			while (sqlite3_step(_sGetIpAssignmentsForNode) == SQLITE_ROW) {
				const unsigned char *ip = (const unsigned char *)sqlite3_column_blob(_sGetIpAssignmentsForNode,0);
				int ipNetmaskBits = sqlite3_column_int(_sGetIpAssignmentsForNode,1);
				if ((ip)&&(sqlite3_column_bytes(_sGetIpAssignmentsForNode,0) >= 4)&&(ipNetmaskBits > 0)&&(ipNetmaskBits <= 32)) {
					char tmp[32];
					Utils::snprintf(tmp,sizeof(tmp),"%d.%d.%d.%d/%d",(int)ip[0],(int)ip[1],(int)ip[2],(int)ip[3],ipNetmaskBits);
					if (v4s.length())
						v4s.push_back(',');
					v4s.append(tmp);
				}
			}

			if (!v4s.length()) {
				// Attempt to auto-assign an IPv4 address from an available pool if one isn't assigned already
				sqlite3_reset(_sGetIpAssignmentPools);
				sqlite3_bind_text(_sGetIpAssignmentPools,1,network.id,16,SQLITE_STATIC);
				sqlite3_bind_int(_sGetIpAssignmentPools,2,4); // 4 == IPv4
				while ((!v4s.length())&&(sqlite3_step(_sGetIpAssignmentPools) == SQLITE_ROW)) {
					const void *ipNetwork = sqlite3_column_blob(_sGetIpAssignmentPools,0);
					int ipNetmaskBits = sqlite3_column_int(_sGetIpAssignmentPools,1);
					if ((ipNetwork)&&(sqlite3_column_bytes(_sGetIpAssignmentPools,0) >= 4)&&(ipNetmaskBits > 0)&&(ipNetmaskBits < 32)) {
						uint32_t n = Utils::ntoh(*((const uint32_t *)ipNetwork)); // network in host byte order e.g. 192.168.0.0
						uint32_t m = 0xffffffff << (32 - ipNetmaskBits); // netmask e.g. 0xffffff00 for '24' since 32 - 24 == 8
						uint32_t im = ~m; // inverse mask, e.g. 0x000000ff for a netmask of 0xffffff00
						uint32_t abits = (uint32_t)(identity.address().toInt() & 0xffffffff); // least significant bits of member ZT address

						for(uint32_t k=0;k<=im;++k) { // try up to the number of IPs possible in this network
							uint32_t ip = ( ((abits + k) & im) | (n & m) ); // build IP using bits from ZT address of member + k
							if ((ip & 0x000000ff) == 0x00) continue; // no IPs ending in .0 allowed
							if ((ip & 0x000000ff) == 0xff) continue; // no IPs ending in .255 allowed

							uint32_t nip = Utils::hton(ip); // IP in big-endian "network" byte order
							sqlite3_reset(_sCheckIfIpIsAllocated);
							sqlite3_bind_text(_sCheckIfIpIsAllocated,1,network.id,16,SQLITE_STATIC);
							sqlite3_bind_blob(_sCheckIfIpIsAllocated,2,(const void *)&nip,4,SQLITE_STATIC);
							sqlite3_bind_int(_sCheckIfIpIsAllocated,3,4); // 4 == IPv4
							if (sqlite3_step(_sCheckIfIpIsAllocated) != SQLITE_ROW) {
								// No rows returned, so the IP is available
								sqlite3_reset(_sAllocateIp);
								sqlite3_bind_text(_sAllocateIp,1,network.id,16,SQLITE_STATIC);
								sqlite3_bind_text(_sAllocateIp,2,member.nodeId,10,SQLITE_STATIC);
								sqlite3_bind_blob(_sAllocateIp,3,(const void *)&nip,4,SQLITE_STATIC);
								sqlite3_bind_int(_sAllocateIp,4,ipNetmaskBits);
								sqlite3_bind_int(_sAllocateIp,5,4); // 4 == IPv4
								if (sqlite3_step(_sAllocateIp) == SQLITE_DONE) {
									char tmp[32];
									Utils::snprintf(tmp,sizeof(tmp),"%d.%d.%d.%d/%d",(int)((ip >> 24) & 0xff),(int)((ip >> 16) & 0xff),(int)((ip >> 8) & 0xff),(int)(ip & 0xff),ipNetmaskBits);
									if (v4s.length())
										v4s.push_back(',');
									v4s.append(tmp);
									break; // IP found and reserved! v4s containing something will cause outer while() to break.
								}
							}
						}
					}
				}
			}

			if (v4s.length())
				netconf[ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC] = v4s;
		}

		// TODO: IPv6 auto-assign once it's supported in UI

		if (network.isPrivate) {
			CertificateOfMembership com(network.revision,ZT1_CERTIFICATE_OF_MEMBERSHIP_REVISION_MAX_DELTA,nwid,identity.address());
			if (com.sign(signingId)) // basically can't fail unless our identity is invalid
				netconf[ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP] = com.toString();
			else {
				netconf["error"] = "unable to sign COM";
				return NETCONF_QUERY_INTERNAL_SERVER_ERROR;
			}
		}

		if (!netconf.sign(signingId,OSUtils::now())) {
			netconf["error"] = "unable to sign netconf dictionary";
			return NETCONF_QUERY_INTERNAL_SERVER_ERROR;
		}

		// Save serialized netconf for future re-use
		std::string netconfSerialized(netconf.toString());
		if (netconfSerialized.length() < 4096) { // sanity check
			sqlite3_reset(_sCacheNetconf);
			sqlite3_bind_blob(_sCacheNetconf,1,(const void *)netconfSerialized.data(),netconfSerialized.length(),SQLITE_STATIC);
			sqlite3_bind_int64(_sCacheNetconf,2,(sqlite3_int64)network.revision);
			sqlite3_bind_int64(_sCacheNetconf,3,member.rowid);
			sqlite3_step(_sCacheNetconf);
		}
	}

	return NetworkController::NETCONF_QUERY_OK;
}

unsigned int SqliteNetworkController::handleControlPlaneHttpGET(
	const std::vector<std::string> &path,
	const std::map<std::string,std::string> &urlArgs,
	const std::map<std::string,std::string> &headers,
	const std::string &body,
	std::string &responseBody,
	std::string &responseContentType)
{
	char json[16384];

	if (path.empty())
		return 404;
	Mutex::Lock _l(_lock);

	if (path[0] == "network") {

		if ((path.size() >= 2)&&(path[1].length() == 16)) {
			uint64_t nwid = Utils::hexStrToU64(path[1].c_str());
			char nwids[24];
			Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);

			if (path.size() >= 3) {
				if ((path.size() == 4)&&(path[2] == "member")&&(path[3].length() == 10)) {
					uint64_t address = Utils::hexStrToU64(path[3].c_str());
					char addrs[24];
					Utils::snprintf(addrs,sizeof(addrs),"%.10llx",address);

					sqlite3_reset(_sGetMember2);
					sqlite3_bind_text(_sGetMember2,1,nwids,16,SQLITE_STATIC);
					sqlite3_bind_text(_sGetMember2,2,addrs,10,SQLITE_STATIC);
					if (sqlite3_step(_sGetMember2) == SQLITE_ROW) {
						Utils::snprintf(json,sizeof(json),
							"{\n"
							"\taddress: \"%s\""
							"\tauthorized: %s,"
							"\tactiveBridge: %s,\n"
							"\tlastAt: \"%s\",\n"
							"\tlastSeen: %llu,\n"
							"\tfirstSeen: %llu,\n"
							"\tidentity: \"%s\"\n"
							"}\n",
							addrs,
							(sqlite3_column_int(_sGetMember2,0) > 0) ? "true" : "false",
							(sqlite3_column_int(_sGetMember2,1) > 0) ? "true" : "false",
							_jsonEscape((const char *)sqlite3_column_text(_sGetMember2,3)).c_str(),
							(unsigned long long)sqlite3_column_int64(_sGetMember2,4),
							(unsigned long long)sqlite3_column_int64(_sGetMember2,5),
							_jsonEscape((const char *)sqlite3_column_text(_sGetMember2,2)).c_str());
						responseBody = json;
						responseContentType = "application/json";
						return 200;
					} // else 404
				} // else 404
			} else {
				// get network info
				sqlite3_reset(_sGetNetworkById);
				sqlite3_bind_text(_sGetNetworkById,1,nwids,16,SQLITE_STATIC);
				if (sqlite3_step(_sGetNetworkById) == SQLITE_ROW) {
					Utils::snprintf(json,sizeof(json),
						"{\n"
						"\tnwid: \"%s\",\n"
						"\tname: \"%s\",\n"
						"\tprivate: %s,\n"
						"\tenableBroadcast: %s,\n"
						"\tallowPassiveBridging: %s,\n"
						"\tv4AssignMode: \"%s\",\n"
						"\tv6AssignMode: \"%s\",\n"
						"\tmulticastLimit: %d,\n"
						"\tcreationTime: %llu,\n",
						"\trevision: %llu,\n"
						"\tmembers: [",
						nwids,
						_jsonEscape((const char *)sqlite3_column_text(_sGetNetworkById,0)).c_str(),
						(sqlite3_column_int(_sGetNetworkById,1) > 0) ? "true" : "false",
						(sqlite3_column_int(_sGetNetworkById,2) > 0) ? "true" : "false",
						(sqlite3_column_int(_sGetNetworkById,3) > 0) ? "true" : "false",
						_jsonEscape((const char *)sqlite3_column_text(_sGetNetworkById,4)).c_str(),
						_jsonEscape((const char *)sqlite3_column_text(_sGetNetworkById,5)).c_str(),
						sqlite3_column_int(_sGetNetworkById,6),
						(unsigned long long)sqlite3_column_int64(_sGetNetworkById,7),
						(unsigned long long)sqlite3_column_int64(_sGetNetworkById,8));
					responseBody = json;

					sqlite3_reset(_sListNetworkMembers);
					sqlite3_bind_text(_sListNetworkMembers,1,nwids,16,SQLITE_STATIC);
					bool firstMember = true;
					while (sqlite3_step(_sListNetworkMembers) == SQLITE_ROW) {
						Utils::snprintf(json,sizeof(json),
							"%s{\n"
							"\t\taddress: \"%s\",\n"
							"\t\tauthorized: %s,\n"
							"\t\tactiveBridge: %s,\n"
							"\t\tlastAt: \"%s\",\n"
							"\t\tlastSeen: %llu,\n"
							"\t\tfirstSeen: %llu\n"
							"\t}",
							firstMember ? "\n\t" : ",",
							(const char *)sqlite3_column_text(_sListNetworkMembers,2),
							(sqlite3_column_int(_sListNetworkMembers,0) > 0) ? "true" : "false",
							(sqlite3_column_int(_sListNetworkMembers,1) > 0) ? "true" : "false",
							_jsonEscape((const char *)sqlite3_column_text(_sListNetworkMembers,3)).c_str(),
							(unsigned long long)sqlite3_column_int64(_sListNetworkMembers,4),
							(unsigned long long)sqlite3_column_int64(_sListNetworkMembers,5));
						responseBody.append(json);
						firstMember = false;
					}
					responseBody.append("],\n\trelays: [");

					sqlite3_reset(_sGetRelays);
					sqlite3_bind_text(_sGetRelays,1,nwids,16,SQLITE_STATIC);
					bool firstRelay = true;
					while (sqlite3_step(_sGetRelays) == SQLITE_ROW) {
						responseBody.append(firstRelay ? "\n\t\t" : ",\n\t\t");
						firstRelay = false;
						responseBody.append("{address:\"");
						responseBody.append((const char *)sqlite3_column_text(_sGetRelays,0));
						responseBody.append("\",phyAddress:\"");
						responseBody.append(_jsonEscape((const char *)sqlite3_column_text(_sGetRelays,1)));
						responseBody.append("\"}");
					}
					responseBody.append("],\n\tipAssignmentPools: [");

					sqlite3_reset(_sGetIpAssignmentPools2);
					sqlite3_bind_text(_sGetIpAssignmentPools2,1,nwids,16,SQLITE_STATIC);
					bool firstIpAssignmentPool = true;
					while (sqlite3_step(_sGetIpAssignmentPools2) == SQLITE_ROW) {
						responseBody.append(firstIpAssignmentPool ? "\n\t\t" : ",\n\t\t");
						firstIpAssignmentPool = false;
						InetAddress ipp((const void *)sqlite3_column_blob(_sGetIpAssignmentPools2,0),(sqlite3_column_int(_sGetIpAssignmentPools2,2) == 6) ? 16 : 4,(unsigned int)sqlite3_column_int(_sGetIpAssignmentPools2,1));
						Utils::snprintf(json,sizeof(json),"{ipNetwork:\"%s\",ipNetmaskBits:%u,ipVersion:%d,active:%s}",
							_jsonEscape(ipp.toIpString()).c_str(),
							ipp.netmaskBits(),
							sqlite3_column_int(_sGetIpAssignmentPools2,2),
							(sqlite3_column_int(_sGetIpAssignmentPools2,3) > 0) ? "true" : "false");
						responseBody.append(json);
					}
					responseBody.append("],\n\trules: [");

					sqlite3_reset(_sListRules);
					sqlite3_bind_text(_sListRules,1,nwids,16,SQLITE_STATIC);
					bool firstRule = true;
					while (sqlite3_step(_sListRules) == SQLITE_ROW) {
						responseBody.append(firstRule ? "\n\t{\n" : ",{\n");
						Utils::snprintf(json,sizeof(json),"\t\truleId: %lld,\n",sqlite3_column_int64(_sListRules,0));
						responseBody.append(json);
						if (sqlite3_column_type(_sListRules,1) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\tnodeId: \"%s\",\n",(const char *)sqlite3_column_text(_sListRules,1));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,2) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\tvlanId: %d,\n",sqlite3_column_int(_sListRules,2));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,3) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\tvlanPcp: %d,\n",sqlite3_column_int(_sListRules,3));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,4) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\tetherType: %d,\n",sqlite3_column_int(_sListRules,4));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,5) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\tmacSource: \"%s\",\n",MAC((const char *)sqlite3_column_text(_sListRules,5)).toString().c_str());
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,6) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\tmacDest: \"%s\",\n",MAC((const char *)sqlite3_column_text(_sListRules,6)).toString().c_str());
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,7) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\tipSource: \"%s\",\n",_jsonEscape((const char *)sqlite3_column_text(_sListRules,7)).c_str());
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,8) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\tipDest: \"%s\",\n",_jsonEscape((const char *)sqlite3_column_text(_sListRules,8)).c_str());
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,9) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\tipTos: %d,\n",sqlite3_column_int(_sListRules,9));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,10) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\tipProtocol: %d,\n",sqlite3_column_int(_sListRules,10));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,11) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\tipSourcePort: %d,\n",sqlite3_column_int(_sListRules,11));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,12) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\tipDestPort: %d,\n",sqlite3_column_int(_sListRules,12));
							responseBody.append(json);
						}
						responseBody.append("\t\taction: \"");
						responseBody.append(_jsonEscape((const char *)sqlite3_column_text(_sListRules,13)));
						responseBody.append("\"\n\t}");
					}

					responseBody.append("]\n}\n");
					responseContentType = "application/json";
					return 200;
				} // else 404
			}
		} else if (path.size() == 1) {
			// list networks
			sqlite3_reset(_sListNetworks);
			responseContentType = "application/json";
			responseBody = "[";
			bool first = true;
			while (sqlite3_step(_sListNetworks) == SQLITE_ROW) {
				if (first) {
					first = false;
					responseBody.push_back('"');
				} else responseBody.append(",\"");
				responseBody.append((const char *)sqlite3_column_text(_sListNetworks,0));
				responseBody.push_back('"');
			}
			responseBody.push_back(']');
			return 200;
		} // else 404

	} // else 404

	return 404;
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
	Mutex::Lock _l(_lock);

	if (path[0] == "network") {

		if ((path.size() >= 2)&&(path[1].length() == 16)) {
			uint64_t nwid = Utils::hexStrToU64(path[1].c_str());
			char nwids[24];
			Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);

			int64_t revision = 0;
			sqlite3_reset(_sGetNetworkRevision);
			sqlite3_bind_text(_sGetNetworkRevision,1,nwids,16,SQLITE_STATIC);
			if (sqlite3_step(_sGetNetworkRevision) == SQLITE_ROW)
				revision = sqlite3_column_int64(_sGetNetworkRevision,0);

			if (path.size() >= 3) {

				if ((path.size() == 4)&&(path[2] == "member")&&(path[3].length() == 10)) {
					uint64_t address = Utils::hexStrToU64(path[3].c_str());
					char addrs[24];
					Utils::snprintf(addrs,sizeof(addrs),"%.10llx",address);

					return handleControlPlaneHttpGET(path,urlArgs,headers,body,responseBody,responseContentType);
				} // else 404

			} else {

				if (revision <= 0) {
					sqlite3_reset(_sCreateNetwork);
					sqlite3_bind_text(_sCreateNetwork,1,nwids,16,SQLITE_STATIC);
					sqlite3_bind_text(_sCreateNetwork,2,nwids,16,SQLITE_STATIC); // default name, will be changed below if a name is specified in JSON
					sqlite3_bind_int64(_sCreateNetwork,3,(long long)OSUtils::now());
					if (sqlite3_step(_sCreateNetwork) != SQLITE_DONE)
						return 500;
				}

				json_value *j = json_parse(body.c_str(),body.length());
				if (j) {
					if (j->type == json_object) {
						for(unsigned int k=0;k<j->u.object.length;++k) {
							sqlite3_reset(_sUpdateNetworkField);
							sqlite3_bind_text(_sUpdateNetworkField,3,nwids,16,SQLITE_STATIC);

							if (!strcmp(j->u.object.values[k].name,"name")) {
								if ((j->u.object.values[k].value->type == json_string)&&(j->u.object.values[k].value->u.string.ptr[0])) {
									sqlite3_bind_text(_sUpdateNetworkField,1,"name",-1,SQLITE_STATIC);
									sqlite3_bind_text(_sUpdateNetworkField,2,j->u.object.values[k].value->u.string.ptr,-1,SQLITE_STATIC);
									sqlite3_step(_sUpdateNetworkField);
								} else return 400;
							}
							if (!strcmp(j->u.object.values[k].name,"private")) {
								if (j->u.object.values[k].value->type == json_boolean) {
									sqlite3_bind_text(_sUpdateNetworkField,1,"private",-1,SQLITE_STATIC);
									sqlite3_bind_int(_sUpdateNetworkField,2,(j->u.object.values[k].value->u.boolean == 0) ? 0 : 1);
									sqlite3_step(_sUpdateNetworkField);
								} else return 400;
							}
							if (!strcmp(j->u.object.values[k].name,"enableBroadcast")) {
								if (j->u.object.values[k].value->type == json_boolean) {
									sqlite3_bind_text(_sUpdateNetworkField,1,"enableBroadcast",-1,SQLITE_STATIC);
									sqlite3_bind_int(_sUpdateNetworkField,2,(j->u.object.values[k].value->u.boolean == 0) ? 0 : 1);
									sqlite3_step(_sUpdateNetworkField);
								} else return 400;
							}
							if (!strcmp(j->u.object.values[k].name,"allowPassiveBridging")) {
								if (j->u.object.values[k].value->type == json_boolean) {
									sqlite3_bind_text(_sUpdateNetworkField,1,"allowPassiveBridging",-1,SQLITE_STATIC);
									sqlite3_bind_int(_sUpdateNetworkField,2,(j->u.object.values[k].value->u.boolean == 0) ? 0 : 1);
									sqlite3_step(_sUpdateNetworkField);
								} else return 400;
							}
							if (!strcmp(j->u.object.values[k].name,"v4AssignMode")) {
								if (j->u.object.values[k].value->type == json_string) {
									sqlite3_bind_text(_sUpdateNetworkField,1,"v4AssignMode",-1,SQLITE_STATIC);
									sqlite3_bind_text(_sUpdateNetworkField,2,j->u.object.values[k].value->u.string.ptr,-1,SQLITE_STATIC);
									sqlite3_step(_sUpdateNetworkField);
								} else return 400;
							}
							if (!strcmp(j->u.object.values[k].name,"v6AssignMode")) {
								if (j->u.object.values[k].value->type == json_string) {
									sqlite3_bind_text(_sUpdateNetworkField,1,"v6AssignMode",-1,SQLITE_STATIC);
									sqlite3_bind_text(_sUpdateNetworkField,2,j->u.object.values[k].value->u.string.ptr,-1,SQLITE_STATIC);
									sqlite3_step(_sUpdateNetworkField);
								} else return 400;
							}
							if (!strcmp(j->u.object.values[k].name,"multicastLimit")) {
								if (j->u.object.values[k].value->type == json_integer) {
									sqlite3_bind_text(_sUpdateNetworkField,1,"multicastLimit",-1,SQLITE_STATIC);
									sqlite3_bind_int(_sUpdateNetworkField,2,(int)j->u.object.values[k].value->u.integer);
									sqlite3_step(_sUpdateNetworkField);
								} else return 400;
							}

							if (!strcmp(j->u.object.values[k].name,"relays")) {
								if (j->u.object.values[k].value->type == json_array) {
								} else return 400;
							}
							if (!strcmp(j->u.object.values[k].name,"ipAssignmentPools")) {
								if (j->u.object.values[k].value->type == json_array) {
								} else return 400;
							}
							if (!strcmp(j->u.object.values[k].name,"rules")) {
								if (j->u.object.values[k].value->type == json_array) {
								} else return 400;
							}
						}
					}
					json_value_free(j);
				}

				return handleControlPlaneHttpGET(path,urlArgs,headers,body,responseBody,responseContentType);
			}

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
	Mutex::Lock _l(_lock);

	return 404;
}

} // namespace ZeroTier
