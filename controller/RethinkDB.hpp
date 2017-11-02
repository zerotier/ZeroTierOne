#ifndef ZT_CONTROLLER_RETHINKDB_HPP
#define ZT_CONTROLLER_RETHINKDB_HPP

#include "../node/Constants.hpp"
#include "../node/Address.hpp"
#include "../node/InetAddress.hpp"
#include "../osdep/OSUtils.hpp"

#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../ext/json/json.hpp"

namespace ZeroTier
{

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

	RethinkDB(const Address &myAddress,const char *host,const int port,const char *db,const char *auth);
	~RethinkDB();

	inline bool ready() const { return (_ready <= 0); }

	inline void waitForReady() const
	{
		while (_ready > 0) {
			_readyLock.lock();
			_readyLock.unlock();
		}
	}

	bool get(const uint64_t networkId,nlohmann::json &network);
	bool get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member,NetworkSummaryInfo &info);
	bool get(const uint64_t networkId,nlohmann::json &network,std::vector<nlohmann::json> &members);
	bool summary(const uint64_t networkId,NetworkSummaryInfo &info);

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

	const Address _myAddress;
	std::string _myAddressStr;
	std::string _host;
	std::string _db;
	std::string _auth;
	const int _port;

	void *_networksDbWatcherConnection;
	void *_membersDbWatcherConnection;
	std::thread _networksDbWatcher;
	std::thread _membersDbWatcher;

	std::unordered_map< uint64_t,std::shared_ptr<_Network> > _networks;
	std::mutex _networks_l;

	mutable std::mutex _readyLock; // locked until ready
	std::atomic<int> _ready;
	std::atomic<int> _run;
};

} // namespace ZeroTier

#endif
