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

#ifndef ZT_DBMIRRORSET_HPP
#define ZT_DBMIRRORSET_HPP

#include "DB.hpp"

#include <vector>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

namespace ZeroTier {

class DBMirrorSet : public DB::ChangeListener
{
public:
	DBMirrorSet(DB::ChangeListener *listener);
	virtual ~DBMirrorSet();

	bool hasNetwork(const uint64_t networkId) const;

	bool get(const uint64_t networkId,nlohmann::json &network);
	bool get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member);
	bool get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member,DB::NetworkSummaryInfo &info);
	bool get(const uint64_t networkId,nlohmann::json &network,std::vector<nlohmann::json> &members);

	void networks(std::set<uint64_t> &networks);

	bool waitForReady();
	bool isReady();
	bool save(nlohmann::json &record,bool notifyListeners);
	void eraseNetwork(const uint64_t networkId);
	void eraseMember(const uint64_t networkId,const uint64_t memberId);
	void nodeIsOnline(const uint64_t networkId,const uint64_t memberId,const InetAddress &physicalAddress);

	// These are called by various DB instances when changes occur.
	virtual void onNetworkUpdate(const void *db,uint64_t networkId,const nlohmann::json &network);
	virtual void onNetworkMemberUpdate(const void *db,uint64_t networkId,uint64_t memberId,const nlohmann::json &member);
	virtual void onNetworkMemberDeauthorize(const void *db,uint64_t networkId,uint64_t memberId);

	std::string getSSOAuthURL(const nlohmann::json &member, const std::string &redirectURL);

	inline void addDB(const std::shared_ptr<DB> &db)
	{
		db->addListener(this);
		std::lock_guard<std::mutex> l(_dbs_l);
		_dbs.push_back(db);
	}

	std::vector<std::pair<uint64_t, uint64_t>> membersExpiringSoon();
	void memberExpiring(int64_t expTime, uint64_t nwid, uint64_t memberId);

private:
	DB::ChangeListener *const _listener;
	std::atomic_bool _running;
	std::thread _syncCheckerThread;
	std::vector< std::shared_ptr< DB > > _dbs;
	mutable std::mutex _dbs_l;
	std::multimap< int64_t, std::pair<uint64_t, uint64_t> > _membersExpiringSoon;
	mutable std::mutex _membersExpiringSoon_l;
};

} // namespace ZeroTier

#endif
