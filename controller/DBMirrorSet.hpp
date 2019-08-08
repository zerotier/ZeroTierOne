/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

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

	inline void addDB(const std::shared_ptr<DB> &db)
	{
		db->addListener(this);
		std::lock_guard<std::mutex> l(_dbs_l);
		_dbs.push_back(db);
	}

private:
	DB::ChangeListener *const _listener;
	std::atomic_bool _running;
	std::thread _syncCheckerThread;
	std::vector< std::shared_ptr< DB > > _dbs;
	mutable std::mutex _dbs_l;
};

} // namespace ZeroTier

#endif
