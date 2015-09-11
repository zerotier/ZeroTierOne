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

#ifndef ZT_SQLITENETWORKCONTROLLER_HPP
#define ZT_SQLITENETWORKCONTROLLER_HPP

#include <stdint.h>

#include <sqlite3.h>

#include <string>
#include <map>
#include <vector>

#include "../node/Constants.hpp"
#include "../node/NetworkController.hpp"
#include "../node/Mutex.hpp"

namespace ZeroTier {

class SqliteNetworkController : public NetworkController
{
public:
	SqliteNetworkController(const char *dbPath);
	virtual ~SqliteNetworkController();

	virtual NetworkController::ResultCode doNetworkConfigRequest(
		const InetAddress &fromAddr,
		const Identity &signingId,
		const Identity &identity,
		uint64_t nwid,
		const Dictionary &metaData,
		Dictionary &netconf);

	unsigned int handleControlPlaneHttpGET(
		const std::vector<std::string> &path,
		const std::map<std::string,std::string> &urlArgs,
		const std::map<std::string,std::string> &headers,
		const std::string &body,
		std::string &responseBody,
		std::string &responseContentType);
	unsigned int handleControlPlaneHttpPOST(
		const std::vector<std::string> &path,
		const std::map<std::string,std::string> &urlArgs,
		const std::map<std::string,std::string> &headers,
		const std::string &body,
		std::string &responseBody,
		std::string &responseContentType);
	unsigned int handleControlPlaneHttpDELETE(
		const std::vector<std::string> &path,
		const std::map<std::string,std::string> &urlArgs,
		const std::map<std::string,std::string> &headers,
		const std::string &body,
		std::string &responseBody,
		std::string &responseContentType);

private:
	enum IpAssignmentType {
		// IP assignment is a static IP address
		ZT_IP_ASSIGNMENT_TYPE_ADDRESS = 0,
		// IP assignment is a network -- a route via this interface, not an address
		ZT_IP_ASSIGNMENT_TYPE_NETWORK = 1
	};

	unsigned int _doCPGet(
		const std::vector<std::string> &path,
		const std::map<std::string,std::string> &urlArgs,
		const std::map<std::string,std::string> &headers,
		const std::string &body,
		std::string &responseBody,
		std::string &responseContentType);
	NetworkController::ResultCode _doNetworkConfigRequest(
		const InetAddress &fromAddr,
		const Identity &signingId,
		const Identity &identity,
		uint64_t nwid,
		const Dictionary &metaData,
		Dictionary &netconf);

	std::string _dbPath;
	std::string _instanceId;

	std::map< std::pair<Address,uint64_t>,uint64_t > _lastRequestTime;

	sqlite3 *_db;

	sqlite3_stmt *_sGetNetworkById;
	sqlite3_stmt *_sGetMember;
	sqlite3_stmt *_sCreateMember;
	sqlite3_stmt *_sGetNodeIdentity;
	sqlite3_stmt *_sCreateOrReplaceNode;
	sqlite3_stmt *_sUpdateNode;
	sqlite3_stmt *_sUpdateNode2;
	sqlite3_stmt *_sGetEtherTypesFromRuleTable;
	sqlite3_stmt *_sGetActiveBridges;
	sqlite3_stmt *_sGetIpAssignmentsForNode;
	sqlite3_stmt *_sGetIpAssignmentPools;
	sqlite3_stmt *_sGetLocalRoutes;
	sqlite3_stmt *_sCheckIfIpIsAllocated;
	sqlite3_stmt *_sAllocateIp;
	sqlite3_stmt *_sDeleteIpAllocations;
	sqlite3_stmt *_sDeleteLocalRoutes;
	sqlite3_stmt *_sGetRelays;
	sqlite3_stmt *_sListNetworks;
	sqlite3_stmt *_sListNetworkMembers;
	sqlite3_stmt *_sGetMember2;
	sqlite3_stmt *_sGetIpAssignmentPools2;
	sqlite3_stmt *_sListRules;
	sqlite3_stmt *_sCreateRule;
	sqlite3_stmt *_sCreateNetwork;
	sqlite3_stmt *_sGetNetworkRevision;
	sqlite3_stmt *_sSetNetworkRevision;
	sqlite3_stmt *_sGetIpAssignmentsForNode2;
	sqlite3_stmt *_sDeleteRelaysForNetwork;
	sqlite3_stmt *_sCreateRelay;
	sqlite3_stmt *_sDeleteIpAssignmentPoolsForNetwork;
	sqlite3_stmt *_sDeleteRulesForNetwork;
	sqlite3_stmt *_sCreateIpAssignmentPool;
	sqlite3_stmt *_sUpdateMemberAuthorized;
	sqlite3_stmt *_sUpdateMemberActiveBridge;
	sqlite3_stmt *_sDeleteMember;
	sqlite3_stmt *_sDeleteAllNetworkMembers;
	sqlite3_stmt *_sDeleteNetwork;
	sqlite3_stmt *_sGetGateways;
	sqlite3_stmt *_sDeleteGateways;
	sqlite3_stmt *_sCreateGateway;
	sqlite3_stmt *_sIncrementMemberRevisionCounter;
	sqlite3_stmt *_sGetConfig;
	sqlite3_stmt *_sSetConfig;
	sqlite3_stmt *_sPutLog;
	sqlite3_stmt *_sGetMemberLog;
	sqlite3_stmt *_sGetRecentMemberLog;

	Mutex _lock;
};

} // namespace ZeroTier

#endif
