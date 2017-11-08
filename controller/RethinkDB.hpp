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

#ifdef ZT_CONTROLLER_USE_RETHINKDB

#ifndef ZT_CONTROLLER_RETHINKDB_HPP
#define ZT_CONTROLLER_RETHINKDB_HPP

#include "DB.hpp"

#define ZT_CONTROLLER_RETHINKDB_COMMIT_THREADS 2

namespace ZeroTier
{

class RethinkDB : public DB
{
public:
	RethinkDB(EmbeddedNetworkController *const nc,const Address &myAddress,const char *path);
	virtual ~RethinkDB();

	virtual void waitForReady() const;

	virtual void save(const nlohmann::json &record);

	virtual void eraseNetwork(const uint64_t networkId);

	virtual void eraseMember(const uint64_t networkId,const uint64_t memberId);

protected:
	std::string _host;
	std::string _db;
	std::string _auth;
	int _port;

	void *_networksDbWatcherConnection;
	void *_membersDbWatcherConnection;
	std::thread _networksDbWatcher;
	std::thread _membersDbWatcher;

	BlockingQueue< nlohmann::json * > _commitQueue;
	std::thread _commitThread[ZT_CONTROLLER_RETHINKDB_COMMIT_THREADS];

	std::thread _heartbeatThread;

	mutable std::mutex _readyLock; // locked until ready
	std::atomic<int> _ready;
	std::atomic<int> _run;
	mutable volatile bool _waitNoticePrinted;
};

} // namespace ZeroTier

#endif

#endif // ZT_CONTROLLER_USE_RETHINKDB
