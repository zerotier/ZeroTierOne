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

#include "../include/ZeroTierOne.h"
#include "../node/Constants.hpp"

#include "../ext/json-parser/json.h"

#include "SqliteNetworkController.hpp"

#include "../node/Node.hpp"
#include "../node/Utils.hpp"
#include "../node/CertificateOfMembership.hpp"
#include "../node/NetworkConfig.hpp"
#include "../node/NetworkConfigRequestMetaData.hpp"
#include "../node/InetAddress.hpp"
#include "../node/MAC.hpp"
#include "../node/Address.hpp"

#include "../osdep/OSUtils.hpp"

// Include ZT_NETCONF_SCHEMA_SQL constant to init database
#include "schema.sql.c"

// Stored in database as schemaVersion key in Config.
// If not present, database is assumed to be empty and at the current schema version
// and this key/value is added automatically.
#define ZT_NETCONF_SQLITE_SCHEMA_VERSION 2
#define ZT_NETCONF_SQLITE_SCHEMA_VERSION_STR "2"

// API version reported via JSON control plane
#define ZT_NETCONF_CONTROLLER_API_VERSION 1

// Min duration between requests for an address/nwid combo to prevent floods
#define ZT_NETCONF_MIN_REQUEST_PERIOD 1000

// Delay between backups in milliseconds
#define ZT_NETCONF_BACKUP_PERIOD 300000

// Number of NodeHistory entries to maintain per node and network (can be changed)
#define ZT_NETCONF_NODE_HISTORY_LENGTH 64

// Nodes are considered active if they've queried in less than this long
#define ZT_NETCONF_NODE_ACTIVE_THRESHOLD ((ZT_NETWORK_AUTOCONF_DELAY * 2) + 5000)

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
	uint64_t memberRevisionCounter;
};

} // anonymous namespace

SqliteNetworkController::SqliteNetworkController(Node *node,const char *dbPath,const char *circuitTestPath) :
	_node(node),
	_backupThreadRun(true),
	_dbPath(dbPath),
	_circuitTestPath(circuitTestPath),
	_db((sqlite3 *)0)
{
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
		} else if (schemaVersion == 1) {
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
					"\n"
					"CREATE INDEX NodeHistory_nodeId ON NodeHistory (nodeId);\n"
					"CREATE INDEX NodeHistory_networkId ON NodeHistory (networkId);\n"
					"CREATE INDEX NodeHistory_requestTime ON NodeHistory (requestTime);\n"
					"\n"
					"UPDATE \"Config\" SET \"v\" = 2 WHERE \"k\" = 'schemaVersion';\n"
				,0,0,0) != SQLITE_OK) {
				char err[1024];
				Utils::snprintf(err,sizeof(err),"SqliteNetworkController cannot upgrade the database to version 2: %s",sqlite3_errmsg(_db));
				sqlite3_close(_db);
				throw std::runtime_error(err);
			}
		} else if (schemaVersion != ZT_NETCONF_SQLITE_SCHEMA_VERSION) {
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

			/* Network */
			  (sqlite3_prepare_v2(_db,"SELECT name,private,enableBroadcast,allowPassiveBridging,v4AssignMode,v6AssignMode,multicastLimit,creationTime,revision,memberRevisionCounter,(SELECT COUNT(1) FROM Member WHERE Member.networkId = Network.id AND Member.authorized > 0) FROM Network WHERE id = ?",-1,&_sGetNetworkById,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT revision FROM Network WHERE id = ?",-1,&_sGetNetworkRevision,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Network SET revision = ? WHERE id = ?",-1,&_sSetNetworkRevision,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Network (id,name,creationTime,revision) VALUES (?,?,?,1)",-1,&_sCreateNetwork,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Network WHERE id = ?",-1,&_sDeleteNetwork,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT id FROM Network ORDER BY id ASC",-1,&_sListNetworks,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Network SET memberRevisionCounter = (memberRevisionCounter + 1) WHERE id = ?",-1,&_sIncrementMemberRevisionCounter,(const char **)0) != SQLITE_OK)

			/* Node */
			||(sqlite3_prepare_v2(_db,"SELECT identity FROM Node WHERE id = ?",-1,&_sGetNodeIdentity,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT OR REPLACE INTO Node (id,identity) VALUES (?,?)",-1,&_sCreateOrReplaceNode,(const char **)0) != SQLITE_OK)

			/* NodeHistory */
			||(sqlite3_prepare_v2(_db,"SELECT IFNULL(MAX(networkVisitCounter),0) FROM NodeHistory WHERE networkId = ? AND nodeId = ?",-1,&_sGetMaxNodeHistoryNetworkVisitCounter,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO NodeHistory (nodeId,networkId,networkVisitCounter,networkRequestAuthorized,requestTime,clientMajorVersion,clientMinorVersion,clientRevision,networkRequestMetaData,fromAddress) VALUES (?,?,?,?,?,?,?,?,?,?)",-1,&_sAddNodeHistoryEntry,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM NodeHistory WHERE networkId = ? AND nodeId = ? AND networkVisitCounter <= ?",-1,&_sDeleteOldNodeHistoryEntries,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT nodeId,requestTime,clientMajorVersion,clientMinorVersion,clientRevision,fromAddress,networkRequestAuthorized FROM NodeHistory WHERE networkId = ? AND requestTime IN (SELECT MAX(requestTime) FROM NodeHistory WHERE networkId = ? AND requestTime >= ? GROUP BY nodeId) ORDER BY nodeId ASC,requestTime DESC",-1,&_sGetActiveNodesOnNetwork,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT networkVisitCounter,networkRequestAuthorized,requestTime,clientMajorVersion,clientMinorVersion,clientRevision,networkRequestMetaData,fromAddress FROM NodeHistory WHERE networkId = ? AND nodeId = ? ORDER BY requestTime DESC",-1,&_sGetNodeHistory,(const char **)0) != SQLITE_OK)

			/* Rule */
			||(sqlite3_prepare_v2(_db,"SELECT etherType FROM Rule WHERE networkId = ? AND \"action\" = 'accept'",-1,&_sGetEtherTypesFromRuleTable,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Rule (networkId,ruleNo,nodeId,sourcePort,destPort,vlanId,vlanPcP,etherType,macSource,macDest,ipSource,ipDest,ipTos,ipProtocol,ipSourcePort,ipDestPort,flags,invFlags,\"action\") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",-1,&_sCreateRule,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT ruleNo,nodeId,sourcePort,destPort,vlanId,vlanPcp,etherType,macSource,macDest,ipSource,ipDest,ipTos,ipProtocol,ipSourcePort,ipDestPort,\"flags\",invFlags,\"action\" FROM Rule WHERE networkId = ? ORDER BY ruleNo ASC",-1,&_sListRules,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Rule WHERE networkId = ?",-1,&_sDeleteRulesForNetwork,(const char **)0) != SQLITE_OK)

			/* IpAssignmentPool */
			||(sqlite3_prepare_v2(_db,"SELECT ipRangeStart,ipRangeEnd FROM IpAssignmentPool WHERE networkId = ? AND ipVersion = ?",-1,&_sGetIpAssignmentPools,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT ipRangeStart,ipRangeEnd,ipVersion FROM IpAssignmentPool WHERE networkId = ? ORDER BY ipRangeStart ASC",-1,&_sGetIpAssignmentPools2,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO IpAssignmentPool (networkId,ipRangeStart,ipRangeEnd,ipVersion) VALUES (?,?,?,?)",-1,&_sCreateIpAssignmentPool,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM IpAssignmentPool WHERE networkId = ?",-1,&_sDeleteIpAssignmentPoolsForNetwork,(const char **)0) != SQLITE_OK)

			/* IpAssignment */
			||(sqlite3_prepare_v2(_db,"SELECT \"type\",ip,ipNetmaskBits FROM IpAssignment WHERE networkId = ? AND (nodeId = ? OR nodeId IS NULL) AND ipVersion = ?",-1,&_sGetIpAssignmentsForNode,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT ip,ipNetmaskBits,ipVersion FROM IpAssignment WHERE networkId = ? AND nodeId = ? AND \"type\" = ? ORDER BY ip ASC",-1,&_sGetIpAssignmentsForNode2,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT ip,ipNetmaskBits,ipVersion FROM IpAssignment WHERE networkId = ? AND nodeId IS NULL AND \"type\" = ?",-1,&_sGetLocalRoutes,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT 1 FROM IpAssignment WHERE networkId = ? AND ip = ? AND ipVersion = ? AND \"type\" = ?",-1,&_sCheckIfIpIsAllocated,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO IpAssignment (networkId,nodeId,\"type\",ip,ipNetmaskBits,ipVersion) VALUES (?,?,?,?,?,?)",-1,&_sAllocateIp,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM IpAssignment WHERE networkId = ? AND nodeId = ? AND \"type\" = ?",-1,&_sDeleteIpAllocations,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM IpAssignment WHERE networkId = ? AND nodeId IS NULL AND \"type\" = ?",-1,&_sDeleteLocalRoutes,(const char **)0) != SQLITE_OK)

			/* Relay */
			||(sqlite3_prepare_v2(_db,"SELECT \"address\",\"phyAddress\" FROM Relay WHERE \"networkId\" = ? ORDER BY \"address\" ASC",-1,&_sGetRelays,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Relay WHERE networkId = ?",-1,&_sDeleteRelaysForNetwork,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Relay (\"networkId\",\"address\",\"phyAddress\") VALUES (?,?,?)",-1,&_sCreateRelay,(const char **)0) != SQLITE_OK)

			/* Member */
			||(sqlite3_prepare_v2(_db,"SELECT rowid,authorized,activeBridge FROM Member WHERE networkId = ? AND nodeId = ?",-1,&_sGetMember,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT m.authorized,m.activeBridge,m.memberRevision,n.identity FROM Member AS m LEFT OUTER JOIN Node AS n ON n.id = m.nodeId WHERE m.networkId = ? AND m.nodeId = ?",-1,&_sGetMember2,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Member (networkId,nodeId,authorized,activeBridge,memberRevision) VALUES (?,?,?,0,(SELECT memberRevisionCounter FROM Network WHERE id = ?))",-1,&_sCreateMember,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT nodeId FROM Member WHERE networkId = ? AND activeBridge > 0 AND authorized > 0",-1,&_sGetActiveBridges,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"SELECT m.nodeId,m.memberRevision FROM Member AS m WHERE m.networkId = ? ORDER BY m.nodeId ASC",-1,&_sListNetworkMembers,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Member SET authorized = ?,memberRevision = (SELECT memberRevisionCounter FROM Network WHERE id = ?) WHERE rowid = ?",-1,&_sUpdateMemberAuthorized,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"UPDATE Member SET activeBridge = ?,memberRevision = (SELECT memberRevisionCounter FROM Network WHERE id = ?) WHERE rowid = ?",-1,&_sUpdateMemberActiveBridge,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Member WHERE networkId = ? AND nodeId = ?",-1,&_sDeleteMember,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Member WHERE networkId = ?",-1,&_sDeleteAllNetworkMembers,(const char **)0) != SQLITE_OK)

			/* Gateway */
			||(sqlite3_prepare_v2(_db,"SELECT \"ip\",ipVersion,metric FROM Gateway WHERE networkId = ? ORDER BY metric ASC",-1,&_sGetGateways,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"DELETE FROM Gateway WHERE networkId = ?",-1,&_sDeleteGateways,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT INTO Gateway (networkId,\"ip\",ipVersion,metric) VALUES (?,?,?,?)",-1,&_sCreateGateway,(const char **)0) != SQLITE_OK)

			/* Config */
			||(sqlite3_prepare_v2(_db,"SELECT \"v\" FROM \"Config\" WHERE \"k\" = ?",-1,&_sGetConfig,(const char **)0) != SQLITE_OK)
			||(sqlite3_prepare_v2(_db,"INSERT OR REPLACE INTO \"Config\" (\"k\",\"v\") VALUES (?,?)",-1,&_sSetConfig,(const char **)0) != SQLITE_OK)

		 ) {
		std::string err(std::string("SqliteNetworkController unable to initialize one or more prepared statements: ") + sqlite3_errmsg(_db));
		sqlite3_close(_db);
		throw std::runtime_error(err);
	}

	/* Generate a 128-bit / 32-character "instance ID" if one isn't already
	 * defined. Clients can use this to determine if this is the same controller
	 * database they know and love. */
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

	_backupThread = Thread::start(this);
}

SqliteNetworkController::~SqliteNetworkController()
{
	_backupThreadRun = false;
	Thread::join(_backupThread);

	Mutex::Lock _l(_lock);
	if (_db) {
		sqlite3_finalize(_sGetNetworkById);
		sqlite3_finalize(_sGetMember);
		sqlite3_finalize(_sCreateMember);
		sqlite3_finalize(_sGetNodeIdentity);
		sqlite3_finalize(_sCreateOrReplaceNode);
		sqlite3_finalize(_sGetMaxNodeHistoryNetworkVisitCounter);
		sqlite3_finalize(_sAddNodeHistoryEntry);
		sqlite3_finalize(_sDeleteOldNodeHistoryEntries);
		sqlite3_finalize(_sGetActiveNodesOnNetwork);
		sqlite3_finalize(_sGetNodeHistory);
		sqlite3_finalize(_sGetEtherTypesFromRuleTable);
		sqlite3_finalize(_sGetActiveBridges);
		sqlite3_finalize(_sGetIpAssignmentsForNode);
		sqlite3_finalize(_sGetIpAssignmentPools);
		sqlite3_finalize(_sGetLocalRoutes);
		sqlite3_finalize(_sCheckIfIpIsAllocated);
		sqlite3_finalize(_sAllocateIp);
		sqlite3_finalize(_sDeleteIpAllocations);
		sqlite3_finalize(_sDeleteLocalRoutes);
		sqlite3_finalize(_sGetRelays);
		sqlite3_finalize(_sListNetworks);
		sqlite3_finalize(_sListNetworkMembers);
		sqlite3_finalize(_sGetMember2);
		sqlite3_finalize(_sGetIpAssignmentPools2);
		sqlite3_finalize(_sListRules);
		sqlite3_finalize(_sCreateRule);
		sqlite3_finalize(_sCreateNetwork);
		sqlite3_finalize(_sGetNetworkRevision);
		sqlite3_finalize(_sSetNetworkRevision);
		sqlite3_finalize(_sGetIpAssignmentsForNode2);
		sqlite3_finalize(_sDeleteRelaysForNetwork);
		sqlite3_finalize(_sCreateRelay);
		sqlite3_finalize(_sDeleteIpAssignmentPoolsForNetwork);
		sqlite3_finalize(_sDeleteRulesForNetwork);
		sqlite3_finalize(_sCreateIpAssignmentPool);
		sqlite3_finalize(_sUpdateMemberAuthorized);
		sqlite3_finalize(_sUpdateMemberActiveBridge);
		sqlite3_finalize(_sDeleteMember);
		sqlite3_finalize(_sDeleteAllNetworkMembers);
		sqlite3_finalize(_sDeleteNetwork);
		sqlite3_finalize(_sGetGateways);
		sqlite3_finalize(_sDeleteGateways);
		sqlite3_finalize(_sCreateGateway);
		sqlite3_finalize(_sIncrementMemberRevisionCounter);
		sqlite3_finalize(_sGetConfig);
		sqlite3_finalize(_sSetConfig);
		sqlite3_close(_db);
	}
}

NetworkController::ResultCode SqliteNetworkController::doNetworkConfigRequest(const InetAddress &fromAddr,const Identity &signingId,const Identity &identity,uint64_t nwid,const NetworkConfigRequestMetaData &metaData,Buffer<8194> &netconf)
{
	Mutex::Lock _l(_lock);
	return _doNetworkConfigRequest(fromAddr,signingId,identity,nwid,metaData,netconf);
}

unsigned int SqliteNetworkController::handleControlPlaneHttpGET(
	const std::vector<std::string> &path,
	const std::map<std::string,std::string> &urlArgs,
	const std::map<std::string,std::string> &headers,
	const std::string &body,
	std::string &responseBody,
	std::string &responseContentType)
{
	Mutex::Lock _l(_lock);
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
	Mutex::Lock _l(_lock);

	if (path[0] == "network") {

		if ((path.size() >= 2)&&(path[1].length() == 16)) {
			uint64_t nwid = Utils::hexStrToU64(path[1].c_str());
			char nwids[24];
			Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);

			int64_t revision = 0;
			sqlite3_reset(_sGetNetworkRevision);
			sqlite3_bind_text(_sGetNetworkRevision,1,nwids,16,SQLITE_STATIC);
			bool networkExists = false;
			if (sqlite3_step(_sGetNetworkRevision) == SQLITE_ROW) {
				networkExists = true;
				revision = sqlite3_column_int64(_sGetNetworkRevision,0);
			}

			if (path.size() >= 3) {

				if (!networkExists)
					return 404;

				if ((path.size() == 4)&&(path[2] == "member")&&(path[3].length() == 10)) {
					uint64_t address = Utils::hexStrToU64(path[3].c_str());
					char addrs[24];
					Utils::snprintf(addrs,sizeof(addrs),"%.10llx",address);

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
										sqlite3_bind_int(_sDeleteIpAllocations,3,(int)ZT_IP_ASSIGNMENT_TYPE_ADDRESS);
										if (sqlite3_step(_sDeleteIpAllocations) != SQLITE_DONE)
											return 500;
										for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
											json_value *ipalloc = j->u.object.values[k].value->u.array.values[kk];
											if (ipalloc->type == json_string) {
												InetAddress a(ipalloc->u.string.ptr);
												char ipBlob[16];
												int ipVersion = 0;
												switch(a.ss_family) {
													case AF_INET:
														if ((a.netmaskBits() > 0)&&(a.netmaskBits() <= 32)) {
															memset(ipBlob,0,12);
															memcpy(ipBlob + 12,a.rawIpData(),4);
															ipVersion = 4;
														}
														break;
													case AF_INET6:
														if ((a.netmaskBits() > 0)&&(a.netmaskBits() <= 128)) {
															memcpy(ipBlob,a.rawIpData(),16);
															ipVersion = 6;
														}
														break;
												}
												if (ipVersion > 0) {
													sqlite3_reset(_sAllocateIp);
													sqlite3_bind_text(_sAllocateIp,1,nwids,16,SQLITE_STATIC);
													sqlite3_bind_text(_sAllocateIp,2,addrs,10,SQLITE_STATIC);
													sqlite3_bind_int(_sAllocateIp,3,(int)ZT_IP_ASSIGNMENT_TYPE_ADDRESS);
													sqlite3_bind_blob(_sAllocateIp,4,(const void *)ipBlob,16,SQLITE_STATIC);
													sqlite3_bind_int(_sAllocateIp,5,(int)a.netmaskBits());
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

					return _doCPGet(path,urlArgs,headers,body,responseBody,responseContentType);
				} else if ((path.size() == 3)&&(path[2] == "test")) {
					ZT_CircuitTest *test = (ZT_CircuitTest *)malloc(sizeof(ZT_CircuitTest));
					memset(test,0,sizeof(ZT_CircuitTest));

					Utils::getSecureRandom(&(test->testId),sizeof(test->testId));
					test->credentialNetworkId = nwid;
					test->ptr = (void *)this;

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
				std::vector<std::string> path_copy(path);

				if (!networkExists) {
					if (path[1].substr(10) == "______") {
						// A special POST /network/##########______ feature lets users create a network
						// with an arbitrary unused network number at this controller.
						nwid = 0;

						uint64_t nwidPrefix = (Utils::hexStrToU64(path[1].substr(0,10).c_str()) << 24) & 0xffffffffff000000ULL;
						uint64_t nwidPostfix = 0;
						Utils::getSecureRandom(&nwidPostfix,sizeof(nwidPostfix));
						uint64_t nwidOriginalPostfix = nwidPostfix;
						do {
							uint64_t tryNwid = nwidPrefix | (nwidPostfix & 0xffffffULL);
							if (!nwidPostfix)
								tryNwid |= 1;
							Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)tryNwid);

							sqlite3_reset(_sGetNetworkRevision);
							sqlite3_bind_text(_sGetNetworkRevision,1,nwids,16,SQLITE_STATIC);
							if (sqlite3_step(_sGetNetworkRevision) != SQLITE_ROW) {
								nwid = tryNwid;
								break;
							}

							++nwidPostfix;
						} while (nwidPostfix != nwidOriginalPostfix);

						// 503 means we have no more free IDs for this prefix. You shouldn't host anywhere
						// near 16 million networks on the same controller, so shouldn't happen.
						if (!nwid)
							return 503;
					}

					sqlite3_reset(_sCreateNetwork);
					sqlite3_bind_text(_sCreateNetwork,1,nwids,16,SQLITE_STATIC);
					sqlite3_bind_text(_sCreateNetwork,2,"",0,SQLITE_STATIC);
					sqlite3_bind_int64(_sCreateNetwork,3,(long long)OSUtils::now());
					if (sqlite3_step(_sCreateNetwork) != SQLITE_DONE)
						return 500;
					path_copy[1].assign(nwids);
				}

				json_value *j = json_parse(body.c_str(),body.length());
				if (j) {
					if (j->type == json_object) {
						for(unsigned int k=0;k<j->u.object.length;++k) {
							sqlite3_stmt *stmt = (sqlite3_stmt *)0;

							if (!strcmp(j->u.object.values[k].name,"name")) {
								if ((j->u.object.values[k].value->type == json_string)&&(j->u.object.values[k].value->u.string.ptr[0])) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET \"name\" = ? WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_text(stmt,1,j->u.object.values[k].value->u.string.ptr,-1,SQLITE_STATIC);
								}
							} else if (!strcmp(j->u.object.values[k].name,"private")) {
								if (j->u.object.values[k].value->type == json_boolean) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET \"private\" = ? WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_int(stmt,1,(j->u.object.values[k].value->u.boolean == 0) ? 0 : 1);
								}
							} else if (!strcmp(j->u.object.values[k].name,"enableBroadcast")) {
								if (j->u.object.values[k].value->type == json_boolean) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET enableBroadcast = ? WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_int(stmt,1,(j->u.object.values[k].value->u.boolean == 0) ? 0 : 1);
								}
							} else if (!strcmp(j->u.object.values[k].name,"allowPassiveBridging")) {
								if (j->u.object.values[k].value->type == json_boolean) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET allowPassiveBridging = ? WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_int(stmt,1,(j->u.object.values[k].value->u.boolean == 0) ? 0 : 1);
								}
							} else if (!strcmp(j->u.object.values[k].name,"v4AssignMode")) {
								if (j->u.object.values[k].value->type == json_string) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET v4AssignMode = ? WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_text(stmt,1,j->u.object.values[k].value->u.string.ptr,-1,SQLITE_STATIC);
								}
							} else if (!strcmp(j->u.object.values[k].name,"v6AssignMode")) {
								if (j->u.object.values[k].value->type == json_string) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET v6AssignMode = ? WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_text(stmt,1,j->u.object.values[k].value->u.string.ptr,-1,SQLITE_STATIC);
								}
							} else if (!strcmp(j->u.object.values[k].name,"multicastLimit")) {
								if (j->u.object.values[k].value->type == json_integer) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET multicastLimit = ? WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_int(stmt,1,(int)j->u.object.values[k].value->u.integer);
								}
							} else if (!strcmp(j->u.object.values[k].name,"relays")) {
								if (j->u.object.values[k].value->type == json_array) {
									std::map<Address,InetAddress> nodeIdToPhyAddress;
									for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
										json_value *relay = j->u.object.values[k].value->u.array.values[kk];
										const char *address = (const char *)0;
										const char *phyAddress = (const char *)0;
										if ((relay)&&(relay->type == json_object)) {
											for(unsigned int rk=0;rk<relay->u.object.length;++rk) {
												if ((!strcmp(relay->u.object.values[rk].name,"address"))&&(relay->u.object.values[rk].value->type == json_string))
													address = relay->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(relay->u.object.values[rk].name,"phyAddress"))&&(relay->u.object.values[rk].value->type == json_string))
													phyAddress = relay->u.object.values[rk].value->u.string.ptr;
											}
										}
										if ((address)&&(phyAddress))
											nodeIdToPhyAddress[Address(address)] = InetAddress(phyAddress);
									}

									sqlite3_reset(_sDeleteRelaysForNetwork);
									sqlite3_bind_text(_sDeleteRelaysForNetwork,1,nwids,16,SQLITE_STATIC);
									sqlite3_step(_sDeleteRelaysForNetwork);

									for(std::map<Address,InetAddress>::iterator rl(nodeIdToPhyAddress.begin());rl!=nodeIdToPhyAddress.end();++rl) {
										sqlite3_reset(_sCreateRelay);
										sqlite3_bind_text(_sCreateRelay,1,nwids,16,SQLITE_STATIC);
										std::string a(rl->first.toString()),b(rl->second.toString()); // don't destroy strings until sqlite3_step()
										sqlite3_bind_text(_sCreateRelay,2,a.c_str(),-1,SQLITE_STATIC);
										sqlite3_bind_text(_sCreateRelay,3,b.c_str(),-1,SQLITE_STATIC);
										sqlite3_step(_sCreateRelay);
									}
								}
							} else if (!strcmp(j->u.object.values[k].name,"gateways")) {
								sqlite3_reset(_sDeleteGateways);
								sqlite3_bind_text(_sDeleteGateways,1,nwids,16,SQLITE_STATIC);
								sqlite3_step(_sDeleteGateways);
								if (j->u.object.values[k].value->type == json_array) {
									for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
										json_value *gateway = j->u.object.values[k].value->u.array.values[kk];
										if ((gateway)&&(gateway->type == json_string)) {
											InetAddress gwip(gateway->u.string.ptr);
											sqlite3_reset(_sCreateGateway);
											sqlite3_bind_text(_sCreateGateway,1,nwids,16,SQLITE_STATIC);
											sqlite3_bind_int(_sCreateGateway,4,(int)gwip.metric());
											if (gwip.ss_family == AF_INET) {
												char ipBlob[16];
												memset(ipBlob,0,12);
												memcpy(ipBlob + 12,gwip.rawIpData(),4);
												sqlite3_bind_blob(_sCreateGateway,2,(const void *)ipBlob,16,SQLITE_STATIC);
												sqlite3_bind_int(_sCreateGateway,3,4);
												sqlite3_step(_sCreateGateway);
											} else if (gwip.ss_family == AF_INET6) {
												sqlite3_bind_blob(_sCreateGateway,2,gwip.rawIpData(),16,SQLITE_STATIC);
												sqlite3_bind_int(_sCreateGateway,3,6);
												sqlite3_step(_sCreateGateway);
											}
										}
									}
								}
							} else if (!strcmp(j->u.object.values[k].name,"ipLocalRoutes")) {
								sqlite3_reset(_sDeleteLocalRoutes);
								sqlite3_bind_text(_sDeleteLocalRoutes,1,nwids,16,SQLITE_STATIC);
								sqlite3_bind_int(_sDeleteLocalRoutes,2,(int)ZT_IP_ASSIGNMENT_TYPE_NETWORK);
								sqlite3_step(_sDeleteLocalRoutes);
								if (j->u.object.values[k].value->type == json_array) {
									for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
										json_value *localRoute = j->u.object.values[k].value->u.array.values[kk];
										if ((localRoute)&&(localRoute->type == json_string)) {
											InetAddress lr(localRoute->u.string.ptr);
											if (lr.ss_family == AF_INET) {
												char ipBlob[16];
												memset(ipBlob,0,12);
												memcpy(ipBlob + 12,lr.rawIpData(),4);
												sqlite3_reset(_sAllocateIp);
												sqlite3_bind_text(_sAllocateIp,1,nwids,16,SQLITE_STATIC);
												sqlite3_bind_null(_sAllocateIp,2);
												sqlite3_bind_int(_sAllocateIp,3,(int)ZT_IP_ASSIGNMENT_TYPE_NETWORK);
												sqlite3_bind_blob(_sAllocateIp,4,(const void *)ipBlob,16,SQLITE_STATIC);
												sqlite3_bind_int(_sAllocateIp,5,lr.netmaskBits());
												sqlite3_bind_int(_sAllocateIp,6,4);
												sqlite3_step(_sAllocateIp);
											} else if (lr.ss_family == AF_INET6) {
												sqlite3_reset(_sAllocateIp);
												sqlite3_bind_text(_sAllocateIp,1,nwids,16,SQLITE_STATIC);
												sqlite3_bind_null(_sAllocateIp,2);
												sqlite3_bind_int(_sAllocateIp,3,(int)ZT_IP_ASSIGNMENT_TYPE_NETWORK);
												sqlite3_bind_blob(_sAllocateIp,4,lr.rawIpData(),16,SQLITE_STATIC);
												sqlite3_bind_int(_sAllocateIp,5,lr.netmaskBits());
												sqlite3_bind_int(_sAllocateIp,6,6);
												sqlite3_step(_sAllocateIp);
											}
										}
									}
								}
							} else if (!strcmp(j->u.object.values[k].name,"ipAssignmentPools")) {
								if (j->u.object.values[k].value->type == json_array) {
									std::vector< std::pair<InetAddress,InetAddress> > pools;
									for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
										json_value *pool = j->u.object.values[k].value->u.array.values[kk];
										const char *iprs = (const char *)0;
										const char *ipre = (const char *)0;
										if ((pool)&&(pool->type == json_object)) {
											for(unsigned int rk=0;rk<pool->u.object.length;++rk) {
												if ((!strcmp(pool->u.object.values[rk].name,"ipRangeStart"))&&(pool->u.object.values[rk].value->type == json_string))
													iprs = pool->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(pool->u.object.values[rk].name,"ipRangeEnd"))&&(pool->u.object.values[rk].value->type == json_string))
													ipre = pool->u.object.values[rk].value->u.string.ptr;
											}
										}
										if ((iprs)&&(ipre)) {
											InetAddress iprs2(iprs);
											InetAddress ipre2(ipre);
											if (iprs2.ss_family == ipre2.ss_family) {
												iprs2.setPort(0);
												ipre2.setPort(0);
												pools.push_back(std::pair<InetAddress,InetAddress>(iprs2,ipre2));
											}
										}
									}
									std::sort(pools.begin(),pools.end());
									pools.erase(std::unique(pools.begin(),pools.end()),pools.end());

									sqlite3_reset(_sDeleteIpAssignmentPoolsForNetwork);
									sqlite3_bind_text(_sDeleteIpAssignmentPoolsForNetwork,1,nwids,16,SQLITE_STATIC);
									sqlite3_step(_sDeleteIpAssignmentPoolsForNetwork);

									for(std::vector< std::pair<InetAddress,InetAddress> >::const_iterator p(pools.begin());p!=pools.end();++p) {
										char ipBlob1[16],ipBlob2[16];
										sqlite3_reset(_sCreateIpAssignmentPool);
										sqlite3_bind_text(_sCreateIpAssignmentPool,1,nwids,16,SQLITE_STATIC);
										if (p->first.ss_family == AF_INET) {
											memset(ipBlob1,0,12);
											memcpy(ipBlob1 + 12,p->first.rawIpData(),4);
											memset(ipBlob2,0,12);
											memcpy(ipBlob2 + 12,p->second.rawIpData(),4);
											sqlite3_bind_blob(_sCreateIpAssignmentPool,2,(const void *)ipBlob1,16,SQLITE_STATIC);
											sqlite3_bind_blob(_sCreateIpAssignmentPool,3,(const void *)ipBlob2,16,SQLITE_STATIC);
											sqlite3_bind_int(_sCreateIpAssignmentPool,4,4);
										} else if (p->first.ss_family == AF_INET6) {
											sqlite3_bind_blob(_sCreateIpAssignmentPool,2,p->first.rawIpData(),16,SQLITE_STATIC);
											sqlite3_bind_blob(_sCreateIpAssignmentPool,3,p->second.rawIpData(),16,SQLITE_STATIC);
											sqlite3_bind_int(_sCreateIpAssignmentPool,4,6);
										} else continue;
										sqlite3_step(_sCreateIpAssignmentPool);
									}
								}
							} else if (!strcmp(j->u.object.values[k].name,"rules")) {
								if (j->u.object.values[k].value->type == json_array) {
									sqlite3_reset(_sDeleteRulesForNetwork);
									sqlite3_bind_text(_sDeleteRulesForNetwork,1,nwids,16,SQLITE_STATIC);
									sqlite3_step(_sDeleteRulesForNetwork);

									for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
										json_value *rj = j->u.object.values[k].value->u.array.values[kk];
										if ((rj)&&(rj->type == json_object)) {
											struct { // NULL pointers indicate missing or NULL -- wildcards
												const json_int_t *ruleNo;
												const char *nodeId;
												const char *sourcePort;
												const char *destPort;
												const json_int_t *vlanId;
												const json_int_t *vlanPcp;
												const json_int_t *etherType;
												const char *macSource;
												const char *macDest;
												const char *ipSource;
												const char *ipDest;
												const json_int_t *ipTos;
												const json_int_t *ipProtocol;
												const json_int_t *ipSourcePort;
												const json_int_t *ipDestPort;
												const json_int_t *flags;
												const json_int_t *invFlags;
												const char *action;
											} rule;
											memset(&rule,0,sizeof(rule));

											for(unsigned int rk=0;rk<rj->u.object.length;++rk) {
												if ((!strcmp(rj->u.object.values[rk].name,"ruleNo"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.ruleNo = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"nodeId"))&&(rj->u.object.values[rk].value->type == json_string))
													rule.nodeId = rj->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(rj->u.object.values[rk].name,"sourcePort"))&&(rj->u.object.values[rk].value->type == json_string))
													rule.sourcePort = rj->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(rj->u.object.values[rk].name,"destPort"))&&(rj->u.object.values[rk].value->type == json_string))
													rule.destPort = rj->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(rj->u.object.values[rk].name,"vlanId"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.vlanId = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"vlanPcp"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.vlanPcp = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"etherType"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.etherType = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"macSource"))&&(rj->u.object.values[rk].value->type == json_string))
													rule.macSource = rj->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(rj->u.object.values[rk].name,"macDest"))&&(rj->u.object.values[rk].value->type == json_string))
													rule.macDest = rj->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(rj->u.object.values[rk].name,"ipSource"))&&(rj->u.object.values[rk].value->type == json_string))
													rule.ipSource = rj->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(rj->u.object.values[rk].name,"ipDest"))&&(rj->u.object.values[rk].value->type == json_string))
													rule.ipDest = rj->u.object.values[rk].value->u.string.ptr;
												else if ((!strcmp(rj->u.object.values[rk].name,"ipTos"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.ipTos = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"ipProtocol"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.ipProtocol = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"ipSourcePort"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.ipSourcePort = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"ipDestPort"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.ipDestPort = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"flags"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.flags = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"invFlags"))&&(rj->u.object.values[rk].value->type == json_integer))
													rule.invFlags = &(rj->u.object.values[rk].value->u.integer);
												else if ((!strcmp(rj->u.object.values[rk].name,"action"))&&(rj->u.object.values[rk].value->type == json_string))
													rule.action = rj->u.object.values[rk].value->u.string.ptr;
											}

											if ((rule.ruleNo)&&(rule.action)&&(rule.action[0])) {
												char mactmp1[16],mactmp2[16];
												sqlite3_reset(_sCreateRule);
												sqlite3_bind_text(_sCreateRule,1,nwids,16,SQLITE_STATIC);
												sqlite3_bind_int64(_sCreateRule,2,*rule.ruleNo);

												// Optional values: null by default
												for(int i=3;i<=18;++i)
													sqlite3_bind_null(_sCreateRule,i);
												if ((rule.nodeId)&&(strlen(rule.nodeId) == 10)) sqlite3_bind_text(_sCreateRule,3,rule.nodeId,10,SQLITE_STATIC);
												if ((rule.sourcePort)&&(strlen(rule.sourcePort) == 10)) sqlite3_bind_text(_sCreateRule,4,rule.sourcePort,10,SQLITE_STATIC);
												if ((rule.destPort)&&(strlen(rule.destPort) == 10)) sqlite3_bind_text(_sCreateRule,5,rule.destPort,10,SQLITE_STATIC);
												if (rule.vlanId) sqlite3_bind_int(_sCreateRule,6,(int)*rule.vlanId);
												if (rule.vlanPcp) sqlite3_bind_int(_sCreateRule,7,(int)*rule.vlanPcp);
												if (rule.etherType) sqlite3_bind_int(_sCreateRule,8,(int)*rule.etherType & (int)0xffff);
												if (rule.macSource) {
													MAC m(rule.macSource);
													Utils::snprintf(mactmp1,sizeof(mactmp1),"%.12llx",(unsigned long long)m.toInt());
													sqlite3_bind_text(_sCreateRule,9,mactmp1,-1,SQLITE_STATIC);
												}
												if (rule.macDest) {
													MAC m(rule.macDest);
													Utils::snprintf(mactmp2,sizeof(mactmp2),"%.12llx",(unsigned long long)m.toInt());
													sqlite3_bind_text(_sCreateRule,10,mactmp2,-1,SQLITE_STATIC);
												}
												if (rule.ipSource) sqlite3_bind_text(_sCreateRule,11,rule.ipSource,-1,SQLITE_STATIC);
												if (rule.ipDest) sqlite3_bind_text(_sCreateRule,12,rule.ipDest,-1,SQLITE_STATIC);
												if (rule.ipTos) sqlite3_bind_int(_sCreateRule,13,(int)*rule.ipTos);
												if (rule.ipProtocol) sqlite3_bind_int(_sCreateRule,14,(int)*rule.ipProtocol);
												if (rule.ipSourcePort) sqlite3_bind_int(_sCreateRule,15,(int)*rule.ipSourcePort & (int)0xffff);
												if (rule.ipDestPort) sqlite3_bind_int(_sCreateRule,16,(int)*rule.ipDestPort & (int)0xffff);
												if (rule.flags) sqlite3_bind_int64(_sCreateRule,17,(int64_t)*rule.flags);
												if (rule.invFlags) sqlite3_bind_int64(_sCreateRule,18,(int64_t)*rule.invFlags);

												sqlite3_bind_text(_sCreateRule,19,rule.action,-1,SQLITE_STATIC);
												sqlite3_step(_sCreateRule);
											}
										}
									}
								}
							}

							if (stmt) {
								sqlite3_bind_text(stmt,2,nwids,16,SQLITE_STATIC);
								sqlite3_step(stmt);
								sqlite3_finalize(stmt);
							}
						}
					}
					json_value_free(j);
				}

				sqlite3_reset(_sSetNetworkRevision);
				sqlite3_bind_int64(_sSetNetworkRevision,1,revision += 1);
				sqlite3_bind_text(_sSetNetworkRevision,2,nwids,16,SQLITE_STATIC);
				sqlite3_step(_sSetNetworkRevision);

				return _doCPGet(path_copy,urlArgs,headers,body,responseBody,responseContentType);
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

	if (path[0] == "network") {

		if ((path.size() >= 2)&&(path[1].length() == 16)) {
			uint64_t nwid = Utils::hexStrToU64(path[1].c_str());
			char nwids[24];
			Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);

			sqlite3_reset(_sGetNetworkById);
			sqlite3_bind_text(_sGetNetworkById,1,nwids,16,SQLITE_STATIC);
			if (sqlite3_step(_sGetNetworkById) != SQLITE_ROW)
				return 404;

			if (path.size() >= 3) {

				if ((path.size() == 4)&&(path[2] == "member")&&(path[3].length() == 10)) {
					uint64_t address = Utils::hexStrToU64(path[3].c_str());
					char addrs[24];
					Utils::snprintf(addrs,sizeof(addrs),"%.10llx",address);

					sqlite3_reset(_sGetMember);
					sqlite3_bind_text(_sGetMember,1,nwids,16,SQLITE_STATIC);
					sqlite3_bind_text(_sGetMember,2,addrs,10,SQLITE_STATIC);
					if (sqlite3_step(_sGetMember) != SQLITE_ROW)
						return 404;

					sqlite3_reset(_sDeleteIpAllocations);
					sqlite3_bind_text(_sDeleteIpAllocations,1,nwids,16,SQLITE_STATIC);
					sqlite3_bind_text(_sDeleteIpAllocations,2,addrs,10,SQLITE_STATIC);
					sqlite3_bind_int(_sDeleteIpAllocations,3,(int)ZT_IP_ASSIGNMENT_TYPE_ADDRESS);
					if (sqlite3_step(_sDeleteIpAllocations) == SQLITE_DONE) {
						sqlite3_reset(_sDeleteMember);
						sqlite3_bind_text(_sDeleteMember,1,nwids,16,SQLITE_STATIC);
						sqlite3_bind_text(_sDeleteMember,2,addrs,10,SQLITE_STATIC);
						if (sqlite3_step(_sDeleteMember) != SQLITE_DONE)
							return 500;
					} else return 500;

					return 200;
				}

			} else {

				sqlite3_reset(_sDeleteNetwork);
				sqlite3_bind_text(_sDeleteNetwork,1,nwids,16,SQLITE_STATIC);
				if (sqlite3_step(_sDeleteNetwork) == SQLITE_DONE) {
					sqlite3_reset(_sDeleteAllNetworkMembers);
					sqlite3_bind_text(_sDeleteAllNetworkMembers,1,nwids,16,SQLITE_STATIC);
					sqlite3_step(_sDeleteAllNetworkMembers);
					return 200;
				} else return 500;

			}
		} // else 404

	} // else 404

	return 404;
}

void SqliteNetworkController::threadMain()
	throw()
{
	uint64_t lastBackupTime = OSUtils::now();
	uint64_t lastCleanupTime = OSUtils::now();

	while (_backupThreadRun) {
		if ((OSUtils::now() - lastCleanupTime) >= 5000) {
			const uint64_t now = OSUtils::now();
			lastCleanupTime = now;

			Mutex::Lock _l(_lock);

			// Clean out really old circuit tests to prevent memory build-up
			for(std::map< uint64_t,_CircuitTestEntry >::iterator ct(_circuitTests.begin());ct!=_circuitTests.end();) {
				if (!ct->second.test) {
					_circuitTests.erase(ct++);
				} else if ((now - ct->second.test->timestamp) >= ZT_SQLITENETWORKCONTROLLER_CIRCUIT_TEST_TIMEOUT) {
					_node->circuitTestEnd(ct->second.test);
					::free((void *)ct->second.test);
					_circuitTests.erase(ct++);
				} else ++ct;
			}
		}

		if ((OSUtils::now() - lastBackupTime) >= ZT_NETCONF_BACKUP_PERIOD) {
			lastBackupTime = OSUtils::now();

			char backupPath[4096],backupPath2[4096];
			Utils::snprintf(backupPath,sizeof(backupPath),"%s.backupInProgress",_dbPath.c_str());
			Utils::snprintf(backupPath2,sizeof(backupPath),"%s.backup",_dbPath.c_str());
			OSUtils::rm(backupPath); // delete any unfinished backups

			sqlite3 *bakdb = (sqlite3 *)0;
			sqlite3_backup *bak = (sqlite3_backup *)0;
			if (sqlite3_open_v2(backupPath,&bakdb,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,(const char *)0) != SQLITE_OK) {
				fprintf(stderr,"SqliteNetworkController: CRITICAL: backup failed on sqlite3_open_v2()"ZT_EOL_S);
				continue;
			}
			bak = sqlite3_backup_init(bakdb,"main",_db,"main");
			if (!bak) {
				sqlite3_close(bakdb);
				OSUtils::rm(backupPath); // delete any unfinished backups
				fprintf(stderr,"SqliteNetworkController: CRITICAL: backup failed on sqlite3_backup_init()"ZT_EOL_S);
				continue;
			}

			int rc = SQLITE_OK;
			for(;;) {
				if (!_backupThreadRun) {
					sqlite3_backup_finish(bak);
					sqlite3_close(bakdb);
					OSUtils::rm(backupPath);
					return;
				}
				_lock.lock();
				rc = sqlite3_backup_step(bak,64);
				_lock.unlock();
				if ((rc == SQLITE_OK)||(rc == SQLITE_LOCKED)||(rc == SQLITE_BUSY))
					Thread::sleep(50);
				else break;
			}

			sqlite3_backup_finish(bak);
			sqlite3_close(bakdb);

			OSUtils::rm(backupPath2);
			::rename(backupPath,backupPath2);
		}

		Thread::sleep(250);
	}
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
	char json[65536];

	if ((path.size() > 0)&&(path[0] == "network")) {

		if ((path.size() >= 2)&&(path[1].length() == 16)) {
			uint64_t nwid = Utils::hexStrToU64(path[1].c_str());
			char nwids[24];
			Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);

			if (path.size() >= 3) {
				// /network/<nwid>/...

				if (path[2] == "member") {

					if (path.size() >= 4) {
						// Get specific member info

						uint64_t address = Utils::hexStrToU64(path[3].c_str());
						char addrs[24];
						Utils::snprintf(addrs,sizeof(addrs),"%.10llx",address);

						sqlite3_reset(_sGetMember2);
						sqlite3_bind_text(_sGetMember2,1,nwids,16,SQLITE_STATIC);
						sqlite3_bind_text(_sGetMember2,2,addrs,10,SQLITE_STATIC);
						if (sqlite3_step(_sGetMember2) == SQLITE_ROW) {
							const char *memberIdStr = (const char *)sqlite3_column_text(_sGetMember2,3);

							// If testSingingId is included in the URL or X-ZT1-TestSigningId in the headers
							// and if it contains an identity with a secret portion, the resturned JSON
							// will contain an extra field called _testConf. This will contain several
							// fields that report the result of doNetworkConfigRequest() for this member.
							std::string testFields;
							/*
							{
								Identity testOutputSigningId;
								std::map<std::string,std::string>::const_iterator sid(urlArgs.find("testSigningId"));
								if (sid != urlArgs.end()) {
									testOutputSigningId.fromString(sid->second.c_str());
								} else {
									sid = headers.find("x-zt1-testsigningid");
									if (sid != headers.end())
										testOutputSigningId.fromString(sid->second.c_str());
								}

								if ((testOutputSigningId.hasPrivate())&&(memberIdStr)) {
									Dictionary testNetconf;
									NetworkController::ResultCode rc = this->_doNetworkConfigRequest(
										InetAddress(),
										testOutputSigningId,
										Identity(memberIdStr),
										nwid,
										NetworkConfigRequestMetaData(), // TODO: allow passing of meta-data for testing
										testNetconf);
									char rcs[16];
									Utils::snprintf(rcs,sizeof(rcs),"%d,\n",(int)rc);
									testFields.append("\t\"_test\": {\n");
									testFields.append("\t\t\"resultCode\": "); testFields.append(rcs);
									testFields.append("\t\t\"result\": \""); testFields.append(_jsonEscape(testNetconf.toString().c_str()).c_str()); testFields.append("\",\n");
									testFields.append("\t\t\"resultJson\": {\n");
									for(Dictionary::const_iterator i(testNetconf.begin());i!=testNetconf.end();++i) {
										if (i != testNetconf.begin())
											testFields.append(",\n");
										testFields.append("\t\t\t\"");
										testFields.append(i->first);
										testFields.append("\": \"");
										testFields.append(_jsonEscape(i->second.c_str()));
										testFields.push_back('"');
									}
									testFields.append("\n\t\t}\n");
									testFields.append("\t},\n");
								}
							}
							*/

							Utils::snprintf(json,sizeof(json),
								"{\n%s"
								"\t\"nwid\": \"%s\",\n"
								"\t\"address\": \"%s\",\n"
								"\t\"controllerInstanceId\": \"%s\",\n"
								"\t\"authorized\": %s,\n"
								"\t\"activeBridge\": %s,\n"
								"\t\"memberRevision\": %llu,\n"
								"\t\"clock\": %llu,\n"
								"\t\"identity\": \"%s\",\n"
								"\t\"ipAssignments\": [",
								testFields.c_str(),
								nwids,
								addrs,
								_instanceId.c_str(),
								(sqlite3_column_int(_sGetMember2,0) > 0) ? "true" : "false",
								(sqlite3_column_int(_sGetMember2,1) > 0) ? "true" : "false",
								(unsigned long long)sqlite3_column_int64(_sGetMember2,2),
								(unsigned long long)OSUtils::now(),
								_jsonEscape(memberIdStr).c_str());
							responseBody = json;

							sqlite3_reset(_sGetIpAssignmentsForNode2);
							sqlite3_bind_text(_sGetIpAssignmentsForNode2,1,nwids,16,SQLITE_STATIC);
							sqlite3_bind_text(_sGetIpAssignmentsForNode2,2,addrs,10,SQLITE_STATIC);
							sqlite3_bind_int(_sGetIpAssignmentsForNode2,3,(int)ZT_IP_ASSIGNMENT_TYPE_ADDRESS);
							bool firstIp = true;
							while (sqlite3_step(_sGetIpAssignmentsForNode2) == SQLITE_ROW) {
								int ipversion = sqlite3_column_int(_sGetIpAssignmentsForNode2,2);
								char ipBlob[16];
								memcpy(ipBlob,(const void *)sqlite3_column_blob(_sGetIpAssignmentsForNode2,0),16);
								InetAddress ip(
									(const void *)(ipversion == 6 ? ipBlob : &ipBlob[12]),
									(ipversion == 6 ? 16 : 4),
									(unsigned int)sqlite3_column_int(_sGetIpAssignmentsForNode2,1)
								);
								responseBody.append(firstIp ? "\"" : ",\"");
								responseBody.append(_jsonEscape(ip.toString()));
								responseBody.push_back('"');
								firstIp = false;
							}

							responseBody.append("],\n\t\"recentLog\": [");

							sqlite3_reset(_sGetNodeHistory);
							sqlite3_bind_text(_sGetNodeHistory,1,nwids,16,SQLITE_STATIC);
							sqlite3_bind_text(_sGetNodeHistory,2,addrs,10,SQLITE_STATIC);
							bool firstHistory = true;
							while (sqlite3_step(_sGetNodeHistory) == SQLITE_ROW) {
								responseBody.append(firstHistory ? "{" : ",{");
								responseBody.append("\"ts\":");
								responseBody.append((const char *)sqlite3_column_text(_sGetNodeHistory,2));
								responseBody.append((sqlite3_column_int(_sGetNodeHistory,1) == 0) ? ",\"authorized\":false,\"clientMajorVersion\":" : ",\"authorized\":true,\"clientMajorVersion\":");
								responseBody.append((const char *)sqlite3_column_text(_sGetNodeHistory,3));
								responseBody.append(",\"clientMinorVersion\":");
								responseBody.append((const char *)sqlite3_column_text(_sGetNodeHistory,4));
								responseBody.append(",\"clientRevision\":");
								responseBody.append((const char *)sqlite3_column_text(_sGetNodeHistory,5));
								responseBody.append(",\"fromAddr\":");
								const char *fa = (const char *)sqlite3_column_text(_sGetNodeHistory,7);
								if (fa) {
									responseBody.push_back('"');
									responseBody.append(_jsonEscape(fa));
									responseBody.append("\"}");
								} else responseBody.append("null}");
								firstHistory = false;
							}
							responseBody.append("]\n}\n");

							responseContentType = "application/json";
							return 200;
						} // else 404

					} else {
						// List members

						sqlite3_reset(_sListNetworkMembers);
						sqlite3_bind_text(_sListNetworkMembers,1,nwids,16,SQLITE_STATIC);
						responseBody.push_back('{');
						bool firstMember = true;
						while (sqlite3_step(_sListNetworkMembers) == SQLITE_ROW) {
							responseBody.append(firstMember ? "\"" : ",\"");
							firstMember = false;
							responseBody.append((const char *)sqlite3_column_text(_sListNetworkMembers,0));
							responseBody.append("\":");
							responseBody.append((const char *)sqlite3_column_text(_sListNetworkMembers,1));
						}
						responseBody.push_back('}');
						responseContentType = "application/json";
						return 200;

					}

				} else if ((path[2] == "active")&&(path.size() == 3)) {

					sqlite3_reset(_sGetActiveNodesOnNetwork);
					sqlite3_bind_text(_sGetActiveNodesOnNetwork,1,nwids,16,SQLITE_STATIC);
					sqlite3_bind_text(_sGetActiveNodesOnNetwork,2,nwids,16,SQLITE_STATIC);
					sqlite3_bind_int64(_sGetActiveNodesOnNetwork,3,(int64_t)(OSUtils::now() - ZT_NETCONF_NODE_ACTIVE_THRESHOLD));

					responseBody.push_back('{');
					bool firstMember = true;
					uint64_t lastNodeId = 0;
					while (sqlite3_step(_sGetActiveNodesOnNetwork) == SQLITE_ROW) {
						const char *nodeId = (const char *)sqlite3_column_text(_sGetActiveNodesOnNetwork,0);
						if (nodeId) {
							const uint64_t nodeIdInt = Utils::hexStrToU64(nodeId);
							if (nodeIdInt == lastNodeId) // technically that SQL query could (rarely) generate a duplicate for a given nodeId, in which case we want the first
								continue;
							lastNodeId = nodeIdInt;

							responseBody.append(firstMember ? "\"" : ",\"");
							firstMember = false;
							responseBody.append(nodeId);
							responseBody.append("\":{");
							responseBody.append("\"ts\":");
							responseBody.append((const char *)sqlite3_column_text(_sGetActiveNodesOnNetwork,1));
							responseBody.append((sqlite3_column_int(_sGetActiveNodesOnNetwork,6) > 0) ? ",\"authorized\":true" : ",\"authorized\":false");
							responseBody.append(",\"clientMajorVersion\":");
							responseBody.append((const char *)sqlite3_column_text(_sGetActiveNodesOnNetwork,2));
							responseBody.append(",\"clientMinorVersion\":");
							responseBody.append((const char *)sqlite3_column_text(_sGetActiveNodesOnNetwork,3));
							responseBody.append(",\"clientRevision\":");
							responseBody.append((const char *)sqlite3_column_text(_sGetActiveNodesOnNetwork,4));
							const char *fromAddr = (const char *)sqlite3_column_text(_sGetActiveNodesOnNetwork,5);
							if ((fromAddr)&&(fromAddr[0])) {
								responseBody.append(",\"fromAddr\":\"");
								responseBody.append(_jsonEscape(fromAddr));
								responseBody.append("\"}");
							} else {
								responseBody.append(",\"fromAddr\":null}");
							}
						}
					}
					responseBody.push_back('}');

					responseContentType = "application/json";
					return 200;

				} else if ((path[2] == "test")&&(path.size() >= 4)) {

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

				sqlite3_reset(_sGetNetworkById);
				sqlite3_bind_text(_sGetNetworkById,1,nwids,16,SQLITE_STATIC);
				if (sqlite3_step(_sGetNetworkById) == SQLITE_ROW) {
					Utils::snprintf(json,sizeof(json),
						"{\n"
						"\t\"nwid\": \"%s\",\n"
						"\t\"controllerInstanceId\": \"%s\",\n"
						"\t\"clock\": %llu,\n"
						"\t\"name\": \"%s\",\n"
						"\t\"private\": %s,\n"
						"\t\"enableBroadcast\": %s,\n"
						"\t\"allowPassiveBridging\": %s,\n"
						"\t\"v4AssignMode\": \"%s\",\n"
						"\t\"v6AssignMode\": \"%s\",\n"
						"\t\"multicastLimit\": %d,\n"
						"\t\"creationTime\": %llu,\n"
						"\t\"revision\": %llu,\n"
						"\t\"memberRevisionCounter\": %llu,\n"
						"\t\"authorizedMemberCount\": %llu,\n"
						"\t\"relays\": [",
						nwids,
						_instanceId.c_str(),
						(unsigned long long)OSUtils::now(),
						_jsonEscape((const char *)sqlite3_column_text(_sGetNetworkById,0)).c_str(),
						(sqlite3_column_int(_sGetNetworkById,1) > 0) ? "true" : "false",
						(sqlite3_column_int(_sGetNetworkById,2) > 0) ? "true" : "false",
						(sqlite3_column_int(_sGetNetworkById,3) > 0) ? "true" : "false",
						_jsonEscape((const char *)sqlite3_column_text(_sGetNetworkById,4)).c_str(),
						_jsonEscape((const char *)sqlite3_column_text(_sGetNetworkById,5)).c_str(),
						sqlite3_column_int(_sGetNetworkById,6),
						(unsigned long long)sqlite3_column_int64(_sGetNetworkById,7),
						(unsigned long long)sqlite3_column_int64(_sGetNetworkById,8),
						(unsigned long long)sqlite3_column_int64(_sGetNetworkById,9),
						(unsigned long long)sqlite3_column_int64(_sGetNetworkById,10));
					responseBody = json;

					sqlite3_reset(_sGetRelays);
					sqlite3_bind_text(_sGetRelays,1,nwids,16,SQLITE_STATIC);
					bool firstRelay = true;
					while (sqlite3_step(_sGetRelays) == SQLITE_ROW) {
						responseBody.append(firstRelay ? "\n\t\t" : ",\n\t\t");
						firstRelay = false;
						responseBody.append("{\"address\":\"");
						responseBody.append((const char *)sqlite3_column_text(_sGetRelays,0));
						responseBody.append("\",\"phyAddress\":\"");
						responseBody.append(_jsonEscape((const char *)sqlite3_column_text(_sGetRelays,1)));
						responseBody.append("\"}");
					}

					responseBody.append("],\n\t\"gateways\": [");

					sqlite3_reset(_sGetGateways);
					sqlite3_bind_text(_sGetGateways,1,nwids,16,SQLITE_STATIC);
					bool firstGateway = true;
					while (sqlite3_step(_sGetGateways) == SQLITE_ROW) {
						char tmp[128];
						const unsigned char *ip = (const unsigned char *)sqlite3_column_blob(_sGetGateways,0);
						switch(sqlite3_column_int(_sGetGateways,1)) { // ipVersion
							case 4:
								Utils::snprintf(tmp,sizeof(tmp),"%s%d.%d.%d.%d/%d\"",
									(firstGateway) ? "\"" : ",\"",
									(int)ip[12],
									(int)ip[13],
									(int)ip[14],
									(int)ip[15],
									(int)sqlite3_column_int(_sGetGateways,2)); // metric
								break;
							case 6:
								Utils::snprintf(tmp,sizeof(tmp),"%s%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x/%d\"",
									(firstGateway) ? "\"" : ",\"",
									(int)ip[0],
									(int)ip[1],
									(int)ip[2],
									(int)ip[3],
									(int)ip[4],
									(int)ip[5],
									(int)ip[6],
									(int)ip[7],
									(int)ip[8],
									(int)ip[9],
									(int)ip[10],
									(int)ip[11],
									(int)ip[12],
									(int)ip[13],
									(int)ip[14],
									(int)ip[15],
									(int)sqlite3_column_int(_sGetGateways,2)); // metric
								break;
						}
						responseBody.append(tmp);
						firstGateway = false;
					}

					responseBody.append("],\n\t\"ipLocalRoutes\": [");

					sqlite3_reset(_sGetLocalRoutes);
					sqlite3_bind_text(_sGetLocalRoutes,1,nwids,16,SQLITE_STATIC);
					sqlite3_bind_int(_sGetLocalRoutes,2,(int)ZT_IP_ASSIGNMENT_TYPE_NETWORK);
					bool firstLocalRoute = true;
					while (sqlite3_step(_sGetLocalRoutes) == SQLITE_ROW) {
						char tmp[128];
						const unsigned char *ip = (const unsigned char *)sqlite3_column_blob(_sGetLocalRoutes,0);
						switch (sqlite3_column_int(_sGetLocalRoutes,2)) {
							case 4:
								Utils::snprintf(tmp,sizeof(tmp),"%s%d.%d.%d.%d/%d\"",
									(firstLocalRoute) ? "\"" : ",\"",
									(int)ip[12],
									(int)ip[13],
									(int)ip[14],
									(int)ip[15],
									(int)sqlite3_column_int(_sGetLocalRoutes,1)); // netmask bits
								break;
							case 6:
								Utils::snprintf(tmp,sizeof(tmp),"%s%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x/%d\"",
									(firstLocalRoute) ? "\"" : ",\"",
									(int)ip[0],
									(int)ip[1],
									(int)ip[2],
									(int)ip[3],
									(int)ip[4],
									(int)ip[5],
									(int)ip[6],
									(int)ip[7],
									(int)ip[8],
									(int)ip[9],
									(int)ip[10],
									(int)ip[11],
									(int)ip[12],
									(int)ip[13],
									(int)ip[14],
									(int)ip[15],
									(int)sqlite3_column_int(_sGetLocalRoutes,1)); // netmask bits
								break;
						}
						responseBody.append(tmp);
						firstLocalRoute = false;
					}

					responseBody.append("],\n\t\"ipAssignmentPools\": [");

					sqlite3_reset(_sGetIpAssignmentPools2);
					sqlite3_bind_text(_sGetIpAssignmentPools2,1,nwids,16,SQLITE_STATIC);
					bool firstIpAssignmentPool = true;
					while (sqlite3_step(_sGetIpAssignmentPools2) == SQLITE_ROW) {
						const char *ipRangeStartB = reinterpret_cast<const char *>(sqlite3_column_blob(_sGetIpAssignmentPools2,0));
						const char *ipRangeEndB = reinterpret_cast<const char *>(sqlite3_column_blob(_sGetIpAssignmentPools2,1));
						if ((ipRangeStartB)&&(ipRangeEndB)) {
							InetAddress ipps,ippe;
							int ipVersion = sqlite3_column_int(_sGetIpAssignmentPools2,2);
							if (ipVersion == 4) {
								ipps.set((const void *)(ipRangeStartB + 12),4,0);
								ippe.set((const void *)(ipRangeEndB + 12),4,0);
							} else if (ipVersion == 6) {
								ipps.set((const void *)ipRangeStartB,16,0);
								ippe.set((const void *)ipRangeEndB,16,0);
							}
							if (ipps) {
								responseBody.append(firstIpAssignmentPool ? "\n\t\t" : ",\n\t\t");
								firstIpAssignmentPool = false;
								Utils::snprintf(json,sizeof(json),"{\"ipRangeStart\":\"%s\",\"ipRangeEnd\":\"%s\"}",
									_jsonEscape(ipps.toIpString()).c_str(),
									_jsonEscape(ippe.toIpString()).c_str());
								responseBody.append(json);
							}
						}
					}

					responseBody.append("],\n\t\"rules\": [");

					sqlite3_reset(_sListRules);
					sqlite3_bind_text(_sListRules,1,nwids,16,SQLITE_STATIC);
					bool firstRule = true;
					while (sqlite3_step(_sListRules) == SQLITE_ROW) {
						responseBody.append(firstRule ? "\n\t{\n" : ",{\n");
						firstRule = false;
						Utils::snprintf(json,sizeof(json),"\t\t\"ruleNo\": %lld,\n",sqlite3_column_int64(_sListRules,0));
						responseBody.append(json);
						if (sqlite3_column_type(_sListRules,1) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"nodeId\": \"%s\",\n",(const char *)sqlite3_column_text(_sListRules,1));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,2) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"sourcePort\": \"%s\",\n",(const char *)sqlite3_column_text(_sListRules,2));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,3) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"destPort\": \"%s\",\n",(const char *)sqlite3_column_text(_sListRules,3));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,4) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"vlanId\": %d,\n",sqlite3_column_int(_sListRules,4));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,5) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"vlanPcp\": %d,\n",sqlite3_column_int(_sListRules,5));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,6) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"etherType\": %d,\n",sqlite3_column_int(_sListRules,6));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,7) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"macSource\": \"%s\",\n",MAC((const char *)sqlite3_column_text(_sListRules,7)).toString().c_str());
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,8) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"macDest\": \"%s\",\n",MAC((const char *)sqlite3_column_text(_sListRules,8)).toString().c_str());
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,9) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"ipSource\": \"%s\",\n",_jsonEscape((const char *)sqlite3_column_text(_sListRules,9)).c_str());
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,10) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"ipDest\": \"%s\",\n",_jsonEscape((const char *)sqlite3_column_text(_sListRules,10)).c_str());
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,11) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"ipTos\": %d,\n",sqlite3_column_int(_sListRules,11));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,12) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"ipProtocol\": %d,\n",sqlite3_column_int(_sListRules,12));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,13) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"ipSourcePort\": %d,\n",sqlite3_column_int(_sListRules,13));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,14) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"ipDestPort\": %d,\n",sqlite3_column_int(_sListRules,14));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,15) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"flags\": %lu,\n",(unsigned long)sqlite3_column_int64(_sListRules,15));
							responseBody.append(json);
						}
						if (sqlite3_column_type(_sListRules,16) != SQLITE_NULL) {
							Utils::snprintf(json,sizeof(json),"\t\t\"invFlags\": %lu,\n",(unsigned long)sqlite3_column_int64(_sListRules,16));
							responseBody.append(json);
						}
						responseBody.append("\t\t\"action\": \"");
						responseBody.append(_jsonEscape( (sqlite3_column_type(_sListRules,17) == SQLITE_NULL) ? "drop" : (const char *)sqlite3_column_text(_sListRules,17) ));
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

	} else {
		// GET /controller returns status and API version if controller is supported
		Utils::snprintf(json,sizeof(json),"{\n\t\"controller\": true,\n\t\"apiVersion\": %d,\n\t\"clock\": %llu,\n\t\"instanceId\": \"%s\"\n}\n",ZT_NETCONF_CONTROLLER_API_VERSION,(unsigned long long)OSUtils::now(),_instanceId.c_str());
		responseBody = json;
		responseContentType = "application/json";
		return 200;
	}

	return 404;
}

NetworkController::ResultCode SqliteNetworkController::_doNetworkConfigRequest(const InetAddress &fromAddr,const Identity &signingId,const Identity &identity,uint64_t nwid,const NetworkConfigRequestMetaData &metaData,Buffer<8194> &netconf)
{
	// Assumes _lock is locked

	if (((!signingId)||(!signingId.hasPrivate()))||(signingId.address().toInt() != (nwid >> 24))) {
		return NetworkController::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
	}

	// Note: we can't reuse prepared statements that return const char * pointers without
	// making our own copy in e.g. a std::string first.

	const bool clientIs104 = (Utils::compareVersion(metaData.majorVersion,metaData.minorVersion,metaData.revision,1,0,4) >= 0);
	const uint64_t now = OSUtils::now();

	// Check rate limit circuit breaker to prevent flooding
	{
		uint64_t &lrt = _lastRequestTime[std::pair<uint64_t,uint64_t>(identity.address().toInt(),nwid)];
		if ((now - lrt) <= ZT_NETCONF_MIN_REQUEST_PERIOD)
			return NetworkController::NETCONF_QUERY_IGNORE;
		lrt = now;
	}

	NetworkRecord network;
	memset(&network,0,sizeof(network));
	Utils::snprintf(network.id,sizeof(network.id),"%.16llx",(unsigned long long)nwid);

	MemberRecord member;
	memset(&member,0,sizeof(member));
	Utils::snprintf(member.nodeId,sizeof(member.nodeId),"%.10llx",(unsigned long long)identity.address().toInt());

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
		network.v4AssignMode = (const char *)sqlite3_column_text(_sGetNetworkById,4);
		network.v6AssignMode = (const char *)sqlite3_column_text(_sGetNetworkById,5);
		network.multicastLimit = sqlite3_column_int(_sGetNetworkById,6);
		network.creationTime = (uint64_t)sqlite3_column_int64(_sGetNetworkById,7);
		network.revision = (uint64_t)sqlite3_column_int64(_sGetNetworkById,8);
		network.memberRevisionCounter = (uint64_t)sqlite3_column_int64(_sGetNetworkById,9);
	} else {
		return NetworkController::NETCONF_QUERY_OBJECT_NOT_FOUND;
	}

	// Fetch Member record

	bool foundMember = false;
	sqlite3_reset(_sGetMember);
	sqlite3_bind_text(_sGetMember,1,network.id,16,SQLITE_STATIC);
	sqlite3_bind_text(_sGetMember,2,member.nodeId,10,SQLITE_STATIC);
	if (sqlite3_step(_sGetMember) == SQLITE_ROW) {
		foundMember = true;
		member.rowid = (int64_t)sqlite3_column_int64(_sGetMember,0);
		member.authorized = (sqlite3_column_int(_sGetMember,1) > 0);
		member.activeBridge = (sqlite3_column_int(_sGetMember,2) > 0);
	}

	// Create Member record for unknown nodes, auto-authorizing if network is public

	if (!foundMember) {
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
		member.rowid = (int64_t)sqlite3_last_insert_rowid(_db);

		sqlite3_reset(_sIncrementMemberRevisionCounter);
		sqlite3_bind_text(_sIncrementMemberRevisionCounter,1,network.id,16,SQLITE_STATIC);
		sqlite3_step(_sIncrementMemberRevisionCounter);
	}

	// Update NodeHistory with new log entry and delete expired entries

	{
		int64_t nextVC = 1;
		sqlite3_reset(_sGetMaxNodeHistoryNetworkVisitCounter);
		sqlite3_bind_text(_sGetMaxNodeHistoryNetworkVisitCounter,1,network.id,16,SQLITE_STATIC);
		sqlite3_bind_text(_sGetMaxNodeHistoryNetworkVisitCounter,2,member.nodeId,10,SQLITE_STATIC);
		if (sqlite3_step(_sGetMaxNodeHistoryNetworkVisitCounter) == SQLITE_ROW) {
			nextVC = (int64_t)sqlite3_column_int64(_sGetMaxNodeHistoryNetworkVisitCounter,0) + 1;
		}

		std::string fastr;
		if (fromAddr)
			fastr = fromAddr.toString();

		sqlite3_reset(_sAddNodeHistoryEntry);
		sqlite3_bind_text(_sAddNodeHistoryEntry,1,member.nodeId,10,SQLITE_STATIC);
		sqlite3_bind_text(_sAddNodeHistoryEntry,2,network.id,16,SQLITE_STATIC);
		sqlite3_bind_int64(_sAddNodeHistoryEntry,3,nextVC);
		sqlite3_bind_int(_sAddNodeHistoryEntry,4,(member.authorized ? 1 : 0));
		sqlite3_bind_int64(_sAddNodeHistoryEntry,5,(long long)now);
		sqlite3_bind_int(_sAddNodeHistoryEntry,6,(int)metaData.majorVersion);
		sqlite3_bind_int(_sAddNodeHistoryEntry,7,(int)metaData.minorVersion);
		sqlite3_bind_int(_sAddNodeHistoryEntry,8,(int)metaData.revision);
		sqlite3_bind_text(_sAddNodeHistoryEntry,9,"",-1,SQLITE_STATIC);
		if (fastr.length() > 0)
			sqlite3_bind_text(_sAddNodeHistoryEntry,10,fastr.c_str(),-1,SQLITE_STATIC);
		else sqlite3_bind_null(_sAddNodeHistoryEntry,10);
		sqlite3_step(_sAddNodeHistoryEntry);

		nextVC -= ZT_NETCONF_NODE_HISTORY_LENGTH;
		if (nextVC >= 0) {
			sqlite3_reset(_sDeleteOldNodeHistoryEntries);
			sqlite3_bind_text(_sDeleteOldNodeHistoryEntries,1,network.id,16,SQLITE_STATIC);
			sqlite3_bind_text(_sDeleteOldNodeHistoryEntries,2,member.nodeId,10,SQLITE_STATIC);
			sqlite3_bind_int64(_sDeleteOldNodeHistoryEntries,3,nextVC);
			sqlite3_step(_sDeleteOldNodeHistoryEntries);
		}
	}

	// Check member authorization

	if (!member.authorized)
		return NetworkController::NETCONF_QUERY_ACCESS_DENIED;

	// Create network configuration -- we create both legacy and new types and send both for backward compatibility

	Dictionary legacy;
	NetworkConfig nc;

	nc.networkId = Utils::hexStrToU64(network.id);
	nc.type = network.isPrivate ? ZT_NETWORK_TYPE_PRIVATE : ZT_NETWORK_TYPE_PUBLIC;
	nc.timestamp = now;
	nc.revision = network.revision;
	nc.issuedTo = member.nodeId;
	if (network.enableBroadcast) nc.flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_BROADCAST;
	if (network.allowPassiveBridging) nc.flags |= ZT_NETWORKCONFIG_FLAG_ALLOW_PASSIVE_BRIDGING;
	memcpy(nc.name,network.name,std::min((unsigned int)ZT_MAX_NETWORK_SHORT_NAME_LENGTH,(unsigned int)strlen(network.name)));

	char tss[24],rs[24];
	Utils::snprintf(tss,sizeof(tss),"%.16llx",(unsigned long long)now);
	Utils::snprintf(rs,sizeof(rs),"%.16llx",(unsigned long long)network.revision);
	legacy[ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP] = tss;
	legacy[ZT_NETWORKCONFIG_DICT_KEY_REVISION] = rs;
	legacy[ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID] = network.id;
	legacy[ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO] = member.nodeId;
	legacy[ZT_NETWORKCONFIG_DICT_KEY_PRIVATE] = network.isPrivate ? "1" : "0";
	legacy[ZT_NETWORKCONFIG_DICT_KEY_NAME] = (network.name) ? network.name : "";
	legacy[ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST] = network.enableBroadcast ? "1" : "0";
	legacy[ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING] = network.allowPassiveBridging ? "1" : "0";

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

		for(long i=0,k=0;((i<(long)allowedEtherTypes.size())&&(k<ZT_MAX_NETWORK_RULES));++i) {
			if (allowedEtherTypes[i] > 0) {
				nc.rules[k].t = ZT_NETWORK_RULE_MATCH_ETHERTYPE;
				nc.rules[k].v.etherType = (uint16_t)allowedEtherTypes[i];
				++k;
			}
			nc.rules[k++].t = ZT_NETWORK_RULE_ACTION_ACCEPT;
		}

		std::string allowedEtherTypesCsv;
		for(std::vector<int>::const_iterator i(allowedEtherTypes.begin());i!=allowedEtherTypes.end();++i) {
			if (allowedEtherTypesCsv.length())
				allowedEtherTypesCsv.push_back(',');
			char tmp[16];
			Utils::snprintf(tmp,sizeof(tmp),"%.4x",(unsigned int)*i);
			allowedEtherTypesCsv.append(tmp);
		}
		legacy[ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES] = allowedEtherTypesCsv;
	}

	nc.multicastLimit = network.multicastLimit;
	if (network.multicastLimit > 0) {
		char ml[16];
		Utils::snprintf(ml,sizeof(ml),"%lx",(unsigned long)network.multicastLimit);
		legacy[ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT] = ml;
	}

	bool amActiveBridge = false;
	{
		std::string activeBridges;
		sqlite3_reset(_sGetActiveBridges);
		sqlite3_bind_text(_sGetActiveBridges,1,network.id,16,SQLITE_STATIC);
		while (sqlite3_step(_sGetActiveBridges) == SQLITE_ROW) {
			const char *ab = (const char *)sqlite3_column_text(_sGetActiveBridges,0);
			if ((ab)&&(strlen(ab) == 10)) {
				const uint64_t ab2 = Utils::hexStrToU64(ab);
				if ((ab2)&&(nc.specialistCount < ZT_MAX_NETWORK_SPECIALISTS)) {
					nc.specialists[nc.specialistCount++] = ab2 | ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE;
				}

				if (activeBridges.length())
					activeBridges.push_back(',');
				activeBridges.append(ab);

				if (!strcmp(member.nodeId,ab))
					amActiveBridge = true;
			}
			if (activeBridges.length() > 1024) // sanity check -- you can't have too many active bridges at the moment
				break;
		}
		if (activeBridges.length())
			legacy[ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES] = activeBridges;
	}

	// Do not send relays to 1.1.0 since it had a serious bug in using them
	// 1.1.0 will still work, it'll just fall back to roots instead of using network preferred relays
	if (!((metaData.majorVersion == 1)&&(metaData.minorVersion == 1)&&(metaData.revision == 0))) {
		std::string relays;
		sqlite3_reset(_sGetRelays);
		sqlite3_bind_text(_sGetRelays,1,network.id,16,SQLITE_STATIC);
		while (sqlite3_step(_sGetRelays) == SQLITE_ROW) {
			const char *n = (const char *)sqlite3_column_text(_sGetRelays,0);
			const char *a = (const char *)sqlite3_column_text(_sGetRelays,1);
			if ((n)&&(a)) {
				Address node(n);
				InetAddress addr(a);
				if (node) {
					bool sf = false;
					for(unsigned int k=0;k<nc.specialistCount;++k) {
						if ((nc.specialists[k] & 0xffffffffffULL) == node.toInt()) {
							nc.specialists[k] |= ZT_NETWORKCONFIG_SPECIALIST_TYPE_NETWORK_PREFERRED_RELAY;
							sf = true;
							break;
						}
					}
					if ((!sf)&&(nc.specialistCount < ZT_MAX_NETWORK_SPECIALISTS)) {
						nc.specialists[nc.specialistCount++] = node.toInt() | ZT_NETWORKCONFIG_SPECIALIST_TYPE_NETWORK_PREFERRED_RELAY;
					}

					if (relays.length())
						relays.push_back(',');
					relays.append(node.toString());
					if (addr) {
						relays.push_back(';');
						relays.append(addr.toString());
					}
				}
			}
		}
		if (relays.length())
			legacy[ZT_NETWORKCONFIG_DICT_KEY_RELAYS] = relays;
	}

	// TODO: this should be routes, going to redo DB
	/*
	{
		char tmp[128];
		std::string gateways;
		sqlite3_reset(_sGetGateways);
		sqlite3_bind_text(_sGetGateways,1,network.id,16,SQLITE_STATIC);
		while (sqlite3_step(_sGetGateways) == SQLITE_ROW) {
			const unsigned char *ip = (const unsigned char *)sqlite3_column_blob(_sGetGateways,0);
			switch(sqlite3_column_int(_sGetGateways,1)) { // ipVersion
				case 4:
					Utils::snprintf(tmp,sizeof(tmp),"%s%d.%d.%d.%d/%d",
						(gateways.length() > 0) ? "," : "",
						(int)ip[12],
						(int)ip[13],
						(int)ip[14],
						(int)ip[15],
						(int)sqlite3_column_int(_sGetGateways,2)); // metric
					gateways.append(tmp);
					break;
				case 6:
					Utils::snprintf(tmp,sizeof(tmp),"%s%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x/%d",
						(gateways.length() > 0) ? "," : "",
						(int)ip[0],
						(int)ip[1],
						(int)ip[2],
						(int)ip[3],
						(int)ip[4],
						(int)ip[5],
						(int)ip[6],
						(int)ip[7],
						(int)ip[8],
						(int)ip[9],
						(int)ip[10],
						(int)ip[11],
						(int)ip[12],
						(int)ip[13],
						(int)ip[14],
						(int)ip[15],
						(int)sqlite3_column_int(_sGetGateways,2)); // metric
					gateways.append(tmp);
					break;
			}
		}
		if (gateways.length())
			netconf[ZT_NETWORKCONFIG_DICT_KEY_GATEWAYS] = gateways;
	}
	*/

	if ((network.v4AssignMode)&&(!strcmp(network.v4AssignMode,"zt"))) {
		std::string v4s;
		std::vector< std::pair<uint32_t,int> > routedNetworks;
		bool haveStaticIpAssignment = false;

		sqlite3_reset(_sGetIpAssignmentsForNode);
		sqlite3_bind_text(_sGetIpAssignmentsForNode,1,network.id,16,SQLITE_STATIC);
		sqlite3_bind_text(_sGetIpAssignmentsForNode,2,member.nodeId,10,SQLITE_STATIC);
		sqlite3_bind_int(_sGetIpAssignmentsForNode,3,4); // 4 == IPv4
		while (sqlite3_step(_sGetIpAssignmentsForNode) == SQLITE_ROW) {
			const unsigned char *const ip = (const unsigned char *)sqlite3_column_blob(_sGetIpAssignmentsForNode,1);
			if ((!ip)||(sqlite3_column_bytes(_sGetIpAssignmentsForNode,1) != 16))
				continue;
			int ipNetmaskBits = sqlite3_column_int(_sGetIpAssignmentsForNode,2);
			if ((ipNetmaskBits <= 0)||(ipNetmaskBits > 32))
				continue;

			char ips[32];
			Utils::snprintf(ips,sizeof(ips),"%d.%d.%d.%d/%d",(int)ip[12],(int)ip[13],(int)ip[14],(int)ip[15],ipNetmaskBits);

			const IpAssignmentType ipt = (IpAssignmentType)sqlite3_column_int(_sGetIpAssignmentsForNode,0);
			switch(ipt) {
				case ZT_IP_ASSIGNMENT_TYPE_ADDRESS:
					haveStaticIpAssignment = true;
					break;
				case ZT_IP_ASSIGNMENT_TYPE_NETWORK:
					routedNetworks.push_back(std::pair<uint32_t,int>(Utils::ntoh(*(reinterpret_cast<const uint32_t *>(ip + 12))),ipNetmaskBits));
					break;
				default:
					continue;
			}

			if ((ipt == ZT_IP_ASSIGNMENT_TYPE_ADDRESS)&&(nc.staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)) {
				InetAddress tmp2(ips);
				if (tmp2)
					nc.staticIps[nc.staticIpCount++] = tmp2;
			}
			// TODO: add routed networks to nc

			// 1.0.4 or newer clients support network routes in addition to IPs.
			// Older clients only support IP address / netmask entries.
			if ((clientIs104)||(ipt == ZT_IP_ASSIGNMENT_TYPE_ADDRESS)) {
				if (v4s.length())
					v4s.push_back(',');
				v4s.append(ips);
			}
		}

		if ((!haveStaticIpAssignment)&&(!amActiveBridge)) {
			// Attempt to auto-assign an IPv4 address from an available routed pool
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

				for(uint32_t k=ipRangeStart,l=0;(k<=ipRangeEnd)&&(l < 1000000);++k,++l) {
					uint32_t ip = (ipRangeLen > 0) ? (ipRangeStart + (ipTrialCounter % ipRangeLen)) : ipRangeStart;
					++ipTrialCounter;
					if ((ip & 0x000000ff) == 0x000000ff)
						continue; // don't allow addresses that end in .255

					for(std::vector< std::pair<uint32_t,int> >::const_iterator r(routedNetworks.begin());r!=routedNetworks.end();++r) {
						if ((ip & (0xffffffff << (32 - r->second))) == r->first) {
							// IP is included in a routed network, so check if it's allocated

							uint32_t ipBlob[4];
							ipBlob[0] = 0; ipBlob[1] = 0; ipBlob[2] = 0; ipBlob[3] = Utils::hton(ip);

							sqlite3_reset(_sCheckIfIpIsAllocated);
							sqlite3_bind_text(_sCheckIfIpIsAllocated,1,network.id,16,SQLITE_STATIC);
							sqlite3_bind_blob(_sCheckIfIpIsAllocated,2,(const void *)ipBlob,16,SQLITE_STATIC);
							sqlite3_bind_int(_sCheckIfIpIsAllocated,3,4); // 4 == IPv4
							sqlite3_bind_int(_sCheckIfIpIsAllocated,4,(int)ZT_IP_ASSIGNMENT_TYPE_ADDRESS);
							if (sqlite3_step(_sCheckIfIpIsAllocated) != SQLITE_ROW) {
								// No rows returned, so the IP is available
								sqlite3_reset(_sAllocateIp);
								sqlite3_bind_text(_sAllocateIp,1,network.id,16,SQLITE_STATIC);
								sqlite3_bind_text(_sAllocateIp,2,member.nodeId,10,SQLITE_STATIC);
								sqlite3_bind_int(_sAllocateIp,3,(int)ZT_IP_ASSIGNMENT_TYPE_ADDRESS);
								sqlite3_bind_blob(_sAllocateIp,4,(const void *)ipBlob,16,SQLITE_STATIC);
								sqlite3_bind_int(_sAllocateIp,5,r->second); // IP netmask bits from matching route
								sqlite3_bind_int(_sAllocateIp,6,4); // 4 == IPv4
								if (sqlite3_step(_sAllocateIp) == SQLITE_DONE) {
									char ips[32];
									Utils::snprintf(ips,sizeof(ips),"%d.%d.%d.%d/%d",(int)((ip >> 24) & 0xff),(int)((ip >> 16) & 0xff),(int)((ip >> 8) & 0xff),(int)(ip & 0xff),r->second);

									if (nc.staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES) {
										InetAddress tmp2(ips);
										if (tmp2)
											nc.staticIps[nc.staticIpCount++] = tmp2;
									}

									if (v4s.length())
										v4s.push_back(',');
									v4s.append(ips);

									haveStaticIpAssignment = true; // break outer loop
								}
							}

							break; // stop checking routed networks
						}
					}

					if (haveStaticIpAssignment)
						break;
				}
			}
		}

		if (v4s.length())
			legacy[ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC] = v4s;
	}

	if ((network.v6AssignMode)&&(!strcmp(network.v6AssignMode,"rfc4193"))) {
		InetAddress rfc4193Addr(InetAddress::makeIpv6rfc4193(nwid,identity.address().toInt()));
		legacy[ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC] = rfc4193Addr.toString();
		if (nc.staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)
			nc.staticIps[nc.staticIpCount++] = rfc4193Addr;
	}

	if (network.isPrivate) {
		CertificateOfMembership com(now,ZT_NETWORK_COM_DEFAULT_REVISION_MAX_DELTA,nwid,identity.address());
		if (com.sign(signingId)) {
			legacy[ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP] = com.toString();
			nc.com = com;
		} else {
			return NETCONF_QUERY_INTERNAL_SERVER_ERROR;
		}
	}

	if (!legacy.sign(signingId,now)) {
		return NETCONF_QUERY_INTERNAL_SERVER_ERROR;
	}

	// First append the legacy dictionary and a terminating NULL, then serialize the new-format one.
	// Newer clients will use the new-format dictionary and older ones will use the old one.
	std::string legacyStr(legacy.toString());
	netconf.append((const void *)legacyStr.data(),(unsigned int)legacyStr.length());
	netconf.append((uint8_t)0);
	nc.serialize(netconf);

	return NetworkController::NETCONF_QUERY_OK;
}

void SqliteNetworkController::_circuitTestCallback(ZT_Node *node,ZT_CircuitTest *test,const ZT_CircuitTestReport *report)
{
	char tmp[65535];
	SqliteNetworkController *const self = reinterpret_cast<SqliteNetworkController *>(test->ptr);

	if (!test)
		return;
	if (!report)
		return;

	Mutex::Lock _l(self->_lock);
	std::map< uint64_t,_CircuitTestEntry >::iterator cte(self->_circuitTests.find(test->testId));

	if (cte == self->_circuitTests.end()) { // sanity check: a circuit test we didn't launch?
		self->_node->circuitTestEnd(test);
		::free((void *)test);
		return;
	}

	Utils::snprintf(tmp,sizeof(tmp),
		"%s{\n"
		"\t\"timestamp\": %llu,"ZT_EOL_S
		"\t\"testId\": \"%.16llx\","ZT_EOL_S
		"\t\"upstream\": \"%.10llx\","ZT_EOL_S
		"\t\"current\": \"%.10llx\","ZT_EOL_S
		"\t\"receivedTimestamp\": %llu,"ZT_EOL_S
		"\t\"remoteTimestamp\": %llu,"ZT_EOL_S
		"\t\"sourcePacketId\": \"%.16llx\","ZT_EOL_S
		"\t\"flags\": %llu,"ZT_EOL_S
		"\t\"sourcePacketHopCount\": %u,"ZT_EOL_S
		"\t\"errorCode\": %u,"ZT_EOL_S
		"\t\"vendor\": %d,"ZT_EOL_S
		"\t\"protocolVersion\": %u,"ZT_EOL_S
		"\t\"majorVersion\": %u,"ZT_EOL_S
		"\t\"minorVersion\": %u,"ZT_EOL_S
		"\t\"revision\": %u,"ZT_EOL_S
		"\t\"platform\": %d,"ZT_EOL_S
		"\t\"architecture\": %d,"ZT_EOL_S
		"\t\"receivedOnLocalAddress\": \"%s\","ZT_EOL_S
		"\t\"receivedFromRemoteAddress\": \"%s\""ZT_EOL_S
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
