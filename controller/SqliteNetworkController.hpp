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
#include "../node/NonCopyable.hpp"

namespace ZeroTier {

class SqliteNetworkController : public NetworkController
{
public:
	class DBC;
	friend class SqliteNetworkController::DBC;

	SqliteNetworkController(const char *dbPath);
	virtual ~SqliteNetworkController();

	virtual NetworkController::ResultCode doNetworkConfigRequest(
		const InetAddress &fromAddr,
		const Identity &signingId,
		const Identity &identity,
		uint64_t nwid,
		const Dictionary &metaData,
		uint64_t haveRevision,
		Dictionary &netconf);

private:
	std::string _dbPath;
	sqlite3 *_db;

	sqlite3_stmt *_sGetNetworkById;
	sqlite3_stmt *_sGetMemberByNetworkAndNodeId;
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

	Mutex _lock;

public:
	/**
	 * Provides a safe interface for direct access to this master's database
	 *
	 * This acts as both a contextual lock of the master's Mutex and a pointer
	 * to the Sqlite3 database instance. Dereferencing this with * yields the
	 * sqlite3* pointer. Create on parent with DBC(SqliteNetworkController &).
	 */
	class DBC : NonCopyable
	{
	public:
		DBC(SqliteNetworkController &nc) : _p(&nc) { nc._lock.lock(); }
		~DBC() { _p->_lock.unlock(); }
		inline sqlite3 *operator*() const throw() { return _p->_db; }
	private:
		SqliteNetworkController *const _p;
	};
};

} // namespace ZeroTier

#endif
