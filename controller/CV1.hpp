/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "DB.hpp"

#ifdef ZT_CONTROLLER_USE_LIBPQ

#ifndef ZT_CONTROLLER_CV1_HPP
#define ZT_CONTROLLER_CV1_HPP

#define ZT_CENTRAL_CONTROLLER_COMMIT_THREADS 4

#include "../node/Metrics.hpp"
#include "ConnectionPool.hpp"
#include "PostgreSQL.hpp"

#include <memory>
#include <pqxx/pqxx>
#include <redis++/redis++.h>

namespace smeeclient {
struct SmeeClient;
}

namespace ZeroTier {

struct RedisConfig;

/**
 * A controller database driver that talks to  PostgreSQL
 *
 * This is for use with ZeroTier Central.  Others are free to build and use it
 * but be aware that we might change it at any time.
 */
class CV1 : public DB {
	friend class MemberNotificationReceiver<CV1>;
	friend class NetworkNotificationReceiver<CV1>;

  public:
	CV1(const Identity& myId, const char* path, int listenPort, RedisConfig* rc);
	virtual ~CV1();

	virtual bool waitForReady();
	virtual bool isReady();
	virtual bool save(nlohmann::json& record, bool notifyListeners);
	virtual void eraseNetwork(const uint64_t networkId);
	virtual void eraseMember(const uint64_t networkId, const uint64_t memberId);
	virtual void nodeIsOnline(const uint64_t networkId, const uint64_t memberId, const InetAddress& physicalAddress);
	virtual void nodeIsOnline(const uint64_t networkId, const uint64_t memberId, const InetAddress& physicalAddress, const char* osArch);
	virtual AuthInfo getSSOAuthInfo(const nlohmann::json& member, const std::string& redirectURL);

	virtual bool ready()
	{
		return _ready == 2;
	}

  protected:
	struct _PairHasher {
		inline std::size_t operator()(const std::pair<uint64_t, uint64_t>& p) const
		{
			return (std::size_t)(p.first ^ p.second);
		}
	};
	virtual void _memberChanged(nlohmann::json& old, nlohmann::json& memberConfig, bool notifyListeners)
	{
		DB::_memberChanged(old, memberConfig, notifyListeners);
	}

	virtual void _networkChanged(nlohmann::json& old, nlohmann::json& networkConfig, bool notifyListeners)
	{
		DB::_networkChanged(old, networkConfig, notifyListeners);
	}

  private:
	void initializeNetworks();
	void initializeMembers();
	void heartbeat();
	void membersDbWatcher();
	void _membersWatcher_Postgres();
	void networksDbWatcher();
	void _networksWatcher_Postgres();

	void _membersWatcher_Redis();
	void _networksWatcher_Redis();

	void commitThread();
	void onlineNotificationThread();
	void onlineNotification_Postgres();
	void onlineNotification_Redis();
	uint64_t _doRedisUpdate(sw::redis::Transaction& tx, std::string& controllerId, std::unordered_map<std::pair<uint64_t, uint64_t>, NodeOnlineRecord, _PairHasher>& lastOnline);

	void configureSmee();
	void notifyNewMember(const std::string& networkID, const std::string& memberID);

	enum OverrideMode { ALLOW_PGBOUNCER_OVERRIDE = 0, NO_OVERRIDE = 1 };

	std::shared_ptr<ConnectionPool<PostgresConnection> > _pool;

	const Identity _myId;
	const Address _myAddress;
	std::string _myAddressStr;
	std::string _connString;

	BlockingQueue<std::pair<nlohmann::json, bool> > _commitQueue;

	std::thread _heartbeatThread;
	std::thread _membersDbWatcher;
	std::thread _networksDbWatcher;
	std::thread _commitThread[ZT_CENTRAL_CONTROLLER_COMMIT_THREADS];
	std::thread _onlineNotificationThread;

	std::unordered_map<std::pair<uint64_t, uint64_t>, NodeOnlineRecord, _PairHasher> _lastOnline;

	mutable std::mutex _lastOnline_l;
	mutable std::mutex _readyLock;
	std::atomic<int> _ready, _connected, _run;
	mutable volatile bool _waitNoticePrinted;

	int _listenPort;
	uint8_t _ssoPsk[48];

	RedisConfig* _rc;
	std::shared_ptr<sw::redis::Redis> _redis;
	std::shared_ptr<sw::redis::RedisCluster> _cluster;
	bool _redisMemberStatus;

	smeeclient::SmeeClient* _smee;
};

}	// namespace ZeroTier

#endif	 // ZT_CONTROLLER_CV1_HPP

#endif	 // ZT_CONTROLLER_USE_LIBPQ
