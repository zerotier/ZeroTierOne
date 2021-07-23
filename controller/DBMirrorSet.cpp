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

#include "DBMirrorSet.hpp"

namespace ZeroTier {

DBMirrorSet::DBMirrorSet(DB::ChangeListener *listener) :
	_listener(listener),
	_running(true)
{
	_syncCheckerThread = std::thread([this]() {
		for(;;) {
			for(int i=0;i<120;++i) { // 1 minute delay between checks
				if (!_running)
					return;
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}

			std::vector< std::shared_ptr<DB> > dbs;
			{
				std::lock_guard<std::mutex> l(_dbs_l);
				if (_dbs.size() <= 1)
					continue; // no need to do this if there's only one DB, so skip the iteration
				dbs = _dbs;
			}

			for(auto db=dbs.begin();db!=dbs.end();++db) {
				(*db)->each([&dbs,&db](uint64_t networkId,const nlohmann::json &network,uint64_t memberId,const nlohmann::json &member) {
					try {
						if (network.is_object()) {
							if (memberId == 0) {
								for(auto db2=dbs.begin();db2!=dbs.end();++db2) {
									if (db->get() != db2->get()) {
										nlohmann::json nw2;
										if ((!(*db2)->get(networkId,nw2))||((nw2.is_object())&&(OSUtils::jsonInt(nw2["revision"],0) < OSUtils::jsonInt(network["revision"],0)))) {
											nw2 = network;
											(*db2)->save(nw2,false);
										}
									}
								}
							} else if (member.is_object()) {
								for(auto db2=dbs.begin();db2!=dbs.end();++db2) {
									if (db->get() != db2->get()) {
										nlohmann::json nw2,m2;
										if ((!(*db2)->get(networkId,nw2,memberId,m2))||((m2.is_object())&&(OSUtils::jsonInt(m2["revision"],0) < OSUtils::jsonInt(member["revision"],0)))) {
											m2 = member;
											(*db2)->save(m2,false);
										}
									}
								}
							}
						}
					} catch ( ... ) {} // skip entries that generate JSON errors
				});
			}
		}
	});
}

DBMirrorSet::~DBMirrorSet()
{
	_running = false;
	_syncCheckerThread.join();
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

std::string DBMirrorSet::getSSOAuthURL(const nlohmann::json &member, const std::string &redirectURL) 
{
	std::lock_guard<std::mutex> l(_dbs_l);
	for(auto d=_dbs.begin();d!=_dbs.end();++d) { 
		std::string url = (*d)->getSSOAuthURL(member, redirectURL);
		if (!url.empty()) {
			return url;
		}
	}
	return "";
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
			if ((*d)->save(record,true))
				return true;
		}
		return false;
	} else {
		bool modified = false;
		for(auto d=dbs.begin();d!=dbs.end();++d) {
			modified |= (*d)->save(record,false);
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

std::vector<std::pair<uint64_t, uint64_t>> DBMirrorSet::membersExpiringSoon()
{
	std::vector<std::pair<uint64_t, uint64_t>> soon;
	std::unique_lock<std::mutex> l(_membersExpiringSoon_l);
	int64_t now = OSUtils::now();
	for(auto next=_membersExpiringSoon.begin();next!=_membersExpiringSoon.end();) {
		if (next->first <= now) {
			// Already expired, so the node will need to re-auth.
			_membersExpiringSoon.erase(next++);
		} else {
			const uint64_t nwid = next->second.first;
			const uint64_t memberId = next->second.second;
			nlohmann::json network, member;
			if (this->get(nwid, network, memberId, member)) {
				try {
					const bool authorized = member["authorized"];
					const bool ssoExempt = member["ssoExempt"];
					const int64_t authenticationExpiryTime = member["authenticationExpiryTime"];
					if ((authenticationExpiryTime == next->first)&&(authorized)&&(!ssoExempt)) {
						if ((authenticationExpiryTime - now) > 10000) {
							// Stop when we get to entries more than 10s in the future.
							break;
						} else {
							soon.push_back(std::pair<uint64_t, uint64_t>(nwid, memberId));
						}
					} else {
						// Obsolete entry, no longer authorized, or SSO exempt.
						_membersExpiringSoon.erase(next++);
					}
				} catch ( ... ) {
					// Invalid member object, erase.
					_membersExpiringSoon.erase(next++);
				}
			} else {
				// Not found, so erase.
				_membersExpiringSoon.erase(next++);
			}
		}
	}
	return soon;
}

void DBMirrorSet::memberExpiring(int64_t expTime, uint64_t nwid, uint64_t memberId)
{
	std::unique_lock<std::mutex> l(_membersExpiringSoon_l);
	_membersExpiringSoon.insert(std::pair< int64_t, std::pair< uint64_t, uint64_t > >(expTime, std::pair< uint64_t, uint64_t >(nwid, memberId)));
}

} // namespace ZeroTier
