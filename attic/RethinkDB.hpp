/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2018  ZeroTier, Inc.
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

#define ZT_CONTROLLER_RETHINKDB_COMMIT_THREADS 4

namespace ZeroTier
{

/**
 * A controller database driver that talks to RethinkDB
 *
 * This is for use with ZeroTier Central. Others are free to build and use it
 * but be aware that we might change it at any time.
 */
class RethinkDB : public DB
{
public:
	RethinkDB(EmbeddedNetworkController *const nc,const Identity &myId,const char *path);
	virtual ~RethinkDB();

	virtual bool waitForReady();
	virtual bool isReady();
	virtual void save(nlohmann::json *orig,nlohmann::json &record);
	virtual void eraseNetwork(const uint64_t networkId);
	virtual void eraseMember(const uint64_t networkId,const uint64_t memberId);
	virtual void nodeIsOnline(const uint64_t networkId,const uint64_t memberId,const InetAddress &physicalAddress);

protected:
	struct _PairHasher
	{
		inline std::size_t operator()(const std::pair<uint64_t,uint64_t> &p) const { return (std::size_t)(p.first ^ p.second); }
	};

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

	std::unordered_map< std::pair<uint64_t,uint64_t>,std::pair<int64_t,InetAddress>,_PairHasher > _lastOnline;
	mutable std::mutex _lastOnline_l;
	std::thread _onlineNotificationThread;

	std::thread _heartbeatThread;

	mutable std::mutex _readyLock; // locked until ready
	std::atomic<int> _ready,_connected,_run;
	mutable volatile bool _waitNoticePrinted;
};

} // namespace ZeroTier

#endif

#endif // ZT_CONTROLLER_USE_RETHINKDB
