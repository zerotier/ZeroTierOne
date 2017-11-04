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

#include "../node/Constants.hpp"
#include "../node/Address.hpp"
#include "../node/InetAddress.hpp"
#include "../osdep/OSUtils.hpp"
#include "../osdep/BlockingQueue.hpp"

#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <atomic>

#include "../ext/json/json.hpp"

#define ZT_CONTROLLER_RETHINKDB_COMMIT_THREADS 2

namespace ZeroTier
{

class EmbeddedNetworkController;

class RethinkDB
{
public:
	struct NetworkSummaryInfo
	{
		NetworkSummaryInfo() : authorizedMemberCount(0),totalMemberCount(0),mostRecentDeauthTime(0) {}
		std::vector<Address> activeBridges;
		std::vector<InetAddress> allocatedIps;
		unsigned long authorizedMemberCount;
		unsigned long totalMemberCount;
		int64_t mostRecentDeauthTime;
	};

	RethinkDB(EmbeddedNetworkController *const nc,const Address &myAddress,const char *path);
	~RethinkDB();

	inline void waitForReady() const
	{
		while (_ready > 0) {
			if (!_waitNoticePrinted) {
				_waitNoticePrinted = true;
				fprintf(stderr,"NOTICE: controller RethinkDB waiting for initial data download..." ZT_EOL_S);
			}
			_readyLock.lock();
			_readyLock.unlock();
		}
	}

	inline bool hasNetwork(const uint64_t networkId) const
	{
		std::lock_guard<std::mutex> l(_networks_l);
		return (_networks.find(networkId) != _networks.end());
	}

	bool get(const uint64_t networkId,nlohmann::json &network);
	bool get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member);
	bool get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member,NetworkSummaryInfo &info);
	bool get(const uint64_t networkId,nlohmann::json &network,std::vector<nlohmann::json> &members);

	bool summary(const uint64_t networkId,NetworkSummaryInfo &info);

	void networks(std::vector<uint64_t> &networks);

	void save(const nlohmann::json &record);

	void eraseNetwork(const uint64_t networkId);
	void eraseMember(const uint64_t networkId,const uint64_t memberId);

private:
	struct _Network
	{
		_Network() : mostRecentDeauthTime(0) {}
		nlohmann::json config;
		std::unordered_map<uint64_t,nlohmann::json> members;
		std::unordered_set<uint64_t> activeBridgeMembers;
		std::unordered_set<uint64_t> authorizedMembers;
		std::unordered_set<InetAddress,InetAddress::Hasher> allocatedIps;
		int64_t mostRecentDeauthTime;
		std::mutex lock;
	};

	void _memberChanged(nlohmann::json &old,nlohmann::json &member);
	void _networkChanged(nlohmann::json &old,nlohmann::json &network);

	inline void _fillSummaryInfo(const std::shared_ptr<_Network> &nw,NetworkSummaryInfo &info)
	{
		for(auto ab=nw->activeBridgeMembers.begin();ab!=nw->activeBridgeMembers.end();++ab)
			info.activeBridges.push_back(Address(*ab));
		for(auto ip=nw->allocatedIps.begin();ip!=nw->allocatedIps.end();++ip)
			info.allocatedIps.push_back(*ip);
		info.authorizedMemberCount = (unsigned long)nw->authorizedMembers.size();
		info.totalMemberCount = (unsigned long)nw->members.size();
		info.mostRecentDeauthTime = nw->mostRecentDeauthTime;
	}

	EmbeddedNetworkController *const _controller;
	const Address _myAddress;
	std::string _myAddressStr;
	std::string _host;
	std::string _db;
	std::string _auth;
	int _port;

	void *_networksDbWatcherConnection;
	void *_membersDbWatcherConnection;
	std::thread _networksDbWatcher;
	std::thread _membersDbWatcher;

	std::unordered_map< uint64_t,std::shared_ptr<_Network> > _networks;
	std::unordered_multimap< uint64_t,uint64_t > _networkByMember;
	mutable std::mutex _networks_l;

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
