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

#ifndef ZT_JSONDB_HPP
#define ZT_JSONDB_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <map>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <unordered_map>

#include "../node/Constants.hpp"
#include "../node/Utils.hpp"
#include "../node/InetAddress.hpp"
#include "../node/Mutex.hpp"
#include "../ext/json/json.hpp"
#include "../osdep/OSUtils.hpp"
#include "../osdep/Http.hpp"
#include "../osdep/Thread.hpp"

namespace ZeroTier {

/**
 * Hierarchical JSON store that persists into the filesystem or via HTTP
 */
class JSONDB
{
public:
	struct NetworkSummaryInfo
	{
		NetworkSummaryInfo() : authorizedMemberCount(0),activeMemberCount(0),totalMemberCount(0),mostRecentDeauthTime(0) {}
		std::vector<Address> activeBridges;
		std::vector<InetAddress> allocatedIps;
		unsigned long authorizedMemberCount;
		unsigned long activeMemberCount;
		unsigned long totalMemberCount;
		uint64_t mostRecentDeauthTime;
	};

	JSONDB(const std::string &basePath);
	~JSONDB();

	bool writeRaw(const std::string &n,const std::string &obj);

	inline bool hasNetwork(const uint64_t networkId) const
	{
		Mutex::Lock _l(_networks_m);
		std::unordered_map<uint64_t,_NW>::const_iterator i(_networks.find(networkId));
		return (i != _networks.end());
	}

	inline bool getNetwork(const uint64_t networkId,nlohmann::json &config) const
	{
		Mutex::Lock _l(_networks_m);
		std::unordered_map<uint64_t,_NW>::const_iterator i(_networks.find(networkId));
		if (i == _networks.end())
			return false;
		config = i->second.config;
		return true;
	}

	inline bool getNetworkSummaryInfo(const uint64_t networkId,NetworkSummaryInfo &ns) const
	{
		Mutex::Lock _l(_networks_m);
		std::unordered_map<uint64_t,_NW>::const_iterator i(_networks.find(networkId));
		if (i == _networks.end())
			return false;
		ns = i->second.summaryInfo;
		return true;
	}

	/**
	 * @return Bit mask: 0 == none, 1 == network only, 3 == network and member
	 */
	inline int getNetworkAndMember(const uint64_t networkId,const uint64_t nodeId,nlohmann::json &networkConfig,nlohmann::json &memberConfig,NetworkSummaryInfo &ns) const
	{
		Mutex::Lock _l(_networks_m);
		std::unordered_map<uint64_t,_NW>::const_iterator i(_networks.find(networkId));
		if (i == _networks.end())
			return 0;
		networkConfig = i->second.config;
		ns = i->second.summaryInfo;
		std::unordered_map<uint64_t,nlohmann::json>::const_iterator j(i->second.members.find(nodeId));
		if (j == i->second.members.end())
			return 1;
		memberConfig = j->second;
		return 3;
	}

	inline bool getNetworkMember(const uint64_t networkId,const uint64_t nodeId,nlohmann::json &memberConfig) const
	{
		Mutex::Lock _l(_networks_m);
		std::unordered_map<uint64_t,_NW>::const_iterator i(_networks.find(networkId));
		if (i == _networks.end())
			return false;
		std::unordered_map<uint64_t,nlohmann::json>::const_iterator j(i->second.members.find(nodeId));
		if (j == i->second.members.end())
			return false;
		memberConfig = j->second;
		return true;
	}

	void saveNetwork(const uint64_t networkId,const nlohmann::json &networkConfig);

	void saveNetworkMember(const uint64_t networkId,const uint64_t nodeId,const nlohmann::json &memberConfig);

	nlohmann::json eraseNetwork(const uint64_t networkId);

	nlohmann::json eraseNetworkMember(const uint64_t networkId,const uint64_t nodeId,bool recomputeSummaryInfo = true);

	std::vector<uint64_t> networkIds() const
	{
		std::vector<uint64_t> r;
		Mutex::Lock _l(_networks_m);
		for(std::unordered_map<uint64_t,_NW>::const_iterator n(_networks.begin());n!=_networks.end();++n)
			r.push_back(n->first);
		return r;
	}

	template<typename F>
	inline void eachMember(const uint64_t networkId,F func)
	{
		Mutex::Lock _l(_networks_m);
		std::unordered_map<uint64_t,_NW>::const_iterator i(_networks.find(networkId));
		if (i != _networks.end()) {
			for(std::unordered_map<uint64_t,nlohmann::json>::const_iterator m(i->second.members.begin());m!=i->second.members.end();++m) {
				try {
					func(networkId,m->first,m->second);
				} catch ( ... ) {}
			}
		}
	}

	void threadMain()
		throw();

private:
	bool _load(const std::string &p);
	void _recomputeSummaryInfo(const uint64_t networkId);
	std::string _genPath(const std::string &n,bool create);

	std::string _basePath;
	InetAddress _httpAddr;

	Thread _summaryThread;
	std::vector<uint64_t> _summaryThreadToDo;
	volatile bool _summaryThreadRun;
	Mutex _summaryThread_m;

	struct _NW
	{
		_NW() : summaryInfoLastComputed(0) {}
		nlohmann::json config;
		NetworkSummaryInfo summaryInfo;
		uint64_t summaryInfoLastComputed;
		std::unordered_map<uint64_t,nlohmann::json> members;
	};

	std::unordered_map<uint64_t,_NW> _networks;
	Mutex _networks_m;
};

} // namespace ZeroTier

#endif
