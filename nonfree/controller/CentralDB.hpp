/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#ifdef ZT_CONTROLLER_USE_LIBPQ

#ifndef ZT_CONTROLLER_CENTRAL_DB_HPP
#define ZT_CONTROLLER_CENTRAL_DB_HPP

#define ZT_CENTRAL_CONTROLLER_COMMIT_THREADS 8

// Max times an INTERNAL (non-PubSub) write is re-queued after a failed DB write before it is
// dropped. PubSub-sourced changes are acked/nacked after the commit (ack-after-commit): a failed
// write nacks so Pub/Sub redelivers, and the subscription's dead-letter policy bounds retries --
// so this cap governs only internal writes, which have no redelivery and are buffered in memory.
#define ZT_CENTRAL_CONTROLLER_MAX_COMMIT_RETRIES 100

#include "../../node/Metrics.hpp"
#include "ConnectionPool.hpp"
#include "DB.hpp"
#include "NotificationListener.hpp"
#include "PostgreSQL.hpp"
#include "StatusWriter.hpp"

#include <condition_variable>
#include <memory>
#include <pqxx/pqxx>
#include <sw/redis++/redis++.h>

namespace ZeroTier {
struct RedisConfig;
struct ControllerConfig;
class ControllerChangeNotifier;
class PubSubWriter;

class CentralDB : public DB {
  public:
	enum ListenerMode {
		LISTENER_MODE_PGSQL = 0,
		LISTENER_MODE_REDIS = 1,
		LISTENER_MODE_PUBSUB = 2,
	};

	enum StatusWriterMode {
		STATUS_WRITER_MODE_PGSQL = 0,
		STATUS_WRITER_MODE_REDIS = 1,
		STATUS_WRITER_MODE_BIGTABLE = 2,
	};

	CentralDB(const Identity& myId,
			  const char* connString,
			  int listenPort,
			  CentralDB::ListenerMode mode,
			  CentralDB::StatusWriterMode statusMode,
			  const ControllerConfig* cc);
	virtual ~CentralDB();

	virtual bool waitForReady();
	virtual bool isReady();
	virtual bool save(nlohmann::json& record, bool notifyListeners);
	virtual void eraseNetwork(const uint64_t networkId);
	virtual void eraseMember(const uint64_t networkId, const uint64_t memberId);
	virtual void nodeIsOnline(const uint64_t networkId, const uint64_t memberId, const InetAddress& physicalAddress);
	virtual void nodeIsOnline(const uint64_t networkId,
							  const uint64_t memberId,
							  const InetAddress& physicalAddress,
							  const char* osArch);
	virtual AuthInfo getSSOAuthInfo(const nlohmann::json& member, const std::string& redirectURL);

	virtual bool ready()
	{ return _ready == 2; }

	virtual void _memberChanged(nlohmann::json& old, nlohmann::json& memberConfig, bool notifyListeners)
	{ DB::_memberChanged(old, memberConfig, notifyListeners); }

	virtual void _networkChanged(nlohmann::json& old, nlohmann::json& networkConfig, bool notifyListeners)
	{ DB::_networkChanged(old, networkConfig, notifyListeners); }

  protected:
	struct _PairHasher {
		inline std::size_t operator()(const std::pair<uint64_t, uint64_t>& p) const
		{ return (std::size_t)(p.first ^ p.second); }
	};

  private:
	void initializeNetworks();
	void initializeMembers();
	void heartbeat();

	void commitThread();
	void onlineNotificationThread();

	nlohmann::json _getNetworkMember(pqxx::work& tx, const std::string networkID, const std::string memberID);

	nlohmann::json _getNetwork(pqxx::work& tx, const std::string networkID);

  private:
	enum OverrideMode { ALLOW_PGBOUNCER_OVERRIDE = 0, NO_OVERRIDE = 1 };

	ListenerMode _listenerMode;
	StatusWriterMode _statusWriterMode;
	const ControllerConfig* _cc;
	std::string _assignedCentralVersion;
	std::shared_ptr<ConnectionPool<PostgresConnection> > _pool;

	const Identity _myId;
	const Address _myAddress;
	std::string _myAddressStr;
	std::string _connString;

	struct _queueItem {
		_queueItem() : jsonData(), notifyListeners(false), traceContext(), retryCount(0), completion()
		{
		}

		~_queueItem()
		{
		}

		nlohmann::json jsonData;
		bool notifyListeners;
		std::map<std::string, std::string> traceContext;
		int retryCount;
		// Deferred ack for the delivery mechanism (PubSub). Null for internal writes
		// (no delivery message) and for backends without redelivery semantics. When set,
		// the commit thread completes it (ack on success, nack on transient failure)
		// instead of re-queuing -- Pub/Sub's own redelivery replaces _requeueFailedCommit.
		std::shared_ptr<NotificationCompletion> completion;
	};
	BlockingQueue<_queueItem> _commitQueue;

	// Re-queue a change whose DB write failed, with capped backoff, until it
	// succeeds or ZT_CENTRAL_CONTROLLER_MAX_COMMIT_RETRIES attempts is reached.
	// Used only for items with no delivery completion (internal writes); items that
	// carry one are nacked for redelivery instead (see _finishCommit).
	void _requeueFailedCommit(_queueItem& qitem);

	// Resolve a commit-queue item's outcome exactly once: if it carries a delivery
	// completion (PubSub), ack on Ok/PermanentFailure or nack on TransientFailure;
	// otherwise fall back to _requeueFailedCommit on TransientFailure (internal writes).
	void _finishCommit(_queueItem& qitem, NotificationResult result);

	std::thread _heartbeatThread;
	std::shared_ptr<NotificationListener> _membersDbWatcher;
	std::shared_ptr<NotificationListener> _networksDbWatcher;
	std::shared_ptr<StatusWriter> _statusWriter;
	std::shared_ptr<ControllerChangeNotifier> _changeNotifier;
	std::shared_ptr<NotificationListener> _ssoAuthListener;
	std::shared_ptr<PubSubWriter> _ssoNonceWriter;
	std::thread _commitThread[ZT_CENTRAL_CONTROLLER_COMMIT_THREADS];
	std::thread _onlineNotificationThread;

	std::unordered_map<std::pair<uint64_t, uint64_t>, NodeOnlineRecord, _PairHasher> _lastOnline;

	mutable std::mutex _lastOnline_l;
	mutable std::mutex _readyLock;
	std::condition_variable _readyCv;	// signaled once _ready reaches 2 (initial load complete)
	std::atomic<int> _ready, _connected, _run;
	mutable volatile bool _waitNoticePrinted;

	int _listenPort;
	uint8_t _ssoPsk[48];

	std::shared_ptr<sw::redis::Redis> _redis;
	std::shared_ptr<sw::redis::RedisCluster> _cluster;
	bool _redisMemberStatus;
};

}	// namespace ZeroTier

#endif	 // ZT_CONTROLLER_CENTRAL_DB_HPP
#endif	 // ZT_CONTROLLER_USE_LIBPQ