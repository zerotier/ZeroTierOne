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

namespace ZeroTier {

class DBMirrorSet : public DB::ChangeListener
{
public:
	DBMirrorSet();
	virtual ~DBMirrorSet();

	bool waitForReady();
	bool isReady();
	void save(nlohmann::json &record);
	void eraseNetwork(const uint64_t networkId);
	void eraseMember(const uint64_t networkId,const uint64_t memberId);
	void nodeIsOnline(const uint64_t networkId,const uint64_t memberId,const InetAddress &physicalAddress);

	// These are called by various DB instances when changes occur.
	virtual void onNetworkUpdate(const DB *db,uint64_t networkId,const nlohmann::json &network);
	virtual void onNetworkMemberUpdate(const DB *db,uint64_t networkId,uint64_t memberId,const nlohmann::json &member);
	virtual void onNetworkMemberDeauthorize(const DB *db,uint64_t networkId,uint64_t memberId);
	virtual void onNetworkMemberOnline(const DB *db,uint64_t networkId,uint64_t memberId,const InetAddress &physicalAddress);

	inline void addDB(const std::shared_ptr<DB> &db)
	{
		std::lock_guard<std::mutex> l(_dbs_l);
		_dbs.push_back(db);
	}

private:
	std::vector< std::shared_ptr< DB > > _dbs;
	std::mutex _dbs_l;
};

} // namespace ZeroTier

#endif
