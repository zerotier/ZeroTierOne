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

#include "../service/ControlPlaneSubsystem.hpp"

namespace ZeroTier {

class SqliteNetworkController : public NetworkController,public ControlPlaneSubsystem
{
public:
	SqliteNetworkController(const char *dbPath);
	virtual ~SqliteNetworkController();

	// NetworkController
	virtual NetworkController::ResultCode doNetworkConfigRequest(
		const InetAddress &fromAddr,
		const Identity &signingId,
		const Identity &identity,
		uint64_t nwid,
		const Dictionary &metaData,
		uint64_t haveRevision,
		Dictionary &netconf);

	// ControlPlaneSubsystem
	virtual unsigned int handleControlPlaneHttpGET(
		const std::vector<std::string> &path,
		const std::map<std::string,std::string> &urlArgs,
		const std::map<std::string,std::string> &headers,
		const std::string &body,
		std::string &responseBody,
		std::string &responseContentType);
	virtual unsigned int handleControlPlaneHttpPOST(
		const std::vector<std::string> &path,
		const std::map<std::string,std::string> &urlArgs,
		const std::map<std::string,std::string> &headers,
		const std::string &body,
		std::string &responseBody,
		std::string &responseContentType);
	virtual unsigned int handleControlPlaneHttpDELETE(
		const std::vector<std::string> &path,
		const std::map<std::string,std::string> &urlArgs,
		const std::map<std::string,std::string> &headers,
		const std::string &body,
		std::string &responseBody,
		std::string &responseContentType);

private:
	std::string _dbPath;
	sqlite3 *_db;

	sqlite3_stmt *_sGetNetworkById;
	sqlite3_stmt *_sGetMember;
	sqlite3_stmt *_sCreateMember;
	sqlite3_stmt *_sGetNodeIdentity;
	sqlite3_stmt *_sCreateNode;
	sqlite3_stmt *_sUpdateNode;
	sqlite3_stmt *_sUpdateNode2;
	sqlite3_stmt *_sUpdateMemberClientReportedRevision;
	sqlite3_stmt *_sGetEtherTypesFromRuleTable;
	sqlite3_stmt *_sGetMulticastRates;
	sqlite3_stmt *_sGetActiveBridges;
	sqlite3_stmt *_sGetIpAssignmentsForNode;
	sqlite3_stmt *_sGetIpAssignmentPools;
	sqlite3_stmt *_sCheckIfIpIsAllocated;
	sqlite3_stmt *_sAllocateIp;
	sqlite3_stmt *_sCacheNetconf;
	sqlite3_stmt *_sGetRelays;
	sqlite3_stmt *_sListNetworks;
	sqlite3_stmt *_sListNetworkMembers;
	sqlite3_stmt *_sGetMember2;
	sqlite3_stmt *_sGetIpAssignmentPools2;
	sqlite3_stmt *_sListRules;
	sqlite3_stmt *_sDeleteRule;
	sqlite3_stmt *_sCreateRule;
	sqlite3_stmt *_sCreateNetwork;
	sqlite3_stmt *_sUpdateNetworkField;
	sqlite3_stmt *_sGetNetworkRevision;
	sqlite3_stmt *_sGetIpAssignmentsForNode2;
	sqlite3_stmt *_sDeleteRelaysForNetwork;
	sqlite3_stmt *_sCreateRelay;

	Mutex _lock;
};

} // namespace ZeroTier

#endif
