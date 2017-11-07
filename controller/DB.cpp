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

#include "DB.hpp"
#include "EmbeddedNetworkController.hpp"

#include <chrono>
#include <algorithm>
#include <stdexcept>

using json = nlohmann::json;

namespace ZeroTier {

DB::DB(EmbeddedNetworkController *const nc,const Address &myAddress,const char *path) :
	_controller(nc),
	_myAddress(myAddress),
	_path((path) ? path : "")
{
	{
		char tmp[32];
		_myAddress.toString(tmp);
		_myAddressStr = tmp;
	}
}

DB::~DB()
{
}

bool DB::get(const uint64_t networkId,nlohmann::json &network)
{
	waitForReady();
	std::shared_ptr<_Network> nw;
	{
		std::lock_guard<std::mutex> l(_networks_l);
		auto nwi = _networks.find(networkId);
		if (nwi == _networks.end())
			return false;
		nw = nwi->second;
	}
	{
		std::lock_guard<std::mutex> l2(nw->lock);
		network = nw->config;
	}
	return true;
}

bool DB::get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member)
{
	waitForReady();
	std::shared_ptr<_Network> nw;
	{
		std::lock_guard<std::mutex> l(_networks_l);
		auto nwi = _networks.find(networkId);
		if (nwi == _networks.end())
			return false;
		nw = nwi->second;
	}
	{
		std::lock_guard<std::mutex> l2(nw->lock);
		network = nw->config;
		auto m = nw->members.find(memberId);
		if (m == nw->members.end())
			return false;
		member = m->second;
	}
	return true;
}

bool DB::get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member,NetworkSummaryInfo &info)
{
	waitForReady();
	std::shared_ptr<_Network> nw;
	{
		std::lock_guard<std::mutex> l(_networks_l);
		auto nwi = _networks.find(networkId);
		if (nwi == _networks.end())
			return false;
		nw = nwi->second;
	}
	{
		std::lock_guard<std::mutex> l2(nw->lock);
		network = nw->config;
		_fillSummaryInfo(nw,info);
		auto m = nw->members.find(memberId);
		if (m == nw->members.end())
			return false;
		member = m->second;
	}
	return true;
}

bool DB::get(const uint64_t networkId,nlohmann::json &network,std::vector<nlohmann::json> &members)
{
	waitForReady();
	std::shared_ptr<_Network> nw;
	{
		std::lock_guard<std::mutex> l(_networks_l);
		auto nwi = _networks.find(networkId);
		if (nwi == _networks.end())
			return false;
		nw = nwi->second;
	}
	{
		std::lock_guard<std::mutex> l2(nw->lock);
		network = nw->config;
		for(auto m=nw->members.begin();m!=nw->members.end();++m)
			members.push_back(m->second);
	}
	return true;
}

bool DB::summary(const uint64_t networkId,NetworkSummaryInfo &info)
{
	waitForReady();
	std::shared_ptr<_Network> nw;
	{
		std::lock_guard<std::mutex> l(_networks_l);
		auto nwi = _networks.find(networkId);
		if (nwi == _networks.end())
			return false;
		nw = nwi->second;
	}
	{
		std::lock_guard<std::mutex> l2(nw->lock);
		_fillSummaryInfo(nw,info);
	}
	return true;
}

void DB::networks(std::vector<uint64_t> &networks)
{
	waitForReady();
	std::lock_guard<std::mutex> l(_networks_l);
	networks.reserve(_networks.size() + 1);
	for(auto n=_networks.begin();n!=_networks.end();++n)
		networks.push_back(n->first);
}

void DB::_memberChanged(nlohmann::json &old,nlohmann::json &member,bool push)
{
	uint64_t memberId = 0;
	uint64_t networkId = 0;
	bool isAuth = false;
	bool wasAuth = false;
	std::shared_ptr<_Network> nw;

	if (old.is_object()) {
		json &config = old["config"];
		if (config.is_object()) {
			memberId = OSUtils::jsonIntHex(config["id"],0ULL);
			networkId = OSUtils::jsonIntHex(config["nwid"],0ULL);
			if ((memberId)&&(networkId)) {
				{
					std::lock_guard<std::mutex> l(_networks_l);
					auto nw2 = _networks.find(networkId);
					if (nw2 != _networks.end())
						nw = nw2->second;
				}
				if (nw) {
					std::lock_guard<std::mutex> l(nw->lock);
					if (OSUtils::jsonBool(config["activeBridge"],false))
						nw->activeBridgeMembers.erase(memberId);
					wasAuth = OSUtils::jsonBool(config["authorized"],false);
					if (wasAuth)
						nw->authorizedMembers.erase(memberId);
					json &ips = config["ipAssignments"];
					if (ips.is_array()) {
						for(unsigned long i=0;i<ips.size();++i) {
							json &ipj = ips[i];
							if (ipj.is_string()) {
								const std::string ips = ipj;
								InetAddress ipa(ips.c_str());
								ipa.setPort(0);
								nw->allocatedIps.erase(ipa);
							}
						}
					}
				}
			}
		}
	}

	if (member.is_object()) {
		json &config = member["config"];
		if (config.is_object()) {
			if (!nw) {
				memberId = OSUtils::jsonIntHex(config["id"],0ULL);
				networkId = OSUtils::jsonIntHex(config["nwid"],0ULL);
				if ((!memberId)||(!networkId))
					return;
				std::lock_guard<std::mutex> l(_networks_l);
				std::shared_ptr<_Network> &nw2 = _networks[networkId];
				if (!nw2)
					nw2.reset(new _Network);
				nw = nw2;
			}

			{
				std::lock_guard<std::mutex> l(nw->lock);

				nw->members[memberId] = config;

				if (OSUtils::jsonBool(config["activeBridge"],false))
					nw->activeBridgeMembers.insert(memberId);
				isAuth = OSUtils::jsonBool(config["authorized"],false);
				if (isAuth)
					nw->authorizedMembers.insert(memberId);
				json &ips = config["ipAssignments"];
				if (ips.is_array()) {
					for(unsigned long i=0;i<ips.size();++i) {
						json &ipj = ips[i];
						if (ipj.is_string()) {
							const std::string ips = ipj;
							InetAddress ipa(ips.c_str());
							ipa.setPort(0);
							nw->allocatedIps.insert(ipa);
						}
					}
				}

				if (!isAuth) {
					const int64_t ldt = (int64_t)OSUtils::jsonInt(config["lastDeauthorizedTime"],0ULL);
					if (ldt > nw->mostRecentDeauthTime)
						nw->mostRecentDeauthTime = ldt;
				}
			}

			if (push)
				_controller->onNetworkMemberUpdate(networkId,memberId);
		}
	} else if (memberId) {
		if (nw) {
			std::lock_guard<std::mutex> l(nw->lock);
			nw->members.erase(memberId);
		}
		if (networkId) {
			std::lock_guard<std::mutex> l(_networks_l);
			auto er = _networkByMember.equal_range(memberId);
			for(auto i=er.first;i!=er.second;++i) {
				if (i->second == networkId) {
					_networkByMember.erase(i);
					break;
				}
			}
		}
	}

	if ((push)&&((wasAuth)&&(!isAuth)&&(networkId)&&(memberId)))
		_controller->onNetworkMemberDeauthorize(networkId,memberId);
}

void DB::_networkChanged(nlohmann::json &old,nlohmann::json &network,bool push)
{
	if (network.is_object()) {
		json &config = network["config"];
		if (config.is_object()) {
			const std::string ids = config["id"];
			const uint64_t id = Utils::hexStrToU64(ids.c_str());
			if (id) {
				std::shared_ptr<_Network> nw;
				{
					std::lock_guard<std::mutex> l(_networks_l);
					std::shared_ptr<_Network> &nw2 = _networks[id];
					if (!nw2)
						nw2.reset(new _Network);
					nw = nw2;
				}
				{
					std::lock_guard<std::mutex> l2(nw->lock);
					nw->config = config;
				}
				if (push)
					_controller->onNetworkUpdate(id);
			}
		}
	} else if (old.is_object()) {
		const std::string ids = old["id"];
		const uint64_t id = Utils::hexStrToU64(ids.c_str());
		if (id) {
			std::lock_guard<std::mutex> l(_networks_l);
			_networks.erase(id);
		}
	}
}

void DB::_fillSummaryInfo(const std::shared_ptr<_Network> &nw,NetworkSummaryInfo &info)
{
	for(auto ab=nw->activeBridgeMembers.begin();ab!=nw->activeBridgeMembers.end();++ab)
		info.activeBridges.push_back(Address(*ab));
	for(auto ip=nw->allocatedIps.begin();ip!=nw->allocatedIps.end();++ip)
		info.allocatedIps.push_back(*ip);
	info.authorizedMemberCount = (unsigned long)nw->authorizedMembers.size();
	info.totalMemberCount = (unsigned long)nw->members.size();
	info.mostRecentDeauthTime = nw->mostRecentDeauthTime;
}

} // namespace ZeroTier
