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

#include "../osdep/OSUtils.hpp"

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

// Delay between backups in milliseconds
//#define ZT_NETCONF_BACKUP_PERIOD 300000

// Nodes are considered active if they've queried in less than this long
#define ZT_NETCONF_NODE_ACTIVE_THRESHOLD ((ZT_NETWORK_AUTOCONF_DELAY * 2) + 5000)

namespace ZeroTier {

SqliteNetworkController::SqliteNetworkController(Node *node,const char *dbPath,const char *circuitTestPath) :
	_node(node),
	_db(dbPath),
	_dbCommitThreadRun(true)
{
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

	_dbCommitThread = Thread::start(this);
}

SqliteNetworkController::~SqliteNetworkController()
{
	_lock.lock();
	_dbCommitThreadRun = false;
	_lock.unlock();
	Thread::join(_dbCommitThread);
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

	_backupNeeded = true;

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
										sqlite3_bind_int(_sDeleteIpAllocations,3,(int)0 /*ZT_IP_ASSIGNMENT_TYPE_ADDRESS*/);
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
													sqlite3_bind_int(_sAllocateIp,3,(int)0 /*ZT_IP_ASSIGNMENT_TYPE_ADDRESS*/);
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
								if ((j->u.object.values[k].value->type == json_string)&&(!strcmp(j->u.object.values[k].value->u.string.ptr,"zt"))) {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET \"flags\" = (\"flags\" | ?) WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_int(stmt,1,(int)ZT_DB_NETWORK_FLAG_ZT_MANAGED_V4_AUTO_ASSIGN);
								} else {
									if (sqlite3_prepare_v2(_db,"UPDATE Network SET \"flags\" = (\"flags\" & ?) WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
										sqlite3_bind_int(stmt,1,(int)(ZT_DB_NETWORK_FLAG_ZT_MANAGED_V4_AUTO_ASSIGN ^ 0xfffffff));
								}
							} else if (!strcmp(j->u.object.values[k].name,"v6AssignMode")) {
								int fl = 0;
								if (j->u.object.values[k].value->type == json_string) {
									char *saveptr = (char *)0;
									for(char *f=Utils::stok(j->u.object.values[k].value->u.string.ptr,",",&saveptr);(f);f=Utils::stok((char *)0,",",&saveptr)) {
										if (!strcmp(f,"rfc4193"))
											fl |= ZT_DB_NETWORK_FLAG_ZT_MANAGED_V6_RFC4193;
										else if (!strcmp(f,"6plane"))
											fl |= ZT_DB_NETWORK_FLAG_ZT_MANAGED_V6_6PLANE;
										else if (!strcmp(f,"zt"))
											fl |= ZT_DB_NETWORK_FLAG_ZT_MANAGED_V6_AUTO_ASSIGN;
									}
								}
								if (sqlite3_prepare_v2(_db,"UPDATE Network SET \"flags\" = ((\"flags\" & " ZT_DB_NETWORK_FLAG_ZT_MANAGED_V6_MASK_S ") | ?) WHERE id = ?",-1,&stmt,(const char **)0) == SQLITE_OK)
									sqlite3_bind_int(stmt,1,fl);
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
							} else if (!strcmp(j->u.object.values[k].name,"routes")) {
								sqlite3_reset(_sDeleteRoutes);
								sqlite3_bind_text(_sDeleteRoutes,1,nwids,16,SQLITE_STATIC);
								sqlite3_step(_sDeleteRoutes);
								if (j->u.object.values[k].value->type == json_array) {
									for(unsigned int kk=0;kk<j->u.object.values[k].value->u.array.length;++kk) {
										json_value *r = j->u.object.values[k].value->u.array.values[kk];
										if ((r)&&(r->type == json_object)) {
											InetAddress r_target,r_via;
											int r_flags = 0;
											int r_metric = 0;
											for(unsigned int rk=0;rk<r->u.object.length;++rk) {
												if ((!strcmp(r->u.object.values[rk].name,"target"))&&(r->u.object.values[rk].value->type == json_string))
													r_target = InetAddress(std::string(r->u.object.values[rk].value->u.string.ptr));
												else if ((!strcmp(r->u.object.values[rk].name,"via"))&&(r->u.object.values[rk].value->type == json_string))
													r_via = InetAddress(std::string(r->u.object.values[rk].value->u.string.ptr),0);
												else if ((!strcmp(r->u.object.values[rk].name,"flags"))&&(r->u.object.values[rk].value->type == json_integer))
													r_flags = (int)(r->u.object.values[rk].value->u.integer & 0xffff);
												else if ((!strcmp(r->u.object.values[rk].name,"metric"))&&(r->u.object.values[rk].value->type == json_integer))
													r_metric = (int)(r->u.object.values[rk].value->u.integer & 0xffff);
											}
											if ((r_target)&&((!r_via)||(r_via.ss_family == r_target.ss_family))) {
												int r_ipVersion = 0;
												char r_targetBlob[16];
												char r_viaBlob[16];
												_ipToBlob(r_target,r_targetBlob,r_ipVersion);
												if (r_ipVersion) {
													int r_targetNetmaskBits = r_target.netmaskBits();
													if ((r_ipVersion == 4)&&(r_targetNetmaskBits > 32)) r_targetNetmaskBits = 32;
													else if ((r_ipVersion == 6)&&(r_targetNetmaskBits > 128)) r_targetNetmaskBits = 128;
													sqlite3_reset(_sCreateRoute);
													sqlite3_bind_text(_sCreateRoute,1,nwids,16,SQLITE_STATIC);
													sqlite3_bind_blob(_sCreateRoute,2,(const void *)r_targetBlob,16,SQLITE_STATIC);
													if (r_via) {
														_ipToBlob(r_via,r_viaBlob,r_ipVersion);
														sqlite3_bind_blob(_sCreateRoute,3,(const void *)r_viaBlob,16,SQLITE_STATIC);
													} else {
														sqlite3_bind_null(_sCreateRoute,3);
													}
													sqlite3_bind_int(_sCreateRoute,4,r_targetNetmaskBits);
													sqlite3_bind_int(_sCreateRoute,5,r_ipVersion);
													sqlite3_bind_int(_sCreateRoute,6,r_flags);
													sqlite3_bind_int(_sCreateRoute,7,r_metric);
													sqlite3_step(_sCreateRoute);
												}
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

	_backupNeeded = true;

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
					sqlite3_bind_int(_sDeleteIpAllocations,3,(int)0 /*ZT_IP_ASSIGNMENT_TYPE_ADDRESS*/);
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
	bool run = true;
	while(run) {
		Thread::sleep(250);
		try {
			std::vector<std::string> errors;
			Mutex::Lock _l(_lock);
			run = _dbCommitThreadRun;
			if (!_db.commit(&errors)) {
				// TODO: handle anything really bad
			}
		} catch ( ... ) {
			// TODO: handle anything really bad
		}
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
	if ((path.size() > 0)&&(path[0] == "network")) {
		auto networks = _db.get<const json::object_t *>("network");
		if (!networks) return 404;

		if ((path.size() >= 2)&&(path[1].length() == 16)) {
			const uint64_t nwid = Utils::hexStrToU64(path[1].c_str());
			char nwids[24];
			Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);
			auto network = _db.get<const json::object_t *>(nwids);
			if (!network) return 404;

			if (path.size() >= 3) {

				if (path[2] == "member") {
					auto members = network->get<const json::object_t *>("member");
					if (!members) return 404;

					if (path.size() >= 4) {
						const uint64_t address = Utils::hexStrToU64(path[3].c_str());
						char addrs[24];
						Utils::snprintf(addrs,sizeof(addrs),"%.10llx",address);
						auto member = members->get<const json::object_t *>(addrs);
						if (!member) return 404;

						nlohmann::json o(member);
						o["nwid"] = nwids;
						o["address"] = addrs;
						o["controllerInstanceId"] = _instanceId;
						o["clock"] = OSUtils::now();
						responseBody = o.dump(2);
						responseContentType = "application/json";
						return 200;

						/*
						sqlite3_reset(_sGetMember2);
						sqlite3_bind_text(_sGetMember2,1,nwids,16,SQLITE_STATIC);
						sqlite3_bind_text(_sGetMember2,2,addrs,10,SQLITE_STATIC);
						if (sqlite3_step(_sGetMember2) == SQLITE_ROW) {
							const char *memberIdStr = (const char *)sqlite3_column_text(_sGetMember2,3);

							Utils::snprintf(json,sizeof(json),
								"{\n"
								"\t\"nwid\": \"%s\",\n"
								"\t\"address\": \"%s\",\n"
								"\t\"controllerInstanceId\": \"%s\",\n"
								"\t\"authorized\": %s,\n"
								"\t\"activeBridge\": %s,\n"
								"\t\"memberRevision\": %llu,\n"
								"\t\"clock\": %llu,\n"
								"\t\"identity\": \"%s\",\n"
								"\t\"ipAssignments\": [",
								nwids,
								addrs,
								_instanceId.c_str(),
								(sqlite3_column_int(_sGetMember2,0) > 0) ? "true" : "false",
								(sqlite3_column_int(_sGetMember2,1) > 0) ? "true" : "false",
								(unsigned long long)sqlite3_column_int64(_sGetMember2,2),
								(unsigned long long)OSUtils::now(),
								_jsonEscape(memberIdStr).c_str());
							responseBody = json;

							sqlite3_reset(_sGetIpAssignmentsForNode);
							sqlite3_bind_text(_sGetIpAssignmentsForNode,1,nwids,16,SQLITE_STATIC);
							sqlite3_bind_text(_sGetIpAssignmentsForNode,2,addrs,10,SQLITE_STATIC);
							bool firstIp = true;
							while (sqlite3_step(_sGetIpAssignmentsForNode) == SQLITE_ROW) {
								int ipversion = sqlite3_column_int(_sGetIpAssignmentsForNode,2);
								char ipBlob[16];
								memcpy(ipBlob,(const void *)sqlite3_column_blob(_sGetIpAssignmentsForNode,0),16);
								InetAddress ip(
									(const void *)(ipversion == 6 ? ipBlob : &ipBlob[12]),
									(ipversion == 6 ? 16 : 4),
									(unsigned int)sqlite3_column_int(_sGetIpAssignmentsForNode,1)
								);
								responseBody.append(firstIp ? "\"" : ",\"");
								responseBody.append(_jsonEscape(ip.toIpString()));
								responseBody.push_back('"');
								firstIp = false;
							}

							responseBody.append("],\n\t\"recentLog\": [");

							const void *histb = sqlite3_column_blob(_sGetMember2,6);
							if (histb) {
								MemberRecentHistory rh;
								rh.fromBlob((const char *)histb,sqlite3_column_bytes(_sGetMember2,6));
								for(MemberRecentHistory::const_iterator i(rh.begin());i!=rh.end();++i) {
									if (i != rh.begin())
										responseBody.push_back(',');
									responseBody.append(*i);
								}
							}

							responseBody.append("]\n}\n");

							responseContentType = "application/json";
							return 200;
						} // else 404
						*/

					} else {

						responseBody.push_back('{');
						for(auto i(members->begin());i!=members->end();++i) {
							responseBody.append((i == members->begin()) ? "\"" : ",\"");
							responseBody.append(i->key());
							responseBody.append("\":\"");
							const std::string rc = i->value().value("memberRevision","0");
							responseBody.append(rc);
							responseBody.append('"');
						}
						responseBody.push_back('}');
						responseContentType = "application/json";
						return 200;

					}

				} else if ((path[2] == "active")&&(path.size() == 3)) {

					responseBody.push_back('{');
					bool firstMember = true;
					const uint64_t threshold = OSUtils::now() - ZT_NETCONF_NODE_ACTIVE_THRESHOLD;
					for(auto i(members->begin());i!=members->end();++i) {
						auto recentLog = i->value()->get<const json::array_t *>("recentLog");
						if ((recentLog)&&(recentLog.size() > 0)) {
							auto mostRecentLog = recentLog[0];
							if ((mostRecentLog.is_object())&&((uint64_t)mostRecentLog.value("ts",0ULL) >= threshold)) {
								responseBody.append((firstMember) ? "\"" : ",\"");
								firstMember = false;
								responseBody.append(i->key());
								responseBody.append("\":");
								responseBody.append(mostRecentLog.dump());
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

				nlohmann::json o(network);
				o["nwid"] = nwids;
				o["controllerInstanceId"] = _instanceId;
				o["clock"] = OSUtils::now();
				responseBody = o.dump(2);
				responseContentType = "application/json";
				return 200;

			}
		} else if (path.size() == 1) {

			responseBody = "[";
			for(auto i(networks->begin());i!=networks.end();++i) {
				responseBody.append((i == networks->begin()) ? "\"" : ",\"");
				responseBody.append(i->key());
				responseBody.append("\"");
			}
			responseBody.push_back(']');
			responseContentType = "application/json";
			return 200;

		} // else 404

	} else if ((path.size() > 0)&&(path[0] == "_dump")) {

		responseBody = _db.dump(2);
		responseContentType = "application/json";
		return 200;

	} else {

		Utils::snprintf(json,sizeof(json),"{\n\t\"controller\": true,\n\t\"apiVersion\": %d,\n\t\"clock\": %llu,\n\t\"instanceId\": \"%s\"\n}\n",ZT_NETCONF_CONTROLLER_API_VERSION,(unsigned long long)OSUtils::now(),_instanceId.c_str());
		responseBody = json;
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
