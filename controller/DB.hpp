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

#ifndef ZT_CONTROLLER_DB_HPP
#define ZT_CONTROLLER_DB_HPP

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

#include "../ext/json/json.hpp"

namespace ZeroTier
{

class EmbeddedNetworkController;

/**
 * Base class with common infrastructure for all controller DB implementations
 */
class DB
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

	/**
	 * Ensure that all network fields are present
	 */
	static void initNetwork(nlohmann::json &network);

	/**
	 * Ensure that all member fields are present
	 */
	static void initMember(nlohmann::json &member);

	/**
	 * Remove old and temporary network fields
	 */
	static void cleanNetwork(nlohmann::json &network);

	/**
	 * Remove old and temporary member fields
	 */
	static void cleanMember(nlohmann::json &member);

	DB(EmbeddedNetworkController *const nc,const Identity &myId,const char *path);
	virtual ~DB();

	virtual bool waitForReady() = 0;
	virtual bool isReady() = 0;

	inline bool hasNetwork(const uint64_t networkId) const
	{
		std::lock_guard<std::mutex> l(_networks_l);
		return (_networks.find(networkId) != _networks.end());
	}

	bool get(const uint64_t networkId,nlohmann::json &network);
	bool get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member);
	bool get(const uint64_t networkId,nlohmann::json &network,const uint64_t memberId,nlohmann::json &member,NetworkSummaryInfo &info);
	bool get(const uint64_t networkId,nlohmann::json &network,std::vector<nlohmann::json> &members);

	bool summary(const uint64_t networkId,NetworkSummaryInfo &info);

	void networks(std::vector<uint64_t> &networks);

	virtual void save(nlohmann::json *orig,nlohmann::json &record) = 0;

	virtual void eraseNetwork(const uint64_t networkId) = 0;

	virtual void eraseMember(const uint64_t networkId,const uint64_t memberId) = 0;

	virtual void nodeIsOnline(const uint64_t networkId,const uint64_t memberId,const InetAddress &physicalAddress) = 0;

protected:
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

	void _memberChanged(nlohmann::json &old,nlohmann::json &memberConfig,bool push);
	void _networkChanged(nlohmann::json &old,nlohmann::json &networkConfig,bool push);
	void _fillSummaryInfo(const std::shared_ptr<_Network> &nw,NetworkSummaryInfo &info);

	EmbeddedNetworkController *const _controller;
	const Identity _myId;
	const Address _myAddress;
	const std::string _path;
	std::string _myAddressStr;

	std::unordered_map< uint64_t,std::shared_ptr<_Network> > _networks;
	std::unordered_multimap< uint64_t,uint64_t > _networkByMember;
	mutable std::mutex _networks_l;
};

} // namespace ZeroTier

#endif
