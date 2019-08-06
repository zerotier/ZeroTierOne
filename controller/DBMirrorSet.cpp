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

DBMirrorSet::DBMirrorSet(DB::ChangeListener *listener) :
	_listener(listener)
{
}

DBMirrorSet::~DBMirrorSet()
{
}

bool DBMirrorSet::hasNetwork(const uint64_t networkId) const
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if ((*d)->hasNetwork(networkId))
			return true;
	}
	return false;
}

bool DBMirrorSet::get(const uint64_t networkId,nlohmann::json &network)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if ((*d)->get(networkId,network)) {
			return true;
		}
	}
	return false;
}

bool DBMirrorSet::get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if ((*d)->get(networkId,network,memberId,member))
			return true;
	}
	return false;
}

bool DBMirrorSet::get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member,DB::NetworkSummaryInfo &info)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if ((*d)->get(networkId,network,memberId,member,info))
			return true;
	}
	return false;
}

bool DBMirrorSet::get(const uint64_t networkId,nlohmann::json &network,std::vector<nlohmann::json> &members)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if ((*d)->get(networkId,network,members))
			return true;
	}
	return false;
}

void DBMirrorSet::networks(std::set<uint64_t> &networks)
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		(*d)->networks(networks);
	}
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

bool DBMirrorSet::save(nlohmann::json &record,bool notifyListeners)
{
	std::vector< std::shared_ptr<DB> > dbs;
	{
		std::lock_guard<std::mutex> l(_dbs_l);
		dbs = _dbs;
	}
	if (notifyListeners) {
		for(auto d=dbs.begin();d!=dbs.end();++d) {
			if ((*d)->save(record,notifyListeners))
				return true;
		}
		return false;
	} else {
		bool modified = false;
		for(auto d=dbs.begin();d!=dbs.end();++d) {
			modified |= (*d)->save(record,notifyListeners);
		}
		return modified;
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

void DBMirrorSet::onNetworkUpdate(const void *db,uint64_t networkId,const nlohmann::json &network)
{
	nlohmann::json record(network);
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if (d->get() != db) {
			(*d)->save(record,false);
		}
	}
	_listener->onNetworkUpdate(this,networkId,network);
}

void DBMirrorSet::onNetworkMemberUpdate(const void *db,uint64_t networkId,uint64_t memberId,const nlohmann::json &member)
{
	nlohmann::json record(member);
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) {
		if (d->get() != db) {
			(*d)->save(record,false);
		}
	}
	_listener->onNetworkMemberUpdate(this,networkId,memberId,member);
}

void DBMirrorSet::onNetworkMemberDeauthorize(const void *db,uint64_t networkId,uint64_t memberId)
{
	_listener->onNetworkMemberDeauthorize(this,networkId,memberId);
}

} // namespace ZeroTier
