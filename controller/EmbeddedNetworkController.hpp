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

#include <nlohmann/json.hpp>

#include <cpp-httplib/httplib.h>

#include "DB.hpp"
#include "DBMirrorSet.hpp"

namespace ZeroTier {

class Node;
struct RedisConfig;

class EmbeddedNetworkController : public NetworkController,public DB::ChangeListener
{
public:
	/**
	 * @param node Parent node
	 * @param dbPath Database path (file path or database credentials)
	 */
	EmbeddedNetworkController(Node *node,const char *ztPath,const char *dbPath, int listenPort, RedisConfig *rc);
	virtual ~EmbeddedNetworkController();

	virtual void init(const Identity &signingId,Sender *sender);

	void setSSORedirectURL(const std::string &url);

	virtual void request(
		uint64_t nwid,
		const InetAddress &fromAddr,
		uint64_t requestPacketId,
		const Identity &identity,
		const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> &metaData);

	void configureHTTPControlPlane(
		httplib::Server &s,
		const std::function<void(const httplib::Request&, httplib::Response&, std::string)>);

	void handleRemoteTrace(const ZT_RemoteTrace &rt);

	virtual void onNetworkUpdate(const void *db,uint64_t networkId,const nlohmann::json &network);
	virtual void onNetworkMemberUpdate(const void *db,uint64_t networkId,uint64_t memberId,const nlohmann::json &member);
	virtual void onNetworkMemberDeauthorize(const void *db,uint64_t networkId,uint64_t memberId);

private:
	void _request(uint64_t nwid,const InetAddress &fromAddr,uint64_t requestPacketId,const Identity &identity,const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> &metaData);
	void _startThreads();
	void _ssoExpiryThread();

	std::string networkUpdateFromPostData(uint64_t networkID, const std::string &body);

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
		inline bool operator<(const _MemberStatusKey &k) const { return (k.networkId < networkId) || ((k.networkId == networkId)&&(k.nodeId < nodeId)); }
	};
	struct _MemberStatus
	{
		_MemberStatus() : lastRequestTime(0),authenticationExpiryTime(-1),vMajor(-1),vMinor(-1),vRev(-1),vProto(-1) {}
		int64_t lastRequestTime;
		int64_t authenticationExpiryTime;
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
	std::string _ztPath;
	std::string _path;
	Identity _signingId;
	std::string _signingIdAddressString;
	NetworkController::Sender *_sender;

	DBMirrorSet _db;
	BlockingQueue< _RQEntry * > _queue;

	std::vector<std::thread> _threads;
	std::mutex _threads_l;

	bool _ssoExpiryRunning;
	std::thread _ssoExpiry;

	std::unordered_map< _MemberStatusKey,_MemberStatus,_MemberStatusHash > _memberStatus;
	std::mutex _memberStatus_l;

	std::set< std::pair<int64_t, _MemberStatusKey> > _expiringSoon;
	std::mutex _expiringSoon_l;

	RedisConfig *_rc;
	std::string _ssoRedirectURL;

#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	prometheus::simpleapi::benchmark_family_t _member_status_lookup;
	prometheus::simpleapi::counter_family_t   _member_status_lookup_count;
	prometheus::simpleapi::benchmark_family_t _node_is_online;
	prometheus::simpleapi::counter_family_t   _node_is_online_count;
	prometheus::simpleapi::benchmark_family_t _get_and_init_member;
	prometheus::simpleapi::counter_family_t   _get_and_init_member_count;
	prometheus::simpleapi::benchmark_family_t _have_identity;
	prometheus::simpleapi::counter_family_t   _have_identity_count;
	prometheus::simpleapi::benchmark_family_t _determine_auth;
	prometheus::simpleapi::counter_family_t   _determine_auth_count;
	prometheus::simpleapi::benchmark_family_t _sso_check;
	prometheus::simpleapi::counter_family_t   _sso_check_count;
	prometheus::simpleapi::benchmark_family_t _auth_check;
	prometheus::simpleapi::counter_family_t   _auth_check_count;
	prometheus::simpleapi::benchmark_family_t _json_schlep;
	prometheus::simpleapi::counter_family_t   _json_schlep_count;
	prometheus::simpleapi::benchmark_family_t _issue_certificate;
	prometheus::simpleapi::counter_family_t   _issue_certificate_count;
	prometheus::simpleapi::benchmark_family_t _save_member;
	prometheus::simpleapi::counter_family_t   _save_member_count;
	prometheus::simpleapi::benchmark_family_t _send_netconf;
	prometheus::simpleapi::counter_family_t   _send_netconf_count;
#endif
};

} // namespace ZeroTier

#endif
