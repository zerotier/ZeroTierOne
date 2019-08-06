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

#include "DBMirrorSet.hpp"

namespace ZeroTier {

DBMirrorSet::DBMirrorSet()
{
}

DBMirrorSet::~DBMirrorSet()
{
}

bool DBMirrorSet::waitForReady()
{
	bool r = false;
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		r |= (*d)->waitForReady();
	}
	return r;
}

bool DBMirrorSet::isReady()
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if (!(*d)->isReady())
			return false;
	}
	return true;
}

void DBMirrorSet::save(nlohmann::json &record)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		(*d)->save(record);
	}
}

void DBMirrorSet::eraseNetwork(const uint64_t networkId)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		(*d)->eraseNetwork(networkId);
	}
}

void DBMirrorSet::eraseMember(const uint64_t networkId,const uint64_t memberId)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		(*d)->eraseMember(networkId,memberId);
	}
}

void DBMirrorSet::nodeIsOnline(const uint64_t networkId,const uint64_t memberId,const InetAddress &physicalAddress)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		(*d)->nodeIsOnline(networkId,memberId,physicalAddress);
	}
}

void DBMirrorSet::onNetworkUpdate(const DB *db,uint64_t networkId,const nlohmann::json &network)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if (d->get() != db) {
		}
	}
}

void DBMirrorSet::onNetworkMemberUpdate(const DB *db,uint64_t networkId,uint64_t memberId,const nlohmann::json &member)
{
}

void DBMirrorSet::onNetworkMemberDeauthorize(const DB *db,uint64_t networkId,uint64_t memberId)
{
}

void DBMirrorSet::onNetworkMemberOnline(const DB *db,uint64_t networkId,uint64_t memberId,const InetAddress &physicalAddress)
{
}

} // namespace ZeroTier
