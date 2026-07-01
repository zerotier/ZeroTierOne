/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#ifndef ZT_SQLITENETWORKCONTROLLER_HPP
#define ZT_SQLITENETWORKCONTROLLER_HPP

#include "../../node/Constants.hpp"
#include "../../node/InetAddress.hpp"
#include "../../node/NetworkController.hpp"
#include "../../osdep/BlockingQueue.hpp"
#include "DB.hpp"
#include "DBMirrorSet.hpp"

#ifdef CMAKE_BUILD
#include <httplib.h>
#else
#include <cpp-httplib/httplib.h>
#endif
#include <array>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>
#include <set>
#include <stdint.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ZeroTier {

class Node;
struct RedisConfig;

#ifdef ZT1_CENTRAL_CONTROLLER
class ControllerConfig;
#endif

class EmbeddedNetworkController
	: public NetworkController
	, public DB::ChangeListener {
  public:
	/**
	 * @param node Parent node
	 * @param dbPath Database path (file path or database credentials)
	 */
	EmbeddedNetworkController(Node* node, const char* ztPath, const char* dbPath, int listenPort, RedisConfig* rc);
#ifdef ZT1_CENTRAL_CONTROLLER
	EmbeddedNetworkController(
		Node* node,
		const char* ztPath,
		const char* dbPath,
		int listenPort,
		const ControllerConfig* cc);
#endif
	virtual ~EmbeddedNetworkController();

	virtual void init(const Identity& signingId, Sender* sender);

	void setSSORedirectURL(const std::string& url);

	virtual void request(
		uint64_t nwid,
		const InetAddress& fromAddr,
		uint64_t requestPacketId,
		const Identity& identity,
		const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY>& metaData);

	void configureHTTPControlPlane(
		httplib::Server& s,
		httplib::Server& sV6,
		const std::function<void(const httplib::Request&, httplib::Response&, std::string)>);

	void handleRemoteTrace(const ZT_RemoteTrace& rt);

	virtual void onNetworkUpdate(const void* db, uint64_t networkId, const nlohmann::json& network);
	virtual void
	onNetworkMemberUpdate(const void* db, uint64_t networkId, uint64_t memberId, const nlohmann::json& member);
	virtual void onNetworkMemberDeauthorize(const void* db, uint64_t networkId, uint64_t memberId);

  private:
	void _request(
		uint64_t nwid,
		const InetAddress& fromAddr,
		uint64_t requestPacketId,
		const Identity& identity,
		const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY>& metaData);
	void _startThreads();
	void _ssoExpiryThread();

	std::string networkUpdateFromPostData(uint64_t networkID, const std::string& body);

	struct _RQEntry {
		uint64_t nwid;
		uint64_t requestPacketId;
		InetAddress fromAddr;
		Identity identity;
		Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> metaData;
		enum { RQENTRY_TYPE_REQUEST = 0 } type;
	};

	struct _MemberStatusKey {
		_MemberStatusKey() : networkId(0), nodeId(0)
		{
		}
		_MemberStatusKey(const uint64_t nwid, const uint64_t nid) : networkId(nwid), nodeId(nid)
		{
		}
		uint64_t networkId;
		uint64_t nodeId;
		inline bool operator==(const _MemberStatusKey& k) const
		{
			return ((k.networkId == networkId) && (k.nodeId == nodeId));
		}
		inline bool operator<(const _MemberStatusKey& k) const
		{
			return (k.networkId < networkId) || ((k.networkId == networkId) && (k.nodeId < nodeId));
		}
	};
	struct _MemberStatus {
		_MemberStatus() : lastRequestTime(0), authenticationExpiryTime(-1), vMajor(-1), vMinor(-1), vRev(-1), vProto(-1)
		{
		}
		int64_t lastRequestTime;
		int64_t authenticationExpiryTime;
		int vMajor, vMinor, vRev, vProto;
		Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> lastRequestMetaData;
		Identity identity;
		inline bool online(const int64_t now) const
		{
			return ((now - lastRequestTime) < (ZT_NETWORK_AUTOCONF_DELAY * 2));
		}
	};
	struct _MemberStatusHash {
		inline std::size_t operator()(const _MemberStatusKey& networkIdNodeId) const
		{
			return (std::size_t)(networkIdNodeId.networkId + networkIdNodeId.nodeId);
		}
	};

	const int64_t _startTime;
	int _listenPort;
	Node* const _node;
	std::string _ztPath;
	std::string _path;
	Identity _signingId;
	std::string _signingIdAddressString;
	NetworkController::Sender* _sender;

	DBMirrorSet _db;
	BlockingQueue<_RQEntry*> _queue;

	std::vector<std::thread> _threads;
	std::mutex _threads_l;

	std::unordered_map<_MemberStatusKey, _MemberStatus, _MemberStatusHash> _memberStatus;
	std::mutex _memberStatus_l;

	// At-most-one queued config request per (network, member): a reconnect herd (or a member
	// re-requesting while an earlier request is still queued) would otherwise stack duplicate
	// _RQEntry (each ~1 KB). Inserted in request(), erased when a worker dequeues the entry.
	std::unordered_set<_MemberStatusKey, _MemberStatusHash> _pendingRequests;
	std::mutex _pendingRequests_l;

	std::set<std::pair<int64_t, _MemberStatusKey> > _expiringSoon;
	std::mutex _expiringSoon_l;

	// Sharded per-member locks: serialize the read-modify-write of a single member's
	// record in _request() so concurrent requests for the same member can't clobber
	// each other's auth/IP-assignment changes. Different members hash to different
	// shards, so unrelated members don't contend.
	static constexpr std::size_t MEMBER_LOCK_SHARDS = 256;
	std::array<std::mutex, MEMBER_LOCK_SHARDS> _memberLocks;
	std::mutex& _memberLock(uint64_t nwid, uint64_t nodeId)
	{ return _memberLocks[(nwid ^ nodeId) % MEMBER_LOCK_SHARDS]; }

	// Set in the destructor so an in-flight request() returns early instead of
	// contending for _threads_l (held across the worker joins) or queuing new work.
	std::atomic<bool> _shuttingDown { false };

	RedisConfig* _rc;
#ifdef ZT1_CENTRAL_CONTROLLER
	const ControllerConfig* _cc;
#endif
	std::string _ssoRedirectURL;

	std::atomic<bool> _ssoExpiryRunning;
	std::thread _ssoExpiry;

#ifdef CENTRAL_CONTROLLER_REQUEST_BENCHMARK
	prometheus::simpleapi::benchmark_family_t _member_status_lookup;
	prometheus::simpleapi::counter_family_t _member_status_lookup_count;
	prometheus::simpleapi::benchmark_family_t _node_is_online;
	prometheus::simpleapi::counter_family_t _node_is_online_count;
	prometheus::simpleapi::benchmark_family_t _get_and_init_member;
	prometheus::simpleapi::counter_family_t _get_and_init_member_count;
	prometheus::simpleapi::benchmark_family_t _have_identity;
	prometheus::simpleapi::counter_family_t _have_identity_count;
	prometheus::simpleapi::benchmark_family_t _determine_auth;
	prometheus::simpleapi::counter_family_t _determine_auth_count;
	prometheus::simpleapi::benchmark_family_t _sso_check;
	prometheus::simpleapi::counter_family_t _sso_check_count;
	prometheus::simpleapi::benchmark_family_t _auth_check;
	prometheus::simpleapi::counter_family_t _auth_check_count;
	prometheus::simpleapi::benchmark_family_t _json_schlep;
	prometheus::simpleapi::counter_family_t _json_schlep_count;
	prometheus::simpleapi::benchmark_family_t _issue_certificate;
	prometheus::simpleapi::counter_family_t _issue_certificate_count;
	prometheus::simpleapi::benchmark_family_t _save_member;
	prometheus::simpleapi::counter_family_t _save_member_count;
	prometheus::simpleapi::benchmark_family_t _send_netconf;
	prometheus::simpleapi::counter_family_t _send_netconf_count;
#endif
};

}	// namespace ZeroTier

#endif
