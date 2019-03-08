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

#ifndef ZT_SQLITENETWORKCONTROLLER_HPP
#define ZT_SQLITENETWORKCONTROLLER_HPP

#include <stdint.h>

#include <string>
#include <map>
#include <vector>
#include <set>
#include <list>
#include <thread>
#include <unordered_map>
#include <atomic>

#include "../node/Constants.hpp"
#include "../node/NetworkController.hpp"
#include "../node/Utils.hpp"
#include "../node/Address.hpp"
#include "../node/InetAddress.hpp"

#include "../osdep/OSUtils.hpp"
#include "../osdep/Thread.hpp"
#include "../osdep/BlockingQueue.hpp"

#include "../ext/json/json.hpp"

#include "DB.hpp"
#include "FileDB.hpp"
#ifdef ZT_CONTROLLER_USE_LIBPQ
#include "PostgreSQL.hpp"
#endif

namespace ZeroTier {

class Node;

struct MQConfig;

class EmbeddedNetworkController : public NetworkController
{
public:
	/**
	 * @param node Parent node
	 * @param dbPath Database path (file path or database credentials)
	 */
	EmbeddedNetworkController(Node *node,const char *dbPath, int listenPort, MQConfig *mqc = NULL);
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

	void handleRemoteTrace(const ZT_RemoteTrace &rt);

	// Called on update via POST or by JSONDB on external update of network or network member records
	void onNetworkUpdate(const uint64_t networkId);
	void onNetworkMemberUpdate(const uint64_t networkId,const uint64_t memberId);
	void onNetworkMemberDeauthorize(const uint64_t networkId,const uint64_t memberId);

private:
	void _request(uint64_t nwid,const InetAddress &fromAddr,uint64_t requestPacketId,const Identity &identity,const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> &metaData);
	void _startThreads();

	struct _RQEntry
	{
		uint64_t nwid;
		uint64_t requestPacketId;
		InetAddress fromAddr;
		Identity identity;
		Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> metaData;
		enum {
			RQENTRY_TYPE_REQUEST = 0
		} type;
	};
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
		inline bool online(const int64_t now) const { return ((now - lastRequestTime) < (ZT_NETWORK_AUTOCONF_DELAY * 2)); }
	};
	struct _MemberStatusHash
	{
		inline std::size_t operator()(const _MemberStatusKey &networkIdNodeId) const
		{
			return (std::size_t)(networkIdNodeId.networkId + networkIdNodeId.nodeId);
		}
	};

	const int64_t _startTime;
	int _listenPort;
	Node *const _node;
	std::string _path;
	Identity _signingId;
	std::string _signingIdAddressString;
	NetworkController::Sender *_sender;

	std::unique_ptr<DB> _db;
	BlockingQueue< _RQEntry * > _queue;

	std::vector<std::thread> _threads;
	std::mutex _threads_l;

	std::unordered_map< _MemberStatusKey,_MemberStatus,_MemberStatusHash > _memberStatus;
	std::mutex _memberStatus_l;

	MQConfig *_mqc;
};

} // namespace ZeroTier

#endif
