/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_CONTROLLER_DB_HPP
#define ZT_CONTROLLER_DB_HPP

//#define ZT_CONTROLLER_USE_LIBPQ

#include "../node/Constants.hpp"
#include "../node/Identity.hpp"
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
#include <shared_mutex>
#include <set>
#include <map>

#include <nlohmann/json.hpp>

#include <prometheus/simpleapi.h>

#define ZT_MEMBER_AUTH_TIMEOUT_NOTIFY_BEFORE 25000

namespace ZeroTier
{

struct AuthInfo
{
public:
	AuthInfo() 
	: enabled(false)
	, version(0)
	, authenticationURL()
	, authenticationExpiryTime(0)
	, issuerURL()
	, centralAuthURL()
	, ssoNonce()
	, ssoState()
	, ssoClientID()
	, ssoProvider("default")
	{}

	bool enabled;
	uint64_t version;
	std::string authenticationURL;
	uint64_t authenticationExpiryTime;
	std::string issuerURL;
	std::string centralAuthURL;
	std::string ssoNonce;
	std::string ssoState;
	std::string ssoClientID;
	std::string ssoProvider;
};

/**
 * Base class with common infrastructure for all controller DB implementations
 */
class DB
{
public:
	class ChangeListener
	{
	public:
		ChangeListener() {}
		virtual ~ChangeListener() {}
		virtual void onNetworkUpdate(const void *db,uint64_t networkId,const nlohmann::json &network) {}
		virtual void onNetworkMemberUpdate(const void *db,uint64_t networkId,uint64_t memberId,const nlohmann::json &member) {}
		virtual void onNetworkMemberDeauthorize(const void *db,uint64_t networkId,uint64_t memberId) {}
	};

	struct NetworkSummaryInfo
	{
		NetworkSummaryInfo() : authorizedMemberCount(0),totalMemberCount(0),mostRecentDeauthTime(0) {}
		std::vector<Address> activeBridges;
		std::vector<InetAddress> allocatedIps;
		unsigned long authorizedMemberCount;
		unsigned long totalMemberCount;
		int64_t mostRecentDeauthTime;
	};

	static void initNetwork(nlohmann::json &network);
	static void initMember(nlohmann::json &member);
	static void cleanNetwork(nlohmann::json &network);
	static void cleanMember(nlohmann::json &member);

	DB();
	virtual ~DB();

	virtual bool waitForReady() = 0;
	virtual bool isReady() = 0;

	inline bool hasNetwork(const uint64_t networkId) const
	{
		std::shared_lock<std::shared_mutex> l(_networks_l);
		return (_networks.find(networkId) != _networks.end());
	}

	bool get(const uint64_t networkId,nlohmann::json &network);
	bool get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member);
	bool get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member,NetworkSummaryInfo &info);
	bool get(const uint64_t networkId,nlohmann::json &network,std::vector<nlohmann::json> &members);

	void networks(std::set<uint64_t> &networks);

	template<typename F>
	inline void each(F f)
	{
		nlohmann::json nullJson;
		std::unique_lock<std::shared_mutex> lck(_networks_l);
		for(auto nw=_networks.begin();nw!=_networks.end();++nw) {
			f(nw->first,nw->second->config,0,nullJson); // first provide network with 0 for member ID
			for(auto m=nw->second->members.begin();m!=nw->second->members.end();++m) {
				f(nw->first,nw->second->config,m->first,m->second);
			}
		}
	}

	virtual bool save(nlohmann::json &record,bool notifyListeners) = 0;
	virtual void eraseNetwork(const uint64_t networkId) = 0;
	virtual void eraseMember(const uint64_t networkId,const uint64_t memberId) = 0;
	virtual void nodeIsOnline(const uint64_t networkId,const uint64_t memberId,const InetAddress &physicalAddress) = 0;

	virtual AuthInfo getSSOAuthInfo(const nlohmann::json &member, const std::string &redirectURL) { return AuthInfo(); }

	inline void addListener(DB::ChangeListener *const listener)
	{
		std::unique_lock<std::shared_mutex> l(_changeListeners_l);
		_changeListeners.push_back(listener);
	}

protected:
	static inline bool _compareRecords(const nlohmann::json &a,const nlohmann::json &b)
	{
		if (a.is_object() == b.is_object()) {
			if (a.is_object()) {
				if (a.size() != b.size())
					return false;
				auto amap = a.get<nlohmann::json::object_t>();
				auto bmap = b.get<nlohmann::json::object_t>();
				for(auto ai=amap.begin();ai!=amap.end();++ai) {
					if (ai->first != "revision") { // ignore revision, compare only non-revision-counter fields
						auto bi = bmap.find(ai->first);
						if ((bi == bmap.end())||(bi->second != ai->second))
							return false;
					}
				}
				return true;
			}
			return (a == b);
		}
		return false;
	}

	struct _Network
	{
		_Network() : mostRecentDeauthTime(0) {}
		nlohmann::json config;
		std::unordered_map<uint64_t,nlohmann::json> members;
		std::unordered_set<uint64_t> activeBridgeMembers;
		std::unordered_set<uint64_t> authorizedMembers;
		std::unordered_set<InetAddress,InetAddress::Hasher> allocatedIps;
		int64_t mostRecentDeauthTime;
		std::shared_mutex lock;
	};

	virtual void _memberChanged(nlohmann::json &old,nlohmann::json &memberConfig,bool notifyListeners);
	virtual void _networkChanged(nlohmann::json &old,nlohmann::json &networkConfig,bool notifyListeners);
	void _fillSummaryInfo(const std::shared_ptr<_Network> &nw,NetworkSummaryInfo &info);

	std::vector<DB::ChangeListener *> _changeListeners;
	std::unordered_map< uint64_t,std::shared_ptr<_Network> > _networks;
	std::unordered_multimap< uint64_t,uint64_t > _networkByMember;
	mutable std::shared_mutex _changeListeners_l;
	mutable std::shared_mutex _networks_l;
};

} // namespace ZeroTier

#endif
