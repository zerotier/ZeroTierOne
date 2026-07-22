/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#ifndef ZT_CONTROLLER_DB_HPP
#define ZT_CONTROLLER_DB_HPP

// #define ZT_CONTROLLER_USE_LIBPQ

#include "../../node/Constants.hpp"
#include "../../node/Identity.hpp"
#include "../../node/InetAddress.hpp"
#include "../../osdep/BlockingQueue.hpp"
#include "../../osdep/OSUtils.hpp"

#include <atomic>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <prometheus/simpleapi.h>
#include <set>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define ZT_MEMBER_AUTH_TIMEOUT_NOTIFY_BEFORE 25000

namespace ZeroTier {

struct AuthInfo {
  public:
	AuthInfo() : enabled(false), version(0), authenticationURL(), authenticationExpiryTime(0), issuerURL(), centralAuthURL(), ssoNonce(), ssoState(), ssoClientID(), ssoProvider("default")
	{
	}

	bool enabled;
	uint64_t version;
	std::string authenticationURL;
	uint64_t authenticationExpiryTime;
	std::string issuerURL;
	std::string centralAuthURL;
	std::string ssoNonce;
	std::string ssoState;
	std::string ssoClientID;
	std::string ssoProvider;
};

/**
 * Base class with common infrastructure for all controller DB implementations
 */
class DB {
  public:
	class ChangeListener {
	  public:
		ChangeListener()
		{
		}
		virtual ~ChangeListener()
		{
		}
		virtual void onNetworkUpdate(const void* db, uint64_t networkId, const nlohmann::json& network)
		{
		}
		virtual void onNetworkMemberUpdate(const void* db, uint64_t networkId, uint64_t memberId, const nlohmann::json& member)
		{
		}
		virtual void onNetworkMemberDeauthorize(const void* db, uint64_t networkId, uint64_t memberId)
		{
		}
	};

	struct NetworkSummaryInfo {
		NetworkSummaryInfo() : authorizedMemberCount(0), totalMemberCount(0), mostRecentDeauthTime(0)
		{
		}
		std::vector<Address> activeBridges;
		std::vector<InetAddress> allocatedIps;
		unsigned long authorizedMemberCount;
		unsigned long totalMemberCount;
		int64_t mostRecentDeauthTime;
	};

	static void initNetwork(nlohmann::json& network);
	static void initMember(nlohmann::json& member);
	static void cleanNetwork(nlohmann::json& network);
	static void cleanMember(nlohmann::json& member);

	DB();
	virtual ~DB();

	virtual bool waitForReady() = 0;
	virtual bool isReady() = 0;

	// Whether the backend can promptly and durably apply changes handed to save().
	// Backends with an async commit pipeline override this so message listeners can
	// defer (nack for redelivery) instead of acknowledging changes into a stalled or
	// drowning queue, where they would be lost if the process died or wedged.
	virtual bool commitPipelineHealthy()
	{
		return true;
	}

	inline bool hasNetwork(const uint64_t networkId) const
	{
		std::shared_lock<std::shared_mutex> l(_networks_l);
		return (_networks.find(networkId) != _networks.end());
	}

	bool get(const uint64_t networkId, nlohmann::json& network);
	bool get(const uint64_t networkId, nlohmann::json& network, const uint64_t memberId, nlohmann::json& member);
	bool get(const uint64_t networkId, nlohmann::json& network, const uint64_t memberId, nlohmann::json& member, NetworkSummaryInfo& info);
	bool get(const uint64_t networkId, nlohmann::json& network, std::vector<nlohmann::json>& members);

	void networks(std::set<uint64_t>& networks);

	template <typename F> inline void each(F f)
	{
		nlohmann::json nullJson;
		// Read-only traversal of _networks: take a shared lock so concurrent readers
		// (get/hasNetwork/...) aren't blocked for the whole iteration. The callback
		// must not mutate this DB's _networks (the only caller, DBMirrorSet's sync,
		// writes to other DBs, not this one).
		std::shared_lock<std::shared_mutex> lck(_networks_l);
		for (auto nw = _networks.begin(); nw != _networks.end(); ++nw) {
			f(nw->first, nw->second->config, 0, nullJson);	 // first provide network with 0 for member ID
			for (auto m = nw->second->members.begin(); m != nw->second->members.end(); ++m) {
				f(nw->first, nw->second->config, m->first, m->second);
			}
		}
	}

	virtual bool save(nlohmann::json& record, bool notifyListeners) = 0;
	virtual void eraseNetwork(const uint64_t networkId) = 0;
	virtual void eraseMember(const uint64_t networkId, const uint64_t memberId) = 0;
	virtual void nodeIsOnline(const uint64_t networkId, const uint64_t memberId, const InetAddress& physicalAddress) = 0;
	virtual void nodeIsOnline(const uint64_t networkId, const uint64_t memberId, const InetAddress& physicalAddress, const char* osArch) = 0;

	virtual AuthInfo getSSOAuthInfo(const nlohmann::json& member, const std::string& redirectURL)
	{
		return AuthInfo();
	}

	inline void addListener(DB::ChangeListener* const listener)
	{
		std::unique_lock<std::shared_mutex> l(_changeListeners_l);
		_changeListeners.push_back(listener);
	}

	// Snapshot the listener list so callbacks can be invoked without holding
	// _changeListeners_l. Listener callbacks fan out into the controller (mirror
	// saves, netconf re-requests) and must never run under this lock: one blocked
	// callback would otherwise wedge every thread that needs to notify, which is
	// how the whole commit pipeline froze in prod. Listeners are only ever added,
	// so the snapshotted pointers stay valid.
	inline std::vector<DB::ChangeListener*> _listenersSnapshot() const
	{
		std::shared_lock<std::shared_mutex> l(_changeListeners_l);
		return _changeListeners;
	}

	virtual void _memberChanged(nlohmann::json& old, nlohmann::json& memberConfig, bool notifyListeners);
	virtual void _networkChanged(nlohmann::json& old, nlohmann::json& networkConfig, bool notifyListeners);

  protected:
	static inline bool _compareRecords(const nlohmann::json& a, const nlohmann::json& b)
	{
		if (a.is_object() != b.is_object())
			return false;
		if (! a.is_object())
			return (a == b);

		// Ignore fields that are metadata about the change rather than record state:
		// "revision" is a monotonic counter, and "change_source" tags which frontend/
		// controller originated the write.  Neither should make an otherwise-identical
		// record look modified -- and "change_source" is frequently present on only one
		// side (a record freshly loaded from the DB carries none), so it can't be handled
		// by a plain size comparison.
		auto ignored = [](const std::string& k) { return (k == "revision") || (k == "change_source"); };

		auto amap = a.get<nlohmann::json::object_t>();
		auto bmap = b.get<nlohmann::json::object_t>();

		size_t aFields = 0;
		for (auto ai = amap.begin(); ai != amap.end(); ++ai) {
			if (ignored(ai->first))
				continue;
			++aFields;
			auto bi = bmap.find(ai->first);
			if ((bi == bmap.end()) || (bi->second != ai->second))
				return false;
		}

		// Ensure b has no extra (non-ignored) fields that a lacks.
		size_t bFields = 0;
		for (auto bi = bmap.begin(); bi != bmap.end(); ++bi) {
			if (! ignored(bi->first))
				++bFields;
		}
		return aFields == bFields;
	}

	struct _Network {
		_Network() : mostRecentDeauthTime(0)
		{
		}
		nlohmann::json config;
		std::unordered_map<uint64_t, nlohmann::json> members;
		std::unordered_set<uint64_t> activeBridgeMembers;
		std::unordered_set<uint64_t> authorizedMembers;
		std::unordered_set<InetAddress, InetAddress::Hasher> allocatedIps;
		int64_t mostRecentDeauthTime;
		std::shared_mutex lock;
	};

	void _fillSummaryInfo(const std::shared_ptr<_Network>& nw, NetworkSummaryInfo& info);

	std::vector<DB::ChangeListener*> _changeListeners;
	std::unordered_map<uint64_t, std::shared_ptr<_Network> > _networks;
	std::unordered_multimap<uint64_t, uint64_t> _networkByMember;
	mutable std::shared_mutex _changeListeners_l;
	mutable std::shared_mutex _networks_l;
};

}	// namespace ZeroTier

#endif
