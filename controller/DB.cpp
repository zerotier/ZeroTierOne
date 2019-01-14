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

#include "DB.hpp"
#include "EmbeddedNetworkController.hpp"

#include <chrono>
#include <algorithm>
#include <stdexcept>

using json = nlohmann::json;

namespace ZeroTier {

void DB::initNetwork(nlohmann::json &network)
{
	if (!network.count("private")) network["private"] = true;
	if (!network.count("creationTime")) network["creationTime"] = OSUtils::now();
	if (!network.count("name")) network["name"] = "";
	if (!network.count("multicastLimit")) network["multicastLimit"] = (uint64_t)32;
	if (!network.count("enableBroadcast")) network["enableBroadcast"] = true;
	if (!network.count("v4AssignMode")) network["v4AssignMode"] = {{"zt",false}};
	if (!network.count("v6AssignMode")) network["v6AssignMode"] = {{"rfc4193",false},{"zt",false},{"6plane",false}};
	if (!network.count("authTokens")) network["authTokens"] = {{}};
	if (!network.count("capabilities")) network["capabilities"] = nlohmann::json::array();
	if (!network.count("tags")) network["tags"] = nlohmann::json::array();
	if (!network.count("routes")) network["routes"] = nlohmann::json::array();
	if (!network.count("ipAssignmentPools")) network["ipAssignmentPools"] = nlohmann::json::array();
	if (!network.count("mtu")) network["mtu"] = ZT_DEFAULT_MTU;
	if (!network.count("remoteTraceTarget")) network["remoteTraceTarget"] = nlohmann::json();
	if (!network.count("removeTraceLevel")) network["remoteTraceLevel"] = 0;
	if (!network.count("rules")) {
		// If unspecified, rules are set to allow anything and behave like a flat L2 segment
		network["rules"] = {{
			{ "not",false },
			{ "or", false },
			{ "type","ACTION_ACCEPT" }
		}};
	}
	network["objtype"] = "network";
}

void DB::initMember(nlohmann::json &member)
{
	if (!member.count("authorized")) member["authorized"] = false;
	if (!member.count("ipAssignments")) member["ipAssignments"] = nlohmann::json::array();
	if (!member.count("activeBridge")) member["activeBridge"] = false;
	if (!member.count("tags")) member["tags"] = nlohmann::json::array();
	if (!member.count("capabilities")) member["capabilities"] = nlohmann::json::array();
	if (!member.count("creationTime")) member["creationTime"] = OSUtils::now();
	if (!member.count("noAutoAssignIps")) member["noAutoAssignIps"] = false;
	if (!member.count("revision")) member["revision"] = 0ULL;
	if (!member.count("lastDeauthorizedTime")) member["lastDeauthorizedTime"] = 0ULL;
	if (!member.count("lastAuthorizedTime")) member["lastAuthorizedTime"] = 0ULL;
	if (!member.count("lastAuthorizedCredentialType")) member["lastAuthorizedCredentialType"] = nlohmann::json();
	if (!member.count("lastAuthorizedCredential")) member["lastAuthorizedCredential"] = nlohmann::json();
	if (!member.count("vMajor")) member["vMajor"] = -1;
	if (!member.count("vMinor")) member["vMinor"] = -1;
	if (!member.count("vRev")) member["vRev"] = -1;
	if (!member.count("vProto")) member["vProto"] = -1;
	if (!member.count("remoteTraceTarget")) member["remoteTraceTarget"] = nlohmann::json();
	if (!member.count("removeTraceLevel")) member["remoteTraceLevel"] = 0;
	member["objtype"] = "member";
}

void DB::cleanNetwork(nlohmann::json &network)
{
	network.erase("clock");
	network.erase("authorizedMemberCount");
	network.erase("activeMemberCount");
	network.erase("totalMemberCount");
	network.erase("lastModified");
}

void DB::cleanMember(nlohmann::json &member)
{
	member.erase("clock");
	member.erase("physicalAddr");
	member.erase("recentLog");
	member.erase("lastModified");
	member.erase("lastRequestMetaData");
}

DB::DB(EmbeddedNetworkController *const nc,const Identity &myId,const char *path) :
	_controller(nc),
	_myId(myId),
	_myAddress(myId.address()),
	_path((path) ? path : "")
{
	char tmp[32];
	_myAddress.toString(tmp);
	_myAddressStr = tmp;
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

void DB::_memberChanged(nlohmann::json &old,nlohmann::json &memberConfig,bool push)
{
	uint64_t memberId = 0;
	uint64_t networkId = 0;
	bool isAuth = false;
	bool wasAuth = false;
	std::shared_ptr<_Network> nw;

	if (old.is_object()) {
		memberId = OSUtils::jsonIntHex(old["id"],0ULL);
		networkId = OSUtils::jsonIntHex(old["nwid"],0ULL);
		if ((memberId)&&(networkId)) {
			{
				std::lock_guard<std::mutex> l(_networks_l);
				auto nw2 = _networks.find(networkId);
				if (nw2 != _networks.end())
					nw = nw2->second;
			}
			if (nw) {
				std::lock_guard<std::mutex> l(nw->lock);
				if (OSUtils::jsonBool(old["activeBridge"],false))
					nw->activeBridgeMembers.erase(memberId);
				wasAuth = OSUtils::jsonBool(old["authorized"],false);
				if (wasAuth)
					nw->authorizedMembers.erase(memberId);
				json &ips = old["ipAssignments"];
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

	if (memberConfig.is_object()) {
		if (!nw) {
			memberId = OSUtils::jsonIntHex(memberConfig["id"],0ULL);
			networkId = OSUtils::jsonIntHex(memberConfig["nwid"],0ULL);
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

			nw->members[memberId] = memberConfig;

			if (OSUtils::jsonBool(memberConfig["activeBridge"],false))
				nw->activeBridgeMembers.insert(memberId);
			isAuth = OSUtils::jsonBool(memberConfig["authorized"],false);
			if (isAuth)
				nw->authorizedMembers.insert(memberId);
			json &ips = memberConfig["ipAssignments"];
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
				const int64_t ldt = (int64_t)OSUtils::jsonInt(memberConfig["lastDeauthorizedTime"],0ULL);
				if (ldt > nw->mostRecentDeauthTime)
					nw->mostRecentDeauthTime = ldt;
			}
		}

		if (push)
			_controller->onNetworkMemberUpdate(networkId,memberId);
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

void DB::_networkChanged(nlohmann::json &old,nlohmann::json &networkConfig,bool push)
{
	if (networkConfig.is_object()) {
		const std::string ids = networkConfig["id"];
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
				nw->config = networkConfig;
			}
			if (push)
				_controller->onNetworkUpdate(id);
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
	std::sort(info.activeBridges.begin(),info.activeBridges.end());
	for(auto ip=nw->allocatedIps.begin();ip!=nw->allocatedIps.end();++ip)
		info.allocatedIps.push_back(*ip);
	std::sort(info.allocatedIps.begin(),info.allocatedIps.end());
	info.authorizedMemberCount = (unsigned long)nw->authorizedMembers.size();
	info.totalMemberCount = (unsigned long)nw->members.size();
	info.mostRecentDeauthTime = nw->mostRecentDeauthTime;
}

} // namespace ZeroTier
