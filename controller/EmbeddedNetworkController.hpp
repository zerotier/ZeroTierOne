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

#ifndef ZT_SQLITENETWORKCONTROLLER_HPP
#define ZT_SQLITENETWORKCONTROLLER_HPP

#include <stdint.h>

#include <string>
#include <map>
#include <vector>
#include <set>
#include <list>
#include <thread>

#include "../node/Constants.hpp"

#include "../node/NetworkController.hpp"
#include "../node/Mutex.hpp"
#include "../node/Utils.hpp"
#include "../node/Address.hpp"
#include "../node/InetAddress.hpp"
#include "../node/NonCopyable.hpp"

#include "../osdep/OSUtils.hpp"
#include "../osdep/Thread.hpp"
#include "../osdep/BlockingQueue.hpp"

#include "../ext/json/json.hpp"

#include "JSONDB.hpp"

namespace ZeroTier {

class Node;

class EmbeddedNetworkController : public NetworkController,NonCopyable
{
public:
	/**
	 * @param node Parent node
	 * @param dbPath Path to store data
	 */
	EmbeddedNetworkController(Node *node,const char *dbPath);
	virtual ~EmbeddedNetworkController();

	virtual void init(const Identity &signingId,Sender *sender);

	virtual void request(
		uint64_t nwid,
		const InetAddress &fromAddr,
		uint64_t requestPacketId,
		const Identity &identity,
		const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> &metaData);

	unsigned int handleControlPlaneHttpGET(
		const std::vector<std::string> &path,
		const std::map<std::string,std::string> &urlArgs,
		const std::map<std::string,std::string> &headers,
		const std::string &body,
		std::string &responseBody,
		std::string &responseContentType);
	unsigned int handleControlPlaneHttpPOST(
		const std::vector<std::string> &path,
		const std::map<std::string,std::string> &urlArgs,
		const std::map<std::string,std::string> &headers,
		const std::string &body,
		std::string &responseBody,
		std::string &responseContentType);
	unsigned int handleControlPlaneHttpDELETE(
		const std::vector<std::string> &path,
		const std::map<std::string,std::string> &urlArgs,
		const std::map<std::string,std::string> &headers,
		const std::string &body,
		std::string &responseBody,
		std::string &responseContentType);

	void threadMain()
		throw();

private:
	struct _RQEntry
	{
		uint64_t nwid;
		uint64_t requestPacketId;
		InetAddress fromAddr;
		Identity identity;
		Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> metaData;
		enum {
			RQENTRY_TYPE_REQUEST = 0,
			RQENTRY_TYPE_PING = 1
		} type;
	};

	void _request(uint64_t nwid,const InetAddress &fromAddr,uint64_t requestPacketId,const Identity &identity,const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> &metaData);

	inline void _startThreads()
	{
		Mutex::Lock _l(_threads_m);
		if (_threads.size() == 0) {
			long hwc = (long)std::thread::hardware_concurrency();
			if (hwc < 1)
				hwc = 1;
			else if (hwc > 16)
				hwc = 16;
			for(long i=0;i<hwc;++i)
				_threads.push_back(Thread::start(this));
		}
	}

	// These init objects with default and static/informational fields
	inline void _initMember(nlohmann::json &member)
	{
		if (!member.count("authorized")) member["authorized"] = false;
		if (!member.count("authHistory")) member["authHistory"] = nlohmann::json::array();
 		if (!member.count("ipAssignments")) member["ipAssignments"] = nlohmann::json::array();
		if (!member.count("activeBridge")) member["activeBridge"] = false;
		if (!member.count("tags")) member["tags"] = nlohmann::json::array();
		if (!member.count("capabilities")) member["capabilities"] = nlohmann::json::array();
		if (!member.count("creationTime")) member["creationTime"] = OSUtils::now();
		if (!member.count("noAutoAssignIps")) member["noAutoAssignIps"] = false;
		if (!member.count("revision")) member["revision"] = 0ULL;
		if (!member.count("lastDeauthorizedTime")) member["lastDeauthorizedTime"] = 0ULL;
		if (!member.count("lastAuthorizedTime")) member["lastAuthorizedTime"] = 0ULL;
		if (!member.count("vMajor")) member["vMajor"] = -1;
		if (!member.count("vMinor")) member["vMinor"] = -1;
		if (!member.count("vRev")) member["vRev"] = -1;
		if (!member.count("vProto")) member["vProto"] = -1;
		if (!member.count("physicalAddr")) member["physicalAddr"] = nlohmann::json();
		member["objtype"] = "member";
	}
	inline void _initNetwork(nlohmann::json &network)
	{
		if (!network.count("private")) network["private"] = true;
		if (!network.count("creationTime")) network["creationTime"] = OSUtils::now();
		if (!network.count("name")) network["name"] = "";
		if (!network.count("multicastLimit")) network["multicastLimit"] = (uint64_t)32;
		if (!network.count("enableBroadcast")) network["enableBroadcast"] = true;
		if (!network.count("v4AssignMode")) network["v4AssignMode"] = {{"zt",false}};
		if (!network.count("v6AssignMode")) network["v6AssignMode"] = {{"rfc4193",false},{"zt",false},{"6plane",false}};
		if (!network.count("authTokens")) network["authTokens"] = nlohmann::json::array();
		if (!network.count("capabilities")) network["capabilities"] = nlohmann::json::array();
		if (!network.count("tags")) network["tags"] = nlohmann::json::array();
		if (!network.count("routes")) network["routes"] = nlohmann::json::array();
		if (!network.count("ipAssignmentPools")) network["ipAssignmentPools"] = nlohmann::json::array();
		if (!network.count("mtu")) network["mtu"] = ZT_DEFAULT_MTU;
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
	inline void _addNetworkNonPersistedFields(nlohmann::json &network,uint64_t now,const JSONDB::NetworkSummaryInfo &ns)
	{
		network["clock"] = now;
		network["authorizedMemberCount"] = ns.authorizedMemberCount;
		network["activeMemberCount"] = ns.activeMemberCount;
		network["totalMemberCount"] = ns.totalMemberCount;
	}
	inline void _removeNetworkNonPersistedFields(nlohmann::json &network)
	{
		network.erase("clock");
		network.erase("authorizedMemberCount");
		network.erase("activeMemberCount");
		network.erase("totalMemberCount");
		// legacy fields
		network.erase("lastModified");
	}
	inline void _addMemberNonPersistedFields(uint64_t nwid,uint64_t nodeId,nlohmann::json &member,uint64_t now)
	{
		member["clock"] = now;
		Mutex::Lock _l(_memberStatus_m);
		member["online"] = _memberStatus[_MemberStatusKey(nwid,nodeId)].online(now);
	}
	inline void _removeMemberNonPersistedFields(nlohmann::json &member)
	{
		member.erase("clock");
		// legacy fields
		member.erase("recentLog");
		member.erase("lastModified");
		member.erase("lastRequestMetaData");
	}

	const uint64_t _startTime;

	volatile bool _running;
	BlockingQueue<_RQEntry *> _queue;
	std::vector<Thread> _threads;
	Mutex _threads_m;

	JSONDB _db;

	Node *const _node;
	std::string _path;

	NetworkController::Sender *_sender;
	Identity _signingId;

	struct _MemberStatusKey
	{
		_MemberStatusKey() : networkId(0),nodeId(0) {}
		_MemberStatusKey(const uint64_t nwid,const uint64_t nid) : networkId(nwid),nodeId(nid) {}
		uint64_t networkId;
		uint64_t nodeId;
		inline bool operator==(const _MemberStatusKey &k) const { return ((k.networkId == networkId)&&(k.nodeId == nodeId)); }
	};
	struct _MemberStatus
	{
		_MemberStatus() : lastRequestTime(0),vMajor(-1),vMinor(-1),vRev(-1),vProto(-1) {}
		uint64_t lastRequestTime;
		int vMajor,vMinor,vRev,vProto;
		Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> lastRequestMetaData;
		Identity identity;
		InetAddress physicalAddr; // last known physical address
		inline bool online(const uint64_t now) const { return ((now - lastRequestTime) < (ZT_NETWORK_AUTOCONF_DELAY * 2)); }
	};
	struct _MemberStatusHash
	{
		inline std::size_t operator()(const _MemberStatusKey &networkIdNodeId) const
		{
			return (std::size_t)(networkIdNodeId.networkId + networkIdNodeId.nodeId);
		}
	};
	std::unordered_map< _MemberStatusKey,_MemberStatus,_MemberStatusHash > _memberStatus;
	Mutex _memberStatus_m;
};

} // namespace ZeroTier

#endif
